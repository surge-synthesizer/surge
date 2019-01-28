//-------------------------------------------------------------------------------------------------------
//	Copyright 2017 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CCursorHidingControl : public VSTGUI::CControl
{
protected:
   CCursorHidingControl(const VSTGUI::CRect& size,
                        VSTGUI::IControlListener* listener,
                        int32_t tag,
                        VSTGUI::CBitmap* pBackground);
   virtual ~CCursorHidingControl();

   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

   virtual void
   onMouseMoveDelta(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons, double dx, double dy) = 0;
   virtual double getMouseDeltaScaling(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

   void detachCursor(VSTGUI::CPoint& where);
   void attachCursor();

private:
   void doDetach(VSTGUI::CPoint& where);
   void doAttach();

   VSTGUI::CPoint _lastPos, _detachPos;
   bool _isDetatched = false;

   // OS specific (screen-space)
   double _sumDX = 0, _sumDY = 0;
   double _hideX = 0, _hideY = 0;
};
