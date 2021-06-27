#include "SurgeBitmaps.h"

#include "CScalableBitmap.h"
#include <iostream>
#include <cassert>

using namespace VSTGUI;

std::atomic<int> SurgeBitmaps::instances(0);
SurgeBitmaps::SurgeBitmaps()
{
    instances++;
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeBitmaps::SurgeBitmaps");
#endif

    // std::cout << "Constructing a SurgeBitmaps; Instances is " << instances << std::endl;
}

SurgeBitmaps::~SurgeBitmaps()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeBitmaps::~SurgeBitmaps");
#endif
    clearAllLoadedBitmaps();
    instances--;
    // std::cout << "Destroying a SurgeBitmaps; Instances is " << instances << std::endl;
}

void SurgeBitmaps::clearAllLoadedBitmaps()
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

void SurgeBitmaps::setupBuiltinBitmaps()
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
    addEntry(IDB_STORE_PATCH);
    addEntry(IDB_MODSOURCE_BG);
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

void SurgeBitmaps::addEntry(int id)
{
    assert(bitmap_registry.find(id) == bitmap_registry.end());

    CScalableBitmap *bitmap = new CScalableBitmap(VSTGUI::CResourceDescription(id));

    bitmap_registry[id] = bitmap;
}

CScalableBitmap *SurgeBitmaps::getBitmap(int id) { return bitmap_registry.at(id); }

CScalableBitmap *SurgeBitmaps::getBitmapByPath(const std::string &path)
{
    if (bitmap_file_registry.find(path) == bitmap_file_registry.end())
        return nullptr;
    return bitmap_file_registry.at(path);
}

CScalableBitmap *SurgeBitmaps::getBitmapByStringID(const std::string &id)
{
    if (bitmap_stringid_registry.find(id) == bitmap_stringid_registry.end())
        return nullptr;
    return bitmap_stringid_registry[id];
}

CScalableBitmap *SurgeBitmaps::loadBitmapByPath(const std::string &path)
{
    if (bitmap_file_registry.find(path) != bitmap_file_registry.end())
    {
        delete bitmap_file_registry[path];
    }
    bitmap_file_registry[path] = new CScalableBitmap(path);
    return bitmap_file_registry[path];
}

CScalableBitmap *SurgeBitmaps::loadBitmapByPathForID(const std::string &path, int id)
{
    if (bitmap_registry.find(id) != bitmap_registry.end())
    {
        delete bitmap_registry[id];
    }
    bitmap_registry[id] = new CScalableBitmap(path);
    return bitmap_registry[id];
}

CScalableBitmap *SurgeBitmaps::loadBitmapByPathForStringID(const std::string &path, std::string id)
{
    if (bitmap_stringid_registry.find(id) != bitmap_stringid_registry.end())
    {
        delete bitmap_stringid_registry[id];
    }
    bitmap_stringid_registry[id] = new CScalableBitmap(path);
    return bitmap_stringid_registry[id];
}

void SurgeBitmaps::setPhysicalZoomFactor(int pzf)
{
    for (auto pair : bitmap_registry)
        pair.second->setPhysicalZoomFactor(pzf);
    for (auto pair : bitmap_file_registry)
        pair.second->setPhysicalZoomFactor(pzf);
    for (auto pair : bitmap_stringid_registry)
        pair.second->setPhysicalZoomFactor(pzf);
}

juce::Drawable *SurgeBitmaps::getDrawable(int id)
{
    auto b = getBitmap(id);
    if (b)
        return b->drawable.get();
    return nullptr;
}

juce::Drawable *SurgeBitmaps::getDrawableByPath(const std::string &filename)
{
    auto b = getBitmapByPath(filename);
    if (b)
        return b->drawable.get();
    return nullptr;
}

juce::Drawable *SurgeBitmaps::getDrawableByStringID(const std::string &id)
{
    auto b = getBitmapByStringID(id);
    if (b)
        return b->drawable.get();
    return nullptr;
}