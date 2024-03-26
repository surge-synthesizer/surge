/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "PatchDB.h"

#include <sstream>
#include <iterator>
#include <chrono>
#include <functional>

#include "sqlite3.h"
#include "SurgeStorage.h"
#include "DebugHelpers.h"

#include "sst/basic-blocks/mechanics/endian-ops.h"
#include "PatchFileHeaderStructs.h"

namespace mech = sst::basic_blocks::mechanics;

#define TRACE_DB 0

namespace Surge
{
namespace PatchStorage
{

namespace SQL
{
struct Exception : public std::runtime_error
{
    explicit Exception(sqlite3 *h) : std::runtime_error(sqlite3_errmsg(h)), rc(sqlite3_errcode(h))
    {
        Surge::Debug::stackTraceToStdout();
    }
    Exception(int rc, const std::string &msg) : std::runtime_error(msg), rc(rc)
    {
        Surge::Debug::stackTraceToStdout();
    }
    const char *what() const noexcept override
    {
        static char msg[1024];
        snprintf(msg, 1024, "SQL Error[%d]: %s", rc, std::runtime_error::what());
        return msg;
    }
    int rc;
};

struct LockedException : public Exception
{
    explicit LockedException(sqlite3 *h) : Exception(h)
    {
        // Surge::Debug::stackTraceToStdout();
    }
    LockedException(int rc, const std::string &msg) : Exception(rc, msg) {}
    const char *what() const noexcept override
    {
        static char msg[1024];
        snprintf(msg, 1024, "SQL Locked Error[%d]: %s", rc, std::runtime_error::what());
        return msg;
    }
    int rc;
};

void Exec(sqlite3 *h, const std::string &statement)
{
    char *emsg;
    auto rc = sqlite3_exec(h, statement.c_str(), nullptr, nullptr, &emsg);
    if (rc != SQLITE_OK)
    {
        std::string sm = emsg;
        sqlite3_free(emsg);
        throw Exception(rc, sm);
    }
}

/*
 * A little class to make prepared statements less clumsy
 */
struct Statement
{
    bool prepared{false};
    std::string statementCopy;
    Statement(sqlite3 *h, const std::string &statement) : h(h), statementCopy(statement)
    {
        auto rc = sqlite3_prepare_v2(h, statement.c_str(), -1, &s, nullptr);
        if (rc != SQLITE_OK)
            throw Exception(rc, "Unable to prepare statement [" + statement + "]");
        prepared = true;
    }
    ~Statement()
    {
        if (prepared)
        {
            std::cout << "ERROR: Prepared Statement never Finalized \n"
                      << statementCopy << "\n"
                      << std::endl;
        }
    }
    void finalize()
    {
        if (s)
            if (sqlite3_finalize(s) != SQLITE_OK)
                throw Exception(h);
        prepared = false;
    }

    int col_int(int c) const { return sqlite3_column_int(s, c); }
    int64_t col_int64(int c) const { return sqlite3_column_int64(s, c); }
    const char *col_charstar(int c) const
    {
        return reinterpret_cast<const char *>(sqlite3_column_text(s, c));
    }
    std::string col_str(int c) const { return col_charstar(c); }

    void bind(int c, const std::string &val)
    {
        if (!s)
            throw Exception(-1, "Statement not initialized in bind");

        auto rc = sqlite3_bind_text(s, c, val.c_str(), val.length(), SQLITE_STATIC);
        if (rc != SQLITE_OK)
            throw Exception(h);
    }

    void bind(int c, int val)
    {
        if (!s)
            throw Exception(-1, "Statement not initialized in bind");

        auto rc = sqlite3_bind_int(s, c, val);
        if (rc != SQLITE_OK)
            throw Exception(h);
    }

    void bindi64(int c, int64_t val)
    {
        if (!s)
            throw Exception(-1, "Statement not initialized in bind");

        auto rc = sqlite3_bind_int64(s, c, val);
        if (rc != SQLITE_OK)
            throw Exception(h);
    }

    void clearBindings()
    {
        if (!s)
            throw Exception(-1, "Statement not initialized in bind");

        auto rc = sqlite3_clear_bindings(s);
        if (rc != SQLITE_OK)
            throw Exception(h);
    }

    void reset()
    {
        if (!s)
            throw Exception(-1, "Statement not initialized in bind");

        auto rc = sqlite3_reset(s);
        if (rc != SQLITE_OK)
            throw Exception(h);
    }

    bool step() const
    {
        if (!s)
            throw Exception(-1, "Statement not initialized in step");

        auto rc = sqlite3_step(s);
        if (rc == SQLITE_ROW)
            return true;
        if (rc == SQLITE_DONE)
            return false;
        throw Exception(h);
    }

    sqlite3_stmt *s{nullptr};
    sqlite3 *h;
};

/*
 * RAII on transactions
 */
struct TxnGuard
{
    bool open{false};
    explicit TxnGuard(sqlite3 *e) : d(e)
    {
        auto rc = sqlite3_exec(d, "BEGIN IMMEDIATE TRANSACTION", nullptr, nullptr, nullptr);
        if (rc == SQLITE_LOCKED || rc == SQLITE_BUSY)
        {
            throw LockedException(d);
        }
        else if (rc != SQLITE_OK)
        {
            throw Exception(d);
        }
        open = true;
    }

