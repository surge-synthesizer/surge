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

#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "CBinaryCounterSwitch.h"

using namespace VSTGUI;

void CBinaryCounterSwitch::calculateHoverValue( const VSTGUI::CPoint &where )
{
   auto cellw = getViewSize().getWidth() / columns;
   hoverCell = (int)( (where.x-getViewSize().getTopLeft().x) / cellw );
}

void CBinaryCounterSwitch::onSkinChanged() {
   if( associatedBitmapStore && skin )
   {
      switchBmp = associatedBitmapStore->getBitmap( bmpid );
      hoverBmp = skin->hoverBitmapOverlayForBackgroundBitmap(skinControl,
                                                             dynamic_cast<CScalableBitmap*>( switchBmp ),
                                                             associatedBitmapStore,
                                                             Surge::UI::Skin::HoverType::HOVER);
   }
}

void CBinaryCounterSwitch::draw(VSTGUI::CDrawContext* dc) {
   if( ! switchBmp )
   {
      dc->setFillColor( kRedCColor );
      dc->drawRect( getViewSize(), kDrawFilled );
      return;
   }

   auto bits = columns;
   auto cellw = getViewSize().getWidth() / bits;

   for( int i=0; i<columns; ++i )
   {
      auto r = getViewSize();
      r.left += i * cellw;
      r.right = r.left + cellw;

      auto off = CPoint( i * cellw, 0 );
      if( internalValue & ( 1 << i ) )
         off.y = r.getHeight();

      if( doingHover && hoverBmp && i == hoverCell )
      {
         hoverBmp->draw( dc, r, off, 0xFF );
      }
      else
      {
         switchBmp->draw( dc, r, off, 0xFF );
      }
   }
   
}

VSTGUI::CMouseEventResult CBinaryCounterSwitch::onMouseDown(VSTGUI::CPoint& where,
                                                            const VSTGUI::CButtonState& buttons)
{
   return kMouseEventHandled;  
}

VSTGUI::CMouseEventResult CBinaryCounterSwitch::onMouseUp(VSTGUI::CPoint& where,
                                                          const VSTGUI::CButtonState& buttons)
{
   auto bits = columns;
   auto cellw = getViewSize().getWidth() / bits;
   int mc = (int)( (where.x-getViewSize().getTopLeft().x) / cellw );
   if( internalValue & ( 1 << mc ) )
   {
      internalValue = (internalValue & ~(1 << mc));
      setValueFromInternalState();
   }
   else
   {
      internalValue = (internalValue | (1 << mc));
      setValueFromInternalState();
   }
   invalid();
   return kMouseEventHandled;
}

void CBinaryCounterSwitch::setValueFromInternalState()
{
   // This is a bit gross
   for( int i=0; i<twotwobits; ++i )
   {
      if( valueRemap[i] == internalValue )
      {
         float v = 0.005 + 0.99f * i / (twotwobits-1);
         setValue( v );
         if( listener )
            listener->valueChanged(this);
         break;
      }
   }
   
}

void CBinaryCounterSwitch::setValue( float f ) {
   int vaf = (floor) ( f * twotwobits );
   if( vaf >=0 && vaf < twotwobits )
      internalValue = valueRemap[vaf];
   else
      internalValue = 0;
   CControl::setValue( f );
}
