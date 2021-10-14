// -*-c++-*-
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

#pragma once
#include <thread>
#include <vector>
#include <deque>
#include <unordered_map>
#include <condition_variable>
#include "filesystem/import.h"
#include <iostream>
#include <vector>

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

    int numberOfJobsOutstanding();

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