    void end()
    {
        auto rc = sqlite3_exec(d, "END TRANSACTION", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
        {
            throw Exception(d);
        }
        open = false;
    }
    ~TxnGuard()
    {
        if (open)
        {
            auto rc = sqlite3_exec(d, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
        }
    }
    sqlite3 *d;
};
} // namespace SQL

struct PatchDB::WriterWorker
{
    static constexpr const char *schema_version = "14"; // I will rebuild if this is not my version

    static constexpr const char *setup_sql = R"SQL(
DROP TABLE IF EXISTS "Patches";
DROP TABLE IF EXISTS "PatchFeature";
DROP TABLE IF EXISTS "Version";
DROP TABLE IF EXISTS "Category";
DROP TABLE IF EXISTS "DebugJunk";
CREATE TABLE "Version" (
    id integer primary key,
    schema_version varchar(256)
);
CREATE TABLE "Patches" (
      id integer primary key,
      path varchar(2048),
      name varchar(256),
      search_over varchar(1024),
      category varchar(2048),
      category_type int,
      last_write_time big int
);
CREATE TABLE PatchFeature (
      id integer primary key,
      patch_id integer,
      feature varchar(64),
      feature_type int,
      feature_ivalue int,
      feature_svalue varchar(64)
);
CREATE TABLE Category (
      id integer primary key,
      name varchar(2048),
      leaf_name varchar(256),
      isroot int,
      type int,
      parent_id int
);
CREATE TABLE DebugJunk (
    id integer primary key,
    junk varchar(2048)
)
    )SQL";

    // language=SQL
    static constexpr const char *setup_user = R"SQL(
CREATE TABLE IF NOT EXISTS Favorites (
    id integer primary key,
    path varchar(2048)
);
)SQL";
    struct EnQAble
    {
        virtual ~EnQAble() = default;
        virtual void go(WriterWorker &) = 0;
    };

    struct EnQPatch : public EnQAble
    {
        EnQPatch(const fs::path &p, const std::string &n, const std::string &cn, const CatType t)
            : path(p), name(n), catname(cn), type(t)
        {
        }
        fs::path path;
        std::string name;
        std::string catname;
        CatType type;

        void go(WriterWorker &w) override { w.parseFXPIntoDB(*this); }
    };

    struct EnQDebugMsg : public EnQAble
    {
        std::string msg;
        EnQDebugMsg(const std::string &msg) : msg(msg) {}
        void go(WriterWorker &w) override { w.addDebug(msg); }
    };

    struct EnQLambda : public EnQAble
    {
        std::function<void()> op{nullptr};
        EnQLambda(std::function<void()> iop) : op(iop) {}
        void go(WriterWorker &w) override
        {
            if (op)
                op();
        }
    };

    struct EnQFavorite : public EnQAble
    {
        std::string path;
        bool value;
        EnQFavorite(const std::string &m, bool v) : path(m), value(v) {}
        void go(WriterWorker &w) override { w.setFavorite(path, value); }
    };

    struct EnQDelete : public EnQAble
    {
        int id;
        EnQDelete(int i) : id(i) {}
        void go(WriterWorker &w) override { w.erasePatch(id); }
    };

    struct EnQCategory : public EnQAble
    {
        std::string name;
        std::string leafname;
        std::string parentname;
        CatType type;
        bool isroot;

        EnQCategory(const std::string &name, CatType t)
            : isroot(true), name(name), leafname(name), parentname(""), type(t)
        {
        }
        EnQCategory(const std::string &name, const std::string &parent, CatType t)
            : isroot(false), name(name), parentname(parent), type(t)
        {
            auto q = fs::path(name);
            auto l = q.filename();
            leafname = l.u8string(); // FIXME
        }
        void go(WriterWorker &w) override
        {
            if (isroot)
                w.addRootCategory(name, type);
            else
                w.addChildCategory(name, leafname, parentname, type);
        }
    };

    void openDb()
    {
#if TRACE_DB
        std::cout << ">>> Opening r/w DB" << std::endl;
#endif
        auto flag = SQLITE_OPEN_NOMUTEX; // basically lock
        flag |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

        auto ec = sqlite3_open_v2(dbname.c_str(), &dbh, flag, nullptr);

        if (ec != SQLITE_OK)
        {
            std::ostringstream oss;
            oss << "An error occurred opening sqlite file '" << dbname << "'. The error was '"
                << sqlite3_errmsg(dbh) << "'.";
            storage->reportError(oss.str(), "Surge Patch Database Error");
            if (dbh)
            {
                // even if opening fails we still need to close the database
                sqlite3_close(dbh);
            }
            dbh = nullptr;
            return;
        }
    }

    void closeDb()
    {
#if TRACE_DB
        std::cout << "<<<< Closing r/w DB" << std::endl;
#endif
        if (dbh)
            sqlite3_close(dbh);
        dbh = nullptr;
    }

    std::string dbname;
    fs::path dbpath;

    explicit WriterWorker(SurgeStorage *storage) : storage(storage)
    {
        dbpath = storage->userDataPath / fs::path{"SurgePatches.db"};
        dbname = path_to_string(dbpath);
    }

