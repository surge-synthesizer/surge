//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "CDIBitmap.h"
#include "DspUtilities.h"

class COscillatorDisplay : public VSTGUI::CControl, public VSTGUI::IDropTarget
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
   
   virtual VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override
   {
       doingDrag = true;
       /* invalid();
          setDirty(true); */

       return VSTGUI::DragOperation::Copy;
   }
   virtual VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override
   {
       return VSTGUI::DragOperation::Copy;
   }
   virtual void onDragLeave(VSTGUI::DragEventData data) override
   {
       doingDrag = false;
       /* invalid();
          setDirty(true); */
   }
   virtual bool onDrop(VSTGUI::DragEventData data) override;
   
   virtual VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget () override { return this; }

   void loadWavetable(int id);
   void loadWavetableFromFile();

   // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

   void setIsMod(bool b)
   {
      is_mod = b;
      mod_time = 0;
   }
   void setModSource(modsources m)
   {
       modsource = m;
   }
   void tickModTime()
   {
      mod_time += 1.0 / 30.0;
   }

protected:
   void populateMenu(VSTGUI::COptionMenu* m, int selectedItem);
   bool populateMenuForCategory(VSTGUI::COptionMenu* parent, int categoryId, int selectedItem);

   OscillatorStorage* oscdata;
   SurgeStorage* storage;
   unsigned controlstate;

   bool doingDrag = false;
   bool is_mod = false;
   modsources modsource = ms_original;
   float mod_time = 0;
   VSTGUI::CRect rnext, rprev, rmenu;
   VSTGUI::CPoint lastpos;
   CLASS_METHODS(COscillatorDisplay, VSTGUI::CControl)
};
