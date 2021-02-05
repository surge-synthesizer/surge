#include "SurgeBitmaps.h"
#include "UserInteractions.h"

#include "UIInstrumentation.h"
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
        pair.second->forget();
    }
    for (auto pair : bitmap_file_registry)
    {
        pair.second->forget();
    }
    for (auto pair : bitmap_stringid_registry)
    {
        pair.second->forget();
    }
    bitmap_registry.clear();
    bitmap_file_registry.clear();
    bitmap_stringid_registry.clear();
}

void SurgeBitmaps::clearAllBitmapOffscreenCaches()
{
    for (auto pair : bitmap_registry)
    {
        pair.second->clearOffscreenCaches();
    }
    for (auto pair : bitmap_file_registry)
    {
        pair.second->clearOffscreenCaches();
    }
    for (auto pair : bitmap_stringid_registry)
    {
        pair.second->clearOffscreenCaches();
    }
}

void SurgeBitmaps::setupBitmapsForFrame(VSTGUI::CFrame *f)
{
    frame = f;
    addEntry(IDB_MAIN_BG, f);
    addEntry(IDB_FILTER_SUBTYPE, f);
    addEntry(IDB_FILTER2_OFFSET, f);
    addEntry(IDB_OSC_SELECT, f);
    addEntry(IDB_FILTER_CONFIG, f);
    addEntry(IDB_SCENE_SELECT, f);
    addEntry(IDB_SCENE_MODE, f);
    addEntry(IDB_OSC_OCTAVE, f);
    addEntry(IDB_SCENE_OCTAVE, f);
    addEntry(IDB_WAVESHAPER_MODE, f);
    addEntry(IDB_PLAY_MODE, f);
    addEntry(IDB_OSC_RETRIGGER, f);
    addEntry(IDB_OSC_KEYTRACK, f);
    addEntry(IDB_MIXER_MUTE, f);
    addEntry(IDB_MIXER_SOLO, f);
    addEntry(IDB_OSC_FM_ROUTING, f);
    addEntry(IDB_FILTER2_RESONANCE_LINK, f);
    addEntry(IDB_MIXER_OSC_ROUTING, f);
    addEntry(IDB_ENV_SHAPE, f);
    addEntry(IDB_FX_GLOBAL_BYPASS, f);
    addEntry(IDB_LFO_TRIGGER_MODE, f);
    addEntry(IDB_PREVNEXT_JOG, f);
    addEntry(IDB_LFO_UNIPOLAR, f);
    addEntry(IDB_OSC_CHARACTER, f);
    addEntry(IDB_STORE_PATCH, f);
    addEntry(IDB_MODSOURCE_BG, f);
    addEntry(IDB_FX_GRID, f);
    addEntry(IDB_FX_TYPE_ICONS, f);
    addEntry(IDB_OSC_MENU, f);
    addEntry(IDB_SLIDER_HORIZ_BG, f);
    addEntry(IDB_SLIDER_VERT_BG, f);
    addEntry(IDB_SLIDER_HORIZ_HANDLE, f);
    addEntry(IDB_SLIDER_VERT_HANDLE, f);
    addEntry(IDB_ENV_MODE, f);
    addEntry(IDB_MAIN_MENU, f);
    addEntry(IDB_LFO_TYPE, f);
    addEntry(IDB_MENU_AS_SLIDER, f);
    addEntry(IDB_FILTER_MENU, f);
    addEntry(IDB_FILTER_ICONS, f);
    addEntry(IDB_MIDI_LEARN, f);

    // == SVG == Do not remove this comment - it indicates the start of the automated SVG adding
    // block
    addEntry(IDB_MSEG_NODES, f);
    addEntry(IDB_MSEG_MOVEMENT_MODE, f);
    addEntry(IDB_SURGE_ICON, f);
    addEntry(IDB_MSEG_VERTICAL_SNAP, f);
    addEntry(IDB_MSEG_HORIZONTAL_SNAP, f);
    addEntry(IDB_MPE_BUTTON, f);
    addEntry(IDB_ZOOM_BUTTON, f);
    addEntry(IDB_TUNE_BUTTON, f);
    addEntry(IDB_NUMFIELD_POLY_SPLIT, f);
    addEntry(IDB_NUMFIELD_PITCHBEND, f);
    addEntry(IDB_NUMFIELD_KEYTRACK_ROOT, f);
    addEntry(IDB_MSEG_LOOP_MODE, f);
    addEntry(IDB_LFO_MSEG_EDIT, f);
    addEntry(IDB_LFO_PRESET_MENU, f);
    addEntry(IDB_MSEG_SNAPVALUE_NUMFIELD, f);
    addEntry(IDB_MODSOURCE_SHOW_LFO, f);
    addEntry(IDB_MSEG_EDIT_MODE, f);
    addEntry(IDB_ABOUT_LOGOS, f);
    addEntry(IDB_ABOUT_BG, f);
    addEntry(IDB_VUMETER_BARS, f);
    // == /SVG == Do not remove this comment
}

void SurgeBitmaps::addEntry(int id, VSTGUI::CFrame *f)
{
    assert(bitmap_registry.find(id) == bitmap_registry.end());

    CScalableBitmap *bitmap = new CScalableBitmap(VSTGUI::CResourceDescription(id), f);

    bitmap_registry[id] = bitmap;
}

CScalableBitmap *SurgeBitmaps::getBitmap(int id) { return bitmap_registry.at(id); }

CScalableBitmap *SurgeBitmaps::getBitmapByPath(std::string path)
{
    if (bitmap_file_registry.find(path) == bitmap_file_registry.end())
        return nullptr;
    return bitmap_file_registry.at(path);
}

CScalableBitmap *SurgeBitmaps::getBitmapByStringID(std::string id)
{
    if (bitmap_stringid_registry.find(id) == bitmap_stringid_registry.end())
        return nullptr;
    return bitmap_stringid_registry[id];
}

CScalableBitmap *SurgeBitmaps::loadBitmapByPath(std::string path)
{
    if (bitmap_file_registry.find(path) != bitmap_file_registry.end())
    {
        bitmap_file_registry[path]->forget();
    }
    bitmap_file_registry[path] = new CScalableBitmap(path, frame);
    return bitmap_file_registry[path];
}

CScalableBitmap *SurgeBitmaps::loadBitmapByPathForID(std::string path, int id)
{
    if (bitmap_registry.find(id) != bitmap_registry.end())
    {
        bitmap_registry[id]->forget();
    }
    bitmap_registry[id] = new CScalableBitmap(path, frame);
    return bitmap_registry[id];
}

CScalableBitmap *SurgeBitmaps::loadBitmapByPathForStringID(std::string path, std::string id)
{
    if (bitmap_stringid_registry.find(id) != bitmap_stringid_registry.end())
    {
        bitmap_stringid_registry[id]->forget();
    }
    bitmap_stringid_registry[id] = new CScalableBitmap(path, frame);
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
