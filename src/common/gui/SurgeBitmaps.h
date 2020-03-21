#pragma once

#include "resource.h"
#include "vstgui/vstgui.h"
#include <map>
#include <atomic>

class CScalableBitmap;

class SurgeBitmaps
{
public:
   SurgeBitmaps();
   virtual ~SurgeBitmaps();

   void setupBitmapsForFrame(VSTGUI::CFrame* f);
   void setPhysicalZoomFactor(int pzf);

   CScalableBitmap* getBitmap(int id);
   CScalableBitmap* getBitmapByPath(std::string filename);
   CScalableBitmap* getBitmapByStringID(std::string id);
   
   CScalableBitmap* loadBitmapByPath(std::string filename);
   CScalableBitmap* loadBitmapByPathForID(std::string filename, int id);
   CScalableBitmap* loadBitmapByPathForStringID(std::string filename, std::string id);
   
protected:
   static std::atomic<int> instances;
   
   void addEntry(int id, VSTGUI::CFrame* f);
   std::map<int, CScalableBitmap*> bitmap_registry;
   std::map<std::string, CScalableBitmap*> bitmap_file_registry;
   std::map<std::string, CScalableBitmap*> bitmap_stringid_registry;
   VSTGUI::CFrame *frame;
};
