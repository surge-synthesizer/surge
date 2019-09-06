#include <iostream>
#include "CGlyphSwitch.h"


CGlyphSwitch::CGlyphSwitch( const VSTGUI::CRect &size,
                            VSTGUI::IControlListener *listener,
                            long tag,
                            int targetvalue )
   : VSTGUI::CControl( size, listener, tag )
{
   for( auto i=0; i<n_drawstates; ++i )
      bgBitmap[i] = nullptr;

   fgColor[On] = fgColor[HoverOn] = fgColor[PressOn] = VSTGUI::kWhiteCColor;
   bgColor[On] = bgColor[HoverOn] = bgColor[PressOn] = VSTGUI::kBlackCColor;

   fgColor[Off] = fgColor[HoverOff] = fgColor[PressOff] = VSTGUI::kBlackCColor;
   bgColor[Off] = bgColor[HoverOff] = bgColor[PressOff] = VSTGUI::kWhiteCColor;
}

void CGlyphSwitch::setValue(float f)
{
   value = f;
   auto scale = (rows > 1 ? rows - 1 : 1 ) * ( cols > 1 ? cols - 1 : 1 );
   multiValue = std::round( f * scale );
   invalid();
}

void CGlyphSwitch::setMultiValue(int i)
{
   multiValue = i;
   auto scale = (rows > 1 ? rows - 1 : 1 ) * ( cols > 1 ? cols - 1 : 1 );
   setValue( i * 1.0 / scale );
}

void CGlyphSwitch::draw(VSTGUI::CDrawContext *dc)
{
   auto size = getViewSize();

   if( ! isMulti() )
   {
      drawSingleElement(dc, size, 0, 0 );
   }
   else
   {
      float sx0 = size.left;
      float sy0 = size.top;
      float dsx = size.getWidth() * 1.0 / cols;
      float dsy = size.getHeight() * 1.0 / rows;
      
      for( int r=0; r<rows; ++r )
         for( int c=0; c<cols; ++c )
         {
            auto x0 = sx0 + dsx * c;
            auto y0 = sy0 + dsy * r;
            VSTGUI::CRect subS(x0,y0,x0+dsx,y0+dsy);
            drawSingleElement(dc, subS, r, c);
         }
   }
   
}
void CGlyphSwitch::drawSingleElement(VSTGUI::CDrawContext *dc, const VSTGUI::CRect &size, int r, int c )
{
   bool isOn = isMulti() ? ( r + c * rows == multiValue ) : value > 0.5;
   int currentDisplay = DrawDisplays::Off;
   if( isOn ) currentDisplay = DrawDisplays::On;

   if( mouseState & DrawState::Press && r == mouseRow && c == mouseCol )
      currentDisplay += 4;
   else if( mouseState & DrawState::Hover && r == mouseRow && c == mouseCol )
      currentDisplay += 2;
   
   switch( bgMode )
   {
   case Fill:
   {
      dc->setFillColor(bgColor[currentDisplay]);
      dc->drawRect(size, VSTGUI::kDrawFilled);
   }
   break;
   case SVG:
   {
      // FIXME that painful waterfall for nulls. For now assuem they are there
      auto b = bgBitmap[currentDisplay];
      if( b != nullptr )
      {
         VSTGUI::CPoint where(0,0);
         b->draw(dc, size, where, 0xff );
      }
      else
      {
         dc->setFillColor(VSTGUI::kRedCColor);
         dc->drawRect(size, VSTGUI::kDrawFilled);
      }
   }
   }

   switch( fgMode )
   {
   case Glyph:
   {
      if( glyph != nullptr )
      {
         glyph->updateWithGlyphColor(fgColor[currentDisplay]);
         VSTGUI::CPoint where(0,0);
         glyph->draw(dc, size, where, 0xff );
      }
   }
   break;
   case GlyphChoice:
   {
      if( glyphChoices[r + c * rows] != nullptr )
      {
         glyphChoices[r + c * rows]->updateWithGlyphColor(fgColor[currentDisplay]);
         VSTGUI::CPoint where(0,0);
         glyphChoices[r + c * rows]->draw(dc, size, where, 0xff );
      }
   }
   break;
   case Text:
   {
      auto stringR = size;
      // dc->setFontColor(fgColor);
      dc->setFontColor(fgColor[currentDisplay]);
      
      VSTGUI::SharedPointer<VSTGUI::CFontDesc> labelFont = new VSTGUI::CFontDesc(fgFont.c_str(), fgFontSize);
      dc->setFont(labelFont);

      if( isMulti() )
      {
         dc->drawString(choiceLabels[r + c * rows].c_str(), stringR, VSTGUI::kCenterText, true);
      }
      else
      {
         dc->drawString(fgText.c_str(), stringR, VSTGUI::kCenterText, true);
      }
   }
   break;
   case None:
   {
      // Obviously do nothing
   }
   break;
   };

}

VSTGUI::CMouseEventResult CGlyphSwitch::onMouseDown (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons)
{
   setMousedRowAndCol(where);
   mouseState |= DrawState::Press;
   invalid();
   return VSTGUI::kMouseEventHandled;
}

VSTGUI::CMouseEventResult CGlyphSwitch::onMouseUp (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons)
{
   setMousedRowAndCol(where);
   mouseState &= ~DrawState::Press;
   if( isMulti() )
      setMultiValue( mouseRow + mouseCol * rows );
   else
      setValue( value > 0.5 ? 0 : 1 );
   
   if( listener )
      listener->valueChanged(this);
   
   invalid();
   return VSTGUI::kMouseEventHandled;
}

VSTGUI::CMouseEventResult CGlyphSwitch::onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons)
{
   setMousedRowAndCol(where);
   mouseState |= DrawState::Hover;
   invalid();
   return VSTGUI::kMouseEventHandled;
}

VSTGUI::CMouseEventResult CGlyphSwitch::onMouseExited (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons)
{
   mouseState &= ~DrawState::Hover;
   invalid();
   return VSTGUI::kMouseEventHandled;
}

VSTGUI::CMouseEventResult CGlyphSwitch::onMouseMoved(VSTGUI::CPoint &where, const VSTGUI::CButtonState &buttons)
{
   setMousedRowAndCol(where);
   return VSTGUI::kMouseEventHandled;
}

void CGlyphSwitch::setMousedRowAndCol(VSTGUI::CPoint &where) {
   if( isMulti() )
   {
      auto size = getViewSize();
      float sx0 = size.left;
      float sy0 = size.top;
      float dsx = size.getWidth() * 1.0 / cols;
      float dsy = size.getHeight() * 1.0 / rows;
      
      for( int r=0; r<rows; ++r )
         for( int c=0; c<cols; ++c )
         {
            auto x0 = sx0 + dsx * c;
            auto y0 = sy0 + dsy * r;
            VSTGUI::CRect subS(x0,y0,x0+dsx,y0+dsy);
            if( subS.pointInside(where) )
            {
               if( r != mouseRow || c != mouseCol )
                  invalid();
               
               mouseRow = r;
               mouseCol = c;
            }
         }

   }
}
