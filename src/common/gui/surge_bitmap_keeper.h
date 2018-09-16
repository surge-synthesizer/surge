#pragma once 

#include "resource.h"
#include <vstgui/vstgui.h>

class surge_bitmap_keeper
{
public:
   surge_bitmap_keeper();
   virtual ~surge_bitmap_keeper();
  
protected:
   void addEntry(int id);
};

VSTGUI::CBitmap* getSurgeBitmap(int id);