    struct EnQSetup : EnQAble
    {
        EnQSetup() {}
        void go(WriterWorker &w)
        {
            w.setupDatabase();
#if TRACE_DB
            std::cout << "Done with EnQSetup" << std::endl;
#endif
        }
    };

    std::atomic<bool> hasSetup{false};
    void setupDatabase()
    {
#if TRACE_DB
        std::cout << "PatchDB : Setup Database " << dbname << std::endl;
#endif
        /*
         * OK check my version
         */
        bool rebuild = true;
        try
        {
            auto st = SQL::Statement(dbh, "SELECT * FROM Version");

            while (st.step())
            {
                int id = st.col_int(0);
                auto ver = st.col_charstar(1);
#if TRACE_DB
                std::cout << "        : schema check. DBVersion='" << ver << "' SchemaVersion='"
                          << schema_version << "'" << std::endl;
#endif
                if (strcmp(ver, schema_version) == 0)
                {
#if TRACE_DB
                    std::cout << "        : Schema matches. Reusing database." << std::endl;
#endif
                    rebuild = false;
                }
            }

            st.finalize();
        }
        catch (const SQL::Exception &e)
        {
            rebuild = true;
            /*
             * In this case, we choose to not report the error since it means
             * that we just need to rebuild everything
             */
            // storage->reportError(e.what(), "SQLLite3 Startup Error");
        }

        char *emsg;
        if (rebuild)
        {
#if TRACE_DB
            std::cout << "        : Schema missing or mismatched. Dropping and Rebuilding Database."
                      << std::endl;
#endif
            try
            {
                SQL::Exec(dbh, setup_sql);
                auto versql = std::string("INSERT INTO VERSION (\"schema_version\") VALUES (\"") +
                              schema_version + "\")";
                SQL::Exec(dbh, versql);

                SQL::Exec(dbh, setup_user);
            }
            catch (const SQL::Exception &e)
            {
                storage->reportError(e.what(), "PatchDB Setup Error");
            }
        }

        hasSetup = true;
    }

    bool haveOpenedForWriteOnce{false};
    void openForWrite()
    {
        if (haveOpenedForWriteOnce)
            return;
        // We know this is called in the lock so can manipulate pathQ properly
        haveOpenedForWriteOnce = true;
        qThread = std::thread([this]() { this->loadQueueFunction(); });

        {
            std::lock_guard<std::mutex> g(qLock);
            pathQ.push_back(new EnQSetup());
        }
        qCV.notify_all();
        while (!waiting)
        {
        }
    }

    ~WriterWorker()
    {
        if (haveOpenedForWriteOnce)
        {
            keepRunning = false;
            qCV.notify_all();
            qThread.join();
            // clean up all the prepared statements
            if (dbh)
                sqlite3_close(dbh);
            dbh = nullptr;
        }

        if (rodbh)
        {
            sqlite3_close(rodbh);
            rodbh = nullptr;
        }
    }

    // FIXME features should be an enum or something

    enum FeatureType
    {
        INT,
        STRING
    };
    typedef std::tuple<std::string, FeatureType, int, std::string> feature;
    std::vector<feature> extractFeaturesFromXML(const char *xml)
    {
        std::vector<feature> res;
        TiXmlDocument doc;
        doc.Parse(xml, nullptr, TIXML_ENCODING_LEGACY);
        if (doc.Error())
        {
#if TRACE_DB
            std::cout << "        - ERROR: Unable to load XML; Skipping file" << std::endl;
#endif
            return res;
        }

        auto patch = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("patch"));

        if (!patch)
        {
#if TRACE_DB
            std::cout << "        - ERROR: No 'patch' element in XML" << std::endl;
#endif
            return res;
        }

        int rev = 0;
        if (patch->QueryIntAttribute("revision", &rev) == TIXML_SUCCESS)
        {
            res.emplace_back("REVISION", INT, rev, "");
        }
        else
        {
#if TRACE_DB
            std::cout << "        - ERROR: No Revision in XML" << std::endl;
#endif
            return res;
        }

        auto meta = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("meta"));
        if (meta)
        {
            if (meta->Attribute("author"))
            {
                res.emplace_back("AUTHOR", STRING, 0, meta->Attribute("author"));
            }

            auto tags = TINYXML_SAFE_TO_ELEMENT(meta->FirstChild("tags"));
            if (tags)
            {
                auto tag = tags->FirstChildElement();
                while (tag)
                {
                    res.emplace_back("TAG", STRING, 0, tag->Attribute("tag"));
                    tag = tag->NextSiblingElement();
                }
            }
        }

        auto parameters = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("parameters"));
        auto par = parameters->FirstChildElement();

        while (par)
        {
            std::string s = par->Value();
            if (s == "scenemode")
            {
                int sm;
                if (par->QueryIntAttribute("value", &sm) == TIXML_SUCCESS)
                {
                    res.emplace_back("SCENE_MODE", STRING, 0, scene_mode_names[sm]);
                }
            }

            if (s.find("fx") == 0 && s.find("_type") != std::string::npos)
            {
                int sm;
                if (par->QueryIntAttribute("value", &sm) == TIXML_SUCCESS)
                {
                    if (sm != 0)
                    {
                        res.emplace_back("FX", STRING, 0, fx_type_shortnames[sm]);
                    }
                }
            }
            if (s.find("_filter") != std::string::npos && s.find("_type") != std::string::npos)
            {
                int sm;
                if (par->QueryIntAttribute("value", &sm) == TIXML_SUCCESS)
                {
                    if (sm != 0)
                    {
                        res.emplace_back("FILTER", STRING, 0, sst::filters::filter_type_names[sm]);
                    }
                }
            }

            par = par->NextSiblingElement();
        }

