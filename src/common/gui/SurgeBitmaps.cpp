#include "SurgeBitmaps.h"
#include <map>

#if MAC  
#define USE_SCALABLE_BITMAPS 1
#endif

#ifdef USE_SCALABLE_BITMAPS
#include "CScalableBitmap.h"
#endif

std::map<int, VSTGUI::CBitmap*> bitmap_registry;

static std::atomic_int refCount(0);

SurgeBitmaps::SurgeBitmaps()
{
   if (refCount == 0)
   {
      addEntry(IDB_BG);
      addEntry(IDB_BUTTON_ABOUT);
      addEntry(IDB_ABOUT);
      addEntry(IDB_FILTERBUTTONS);
      addEntry(IDB_OSCSWITCH); 
      addEntry(IDB_FILTERSUBTYPE);
      addEntry(IDB_RELATIVE_TOGGLE);
      addEntry(IDB_OSCSELECT);
      addEntry(IDB_FBCONFIG);
      addEntry(IDB_SCENESWITCH);
      addEntry(IDB_SCENEMODE);
      addEntry(IDB_OCTAVES);
      addEntry(IDB_WAVESHAPER);
      addEntry(IDB_POLYMODE);
      addEntry(IDB_SWITCH_RETRIGGER);
      addEntry(IDB_SWITCH_KTRK);
      addEntry(IDB_SWITCH_MUTE);
      addEntry(IDB_SWITCH_SOLO);
      addEntry(IDB_FMCONFIG);
      addEntry(IDB_SWITCH_LINK);
      addEntry(IDB_OSCROUTE);
      addEntry(IDB_ENVSHAPE);
      addEntry(IDB_FXBYPASS);
      addEntry(IDB_LFOTRIGGER);
      addEntry(IDB_BUTTON_CHECK);
      addEntry(IDB_BUTTON_MINUSPLUS);
      addEntry(IDB_UNIPOLAR);
      addEntry(IDB_CHARACTER);
      addEntry(IDB_BUTTON_STORE);
      addEntry(IDB_MODSRC_BG);
      addEntry(IDB_FXCONF);
      addEntry(IDB_FXCONF_SYMBOLS);
      addEntry(IDB_OSCMENU);
      addEntry(IDB_FADERH_BG);
      addEntry(IDB_FADERV_BG);
      addEntry(IDB_FADERH_HANDLE);
      addEntry(IDB_FADERV_HANDLE);
      addEntry(IDB_ENVMODE);
      addEntry(IDB_STOREPATCH);
   }
   refCount++;
}

SurgeBitmaps::~SurgeBitmaps()
{
   refCount--;

   if (refCount == 0)
   {
      std::map<int, VSTGUI::CBitmap*>::iterator iter;

      for (iter = bitmap_registry.begin(); iter != bitmap_registry.end(); ++iter)
      {
         iter->second->forget();
      }
      bitmap_registry.clear();
   }
}

void SurgeBitmaps::addEntry(int id)
{
   assert(bitmap_registry.find(id) == bitmap_registry.end());

#ifdef USE_SCALABLE_BITMAPS
   VSTGUI::CBitmap *bitmap = new CScalableBitmap(CResourceDescription(id));
#else
   VSTGUI::CBitmap* bitmap = new VSTGUI::CBitmap(CResourceDescription(id));
#endif
   bitmap_registry[id] = bitmap;
}

VSTGUI::CBitmap* getSurgeBitmap(int id)
{
   return bitmap_registry.at(id);
}
