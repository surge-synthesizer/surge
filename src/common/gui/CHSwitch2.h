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
#include "DebugHelpers.h"

class CHSwitch2 : public VSTGUI::CHorizontalSwitch, public Surge::UI::SkinConsumingComponnt
{
public:
   CHSwitch2(const VSTGUI::CRect& size,
             VSTGUI::IControlListener* listener,
             long tag,
             long subPixmaps,       // number of subPixmaps
             long heightOfOneImage, // pixel
             long rows,
             long columns,
             VSTGUI::CBitmap* background,
             VSTGUI::CPoint& offset,
             bool dragable = false)
       : CHorizontalSwitch(
             size, listener, tag, subPixmaps, heightOfOneImage, subPixmaps, background, offset)
   {
      this->rows = rows;
      this->columns = columns;
      this->dragable = dragable;

      mouseDowns = 0;
      imgoffset = 0;
      usesMouseWheel = true; // use mousewheel by default
   }

   int rows, columns;
   // This matches the paint offset
   virtual int getIValue() { return (int)(value * (float)(rows * columns - 1) + 0.5f); }
   
   int mouseDowns;
   int imgoffset;
   bool dragable;
   bool usesMouseWheel;

   bool lookedForHover = false;
   CScalableBitmap *hoverBmp = nullptr, *hoverOnBmp = nullptr;
   bool doingHover = false;
   float hoverValue = 0.0f;
   
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override; ///< called when a mouse up event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseMoved(VSTGUI::CPoint& where,
                const VSTGUI::CButtonState& buttons) override; ///< called when a mouse move event occurs
   virtual bool
   onWheel (const VSTGUI::CPoint& where, const float& distance, const VSTGUI::CButtonState& buttons) override; ///< called when scrollwheel events occurs

   virtual VSTGUI::CMouseEventResult onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // disable hand as we move to hovertasticism
      // getFrame()->setCursor( VSTGUI::kCursorHand );
      doingHover = true;
      hoverValue = -1;
      calculateHoverValue(where);
      invalid();
      return VSTGUI::kMouseEventHandled;
   }
   void calculateHoverValue( const VSTGUI::CPoint &where );
   virtual VSTGUI::CMouseEventResult onMouseExited (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // getFrame()->setCursor( VSTGUI::kCursorDefault );
      doingHover = false;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }

   
   CLASS_METHODS(CHSwitch2, VSTGUI::CControl)
   void setUsesMouseWheel(bool wheel);
};