        return res;
    }

    /*
     * Functions for the write thread
     */
    std::atomic<bool> waiting{false};
    void loadQueueFunction()
    {
        static constexpr auto transChunkSize = 10; // How many FXP to load in a single txn
        int lock_retries{0};
        while (keepRunning)
        {
            std::vector<EnQAble *> doThis;
            {
                std::unique_lock<std::mutex> lk(qLock);

                while (keepRunning && pathQ.empty())
                {
                    if (dbh)
                        closeDb();
                    waiting = true;
                    qCV.wait(lk);
                    waiting = false;
                }

                if (keepRunning)
                {
                    auto b = pathQ.begin();
                    auto e = (pathQ.size() < transChunkSize) ? pathQ.end()
                                                             : pathQ.begin() + transChunkSize;
                    std::copy(b, e, std::back_inserter(doThis));
                    pathQ.erase(b, e);
                }
            }
            if (!doThis.empty())
            {
                if (!dbh)
                    openDb();
                if (dbh == nullptr)
                {
                }
                else
                {
                    try
                    {
                        SQL::TxnGuard tg(dbh);

                        for (auto *p : doThis)
                        {
                            p->go(*this);
                            delete p;
                        }

                        tg.end();
                    }
                    catch (SQL::LockedException &le)
                    {
                        std::ostringstream oss;
                        oss << le.what() << "\n"
                            << "Patch database is locked for writing. Most likely, another Surge "
                               "XT instance has exclusive write access. We will attempt to retry "
                               "writing up to 10 more times. "
                               "Please dismiss this error in the meantime!\n\n Attempt: "
                            << lock_retries;
                        storage->reportError(oss.str(), "Patch Database Locked");
                        // OK so in this case, we reload doThis onto the front of the queue and
                        // sleep
                        lock_retries++;
                        if (lock_retries < 10)
                        {
                            {
                                std::unique_lock<std::mutex> lk(qLock);
                                std::reverse(doThis.begin(), doThis.end());
                                for (auto p : doThis)
                                {
                                    pathQ.push_front(p);
                                }
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(lock_retries * 3));
                        }
                        else
                        {
                            storage->reportError(
                                "Database is locked and unwritable after multiple attempts!",
                                "Patch Database Locked");
                        }
                    }
                    catch (SQL::Exception &e)
                    {
                        storage->reportError(e.what(), "Patch DB");
                    }
                }
            }
        }
    }

    void parseFXPIntoDB(const EnQPatch &p)
    {
        sqlite3_stmt *insertStmt = nullptr, *insfeatureStmt = nullptr, *dropIdStmt = nullptr;

        if (!fs::exists(p.path))
        {
#if TRACE_DB
            std::cout << "    - Warning: Non existent " << path_to_string(p.path) << std::endl;
#endif
            return;
        }
        // Check with
        auto qtime = fs::last_write_time(p.path);
        int64_t qtimeInt =
            std::chrono::duration_cast<std::chrono::seconds>(qtime.time_since_epoch()).count();

        bool patchLoaded = false;
        std::vector<int> dropIds;
        try
        {
            auto exists = SQL::Statement(
                dbh, "SELECT id, last_write_time from Patches WHERE Patches.Path LIKE ?1");
            const auto path(p.path.u8string());
            exists.bind(1, path);

            // Drop all the ones with this path independent of time if I'm adding
            while (exists.step())
            {
                auto id = exists.col_int(0);
                auto t = exists.col_int64(1);
                dropIds.push_back(id);
            }

            exists.finalize();

            if (!dropIds.empty())
            {
                auto drop = SQL::Statement(dbh, "DELETE FROM Patches WHERE ID=?1;");
                for (auto did : dropIds)
                {
                    drop.bind(1, did);
                    while (drop.step())
                    {
                    }
                    drop.clearBindings();
                    drop.reset();
                }

                drop.finalize();

                auto dropF = SQL::Statement(dbh, "DELETE FROM PatchFeature WHERE PATCH_ID=?1;");
                for (auto did : dropIds)
                {
                    dropF.bind(1, did);
                    while (dropF.step())
                    {
                    }
                    dropF.clearBindings();
                    dropF.reset();
                }

                dropF.finalize();
            }
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Load Check");
            return;
        }

        if (patchLoaded)
            return;

        int64_t patchid = -1;
        try
        {
            auto ins = SQL::Statement(dbh, "INSERT INTO PATCHES ( \"path\", \"name\", "
                                           "\"category\", \"category_type\", \"last_write_time\" ) "
                                           "VALUES ( ?1, ?2, ?3, ?4, ?5 )");
            const auto path(p.path.u8string());
            ins.bind(1, path);
            ins.bind(2, p.name);
            ins.bind(3, p.catname);
            ins.bind(4, (int)p.type);
            ins.bindi64(5, qtimeInt);

            ins.step();

            // No real need to encapsulate this
            patchid = sqlite3_last_insert_rowid(dbh);

            ins.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Insert Patch");
            return;
        }

        std::ostringstream searchName;
        searchName << p.name << " ";

        if (storage)
        {
            auto pTmp = p.path.parent_path();
            std::vector<fs::path> parentFiles;
            int maxItForSafety{0};
            while ((pTmp != storage->userPatchesPath) &&
                   (pTmp != storage->datapath / "patches_factory") &&
                   (pTmp != storage->datapath / "patches_3rdparty") && !pTmp.empty() &&
                   (pTmp != pTmp.root_directory()) && maxItForSafety < 10)
            {
                parentFiles.push_back(pTmp.filename());
                pTmp = pTmp.parent_path();
                maxItForSafety++;
            }

            if (pTmp == storage->datapath / "patches_3rdparty")
            {
                parentFiles.erase(parentFiles.end() - 1);
            }

            for (auto pf : parentFiles)
            {
                searchName << pf.u8string() << " ";
            }
        }

        std::ifstream stream(p.path, std::ios::in | std::ios::binary);

        std::vector<char> fxChunk;
        fxChunk.resize(sizeof(sst::io::fxChunkSetCustom));
        stream.read(fxChunk.data(), fxChunk.size());
        if (!stream)
        {
            return;
        }

        auto *fxp = (sst::io::fxChunkSetCustom *)(fxChunk.data());
        if ((mech::endian_read_int32BE(fxp->chunkMagic) != 'CcnK') ||
            (mech::endian_read_int32BE(fxp->fxMagic) != 'FPCh') ||
            (mech::endian_read_int32BE(fxp->fxID) != 'cjs3'))
        {
            return;
        }

        std::vector<char> patchHeaderChunk;
        patchHeaderChunk.resize(sizeof(sst::io::patch_header));
        stream.read(patchHeaderChunk.data(), patchHeaderChunk.size());
        if (!stream)
        {
            return;
        }
        auto *ph = (sst::io::patch_header *)(patchHeaderChunk.data());
        auto xmlSz = mech::endian_read_int32LE(ph->xmlsize);

        if (!memcpy(ph->tag, "sub3", 4) || xmlSz < 0 || xmlSz > 1024 * 1024 * 1024)
        {
            std::cerr << "Skipping invalid patch : [" << p.path.u8string() << "]" << std::endl;
            return;
        }

        std::vector<char> xmlData;
        xmlData.resize(xmlSz);
        stream.read(xmlData.data(), xmlData.size());
        if (!stream)
            return;
        try
        {
            auto ins =
                SQL::Statement(dbh, "INSERT INTO PATCHFEATURE ( \"patch_id\", \"feature\", "
                                    "\"feature_type\", \"feature_ivalue\", \"feature_svalue\" ) "
                                    "VALUES ( ?1, ?2, ?3, ?4, ?5 )");
            auto feat = extractFeaturesFromXML(xmlData.data());
            for (auto f : feat)
            {
                auto ftype = std::get<0>(f);
                ins.bindi64(1, patchid);
                ins.bind(2, std::get<0>(f));
                ins.bind(3, (int)std::get<1>(f));
                ins.bind(4, std::get<2>(f));
                ins.bind(5, std::get<3>(f));

                ins.step();

                ins.clearBindings();
                ins.reset();
                if (ftype == "TAG")
                {
                    searchName << " " << std::get<3>(f);
                }
            }

            ins.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - FXP Features");
            return;
        }

        auto sns = searchName.str();
        try
        {
            auto ins = SQL::Statement(dbh, "UPDATE PATCHES SET search_over=?1 WHERE id=?2");
            ins.bind(1, sns);
            ins.bind(2, patchid);

            ins.step();
            ins.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - FXP Features");
            return;
        }
    }

    void setFavorite(const std::string &p, bool v)
    {
        try
        {
            if (v)
            {
                auto there = SQL::Statement(dbh, "INSERT INTO Favorites  (\"path\") VALUES (?1)");
                there.bind(1, p);
                there.step();
                there.finalize();
            }
            else
            {
                auto there = SQL::Statement(dbh, "DELETE FROM Favorites WHERE path = ?1");
                there.bind(1, p);
                there.step();
                there.finalize();
            }
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Junk gave Junk");
        }
    }

    void erasePatch(int id)
    {
        try
        {
            auto there = SQL::Statement(dbh, "DELETE FROM Patches WHERE id=?");
            there.bind(1, id);
            there.step();
            there.finalize();

            auto feat = SQL::Statement(dbh, "DELETE FROM PatchFeature where patch_id=?");
            feat.bind(1, id);
            feat.step();
            feat.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Junk gave Junk");
        }
    }

    void addDebug(const std::string &m)
    {
        try
        {
            auto there = SQL::Statement(dbh, "INSERT INTO DebugJunk  (\"junk\") VALUES (?1)");
            there.bind(1, m);
            there.step();
            there.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Junk gave Junk");
        }
    }
    void addRootCategory(const std::string &name, CatType type)
    {
        // Check if it is there
        try
        {
            auto there =
                SQL::Statement(dbh, "SELECT COUNT(id) from Category WHERE Category.name LIKE ?1 "
                                    "AND Category.type = ?2 AND Category.isroot = 1");
            there.bind(1, name);
            there.bind(2, (int)type);
            there.step();
            auto count = there.col_int(0);
            there.finalize();
            if (count > 0)
                return;
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Category Query");
        }

        try
        {
            auto add = SQL::Statement(dbh, "INSERT INTO Category ( \"name\", \"leaf_name\", "
                                           "\"isroot\", \"type\", \"parent_id\" ) "
                                           "VALUES ( ?1, ?1, 1, ?2, -1 )");
            add.bind(1, name);
            add.bind(2, (int)type);
            add.step();

            add.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Category Root Insert");
        }
    }

    void addChildCategory(const std::string &name, const std::string &leafname,
                          const std::string &parentName, CatType type)
    {
        try
        {
            auto there =
                SQL::Statement(dbh, "SELECT COUNT(id) from Category WHERE Category.name LIKE ?1 "
                                    "AND Category.type = ?2 AND Category.isroot = 0");
            there.bind(1, name);
            there.bind(2, (int)type);
            there.step();
            auto count = there.col_int(0);

            there.finalize();

            if (count > 0)
                return;
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Category Query");
        }

        try
        {
            int parentId = -1;
            auto par = SQL::Statement(
                dbh, "SELECT id from Category WHERE Category.name LIKE ?1 AND Category.type = ?2");
            par.bind(1, parentName);
            par.bind(2, (int)type);
            if (par.step())
                parentId = par.col_int(0);

            auto add = SQL::Statement(dbh, "INSERT INTO Category ( \"name\", \"leaf_name\", "
                                           "\"isroot\", \"type\", \"parent_id\" ) "
                                           "VALUES ( ?1, ?2, 0, ?3, ?4 )");
            add.bind(1, name);
            add.bind(2, leafname);
            add.bind(3, (int)type);
            add.bind(4, parentId);
            add.step();

            add.finalize();
            par.finalize();
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Category Root Insert");
        }
    }

    // FIXME for now I am coding this with a locked vector but probably a
    // thread safe queue is the way to go
    std::thread qThread;
    std::mutex qLock;
    std::condition_variable qCV;
    std::deque<EnQAble *> pathQ;
    std::atomic<bool> keepRunning{true};

    /*
     * Call this from any thread
     */
    void enqueueWorkItem(EnQAble *p)
    {
        {
            std::lock_guard<std::mutex> g(qLock);

            pathQ.push_back(p);
        }
        qCV.notify_all();
    }

    sqlite3 *getReadOnlyConn(bool notifyOnError = true)
    {
        if (!rodbh)
        {
            auto flag = SQLITE_OPEN_NOMUTEX; // basically lock
            flag |= SQLITE_OPEN_READONLY;

            // std::cout << ">>>RO> Opening r/o DB" << std::endl;
            auto ec = sqlite3_open_v2(dbname.c_str(), &rodbh, flag, nullptr);

            if (ec != SQLITE_OK)
            {
                if (notifyOnError)
                {
                    std::ostringstream oss;
                    oss << "An error occurred opening r/o sqlite file '" << dbname
                        << "'. The error was '" << sqlite3_errmsg(dbh) << "'.";
                    storage->reportError(oss.str(), "Surge Patch Database Error");
                }
                if (rodbh)
                    sqlite3_close(rodbh);
                rodbh = nullptr;
            }
        }
        return rodbh;
    }

  private:
    sqlite3 *rodbh{nullptr};
    sqlite3 *dbh{nullptr};
    SurgeStorage *storage;
};
PatchDB::PatchDB(SurgeStorage *s) : storage(s) { initialize(); }

