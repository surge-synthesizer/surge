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

class SurgeStorage;

namespace Surge
{
namespace PatchStorage
{
struct PatchDB
{
    struct workerS;
    struct record
    {
        record(int i, const std::string &f, const std::string &c, const std::string &n,
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

    explicit PatchDB(SurgeStorage *);
    ~PatchDB();

    void initialize();

    SurgeStorage *storage;

    std::unique_ptr<workerS> worker;
    void considerFXPForLoad(const fs::path &fxp, const std::string &name,
                            const std::string &catName) const;

    // This is a temporary API point
    std::vector<record> rawQueryForNameLike(const std::string &nameLikeThis);
};

} // namespace PatchStorage
} // namespace Surge
