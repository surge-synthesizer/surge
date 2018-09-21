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
       const CRect& size, IControlListener* listener, long tag, int state, int msid);
   ~CModulationSourceButton();

   virtual void setValue(float val)
   {
      if (value != val)
         invalid();
      value = val;
   }

   virtual void onMouseMoveDelta(CPoint& where, const CButtonState& buttons, double dx, double dy);
   virtual double getMouseDeltaScaling(CPoint& where, const CButtonState& buttons);

   int state, msid, controlstate;

   bool tint, used, blink;
   int dispval;
   bool click_is_editpart, event_is_drag, is_metacontroller, bipolar;
   char label[16];
   CRect MCRect;
   CPoint LastPoint;
   CPoint SourcePoint;
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
   virtual void draw(CDrawContext* dc);
   // virtual void mouse (CDrawContext *pContext, CPoint &where, long button = -1);
   virtual CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons);
   virtual CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons);
   CLASS_METHODS(CModulationSourceButton, CControl)
};