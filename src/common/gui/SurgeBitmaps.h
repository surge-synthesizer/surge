#pragma once

#include "resource.h"
#include <vstgui/vstgui.h>

class SurgeBitmaps
{
public:
   SurgeBitmaps();
   virtual ~SurgeBitmaps();

protected:
   void addEntry(int id);
};

VSTGUI::CBitmap* getSurgeBitmap(int id);