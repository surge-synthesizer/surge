#include "SurgeBitmaps.h"
#include "UserInteractions.h"

#include "CScalableBitmap.h"
#include <iostream>

using namespace VSTGUI;

SurgeBitmaps::SurgeBitmaps()
{
}

SurgeBitmaps::~SurgeBitmaps()
{
   for (auto pair : bitmap_registry)
   {
      pair.second->forget();
   }
   bitmap_registry.clear();

   for (auto pair : layoutBitmapRegistry)
   {
      pair.second->forget();
   }
   layoutBitmapRegistry.clear();
}

void SurgeBitmaps::setupBitmapsForFrame(VSTGUI::CFrame* f)
{
   addEntry(IDB_BG, f);
   addEntry(IDB_BUTTON_ABOUT, f);
   addEntry(IDB_ABOUT, f);
   addEntry(IDB_FILTERBUTTONS, f);
   addEntry(IDB_FILTERSUBTYPE, f);
   addEntry(IDB_RELATIVE_TOGGLE, f);
   addEntry(IDB_OSCSELECT, f);
   addEntry(IDB_FBCONFIG, f);
   addEntry(IDB_SCENESWITCH, f);
   addEntry(IDB_SCENEMODE, f);
   addEntry(IDB_OCTAVES, f);
   addEntry(IDB_WAVESHAPER, f);
   addEntry(IDB_POLYMODE, f);
   addEntry(IDB_SWITCH_RETRIGGER, f);
   addEntry(IDB_SWITCH_KTRK, f);
   addEntry(IDB_SWITCH_MUTE, f);
   addEntry(IDB_SWITCH_SOLO, f);
   addEntry(IDB_FMCONFIG, f);
   addEntry(IDB_SWITCH_LINK, f);
   addEntry(IDB_OSCROUTE, f);
   addEntry(IDB_ENVSHAPE, f);
   addEntry(IDB_FXBYPASS, f);
   addEntry(IDB_LFOTRIGGER, f);
   addEntry(IDB_BUTTON_MINUSPLUS, f);
   addEntry(IDB_UNIPOLAR, f);
   addEntry(IDB_CHARACTER, f);
   addEntry(IDB_BUTTON_STORE, f);
   addEntry(IDB_MODSRC_BG, f);
   addEntry(IDB_FXCONF, f);
   addEntry(IDB_FXCONF_SYMBOLS, f);
   addEntry(IDB_OSCMENU, f);
   addEntry(IDB_FADERH_BG, f);
   addEntry(IDB_FADERV_BG, f);
   addEntry(IDB_FADERH_HANDLE, f);
   addEntry(IDB_FADERV_HANDLE, f);
   addEntry(IDB_ENVMODE, f);
   addEntry(IDB_STOREPATCH, f);
   addEntry(IDB_BUTTON_MENU, f);
}

void SurgeBitmaps::addEntry(int id, VSTGUI::CFrame* f)
{
   assert(bitmap_registry.find(id) == bitmap_registry.end());

   CScalableBitmap* bitmap = new CScalableBitmap(VSTGUI::CResourceDescription(id), f);

   bitmap_registry[id] = bitmap;
}

CScalableBitmap* SurgeBitmaps::getBitmap(int id)
{
   return bitmap_registry.at(id);
}

void SurgeBitmaps::setPhysicalZoomFactor(int pzf)
{
   for (auto pair : bitmap_registry)
      pair.second->setPhysicalZoomFactor(pzf);
   for (auto pair : layoutBitmapRegistry)
      pair.second->setPhysicalZoomFactor(pzf);
}

bool SurgeBitmaps::containsLayoutBitmap(int layoutid, std::string key)
{
   auto k = std::make_pair(layoutid, key);
   return layoutBitmapRegistry.find(k) != layoutBitmapRegistry.end();
}

CScalableBitmap* SurgeBitmaps::storeLayoutBitmap(int layoutid,
                                     std::string key,
                                     std::string svgContents,
                                     VSTGUI::CFrame* f)
{
   auto k = std::make_pair(layoutid, key);
   CScalableBitmap* bitmap = new CScalableBitmap(svgContents, f);
   layoutBitmapRegistry[k] = bitmap;
   return bitmap;
}

CScalableBitmap* SurgeBitmaps::getLayoutBitmap(int layoutid, std::string key)
{
   auto k = std::make_pair(layoutid, key);
   return layoutBitmapRegistry[k];
}
