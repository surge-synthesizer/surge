#include "SurgeImageStore.h"
#include "SurgeImage.h"

#include "fmt/core.h"

#include <iostream>
#include <cassert>

std::atomic<int> SurgeImageStore::instances(0);
SurgeImageStore::SurgeImageStore()
{
    instances++;
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeImageStore::SurgeImageStore");
#endif

    // std::cout << "Constructing a SurgeImageStore; Instances is " << instances << std::endl;
}

SurgeImageStore::~SurgeImageStore()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeImageStore::SurgeImageStoreore");
#endif
    clearAllLoadedBitmaps();
    instances--;
    // std::cout << "Destroying a SurgeImageStore; Instances is " << instances << std::endl;
}

void SurgeImageStore::clearAllLoadedBitmaps()
{
    for (auto pair : bitmap_registry)
    {
        delete pair.second;
    }
    for (auto pair : bitmap_file_registry)
    {
        delete pair.second;
    }
    for (auto pair : bitmap_stringid_registry)
    {
        delete pair.second;
    }
    bitmap_registry.clear();
    bitmap_file_registry.clear();
    bitmap_stringid_registry.clear();
}

void SurgeImageStore::setupBuiltinBitmaps()
{
    addEntry(IDB_MAIN_BG);
    addEntry(IDB_FILTER_SUBTYPE);
    addEntry(IDB_FILTER2_OFFSET);
    addEntry(IDB_OSC_SELECT);
    addEntry(IDB_FILTER_CONFIG);
    addEntry(IDB_SCENE_SELECT);
    addEntry(IDB_SCENE_MODE);
    addEntry(IDB_OSC_OCTAVE);
    addEntry(IDB_SCENE_OCTAVE);
    addEntry(IDB_WAVESHAPER_MODE);
    addEntry(IDB_PLAY_MODE);
    addEntry(IDB_OSC_RETRIGGER);
    addEntry(IDB_OSC_KEYTRACK);
    addEntry(IDB_MIXER_MUTE);
    addEntry(IDB_MIXER_SOLO);
    addEntry(IDB_OSC_FM_ROUTING);
    addEntry(IDB_FILTER2_RESONANCE_LINK);
    addEntry(IDB_MIXER_OSC_ROUTING);
    addEntry(IDB_ENV_SHAPE);
    addEntry(IDB_FX_GLOBAL_BYPASS);
    addEntry(IDB_LFO_TRIGGER_MODE);
    addEntry(IDB_PREVNEXT_JOG);
    addEntry(IDB_LFO_UNIPOLAR);
    addEntry(IDB_OSC_CHARACTER);
    addEntry(IDB_SAVE_PATCH);
    addEntry(IDB_FX_GRID);
    addEntry(IDB_FX_TYPE_ICONS);
    addEntry(IDB_OSC_MENU);
    addEntry(IDB_SLIDER_HORIZ_BG);
    addEntry(IDB_SLIDER_VERT_BG);
    addEntry(IDB_SLIDER_HORIZ_HANDLE);
    addEntry(IDB_SLIDER_VERT_HANDLE);
    addEntry(IDB_ENV_MODE);
    addEntry(IDB_MAIN_MENU);
    addEntry(IDB_LFO_TYPE);
    addEntry(IDB_MENU_AS_SLIDER);
    addEntry(IDB_FILTER_MENU);
    addEntry(IDB_FILTER_ICONS);
    addEntry(IDB_MIDI_LEARN);
    addEntry(IDB_WAVESHAPER_BG);
    addEntry(IDB_WAVESHAPER_ANALYSIS);
    addEntry(IDB_MODMENU_ICONS);
    addEntry(IDB_FAVORITE_BUTTON);
    addEntry(IDB_SEARCH_BUTTON);
    addEntry(IDB_FAVORITE_MENU_ICON);
    addEntry(IDB_UNDO_BUTTON);
    addEntry(IDB_REDO_BUTTON);
    addEntry(IDB_FILTER_ANALYSIS);

    // == SVG == Do not remove this comment - it indicates the start of the automated SVG adding
    // block
    addEntry(IDB_MSEG_NODES);
    addEntry(IDB_MSEG_MOVEMENT_MODE);
    addEntry(IDB_SURGE_ICON);
    addEntry(IDB_MSEG_VERTICAL_SNAP);
    addEntry(IDB_MSEG_HORIZONTAL_SNAP);
    addEntry(IDB_MPE_BUTTON);
    addEntry(IDB_ZOOM_BUTTON);
    addEntry(IDB_TUNE_BUTTON);
    addEntry(IDB_NUMFIELD_POLY_SPLIT);
    addEntry(IDB_NUMFIELD_PITCHBEND);
    addEntry(IDB_NUMFIELD_KEYTRACK_ROOT);
    addEntry(IDB_MSEG_LOOP_MODE);
    addEntry(IDB_LFO_MSEG_EDIT);
    addEntry(IDB_LFO_PRESET_MENU);
    addEntry(IDB_MSEG_SNAPVALUE_NUMFIELD);
    addEntry(IDB_MODSOURCE_SHOW_LFO);
    addEntry(IDB_MSEG_EDIT_MODE);
    addEntry(IDB_ABOUT_LOGOS);
    addEntry(IDB_ABOUT_BG);
    addEntry(IDB_VUMETER_BARS);
    // == /SVG == Do not remove this comment
}

