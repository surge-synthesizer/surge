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
#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEIMAGESTORE_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEIMAGESTORE_H

#include "resource.h"
#include <string>
#include <map>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <vector>

class SurgeImage;

class SurgeImageStore
{
  public:
    SurgeImageStore();
    virtual ~SurgeImageStore();
    void clearAllLoadedBitmaps(); // Call very carefully.

    void setupBuiltinBitmaps();
    void setPhysicalZoomFactor(int pzf);

    SurgeImage *getImage(int id);
    SurgeImage *getImageByPath(const std::string &filename);
    SurgeImage *getImageByStringID(const std::string &id);

    SurgeImage *loadImageByPath(const std::string &filename);
    SurgeImage *loadImageByPathForID(const std::string &filename, int id);
    SurgeImage *loadImageByPathForStringID(const std::string &filename, std::string id);

    enum StringResourceType
    {
        PATH,
        STRINGID
    };
    std::vector<std::string> nonResourceBitmapIDs(StringResourceType t)
    {
        std::vector<std::string> res;
        switch (t)
        {
        case PATH:
            for (auto &c : bitmap_file_registry)
                res.push_back(c.first);
        case STRINGID:
            for (auto &c : bitmap_stringid_registry)
                res.push_back(c.first);
        }
        return res;
    }

  protected:
    static std::atomic<int> instances;

    void addEntry(int id);
    // I own and am responsible for deleting these
    std::map<int, SurgeImage *> bitmap_registry;
    std::map<std::string, SurgeImage *> bitmap_file_registry;

    struct cicomp
    {
        bool operator()(const std::string &a, const std::string &b) const
        {
            std::string al = a;
            std::transform(al.begin(), al.end(), al.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::string bl = b;
            std::transform(bl.begin(), bl.end(), bl.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return al < bl;
        }
    };
    std::map<std::string, SurgeImage *, cicomp> bitmap_stringid_registry;
};

#endif // SURGE_SRC_SURGE_XT_GUI_SURGEIMAGESTORE_H
