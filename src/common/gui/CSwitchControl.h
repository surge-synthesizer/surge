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
#include "SkinSupport.h"
#include "CScalableBitmap.h"

class CSwitchControl : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponnt
{
public:
   CSwitchControl(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag, VSTGUI::CBitmap* background);
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   virtual void setValue(float f) override;
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override; ///< called when a mouse up event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseMoved(VSTGUI::CPoint& where,
                const VSTGUI::CButtonState& buttons) override; ///< called when a mouse move event occurs

   virtual VSTGUI::CMouseEventResult onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // getFrame()->setCursor( VSTGUI::kCursorHand );
      doingHover = true;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }
   virtual VSTGUI::CMouseEventResult onMouseExited (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // getFrame()->setCursor( VSTGUI::kCursorDefault );
      doingHover = false;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }


   int ivalue, imax;
   bool is_itype;
   bool lookedForHover = false;
   bool doingHover = false;
   CScalableBitmap *hoverBmp = nullptr;

private:
   bool down;
   float heightOfSingleImage;

   CLASS_METHODS(CSwitchControl, VSTGUI::CControl)
};