void SurgeImageStore::addEntry(int id)
{
    assert(bitmap_registry.find(id) == bitmap_registry.end());

    SurgeImage *bitmap = new SurgeImage(id);

    bitmap_registry[id] = bitmap;

    auto checkAndAdd = [this, id](const std::string &pfx) {
        if (auto *h = SurgeImage::createFromBinaryWithPrefix(pfx, id))
        {
            std::string name = fmt::format("DEFAULT/{}{:05d}.svg", pfx, id);
            bitmap_stringid_registry[name] = h;
        }
    };

    checkAndAdd("hover");
    checkAndAdd("hoverOn");
    checkAndAdd("bmpTS");
    checkAndAdd("hoverTS");
}

SurgeImage *SurgeImageStore::getImage(int id) { return bitmap_registry.at(id); }

SurgeImage *SurgeImageStore::getImageByPath(const std::string &filename)
{
    if (bitmap_file_registry.find(filename) == bitmap_file_registry.end())
        return nullptr;
    return bitmap_file_registry.at(filename);
}

SurgeImage *SurgeImageStore::getImageByStringID(const std::string &id)
{
    if (bitmap_stringid_registry.find(id) == bitmap_stringid_registry.end())
        return nullptr;
    return bitmap_stringid_registry[id];
}

SurgeImage *SurgeImageStore::loadImageByPath(const std::string &filename)
{
    if (bitmap_file_registry.find(filename) != bitmap_file_registry.end())
    {
        delete bitmap_file_registry[filename];
    }
    bitmap_file_registry[filename] = new SurgeImage(filename);
    return bitmap_file_registry[filename];
}

SurgeImage *SurgeImageStore::loadImageByPathForID(const std::string &filename, int id)
{
    if (bitmap_registry.find(id) != bitmap_registry.end())
    {
        delete bitmap_registry[id];
    }
    bitmap_registry[id] = new SurgeImage(filename);
    return bitmap_registry[id];
}

SurgeImage *SurgeImageStore::loadImageByPathForStringID(const std::string &filename, std::string id)
{
    if (bitmap_stringid_registry.find(id) != bitmap_stringid_registry.end())
    {
        delete bitmap_stringid_registry[id];
    }
    bitmap_stringid_registry[id] = new SurgeImage(filename);
    return bitmap_stringid_registry[id];
}

void SurgeImageStore::setPhysicalZoomFactor(int pzf)
{
    for (auto pair : bitmap_registry)
        pair.second->setPhysicalZoomFactor(pzf);
    for (auto pair : bitmap_file_registry)
        pair.second->setPhysicalZoomFactor(pzf);
    for (auto pair : bitmap_stringid_registry)
        pair.second->setPhysicalZoomFactor(pzf);
}