PatchDB::~PatchDB() = default;

void PatchDB::initialize()
{
    if (!worker)
        worker = std::make_unique<WriterWorker>(storage);
}
void PatchDB::prepareForWrites() { worker->openForWrite(); }

void PatchDB::considerFXPForLoad(const fs::path &fxp, const std::string &name,
                                 const std::string &catName, const CatType type) const
{
    worker->enqueueWorkItem(new WriterWorker::EnQPatch(fxp, name, catName, type));
}

void PatchDB::addRootCategory(const std::string &name, CatType type)
{
    worker->enqueueWorkItem(new WriterWorker::EnQCategory(name, type));
}
void PatchDB::addSubCategory(const std::string &name, const std::string &parent, CatType type)
{
    worker->enqueueWorkItem(new WriterWorker::EnQCategory(name, parent, type));
}
void PatchDB::addDebugMessage(const std::string &debug)
{
    worker->enqueueWorkItem(new WriterWorker::EnQDebugMsg(debug));
}

void PatchDB::doAfterCurrentQueueDrained(std::function<void()> op)
{
    worker->enqueueWorkItem(new WriterWorker::EnQLambda(op));
}

std::vector<std::pair<std::string, int>> PatchDB::readAllFeatures()
{

    std::vector<std::pair<std::string, int>> res;

    // language=SQL
    std::string query = "SELECT DISTINCT feature, feature_type from PatchFeature order by feature";
    try
    {
        auto q = SQL::Statement(worker->getReadOnlyConn(), query);
        while (q.step())
        {
            res.emplace_back(q.col_str(0), q.col_int(1));
        }
        q.finalize();
    }
    catch (SQL::Exception &e)
    {
        storage->reportError(e.what(), "PatchDB - readFeatures");
    }
    return res;
}

