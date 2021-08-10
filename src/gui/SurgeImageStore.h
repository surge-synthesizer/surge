#pragma once

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
