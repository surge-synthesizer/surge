/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

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

   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;

   virtual void
   onMouseMoveDelta(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons, double dx, double dy) = 0;
   virtual double getMouseDeltaScaling(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

   void detachCursor(VSTGUI::CPoint& where);
   void attachCursor();

   bool hideCursor = true;
   bool scaleAnyway = false;

private:
   void doDetach(VSTGUI::CPoint& where);
   void doAttach();

   VSTGUI::CPoint _lastPos, _detachPos;
   bool _isDetatched = false;

   // OS specific (screen-space)
   double _hideX = 0, _hideY = 0;
   // double _sumDX = 0, _sumDY = 0;    // this was there before but interfered with proper cursor hiding operation
   // but you know, let's keep it here because we don't know if it might be necessary for something else!
};