std::vector<std::string> PatchDB::readAllFeatureValueString(const std::string &feature)
{

    std::vector<std::string> res;

    // language=SQL
    std::string query = "SELECT DISTINCT feature_svalue from PatchFeature WHERE feature = ? "
                        " order by feature_svalue";
    try
    {
        auto q = SQL::Statement(worker->getReadOnlyConn(), query);
        q.bind(1, feature);
        while (q.step())
        {
            res.emplace_back(q.col_str(0));
        }
        q.finalize();
    }
    catch (SQL::Exception &e)
    {
        storage->reportError(e.what(), "PatchDB - readFeatures");
    }
    return res;
}

std::vector<int> PatchDB::readAllFeatureValueInt(const std::string &feature)
{

    std::vector<int> res;

    // language=SQL
    std::string query = "SELECT DISTINCT feature_ivalue from PatchFeature WHERE feature = ? "
                        " order by feature_ivalue";
    try
    {
        auto q = SQL::Statement(worker->getReadOnlyConn(), query);
        q.bind(1, feature);
        while (q.step())
        {
            res.emplace_back(q.col_int(0));
        }
        q.finalize();
    }
    catch (SQL::Exception &e)
    {
        storage->reportError(e.what(), "PatchDB - readFeatures");
    }
    return res;
}

