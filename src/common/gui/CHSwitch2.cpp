//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "CHSwitch2.h"
#include <vt_dsp/basic_dsp.h>
#include "CScalableBitmap.h"

using namespace VSTGUI;

void CHSwitch2::draw(CDrawContext* dc)
{
   if (getBackground())
   {
      // source position in bitmap
      CPoint where(0, heightOfOneImage *
                          (long)(imgoffset + ((value * (float)(rows * columns - 1) + 0.5f))));
      getBackground()->draw(dc, getViewSize(), where, 0xff);

      if( ! lookedForHover && skin.get() )
      {
         lookedForHover = true;
         hoverBmp = skin->hoverBitmapOverlayForBackgroundBitmap( skinControl, dynamic_cast<CScalableBitmap*>( getBackground() ), associatedBitmapStore, Surge::UI::Skin::HoverType::HOVER );
         hoverOnBmp = skin->hoverBitmapOverlayForBackgroundBitmap( skinControl, dynamic_cast<CScalableBitmap*>( getBackground() ), associatedBitmapStore, Surge::UI::Skin::HoverType::HOVER_OVER_ON );
      }

      long vv = (long)(imgoffset + ((value * (float)(rows * columns - 1) + 0.5f)));
      long hv = (long)(imgoffset + ((hoverValue * (float)(rows * columns - 1) + 0.5f)));
      if( doingHover && hoverOnBmp && vv == hv )
      {
         CPoint hwhere(0, heightOfOneImage *
                       (long)(imgoffset + ((hoverValue * (float)(rows * columns - 1) + 0.5f))));
         
         hoverOnBmp->draw(dc, getViewSize(), hwhere, 0xff);
      }
      else if( hoverBmp && doingHover )
      {
         CPoint hwhere(0, heightOfOneImage *
                      (long)(imgoffset + ((hoverValue * (float)(rows * columns - 1) + 0.5f))));
         
         hoverBmp->draw(dc, getViewSize(), hwhere, 0xff);
      }
   }
   setDirty(false);
}

CMouseEventResult CHSwitch2::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (listener && buttons & (kAlt | kShift | kRButton | kControl | kApple))
   {
      if (listener->controlModifierClicked(this, buttons) != 0)
         return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   if (!(buttons & kLButton))
      return kMouseEventNotHandled;

   if (!dragable)
   {
      auto mouseableArea = getMouseableArea();
      beginEdit();
      double coefX, coefY;
      coefX = (double)mouseableArea.getWidth() / (double)columns;
      coefY = (double)mouseableArea.getHeight() / (double)rows;

      int y = (int)((where.y - mouseableArea.top) / coefY);
      int x = (int)((where.x - mouseableArea.left) / coefX);

      if (columns * rows > 1)
      {
         value = (float)(x + y * columns) / (float)(columns * rows - 1);
         
         if (value > 1.f)
            value = 1.f;
         else if (value < 0.f)
            value = 0.f;
      }

      if (listener)
         listener->valueChanged(this);

      endEdit();

      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }
   else
   {
      beginEdit();
      return onMouseMoved(where, buttons);
   }

   return kMouseEventNotHandled;
}
CMouseEventResult CHSwitch2::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   if (dragable)
   {
      endEdit();
      return kMouseEventHandled;
   }
   return kMouseEventNotHandled;
}
CMouseEventResult CHSwitch2::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   if (dragable && ( buttons.getButtonState() ))
   {
      auto mouseableArea = getMouseableArea();
      double coefX, coefY;
      coefX = (double)mouseableArea.getWidth() / (double)columns;
      coefY = (double)mouseableArea.getHeight() / (double)rows;

      int y = (int)((where.y - mouseableArea.top) / coefY);
      int x = (int)((where.x - mouseableArea.left) / coefX);

      x = limit_range(x, 0, columns - 1);
      y = limit_range(y, 0, rows - 1);

      if (columns * rows > 1)
      {
         value = (float)(x + y * columns) / (float)(columns * rows - 1);
         
         if (value > 1.f)
            value = 1.f;
         else if (value < 0.f)
            value = 0.f;
      }

      invalid();
      if (listener)
         listener->valueChanged(this);

      return kMouseEventHandled;
   }

   if( doingHover )
   {
      auto mouseableArea = getMouseableArea();
      double coefX, coefY;
      coefX = (double)mouseableArea.getWidth() / (double)columns;
      coefY = (double)mouseableArea.getHeight() / (double)rows;

      int y = (int)((where.y - mouseableArea.top) / coefY);
      int x = (int)((where.x - mouseableArea.left) / coefX);

      x = limit_range(x, 0, columns - 1);
      y = limit_range(y, 0, rows - 1);

      if (columns * rows > 1)
      {
         float nhoverValue = (float)(x + y * columns) / (float)(columns * rows - 1);

         nhoverValue = limit_range( nhoverValue, 0.f, 1.f );

         if( nhoverValue != hoverValue )
         {
            hoverValue = nhoverValue;
            invalid();
         }
      }
   }
   
   return kMouseEventNotHandled;
}
bool CHSwitch2::onWheel(const CPoint& where, const float& distance, const CButtonState& buttons)
{
   if (usesMouseWheel)
   {
      float newVal=value;
      float rate = 1.0f;
      float range = getRange();
      if (columns >1)
      {
         rate = range / (float)(columns-1); // Colums-1 = number of scroll steps
         newVal += rate * distance;
      }
      else
      {
         rate = range / (float)(rows-1); // Rows-1 = Number of scroll steps
         newVal += rate * -distance; // flip distance (==direction) because it makes more sense when wheeling
      }
      beginEdit();
      value = newVal;

      bounceValue();
      if (listener)
         listener->valueChanged(this);
      setValue(value);
      endEdit();
      return true;
   }
   return false;
}
void CHSwitch2::setUsesMouseWheel(bool wheel)
{
   usesMouseWheel = wheel;
}
