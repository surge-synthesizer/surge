//-------------------------------------------------------------------------------------------------------
//	Copyright 2017 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CCursorHidingControl : public CControl
{
protected:
   CCursorHidingControl(const CRect& size,
                        IControlListener* listener,
                        int32_t tag,
                        CBitmap* pBackground);
   virtual ~CCursorHidingControl();

   virtual CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons);
   virtual CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons);
   virtual CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons);

   virtual void
   onMouseMoveDelta(CPoint& where, const CButtonState& buttons, double dx, double dy) = 0;
   virtual double getMouseDeltaScaling(CPoint& where, const CButtonState& buttons);

   void detachCursor(CPoint& where);
   void attachCursor();

private:
   void doDetach(CPoint& where);
   void doAttach();

   CPoint _lastPos, _detachPos;
   bool _isDetatched = false;

   // OS specific (screen-space)
   double _sumDX = 0, _sumDY = 0;
   double _hideX = 0, _hideY = 0;
};