// FIXME - push to worker perhaps?
std::vector<PatchDB::patchRecord> PatchDB::rawQueryForNameLike(const std::string &nameLikeThisP)
{
    std::vector<PatchDB::patchRecord> res;

    // FIXME - cache this by pushing it to the worker
    std::string query = "select p.id, p.path, p.category, p.name, pf.feature_svalue from Patches "
                        "as p, PatchFeature as pf where pf.patch_id == p.id and pf.feature LIKE "
                        "'AUTHOR' and p.name LIKE ? ORDER BY p.category_type, p.category, p.name";

    try
    {
        auto conn = worker->getReadOnlyConn(false);
        if (!conn)
            return res;

        auto q = SQL::Statement(conn, query);
        std::string nameLikeThis = "%" + nameLikeThisP + "%";
        q.bind(1, nameLikeThis);

        while (q.step())
        {
            int id = q.col_int(0);
            auto path = q.col_str(1);
            auto cat = q.col_str(2);
            auto name = q.col_str(3);
            auto auth = q.col_str(4);
            res.emplace_back(id, path, cat, name, auth);
        }

        q.finalize();
    }
    catch (SQL::Exception &e)
    {
        if (e.rc == SQLITE_BUSY)
        {
            // Oh well
        }
        else
        {
            storage->reportError(e.what(), "PatchDB - rawQueryForNameLike");
        }
    }

    return res;
}

std::vector<PatchDB::catRecord> PatchDB::rootCategoriesForType(const CatType t)
{
    std::string query = "select c.id, c.name, c.leaf_name, c.isroot, c.type from Category "
                        "as c where c.isroot = 1 and c.type = ?";
    return internalCategories((int)t, query);
}

std::vector<PatchDB::catRecord> PatchDB::childCategoriesOf(int catId)
{
    // FIXME - cache this by pushing it to the worker
    std::string query = "select c.id, c.name, c.leaf_name, c.isroot, c.type from Category "
                        "as c where c.parent_id = ?";
    return internalCategories((int)catId, query);
}

std::vector<PatchDB::catRecord> PatchDB::internalCategories(int t, const std::string &query)
{
    std::vector<PatchDB::catRecord> res;

    try
    {
        auto q = SQL::Statement(worker->getReadOnlyConn(), query);
        q.bind(1, t);

        while (q.step())
        {
            auto cr = catRecord();
            cr.id = q.col_int(0);
            cr.name = q.col_str(1);
            cr.leaf_name = q.col_str(2);
            cr.isroot = q.col_int(3);
            cr.type = (CatType)q.col_int(4);
            cr.isleaf = false;

            res.push_back(cr);
        }

        q.finalize();

        auto par = SQL::Statement(worker->getReadOnlyConn(),
                                  "select COUNT(id) from category where category.parent_id = ?");
        for (auto &cr : res)
        {
            par.bind(1, cr.id);
            if (par.step())
            {
                cr.isleaf = (par.col_int(0) == 0);
            }
            par.clearBindings();
            par.reset();
        }

        par.finalize();
    }
    catch (SQL::Exception &e)
    {
        storage->reportError(e.what(), "PatchDB - Loading Categories");
    }

    return res;
}

void PatchDB::setUserFavorite(const std::string &path, bool isIt)
{
    prepareForWrites();
    worker->enqueueWorkItem(new WriterWorker::EnQFavorite(path, isIt));
}

void PatchDB::erasePatchByID(int id)
{
    prepareForWrites();
    worker->enqueueWorkItem(new WriterWorker::EnQDelete(id));
}

