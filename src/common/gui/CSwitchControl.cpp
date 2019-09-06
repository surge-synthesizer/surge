#include "CSwitchControl.h"
#include "CScalableBitmap.h"
#include "vstgui/lib/cbitmap.h"
#include <iostream>

using namespace VSTGUI;

CSwitchControl::CSwitchControl(const CRect& size,
                               IControlListener* listener,
                               long tag,
                               CBitmap* background)
    : CControl(size, listener, tag, background)
{
   down = false;
   isMultiValued = false;
   heightOfSingleImage = size.getHeight();
   for( int i=0; i<max_states; ++i )
      bitmaps[i] = nullptr;
}

CSwitchControl::~CSwitchControl()
{
   for( int i=0; i<max_states; ++i )
      if( bitmaps[i] )
      {
         bitmaps[i]->forget();
         bitmaps[i] = nullptr;
      }
}

void CSwitchControl::setBitmapForState(int state, VSTGUI::CBitmap *bitmap)
{
   if( state >= max_states || state < 0 )
   {
      // FIXME - error message of some form
      return;
   }

   if( bitmaps[state] != nullptr )
   {
      bitmaps[state]->forget();
   }
   useStateBitmaps = true;
   bitmap->remember();
   bitmaps[state] = bitmap;
   invalid();
}

void CSwitchControl::draw(CDrawContext* dc)
{
   CRect size = getViewSize();
   if( useStateBitmaps )
   {
      int v = isMultiValued ? ivalue : value;
      auto b = bitmaps[v];
      if( b )
      {
         CPoint where(0,0);
         b->draw(dc, size, where, 0xff );
      }
   }
   else
   {
      auto pBackground = getBackground();
      if (pBackground)
      {
         // source position in bitmap
         if (isMultiValued)
         {
            CPoint where(0, size.getHeight() * ivalue);
            pBackground->draw(dc, size, where, 0xff);
         }
         else
         {
            CPoint where(0, (down ? heightOfSingleImage : 0));
            pBackground->draw(dc, size, where, 0xff);
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

   if (isMultiValued)
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
