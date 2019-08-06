//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "CDIBitmap.h"
#include "DspUtilities.h"

class COscillatorDisplay : public VSTGUI::CControl
{
public:
   COscillatorDisplay(const VSTGUI::CRect& size, OscillatorStorage* oscdata, SurgeStorage* storage)
       : VSTGUI::CControl(size, 0, 0, 0)
   {
      this->oscdata = oscdata;
      this->storage = storage;
      controlstate = 0;
   }
   virtual ~COscillatorDisplay()
   {
   }
   virtual void draw(VSTGUI::CDrawContext* dc);
   virtual bool onDrop(VSTGUI::IDataPackage* drag, const VSTGUI::CPoint& where);

   void loadWavetable(int id);
   void loadWavetableFromFile();

   // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

protected:
   void populateMenu(VSTGUI::COptionMenu* m, int selectedItem);
   bool populateMenuForCategory(VSTGUI::COptionMenu* parent, int categoryId, int selectedItem);

   OscillatorStorage* oscdata;
   SurgeStorage* storage;
   unsigned controlstate;

   VSTGUI::CRect rnext, rprev, rmenu;
   VSTGUI::CPoint lastpos;
   CLASS_METHODS(COscillatorDisplay, VSTGUI::CControl)
};
