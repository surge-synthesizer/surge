#include "CSwitchControl.h"

using namespace VSTGUI;

CSwitchControl::CSwitchControl(const CRect& size,
                               IControlListener* listener,
                               long tag,
                               CBitmap* background)
    : CControl(size, listener, tag, background)
{
   down = false;
   is_itype = false;
   heightOfSingleImage = size.getHeight(); 
 }

void CSwitchControl::draw(CDrawContext* dc)
{
   CRect size = getViewSize();
   auto pBackground = getBackground();
   if (pBackground)
   {
      // source position in bitmap
      if (is_itype)
      {
         CPoint where(0, size.getHeight() * ivalue);
         pBackground->draw(dc, size, where, 0xff);
      }
      else
      {
         CPoint where(0, (down ? heightOfSingleImage : 0));
         pBackground->draw(dc, size, where, 0xff);
      }

      if( ! lookedForHover && skin.get() )
      {
         lookedForHover = true;
         hoverBmp = skin->hoverBitmapOverlayForBackgroundBitmap( skinControl, dynamic_cast<CScalableBitmap*>( getBackground() ), associatedBitmapStore, Surge::UI::Skin::HoverType::HOVER );
      }

      if( hoverBmp && doingHover )
      {
         if (is_itype)
         {
            CPoint where(0, size.getHeight() * ivalue);
            hoverBmp->draw(dc, size, where, 0xff);
         }
         else
         {
            CPoint where(0, (down ? heightOfSingleImage : 0));
            hoverBmp->draw(dc, size, where, 0xff);
         }

      }
   }
   setDirty(false);
}

void CSwitchControl::setValue(float f)
{
   down = f > 0.5;
   value = down ? 1 : 0;
}

CMouseEventResult CSwitchControl::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (listener && buttons & (kAlt | kShift | kControl | kApple))
   {
      if (listener->controlModifierClicked(this, buttons) != 0)
         return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   if (!(buttons & kLButton))
      return kMouseEventNotHandled;

   beginEdit();

   if (is_itype)
   {
      // ivalue = (ivalue & imax) + 1;
   }
   else
   {
      down = !down;
      value = down ? 1 : 0;
   }

   if (listener)
      listener->valueChanged(this);

   endEdit();

   invalid();

   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
CMouseEventResult CSwitchControl::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   return kMouseEventHandled;
}
CMouseEventResult CSwitchControl::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   return kMouseEventHandled;
}
/*
void gui_switch::mouse (CPoint &where, long button)
{
        if (!bMouseEnabled)
                return;

        if (listener && button & (kAlt | kShift | kControl | kApple))
        {
                if (listener->controlModifierClicked (this, button) != 0)
                        return;
        }

        if (!(button & kLButton))
                return;

        // check if default value wanted
        if (checkDefaultValue (button))
                return;
        

        if(is_itype)
        {
                //ivalue = (ivalue & imax) + 1;
        }
        else
        {
                down = !down;
                value = down?1:0;
        }

        if (listener)
                listener->valueChanged (this);
}*/
