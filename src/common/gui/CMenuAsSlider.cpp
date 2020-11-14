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
                             const VSTGUI::CPoint &sz,
                             VSTGUI::IControlListener* listener,
                             long tag,
                             std::shared_ptr<SurgeBitmaps> bitmapStore,
                             SurgeStorage* storage) :
   CControl(CRect( loc, sz ), listener, tag, nullptr ),
      Surge::UI::CursorControlAdapter<CMenuAsSlider>(storage)
{
   // this->storage = storage;
   auto size = CRect( 0, 0, sz.x, sz.y );
   size.offset( loc.x, loc.y );

   setViewSize( size );
   setMouseableArea( size );
}

CMenuAsSlider::~CMenuAsSlider() {
}

void CMenuAsSlider::draw( VSTGUI::CDrawContext *dc )
{
   auto r = getViewSize();
#if DEBUGLAYOUT
   if( isHover )
   {
      dc->setFillColor( kRedCColor );
      dc->drawRect( r, kDrawFilled );
   }
   else
   {
      dc->setFillColor( kGreenCColor );
      dc->drawRect( r, kDrawFilled );
   }
#endif   

   auto d = r;
   d.top += 2; d.bottom -= 2; 

   if (hasDragRegion)
   {
      d.left += dragRegion.getWidth();
   }

   if( pBackground )
   {
      pBackground->draw( dc, d );
   }

   if( isHover && pBackgroundHover )
   {
      pBackgroundHover->draw( dc, d );
   }

   auto sge = dynamic_cast<SurgeGUIEditor*>(listener);

   dc->setFont(displayFont);
   int splitPoint = 48;
   if( sge )
   {
      std::string dt = sge->getDisplayForTag( getTag() );
      auto valcol = skin->getColor(Colors::Menu::Value);

      if( isHover )
         valcol = skin->getColor(Colors::Menu::ValueHover);

      if( deactivated )
         valcol = skin->getColor(Colors::Menu::ValueDeactivated);

      if( filtermode )
      {
         valcol = skin->getColor(Colors::Menu::FilterValue);
         if( isHover )
            valcol = skin->getColor(Colors::Menu::FilterValueHover);
      }
      
      dc->setFont( displayFont );
      dc->setFontColor( valcol );

      bool trunc = false;
      auto align = kRightText;
      auto t = d;

      t.right -= 14;
      if( filtermode )
      {
         t.right -= 6;
         t.left += 6;
         t.top += 1;
         align = kLeftText;
      }
      else
         t.left += splitPoint;
      auto td = dt;
      while( dc->getStringWidth( td.c_str() ) > t.getWidth() && td.length() > 4 )
      {
         td = td.substr( 0, td.length() - 3 );
         trunc = true;
      }
      if( trunc ) td += "...";
      dc->drawString( td.c_str(), t, align, true );

      if( ! filtermode )
      {
         auto l = d;
         l.left += 6;
         l.right = d.left + splitPoint;
         auto tl = label;
         trunc = false;

         while( dc->getStringWidth( tl.c_str() ) > l.getWidth() && tl.length() > 4 )
         {
            tl = tl.substr( 0, tl.length() - 3 );
            trunc = true;
         }

         if( trunc ) tl += "...";
         auto labcol = skin->getColor(Colors::Menu::Name);

         if (isHover)
            labcol = skin->getColor(Colors::Menu::NameHover);
         
         if( deactivated )
            labcol = skin->getColor( Colors::Menu::NameDeactivated);
         
         dc->setFontColor( labcol );
         dc->drawString( tl.c_str(), l, kLeftText, true );
      }

      if( hasDragRegion )
      {
         int iv = floor( getValue() * ( iMax - iMin ) + 0.5 );
         auto dbox = getViewSize();
         dbox.right = dbox.left + dglyphsize;
         float dw = (dbox.getHeight() - dglyphsize) / 2.0;
         dbox.top += dw;
         dbox.bottom -= dw;
         VSTGUI::CPoint p( 0, iv * dglyphsize );

         if( pGlyph )
         {
            pGlyph->draw( dc, dbox, p, 0xff );
         }

         if( isHover && pGlyphHover )
         {
            pGlyphHover->draw( dc, dbox, p, 0xff );
         }
      }
   }
}

