#include "SurgeBitmaps.h"
#include "UserInteractions.h"

#include "UIInstrumentation.h"
#include "CScalableBitmap.h"
#include <iostream>

using namespace VSTGUI;

std::atomic<int> SurgeBitmaps::instances( 0 );
SurgeBitmaps::SurgeBitmaps()
{
   instances++;
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SurgeBitmaps::SurgeBitmaps" );
#endif   

   // std::cout << "Constructing a SurgeBitmaps; Instances is " << instances << std::endl;
}

SurgeBitmaps::~SurgeBitmaps()
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SurgeBitmaps::~SurgeBitmaps" );
#endif   
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
   instances --;
   // std::cout << "Destroying a SurgeBitmaps; Instances is " << instances << std::endl;
}

void SurgeBitmaps::setupBitmapsForFrame(VSTGUI::CFrame* f)
{
   frame = f;
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
   addEntry(IDB_OCTAVES_OSC , f);
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

CScalableBitmap* SurgeBitmaps::getBitmapByPath(std::string path)
{
   if( bitmap_file_registry.find(path) == bitmap_file_registry.end() )
      return nullptr;
   return bitmap_file_registry.at(path);
}

CScalableBitmap* SurgeBitmaps::getBitmapByStringID(std::string id)
{
   if( bitmap_stringid_registry.find(id) == bitmap_stringid_registry.end() )
      return nullptr;
   return bitmap_stringid_registry[id];
}

CScalableBitmap* SurgeBitmaps::loadBitmapByPath(std::string path )
{
   if( bitmap_file_registry.find(path) != bitmap_file_registry.end() )
   {
      bitmap_file_registry[path]->forget();
   }
   bitmap_file_registry[path] = new CScalableBitmap(path, frame);
   return bitmap_file_registry[path];
}

CScalableBitmap* SurgeBitmaps::loadBitmapByPathForID(std::string path, int id)
{
   if( bitmap_registry.find(id) != bitmap_registry.end() )
   {
      bitmap_registry[id]->forget();
   }
   bitmap_registry[id] = new CScalableBitmap( path, frame );
   return bitmap_registry[id];
}

CScalableBitmap* SurgeBitmaps::loadBitmapByPathForStringID(std::string path, std::string id)
{
   if( bitmap_stringid_registry.find(id) != bitmap_stringid_registry.end() )
   {
      bitmap_stringid_registry[id]->forget();
   }
   bitmap_stringid_registry[id] = new CScalableBitmap( path, frame );
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

