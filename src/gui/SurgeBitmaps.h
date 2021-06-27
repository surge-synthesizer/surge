#pragma once

#include "resource.h"
#include <JuceHeader.h>
#include "efvg/escape_from_vstgui.h"
#include <map>
#include <atomic>
#include <algorithm>
#include <cctype>

class CScalableBitmap;

class SurgeBitmaps
{
  public:
    SurgeBitmaps();
    virtual ~SurgeBitmaps();
    void clearAllLoadedBitmaps(); // Call very carefully.

    void setupBuiltinBitmaps();
    void setPhysicalZoomFactor(int pzf);

    CScalableBitmap *getBitmap(int id);
    CScalableBitmap *getBitmapByPath(const std::string &filename);
    CScalableBitmap *getBitmapByStringID(const std::string &id);

    juce::Drawable *getDrawable(int id);
    juce::Drawable *getDrawableByPath(const std::string &filename);
    juce::Drawable *getDrawableByStringID(const std::string &id);

    CScalableBitmap *loadBitmapByPath(const std::string &filename);
    CScalableBitmap *loadBitmapByPathForID(const std::string &filename, int id);
    CScalableBitmap *loadBitmapByPathForStringID(const std::string &filename, std::string id);

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
    std::map<int, CScalableBitmap *> bitmap_registry;
    std::map<std::string, CScalableBitmap *> bitmap_file_registry;

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
    std::map<std::string, CScalableBitmap *, cicomp> bitmap_stringid_registry;
};
