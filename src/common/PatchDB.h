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
#include <condition_variable>
#include "filesystem/import.h"
#include <iostream>

class SurgeStorage;

namespace Surge
{
namespace PatchStorage
{
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

    SurgeStorage *storage;

    std::unique_ptr<WriterWorker> worker;

    void considerFXPForLoad(const fs::path &fxp, const std::string &name,
                            const std::string &catName, const CatType type) const;

    void addRootCategory(const std::string &name, CatType type);
    void addSubCategory(const std::string &name, const std::string &parent, CatType type);

    void addDebugMessage(const std::string &debug);

    // Query APIs
    std::vector<std::pair<std::string, int>> readAllFeatures();
    std::vector<std::string> readAllFeatureValueString(const std::string &feature);
    std::vector<int> readAllFeatureValueInt(const std::string &feature);

    // This is a temporary API point
    std::vector<patchRecord> rawQueryForNameLike(const std::string &nameLikeThis);
    std::vector<catRecord> rootCategoriesForType(const CatType t);
    std::vector<catRecord> childCategoriesOf(int catId);

  private:
    std::vector<catRecord> internalCategories(int arg, const std::string &query);
};

} // namespace PatchStorage
} // namespace Surge
