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

#ifndef SURGE_SRC_COMMON_PATCHDB_H
#define SURGE_SRC_COMMON_PATCHDB_H
#include <thread>
#include <vector>
#include <deque>
#include <unordered_map>
#include <condition_variable>
#include "filesystem/import.h"
#include <iostream>
#include <functional>

class SurgeStorage;

namespace Surge
{
namespace PatchStorage
{

struct PatchDBQueryParser
{
    PatchDBQueryParser() = default;

    enum TokenType
    {
        INVALID,
        LITERAL,
        AND,
        OR,
        KEYWORD_EQUALS
    };

    struct Token
    {
        TokenType type{INVALID};
        std::string prefix;
        std::string content;
        std::vector<std::unique_ptr<Token>> children;
    };

    static std::unique_ptr<Token> parseQuery(const std::string &q);
    static void printParseTree(std::ostream &os, const std::unique_ptr<Token> &t,
                               const std::string pfx = "");
};

struct PatchDB
{
    struct WriterWorker;
    struct patchRecord
    {
        patchRecord(int i, const std::string &f, const std::string &c, const std::string &n,
                    const std::string &a)
            : id(i), file(f), cat(c), name(n), author(a)
        {
        }
        int id;
        std::string file;
        std::string cat;
        std::string name;
        std::string author;
    };

    enum CatType
    {
        FACTORY,
        THIRD_PARTY,
        USER
    };

    struct catRecord
    {
        int id;
        std::string name;
        std::string leaf_name;
        int parentid;
        bool isroot;
        bool isleaf; // you can be a root and a leaf both if you have no children at top lev
        CatType type;
    };

    explicit PatchDB(SurgeStorage *);
    ~PatchDB();

    void initialize();
    void prepareForWrites();

    SurgeStorage *storage;

    std::unique_ptr<WriterWorker> worker;

    // Write APIs
    void considerFXPForLoad(const fs::path &fxp, const std::string &name,
                            const std::string &catName, const CatType type) const;
    void addRootCategory(const std::string &name, CatType type);
    void addSubCategory(const std::string &name, const std::string &parent, CatType type);
    void addDebugMessage(const std::string &debug);
    void setUserFavorite(const std::string &path, bool isIt);
    void erasePatchByID(int id);
    void doAfterCurrentQueueDrained(std::function<void()> op);

    /*
     * Returns a moment-in-time snapshot of number of jobs in the processing queue
     */
    int numberOfJobsOutstanding();
    /*
     * Waits for the processing queue to drain. Obviously don't call this from the processing
     * queue. The max timeout in ms is the longest wait and at the end it returns the
     * number of jobs outstanding.
     */
    int waitForJobsOutstandingComplete(int maxWaitInMS);

    // Query APIs
    std::vector<std::pair<std::string, int>> readAllFeatures();
    std::vector<std::string> readAllFeatureValueString(const std::string &feature);
    std::vector<int> readAllFeatureValueInt(const std::string &feature);
    std::vector<std::string> readUserFavorites();

    std::unordered_map<std::string, std::pair<int, int64_t>> readAllPatchPathsWithIdAndModTime();

    // How the query string works
    static std::string sqlWhereClauseFor(const std::unique_ptr<PatchDBQueryParser::Token> &t);
    std::vector<patchRecord> queryFromQueryString(const std::string &query)
    {
        return queryFromQueryString(PatchDBQueryParser::parseQuery(query));
    }
    std::vector<patchRecord>
    queryFromQueryString(const std::unique_ptr<PatchDBQueryParser::Token> &t);

    // This is a temporary API point
    std::vector<patchRecord> rawQueryForNameLike(const std::string &nameLikeThis);
    std::vector<catRecord> rootCategoriesForType(const CatType t);
    std::vector<catRecord> childCategoriesOf(int catId);

  private:
    std::vector<catRecord> internalCategories(int arg, const std::string &query);
};

} // namespace PatchStorage
} // namespace Surge

#endif // SURGE_SRC_COMMON_PATCHDB_H
