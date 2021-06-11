/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "PatchDB.h"

#include "sqlite3.h"
#include "SurgeStorage.h"
#include <sstream>
#include <iterator>
#include "vt_dsp_endian.h"

namespace Surge
{
namespace PatchStorage
{
struct PatchDB::workerS
{
    static constexpr const char *schema_version = "2"; // I will rebuild if this is not my verion

    /*
     * Obviously a lot of thought needs to go into this
     */
    static constexpr const char *setup_sql = R"SQL(
DROP TABLE IF EXISTS "Patches";
DROP TABLE IF EXISTS "PatchFeature";
DROP TABLE IF EXISTS "Version";
CREATE TABLE "Version" (
    id integer primary key,
    schema_version varchar(256)
);
CREATE TABLE "Patches" (
      id integer primary key,
      path varchar(2048),
      name varchar(256),
      category varchar(256),
      last_write_time big int
);
CREATE TABLE PatchFeature (
      id integer primary key,
      patch_id integer,
      feature varchar(64),
      featurre_group int,
      feature_ivalue int,
      feature_svalue varchar(64)
)
    )SQL";

    struct txnGuard
    {
        explicit txnGuard(sqlite3 *e) : d(e)
        {
            sqlite3_exec(d, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
        }
        ~txnGuard() { sqlite3_exec(d, "END TRANSACTION", nullptr, nullptr, nullptr); }
        sqlite3 *d;
    };

    explicit workerS(SurgeStorage *storage) : storage(storage)
    {
        fs::create_directories(string_to_path(storage->userDataPath));
        auto dbname = storage->userDataPath + "/SurgePatches.db";
        auto flag = SQLITE_OPEN_FULLMUTEX; // basically lock
        flag |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

        std::cout << "\nPatchDB : Opening sqlitedb " << dbname << std::endl;
        auto ec = sqlite3_open_v2(dbname.c_str(), &dbh, flag, nullptr);

        if (ec != SQLITE_OK)
        {
            std::ostringstream oss;
            oss << "An error occured opening sqlite file '" << dbname << "'. The error was '"
                << sqlite3_errmsg(dbh) << "'.";
            storage->reportError(oss.str(), "Surge Patch Database Error");
            dbh = nullptr;
        }

        /*
         * OK check my version
         */
        bool rebuild = true;
        char *emsg;

        sqlite3_stmt *qs;
        auto rc = sqlite3_prepare_v2(dbh, "SELECT * FROM Version", -1, &qs, nullptr);
        // FIXME - error handling everywhere of course
        if (rc != SQLITE_OK)
        {
            std::cout << "        : No VERSION table. Rebuilding database" << std::endl;
            rebuild = true;
        }
        else
        {
            while (sqlite3_step(qs) == SQLITE_ROW)
            {
                int id = sqlite3_column_int(qs, 0);
                auto ver = reinterpret_cast<const char *>(sqlite3_column_text(qs, 1));
                std::cout << "        : schema check. DBVersion='" << ver << "' SchemaVersion='"
                          << schema_version << "'" << std::endl;
                if (strcmp(ver, schema_version) == 0)
                {
                    std::cout << "        : Schema matches. Reusing database." << std::endl;
                    rebuild = false;
                }
            }
        }

        sqlite3_finalize(qs);

        if (rebuild)
        {
            std::cout << "        : Schema missing or mismatched. Dropping and Rebuilding Database."
                      << std::endl;

            auto res = sqlite3_exec(dbh, setup_sql, nullptr, nullptr, &emsg);
            if (res != SQLITE_OK)
            {
                std::ostringstream oss;
                oss << "Error creating SQL database: " << emsg;
                storage->reportError(oss.str(), "PatchDB Error");
                sqlite3_free(emsg);
            }
            auto versql = std::string("INSERT INTO VERSION (\"schema_version\") VALUES (\"") +
                          schema_version + "\")";
            res = sqlite3_exec(dbh, versql.c_str(), nullptr, nullptr, &emsg);
            if (res != SQLITE_OK)
            {
                std::ostringstream oss;
                oss << "Error writing SQL Schema Version: " << emsg;
                storage->reportError(oss.str(), "PatchDB Error");
                sqlite3_free(emsg);
            }
        }
        qThread = std::thread([this]() { this->loadQueueFunction(); });
    }

    ~workerS()
    {
        keepRunning = false;
        qCV.notify_all();
        qThread.join();
        // clean up all the prepared statements
        if (dbh)
            sqlite3_close(dbh);
    }

