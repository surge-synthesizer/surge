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

#include "SurgeGUIEditor.h"
#include "CMenuAsSlider.h"
#include "CScalableBitmap.h"
#include <iostream>
#include "SkinColors.h"
#include "DebugHelpers.h"

using namespace VSTGUI;

extern VSTGUI::CFontRef displayFont;

CMenuAsSlider::CMenuAsSlider(const VSTGUI::CPoint& loc,
                             VSTGUI::IControlListener* listener,
                             long tag,
                             std::shared_ptr<SurgeBitmaps> bitmapStore,
                             SurgeStorage* storage) :
   CControl(CRect( loc, CPoint( 133, 22 ) ), listener, tag )
{
   // this->storage = storage;
   auto size = CRect( 0, 0, 133, 22);
   size.offset( loc.x, loc.y );
   setViewSize( size );
   setMouseableArea( size );
}

CMenuAsSlider::~CMenuAsSlider() {
}

void CMenuAsSlider::draw( VSTGUI::CDrawContext *dc )
{
   auto r = getViewSize();

   auto d = r;
   d.top += 2; d.bottom -= 2; 


   if( isHover && pBackgroundHover )
   {
      pBackgroundHover->draw( dc, d );
   }
   else if( pBackground )
   {
      pBackground->draw( dc, d );
   }

   auto sge = dynamic_cast<SurgeGUIEditor*>(listener);

   dc->setFont(displayFont);
   int splitPoint = 48;
   if( sge )
   {
      std::string dt = sge->getDisplayForTag( getTag() );
      auto valcol = skin->getColor(Colors::Menu::Value, kBlackCColor);
      if( isHover )
         valcol = skin->getColor(Colors::Menu::ValueHover, CColor(60, 20, 0));

      if( deactivated )
         valcol = skin->getColor( Colors::Menu::ValueDeactivated, CColor( 180, 180, 180 ) );
      
      dc->setFontColor( valcol );
      auto t = d;
      t.right -= 14;
      t.left += splitPoint;
      auto td = dt;
      bool trunc = false;
      while( dc->getStringWidth( td.c_str() ) > t.getWidth() && td.length() > 4 )
      {
         td = td.substr( 0, td.length() - 3 );
         trunc = true;
      }
      if( trunc ) td += "...";
      dc->drawString( td.c_str(), t, kRightText, true );

      auto l = d;
      l.left += 5;
      l.right = d.left + splitPoint;
      auto tl = label;
      trunc = false;
      while( dc->getStringWidth( tl.c_str() ) > l.getWidth() && tl.length() > 4 )
      {
         tl = tl.substr( 0, tl.length() - 3 );
         trunc = true;
      }
      if( trunc ) tl += "...";
      auto labcol = skin->getColor(Colors::Menu::Name, kBlackCColor);
      if (isHover)
         labcol = skin->getColor(Colors::Menu::NameHover, CColor(60, 20, 0));

      if( deactivated )
         labcol = skin->getColor( Colors::Menu::NameDeactivated, CColor( 180, 180, 180 ) );

      dc->setFontColor( labcol );
      dc->drawString( tl.c_str(), l, kLeftText, true ); 
   }
}

CMouseEventResult CMenuAsSlider::onMouseDown( CPoint &w, const CButtonState &buttons ) {
   if( ! deactivated )
   {
      // fake up an RMB since we are a slider sit-in
      listener->controlModifierClicked( this, buttons | kRButton );
   }
   return kMouseEventHandled;
}

void CMenuAsSlider::onSkinChanged()
{
   pBackground = associatedBitmapStore->getBitmap( IDB_MENU_IN_SLIDER_BG );
   pBackgroundHover = skin->hoverBitmapOverlayForBackgroundBitmap(skinControl,
                                                              dynamic_cast<CScalableBitmap*>( pBackground ),
                                                              associatedBitmapStore,
                                                              Surge::UI::Skin::HoverType::HOVER);
}

bool CMenuAsSlider::onWheel( const VSTGUI::CPoint &where, const float &distance, const VSTGUI::CButtonState &buttons ) {
   wheelDistance += distance;
   Surge::Debug::stackTraceToStdout();
   float threshold = 1;
   auto fv = [this](float v, int inc)
                {
                   int iv = floor( getValue() * ( iMax - iMin ) + 0.5 );
                   iv = iv + inc;
                   if( iv < 0 ) iv = iMax;
                   if( iv > iMax ) iv = 0;
                   // This is the get_value_f01 code
                   float r = 0.005 + 0.99 * ( iv - iMin ) / ( float) ( iMax - iMin );
                   return r;

                };
#if WINDOWS
   threshold = 0.33;
#endif   
   if( wheelDistance > threshold ) {
      wheelDistance -= threshold;
      setValue( fv( getValue(), -1 ) );
      if( listener )
         listener->valueChanged(this);
   }
   if( wheelDistance < -threshold ) {
      wheelDistance += threshold;
      setValue( fv( getValue(), +1 ) );
      if( listener )
         listener->valueChanged(this);
   }
   return true;
}
