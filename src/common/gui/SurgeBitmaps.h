#pragma once

#include "resource.h"
#include "vstgui/vstgui.h"
#include <map>

class CScalableBitmap;

class SurgeBitmaps
{
public:
   SurgeBitmaps();
   virtual ~SurgeBitmaps();

   void setupBitmapsForFrame(VSTGUI::CFrame* f);
   void setPhysicalZoomFactor(int pzf);

   CScalableBitmap* getBitmap(int id);

   bool containsLayoutBitmap(int layoutid, std::string key);
   CScalableBitmap *storeLayoutBitmap(int layoutid, std::string key, std::string svgContents, VSTGUI::CFrame* f);
   CScalableBitmap* getLayoutBitmap(int layoutid, std::string key);

protected:
   void addEntry(int id, VSTGUI::CFrame* f);
   std::map<int, CScalableBitmap*> bitmap_registry;
   std::map<std::pair<int, std::string>, CScalableBitmap*> layoutBitmapRegistry;
};