    // FIXME features shuld be an enum or something

    enum FeatureType
    {
        INT,
        STRING
    };
    typedef std::tuple<std::string, FeatureType, int, std::string> feature;
    std::vector<feature> extractFeaturesFromXML(const std::string &xml)
    {
        std::vector<feature> res;
        TiXmlDocument doc;
        doc.Parse(xml.c_str(), nullptr, TIXML_ENCODING_LEGACY);
        if (doc.Error())
        {
            std::cout << "        - ERROR: Unable to load XML; Skipping file" << std::endl;
            return res;
        }

        auto patch = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("patch"));

        if (!patch)
        {
            std::cout << "        - ERROR: No 'patch' element in XML" << std::endl;
            return res;
        }

        int rev = 0;
        if (patch->QueryIntAttribute("revision", &rev) == TIXML_SUCCESS)
        {
            res.emplace_back("REVISION", INT, rev, "");
        }
        else
        {
            std::cout << "        - ERROR: No Revision in XML" << std::endl;
            return res;
        }

        auto meta = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("meta"));
        if (meta)
        {
            if (meta->Attribute("author"))
            {
                res.emplace_back("AUTHOR", STRING, 0, meta->Attribute("author"));
            }
        }
        return res;
    }

    /*
     * Functions for the write thread
     */
    void loadQueueFunction()
    {
        static constexpr auto transChunkSize = 10; // How many FXP to load in a single txn
        while (keepRunning)
        {
            std::vector<std::tuple<fs::path, std::string, std::string>> doThis;
            {
                std::unique_lock<std::mutex> lk(qLock);

                while (keepRunning && pathQ.empty())
                {
                    qCV.wait(lk);
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
            {
                txnGuard tg(dbh);
                for (const auto &p : doThis)
                    parseFXPIntoDB(p);
            }
        }
    }

    sqlite3_stmt *insertStmt = nullptr, *insfeatureStmt = nullptr, *checkExistsStmt = nullptr,
                 *dropIdStmt = nullptr;

    void parseFXPIntoDB(const std::tuple<fs::path, std::string, std::string> &p)
    {
        if (!fs::exists(std::get<0>(p)))
        {
            std::cout << "    - Warning: Non existent " << path_to_string(std::get<0>(p))
                      << std::endl;
            return;
        }
        // Check with
        auto qtime = fs::last_write_time(std::get<0>(p));
        int64_t qtimeInt =
            std::chrono::duration_cast<std::chrono::seconds>(qtime.time_since_epoch()).count();

        if (checkExistsStmt == nullptr)
        {
            auto rc = sqlite3_prepare_v2(
                dbh, "SELECT id, last_write_time from Patches WHERE Patches.Path LIKE ?1;", -1,
                &checkExistsStmt, nullptr);
            if (rc != SQLITE_OK)
            {
                checkExistsStmt = nullptr;
                return;
            }
        }
        bool patchLoaded = false;
        std::vector<int> dropIds;

        {
            auto s = path_to_string(std::get<0>(p));
            auto rc = sqlite3_bind_text(checkExistsStmt, 1, s.c_str(), s.length(), SQLITE_STATIC);
            if (rc != SQLITE_OK)
            {
                std::cout << "COULD NOT BIND" << std::endl;
            }
            bool first = true;
            while (sqlite3_step(checkExistsStmt) == SQLITE_ROW)
            {
                auto id = sqlite3_column_int(checkExistsStmt, 0);
                auto t = sqlite3_column_int64(checkExistsStmt, 1);
                if (t < qtimeInt || (t == qtimeInt && patchLoaded))
                {
                    dropIds.push_back(id);
                }
                if (t >= qtimeInt)
                    patchLoaded = true;
                first = false;
            }
            rc = sqlite3_clear_bindings(checkExistsStmt);
            if (rc != SQLITE_OK)
                std::cout << "Cant Clear Check" << std::endl;
            rc = sqlite3_reset(checkExistsStmt);
            if (rc != SQLITE_OK)
                std::cout << "Cant Reset Check" << std::endl;
        }

        if (dropIdStmt == nullptr)
        {
            auto rc = sqlite3_prepare_v2(
                dbh, "DELETE FROM Patches WHERE ID=?1; DELETE FROM PatchFeature WHERE ID=?1", -1,
                &dropIdStmt, nullptr);
            if (rc != SQLITE_OK)
            {
                dropIdStmt = nullptr;
                std::cout << "Can't build dropStatement" << std::endl;
            }
        }

        if (dropIdStmt)
        {
            for (auto did : dropIds)
            {
                auto rc = sqlite3_bind_int64(dropIdStmt, 1, did);
                if (rc != SQLITE_OK)
                {
                    std::cout << "COULD NOT BIND DID 1" << std::endl;
                }
                /*rc = sqlite3_bind_int(dropIdStmt, 2, did);
                if (rc != SQLITE_OK)
                {
                    std::cout << "COULD NOT BIND DID 2" << std::endl;
                }*/
                while (sqlite3_step(dropIdStmt) == SQLITE_ROW)
                {
                }
                rc = sqlite3_clear_bindings(dropIdStmt);
                if (rc != SQLITE_OK)
                    std::cout << "Cant Clear Check" << std::endl;
                rc = sqlite3_reset(dropIdStmt);
            }
        }

        if (patchLoaded)
            return;

        std::cout << "        - Loading '" << path_to_string(std::get<0>(p)) << "'" << std::endl;

        // Oh so much to fix here, but lets start with error handling
        if (insertStmt == nullptr)
        {
            auto rc = sqlite3_prepare_v2(
                dbh,
                "INSERT INTO PATCHES ( \"path\", \"name\", \"category\", \"last_write_time\" ) "
                "VALUES ( ?1, ?2, ?3, ?4 )",
                -1, &insertStmt, nullptr);
            if (rc != SQLITE_OK)
            {
                insertStmt = nullptr;
                return;
            }
        }

        if (!insertStmt)
            return;
        auto s = path_to_string(std::get<0>(p));
        auto rc = sqlite3_bind_text(insertStmt, 1, s.c_str(), s.length() + 1, SQLITE_STATIC);
        if (rc != SQLITE_OK)
            std::cout << "BAD BIND 1" << std::endl;

        auto s2 = std::get<1>(p);
        rc = sqlite3_bind_text(insertStmt, 2, s2.c_str(), s2.length() + 1, SQLITE_STATIC);
        if (rc != SQLITE_OK)
            std::cout << "BAD BIND 2" << std::endl;

        auto s3 = std::get<2>(p);
        rc = sqlite3_bind_text(insertStmt, 3, s3.c_str(), s3.length() + 1, SQLITE_STATIC);
        if (rc != SQLITE_OK)
            std::cout << "BAD BIND 3" << std::endl;

        sqlite3_bind_int64(insertStmt, 4, qtimeInt);

        rc = sqlite3_step(insertStmt);
        if (rc != SQLITE_DONE)
            std::cout << "BAD STEP" << std::endl;

        rc = sqlite3_clear_bindings(insertStmt);
        rc = sqlite3_reset(insertStmt);

        if (rc != SQLITE_OK)
        {
            std::cout << "SPLAT" << std::endl;
        }

        // Great now get its id
        auto patchid = sqlite3_last_insert_rowid(dbh);
        // std::cout << patchid << " " << s2 << std::endl;

        std::ifstream stream(std::get<0>(p), std::ios::in | std::ios::binary);
        std::vector<uint8_t> contents((std::istreambuf_iterator<char>(stream)),
                                      std::istreambuf_iterator<char>());

#pragma pack(push, 1)
        struct patch_header
        {
            char tag[4];
            unsigned int xmlsize,
                wtsize[2][3]; // TODO: FIX SCENE AND OSC COUNT ASSUMPTION (but also since
            // it's used in streaming, do it with care!)
        };

        struct fxChunkSetCustom
        {
            int chunkMagic; // 'CcnK'
            int byteSize;   // of this chunk, excl. magic + byteSize

            int fxMagic; // 'FPCh'
            int version;
            int fxID; // fx unique id
            int fxVersion;

            int numPrograms;
            char prgName[28];

            int chunkSize;
            // char chunk[8]; // variable
        };
#pragma pack(pop)

        uint8_t *d = contents.data();
        auto *fxp = (fxChunkSetCustom *)d;
        if ((vt_read_int32BE(fxp->chunkMagic) != 'CcnK') ||
            (vt_read_int32BE(fxp->fxMagic) != 'FPCh') || (vt_read_int32BE(fxp->fxID) != 'cjs3'))
        {
            std::cout << "Not a surge patch; bailing" << std::endl;
            return;
        }
        // FIXME - checks for CcnK and stuff

        auto phd = d + sizeof(fxChunkSetCustom);
        auto *ph = (patch_header *)phd;
        auto xmlSz = vt_read_int32LE(ph->xmlsize);

        auto xd = phd + sizeof(patch_header);
        std::string xml(xd, xd + xmlSz);
        // std::cout << xml << std::endl;

        if (insfeatureStmt == nullptr)
        {
            rc = sqlite3_prepare_v2(dbh,
                                    "INSERT INTO PATCHFEATURE ( \"patch_id\", \"feature\", "
                                    "\"feature_ivalue\", \"feature_svalue\" ) "
                                    "VALUES ( ?1, ?2, ?3, ?4 )",
                                    -1, &insfeatureStmt, nullptr);
        }
        auto feat = extractFeaturesFromXML(xml);
        for (auto f : feat)
        {
            sqlite3_bind_int(insfeatureStmt, 1, patchid);
            auto sf = std::get<0>(f);
            sqlite3_bind_text(insfeatureStmt, 2, sf.c_str(), sf.length(), SQLITE_STATIC);
            sqlite3_bind_int(insfeatureStmt, 3, std::get<2>(f));

            auto ss = std::get<3>(f);
            sqlite3_bind_text(insfeatureStmt, 4, ss.c_str(), ss.length(), SQLITE_STATIC);

            rc = sqlite3_step(insfeatureStmt);
            if (rc != SQLITE_DONE)
                std::cout << "BAD FEATURE STEP" << std::endl;

            rc = sqlite3_clear_bindings(insfeatureStmt);
            rc = sqlite3_reset(insfeatureStmt);
        }
    }

    /*
     * Call this from any thread
     */
    void enqueuePath(const std::tuple<fs::path, std::string, std::string> &p)
    {
        {
            std::lock_guard<std::mutex> g(qLock);
            pathQ.push_back(p);
        }
        qCV.notify_all();
    }

    // FIXME for now I am coding this with a locked vector but probably a
    // thread safe queue is the way to go
    std::thread qThread;
    std::mutex qLock;
    std::condition_variable qCV;
    std::deque<std::tuple<fs::path, std::string, std::string>> pathQ;
    std::atomic<bool> keepRunning{true};

    sqlite3 *dbh;
    SurgeStorage *storage;
};
PatchDB::PatchDB(SurgeStorage *s) : storage(s) { initialize(); }

PatchDB::~PatchDB() = default;

void PatchDB::initialize() { worker = std::make_unique<workerS>(storage); }

void PatchDB::considerFXPForLoad(const fs::path &fxp, const std::string &name,
                                 const std::string &catName) const
{
    worker->enqueuePath(std::make_tuple(fxp, name, catName));
}

// FIXME - push to worker perhaps?
std::vector<PatchDB::record> PatchDB::rawQueryForNameLike(const std::string &nameLikeThisP)
{
    std::vector<PatchDB::record> res;

    // FIXME - cache this by pushing it to the worker
    std::string query = "select p.id, p.path, p.category, p.name, pf.feature_svalue from Patches "
                        "as p, PatchFeature as pf where pf.patch_id == p.id and pf.feature LIKE "
                        "'AUTHOR' and p.name LIKE ?";
    sqlite3_stmt *qs;
    auto rc = sqlite3_prepare_v2(worker->dbh, query.c_str(), -1, &qs, nullptr);
    // FIXME - error handling everywhere of course
    if (rc != SQLITE_OK)
    {
        std::ostringstream oss;
        oss << "An error occured opening querying for '" << nameLikeThisP << "'. The error was '"
            << sqlite3_errmsg(worker->dbh) << "'.";
        storage->reportError(oss.str(), "Surge Patch Database Error");
        return res;
    }

    std::string nameLikeThis = "%" + nameLikeThisP + "%";
    if (sqlite3_bind_text(qs, 1, nameLikeThis.c_str(), nameLikeThis.length() + 1, SQLITE_STATIC) !=
        SQLITE_OK)
    {
        std::cout << "Can't bind" << std::endl;
        return res;
    }

    int maxrows = 0;
    while (sqlite3_step(qs) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(qs, 0);
        auto path = reinterpret_cast<const char *>(sqlite3_column_text(qs, 1));
        auto cat = reinterpret_cast<const char *>(sqlite3_column_text(qs, 2));
        auto name = reinterpret_cast<const char *>(sqlite3_column_text(qs, 3));
        auto auth = reinterpret_cast<const char *>(sqlite3_column_text(qs, 4));
        res.emplace_back(id, path, cat, name, auth);
    }

    sqlite3_finalize(qs);
    return res;
}

} // namespace PatchStorage
} // namespace Surge
