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

/**
 * get the loaded SurgeBitmap by ID. 
 *
 * @param id is the id enumerated in resource.h
 * @param newInstance  returns a distinct instance of the bitmap 
 * class rather than a shared one, which allows us to overcome a problem 
 * with background image scaling in vstgui. In current implementation
 * it will result in an error if used with any id other than ID_BG
 */
VSTGUI::CBitmap* getSurgeBitmap(int id, bool newInstance = false);
