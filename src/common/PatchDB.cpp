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

namespace SQL
{
struct Exception : public std::runtime_error
{
    explicit Exception(sqlite3 *h) : std::runtime_error(sqlite3_errmsg(h)), rc(sqlite3_errcode(h))
    {
    }
    Exception(int rc, const std::string &msg) : std::runtime_error(msg), rc(rc) {}
    const char *what() const noexcept override
    {
        static char msg[1024];
        snprintf(msg, 1024, "SQL Error[%d]: %s", rc, std::runtime_error::what());
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
    Statement(sqlite3 *h, const std::string &statement) : h(h)
    {
        auto rc = sqlite3_prepare_v2(h, statement.c_str(), -1, &s, nullptr);
        if (rc != SQLITE_OK)
            throw Exception(h);
    }
    ~Statement() noexcept(false)
    {
        if (s)
            if (sqlite3_finalize(s) != SQLITE_OK)
                throw Exception(h);
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
    explicit TxnGuard(sqlite3 *e) : d(e)
    {
        auto rc = sqlite3_exec(d, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
        {
            throw Exception(d);
        }
    }
    ~TxnGuard() noexcept(false)
    {
        auto rc = sqlite3_exec(d, "END TRANSACTION", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
        {
            throw Exception(d);
        }
    }
    sqlite3 *d;
};
} // namespace SQL

struct PatchDB::workerS
{
    static constexpr const char *schema_version = "3"; // I will rebuild if this is not my verion

    /*
     * Obviously a lot of thought needs to go into this
     */
    static constexpr const char *setup_sql = R"SQL(
DROP TABLE IF EXISTS "Patches";
DROP TABLE IF EXISTS "PatchFeature";
DROP TABLE IF EXISTS "Version";
DROP TABLE IF EXISTS "Category";
CREATE TABLE "Version" (
    id integer primary key,
    schema_version varchar(256)
);
CREATE TABLE "Patches" (
      id integer primary key,
      path varchar(2048),
      name varchar(256),
      category varchar(2048),
      category_type int,
      last_write_time big int
);
CREATE TABLE PatchFeature (
      id integer primary key,
      patch_id integer,
      feature varchar(64),
      featurre_group int,
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
)
    )SQL";

    struct EnQAble
    {
        virtual ~EnQAble() = default;
        virtual void go(workerS &) = 0;
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

        void go(workerS &w) override { w.parseFXPIntoDB(*this); }
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
            leafname = path_to_string(l); // FIXME
        }
        void go(workerS &w) override
        {
            if (isroot)
                w.addRootCategory(name, type);
            else
                w.addChildCategory(name, leafname, parentname, type);
        }
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
            return;
        }

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
                std::cout << "        : schema check. DBVersion='" << ver << "' SchemaVersion='"
                          << schema_version << "'" << std::endl;
                if (strcmp(ver, schema_version) == 0)
                {
                    std::cout << "        : Schema matches. Reusing database." << std::endl;
                    rebuild = false;
                }
            }
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
            std::cout << "        : Schema missing or mismatched. Dropping and Rebuilding Database."
                      << std::endl;
            try
            {
                SQL::Exec(dbh, setup_sql);
                auto versql = std::string("INSERT INTO VERSION (\"schema_version\") VALUES (\"") +
                              schema_version + "\")";
                SQL::Exec(dbh, versql);
            }
            catch (const SQL::Exception &e)
            {
                storage->reportError(e.what(), "PatchDB Setup Error");
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
            std::vector<EnQAble *> doThis;
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
                SQL::TxnGuard tg(dbh);

                for (auto *p : doThis)
                {
                    p->go(*this);
                    delete p;
                }
            }
        }
    }

    void parseFXPIntoDB(const EnQPatch &p)
    {
        sqlite3_stmt *insertStmt = nullptr, *insfeatureStmt = nullptr, *dropIdStmt = nullptr;

        if (!fs::exists(p.path))
        {
            std::cout << "    - Warning: Non existent " << path_to_string(p.path) << std::endl;
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
            auto s = path_to_string(p.path);
            exists.bind(1, s);

            while (exists.step())
            {
                auto id = exists.col_int(0);
                auto t = exists.col_int64(1);
                if (t < qtimeInt || (t == qtimeInt && patchLoaded))
                {
                    dropIds.push_back(id);
                }
                if (t >= qtimeInt)
                    patchLoaded = true;
            }

            if (!dropIds.empty())
            {
                auto drop = SQL::Statement(
                    dbh, "DELETE FROM Patches WHERE ID=?1; DELETE FROM PatchFeature WHERE ID=?1");
                for (auto did : dropIds)
                {
                    drop.bindi64(1, did);
                    while (drop.step())
                    {
                    }
                    drop.clearBindings();
                    drop.reset();
                }
            }
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Load Check");
            return;
        }

        if (patchLoaded)
            return;

        std::cout << "        - Loading '" << path_to_string(p.path) << "'" << std::endl;

        int64_t patchid = -1;
        try
        {
            auto ins = SQL::Statement(dbh, "INSERT INTO PATCHES ( \"path\", \"name\", "
                                           "\"category\", \"category_type\", \"last_write_time\" ) "
                                           "VALUES ( ?1, ?2, ?3, ?4, ?5 )");
            ins.bind(1, path_to_string(p.path));
            ins.bind(2, p.name);
            ins.bind(3, p.catname);
            ins.bind(4, (int)p.type);
            ins.bindi64(5, qtimeInt);

            ins.step();

            // No real need to encapsulate this
            patchid = sqlite3_last_insert_rowid(dbh);
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - Insert Patch");
            return;
        }

        std::ifstream stream(p.path, std::ios::in | std::ios::binary);
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

        auto phd = d + sizeof(fxChunkSetCustom);
        auto *ph = (patch_header *)phd;
        auto xmlSz = vt_read_int32LE(ph->xmlsize);

        auto xd = phd + sizeof(patch_header);
        std::string xml(xd, xd + xmlSz);

        try
        {
            auto ins = SQL::Statement(dbh, "INSERT INTO PATCHFEATURE ( \"patch_id\", \"feature\", "
                                           "\"feature_ivalue\", \"feature_svalue\" ) "
                                           "VALUES ( ?1, ?2, ?3, ?4 )");
            auto feat = extractFeaturesFromXML(xml);
            for (auto f : feat)
            {
                ins.bindi64(1, patchid);
                ins.bind(2, std::get<0>(f));
                ins.bind(3, std::get<2>(f));
                ins.bind(4, std::get<3>(f));

                ins.step();

                ins.clearBindings();
                ins.reset();
            }
        }
        catch (const SQL::Exception &e)
        {
            storage->reportError(e.what(), "PatchDB - FXP Features");
            return;
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

    sqlite3 *dbh;
    SurgeStorage *storage;
};
PatchDB::PatchDB(SurgeStorage *s) : storage(s) { initialize(); }

PatchDB::~PatchDB() = default;

void PatchDB::initialize() { worker = std::make_unique<workerS>(storage); }

void PatchDB::considerFXPForLoad(const fs::path &fxp, const std::string &name,
                                 const std::string &catName, const CatType type) const
{
    worker->enqueueWorkItem(new workerS::EnQPatch(fxp, name, catName, type));
}

void PatchDB::addRootCategory(const std::string &name, CatType type)
{
    worker->enqueueWorkItem(new workerS::EnQCategory(name, type));
}
void PatchDB::addSubCategory(const std::string &name, const std::string &parent, CatType type)
{
    worker->enqueueWorkItem(new workerS::EnQCategory(name, parent, type));
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
        auto q = SQL::Statement(worker->dbh, query);
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
    }
    catch (SQL::Exception &e)
    {
        storage->reportError(e.what(), "PatchDB - rawQueryForNameLike");
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
        auto q = SQL::Statement(worker->dbh, query);
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

        auto par = SQL::Statement(worker->dbh,
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
    }
    catch (SQL::Exception &e)
    {
        storage->reportError(e.what(), "PatchDB - Loading Categories");
    }

    return res;
}

} // namespace PatchStorage
} // namespace Surge
