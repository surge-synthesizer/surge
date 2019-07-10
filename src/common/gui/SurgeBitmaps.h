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

protected:
   void addEntry(int id, VSTGUI::CFrame* f);
   std::map<int, CScalableBitmap*> bitmap_registry;
};