CMouseEventResult CMenuAsSlider::onMouseDown( CPoint &w, const CButtonState &buttons ) {
   if( hasDragRegion && dragRegion.pointInside( w ) )
   {
      if( buttons & kDoubleClick )
      {
         setValue(0);
         if( listener )
            listener->valueChanged(this);
      }
      else
      {
         startCursorHide(w);
         isDragRegionDrag = true;
         dragStart = w;
         dragDir = unk;
      }
      return kMouseEventHandled;
   }
   if( ! deactivated )
   {
      // fake up an RMB since we are a slider sit-in
      listener->controlModifierClicked( this, buttons | kRButton );
   }
   return kMouseEventHandled;
}

CMouseEventResult CMenuAsSlider::onMouseMoved( CPoint &w, const CButtonState &buttons ) {
   if( isDragRegionDrag )
   {
      double dx = ( w.x - dragStart.x );
      double dy =( w.y - dragStart.y );
      auto distx = ( dx / dragRegion.getWidth() * 3 );
      auto disty = ( dy / dragRegion.getWidth() * 3 );

      if( dragDir == unk )
      {
         if( distx > 1 || distx < -1 ) dragDir = CMenuAsSlider::dirx;
         if( disty > 1 || disty < -1 ) dragDir = CMenuAsSlider::diry;
      }

      float dist = ( dragDir == dirx ? distx : ( dragDir == diry ? disty : 0.0 ) );
      int inc = 0;
      if( dist > 1 )
      {
         inc = 1;
         if( ! resetToShowLocation() ) dragStart = w;
      }
      if( dist < -1 )
      {
         inc = -1;
         if( ! resetToShowLocation() ) dragStart = w;
      }

      if( inc != 0 )
      {
         int iv = floor( getValue() * ( iMax - iMin ) + 0.5 );
         
         iv = iv + inc;
         if( iv < 0 ) iv = iMax;
         if( iv > iMax ) iv = 0;
         // This is the get_value_f01 code
         float r = 0.005 + 0.99 * ( iv - iMin ) / ( float) ( iMax - iMin );
         setValue( r );
         if( listener )
            listener->valueChanged(this);
      }

   }
   return kMouseEventHandled;
}

CMouseEventResult CMenuAsSlider::onMouseUp( CPoint &w, const CButtonState &buttons ) {
   if( isDragRegionDrag )
   {
      endCursorHide();
      isDragRegionDrag = false;
   }
   return kMouseEventHandled;
}

void CMenuAsSlider::onSkinChanged()
{
   if( associatedBitmapStore && skin )
   {
      pBackground = associatedBitmapStore->getBitmap( bgid );
      pBackgroundHover = skin->hoverBitmapOverlayForBackgroundBitmap(skinControl,
                                                                     dynamic_cast<CScalableBitmap*>( pBackground ),
                                                                     associatedBitmapStore,
                                                                     Surge::UI::Skin::HoverType::HOVER);

      if( dglphyid > 0 )
      {
         pGlyph = associatedBitmapStore->getBitmap( dglphyid );
         pGlyphHover = skin->hoverBitmapOverlayForBackgroundBitmap(skinControl,
                                                                   dynamic_cast<CScalableBitmap*>( pGlyph ),
                                                                   associatedBitmapStore,
                                                                   Surge::UI::Skin::HoverType::HOVER);

      }
   }
}

bool CMenuAsSlider::onWheel( const VSTGUI::CPoint &where, const float &distance, const VSTGUI::CButtonState &buttons ) {
   wheelDistance += distance;

   float threshold = 1;
#if WINDOWS
   threshold = 0.333333;
#endif   

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

   if( wheelDistance > threshold ) {
      wheelDistance = 0;
      setValue( fv( getValue(), -1 ) );
      if( listener )
         listener->valueChanged(this);
   }
   if( wheelDistance < -threshold ) {
      wheelDistance = 0;
      setValue( fv( getValue(), +1 ) );
      if( listener )
         listener->valueChanged(this);
   }
   return true;
}

void CMenuAsSlider::setDragRegion( const VSTGUI::CRect &r )
{
   hasDragRegion = true;
   dragRegion = r;
}
