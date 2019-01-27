//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CModulationSourceButton : public CCursorHidingControl
{
private:
   typedef CCursorHidingControl super;

public:
   CModulationSourceButton(
       const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag, int state, int msid);
   ~CModulationSourceButton();

   virtual void setValue(float val)
   {
      if (value != val)
         invalid();
      value = val;
   }

   virtual void onMouseMoveDelta(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons, double dx, double dy);
   virtual double getMouseDeltaScaling(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

   int state, msid, controlstate;

   bool tint, used, blink;
   int dispval;
   bool click_is_editpart, event_is_drag, is_metacontroller, bipolar;
   char label[16];
   VSTGUI::CRect  MCRect;
   VSTGUI::CPoint LastPoint;
   VSTGUI::CPoint SourcePoint;
   float OldValue;

   void setblink(bool state);
   void setlabel(const char*);
   void set_ismeta(bool);
   virtual void setBipolar(bool);
   void update_rt_vals(bool ntint, float fdispval, bool used);
   int get_state()
   {
      return state;
   }
   virtual void draw(VSTGUI::CDrawContext* dc);
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   CLASS_METHODS(CModulationSourceButton, VSTGUI::CControl)
};
