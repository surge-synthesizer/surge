// -*-c++-*-
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

class CBinaryCounterSwitch : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
public:
   CBinaryCounterSwitch(const VSTGUI::CRect& size,
                        VSTGUI::IControlListener* listener,
                        long tag,
                        int bits,
                        int bmpid )
                        : VSTGUI::CControl( size, listener, tag )
   {
      this->rows = 2;
      this->columns = bits;
      if( bits > 7 )
      {
         std::cerr << "HORRIBLE ERROR CONDITION ON INTERNAL ARRAY" << std::endl;
      }
      twotwobits = 1 << bits;
      for( int i=0; i<twotwobits; ++i )
         valueRemap[i] = i;
      this->bmpid = bmpid;
   }

   int rows, columns;
   
   CScalableBitmap *switchBmp = nullptr;
   CScalableBitmap *hoverBmp = nullptr;
   bool doingHover = false;
   
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where,
                                                 const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where,
                                               const VSTGUI::CButtonState& buttons) override; ///< called when a mouse up event occurs
   
   virtual VSTGUI::CMouseEventResult onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // disable hand as we move to hovertasticism
      // getFrame()->setCursor( VSTGUI::kCursorHand );
      doingHover = true;
      // hoverValue = -1;
      calculateHoverValue(where);
      invalid();
      return VSTGUI::kMouseEventHandled;
   }
   virtual VSTGUI::CMouseEventResult onMouseMoved (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // disable hand as we move to hovertasticism
      // getFrame()->setCursor( VSTGUI::kCursorHand );
      doingHover = true;
      // hoverValue = -1;
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

   void setValueFromInternalState( );

   virtual void setValue( float f ) override;
   virtual void onSkinChanged() override;

   int bmpid;
   int hoverCell;
   int internalValue;
   int twotwobits;
   int valueRemap[128];
   
   CLASS_METHODS(CBinaryCounterSwitch, VSTGUI::CControl)
};