std::vector<std::string> PatchDB::readUserFavorites()
{
    auto conn = worker->getReadOnlyConn(false);
    if (!conn)
        return std::vector<std::string>();
    try
    {
        auto res = std::vector<std::string>();
        auto hasFav = SQL::Statement(
            conn, "SELECT count(*) from sqlite_master where tbl_name = \"Favorites\"");
        int favCt = 0;
        while (hasFav.step())
        {
            favCt = hasFav.col_int(0);
        }
        hasFav.finalize();

        if (favCt == 0)
            return std::vector<std::string>();

        auto st = SQL::Statement(conn, "select path from Favorites;");
        while (st.step())
        {
            res.push_back(st.col_str(0));
        }
        st.finalize();
        return res;
    }
    catch (SQL::Exception &e)
    {
        // This error really doesn't matter most of the time
        storage->reportError(e.what(), "PatchDB - Loading Favorites");
    }
    return std::vector<std::string>();
}

std::unordered_map<std::string, std::pair<int, int64_t>>
PatchDB::readAllPatchPathsWithIdAndModTime()
{
    std::unordered_map<std::string, std::pair<int, int64_t>> res;

    auto conn = worker->getReadOnlyConn(false);
    if (!conn)
        return res;

    try
    {
        auto st = SQL::Statement(conn, "select id, path, last_write_time from Patches;");
        while (st.step())
        {
            auto id = st.col_int(0);
            auto pt = st.col_str(1);
            auto lw = st.col_int64(2);
            res[pt] = std::make_pair(id, lw);
        }
        st.finalize();
    }
    catch (SQL::Exception &e)
    {
        // This error really doesn't matter most of the time
        storage->reportError(e.what(), "PatchDB - Loading Favorites");
    }
    return res;
}

int PatchDB::numberOfJobsOutstanding()
{
    std::lock_guard<std::mutex> guard(worker->qLock);
    return worker->pathQ.size();
}

int PatchDB::waitForJobsOutstandingComplete(int maxWaitInMS)
{
    int maxIts = maxWaitInMS / 10;
    int njo = 0;
    while ((njo = numberOfJobsOutstanding()) > 0 && maxIts > 0)
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
        maxIts--;
    }
    return numberOfJobsOutstanding();
}

std::string PatchDB::sqlWhereClauseFor(const std::unique_ptr<PatchDBQueryParser::Token> &t)
{
    auto protect = [](const std::string &s) -> std::string {
        std::vector<std::pair<std::string, std::string>> replacements{{"'", "''"}, {"%", "%%"}};
        auto res = s;
        for (const auto &[search, replace] : replacements)
        {
            size_t pos = res.find(search);
            // Repeat till end is reached
            while (pos != std::string::npos)
            {
                // Replace this occurrence of Sub String
                res.replace(pos, search.size(), replace);
                // Get the next occurrence from the current position
                pos = res.find(search, pos + replace.size());
            }
        }
        return res;
    };
    std::ostringstream oss;
    switch (t->type)
    {
    case PatchDBQueryParser::INVALID:
        oss << "(1 == 0)";
        break;
    case PatchDBQueryParser::KEYWORD_EQUALS:
        if ((t->content == "AUTHOR" || t->content == "AUTH") && !t->children[0]->content.empty())
        {
            oss << "(author LIKE '%" << protect(t->children[0]->content) << "%' )";
        }
        else if ((t->content == "CATEGORY" || t->content == "CAT") &&
                 !t->children[0]->content.empty())
        {
            oss << "(category LIKE '%" << protect(t->children[0]->content) << "%' )";
        }
        else
        {
            oss << "(1 == 1)";
        }
        break;
    case PatchDBQueryParser::LITERAL:
        oss << "( p.search_over LIKE '%" << protect(t->content) << "%' )";
        break;
    case PatchDBQueryParser::AND:
    case PatchDBQueryParser::OR:
    {
        oss << "( ";
        std::string inter = "";
        for (auto &c : t->children)
        {
            oss << inter;
            oss << sqlWhereClauseFor(c);
            inter = t->type == PatchDBQueryParser::AND ? " AND " : " OR ";
        }
        oss << " )";
        break;
    }
    }

    return oss.str();
}

std::vector<PatchDB::patchRecord>
PatchDB::queryFromQueryString(const std::unique_ptr<PatchDBQueryParser::Token> &t)
{
    std::vector<PatchDB::patchRecord> res;

    // FIXME - cache this by pushing it to the worker
    std::string query = "select p.id, p.path, p.category as category, p.name, pf.feature_svalue as "
                        "author, p.search_over from Patches "
                        "as p, PatchFeature as pf where pf.patch_id == p.id and pf.feature LIKE "
                        "'AUTHOR' and " +
                        sqlWhereClauseFor(t) + " ORDER BY p.category_type, p.category, p.name";

    // std::cout << "QUERY IS \n" << query << "\n";
    try
    {
        auto conn = worker->getReadOnlyConn(false);
        if (!conn)
            return res;

        auto q = SQL::Statement(conn, query);

        while (q.step())
        {
            int id = q.col_int(0);
            auto path = q.col_str(1);
            auto cat = q.col_str(2);
            auto name = q.col_str(3);
            auto auth = q.col_str(4);
            res.emplace_back(id, path, cat, name, auth);
        }

        q.finalize();
    }
    catch (SQL::Exception &e)
    {
        if (e.rc == SQLITE_BUSY)
        {
            // Oh well
        }
        else
        {
            storage->reportError(e.what(), "PatchDB - rawQueryForNameLike");
        }
    }

    return res;
}

} // namespace PatchStorage
} // namespace Surge
