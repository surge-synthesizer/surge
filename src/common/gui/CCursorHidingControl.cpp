#include "CCursorHidingControl.h"

#if WINDOWS
#include <Windows.h>
#endif

using namespace VSTGUI;

CCursorHidingControl::CCursorHidingControl(const CRect& size,
                                           IControlListener* listener,
                                           int32_t tag,
                                           CBitmap* pBackground)
    : CControl(size, listener, tag, pBackground)
{}

CCursorHidingControl::~CCursorHidingControl()
{
   attachCursor();
   /*
   // restore the cursor if dragging
   ShowCursor(true);
#if MAC
   CGAssociateMouseAndMouseCursorPosition(true);
#endif*/
}

CMouseEventResult CCursorHidingControl::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   _lastPos = where;
   _sumDX = 0;
   _sumDY = 0;
   return kMouseEventHandled;
}

CMouseEventResult CCursorHidingControl::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   return kMouseEventHandled;
}

CMouseEventResult CCursorHidingControl::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   double dx = where.x - _lastPos.x;
   double dy = where.y - _lastPos.y;
   _lastPos = where;

   if (( scaleAnyway ||  _isDetached ) && !buttons.isTouch())
   {
      double scaling = getMouseDeltaScaling(where, buttons);

      dx *= scaling;
      dy *= scaling;

      _sumDX += dx;
      _sumDY += dy;
   }

   onMouseMoveDelta(where, buttons, dx, dy);

#if WINDOWS
   if (_isDetached && !buttons.isTouch())
   {
       double ddx = where.x - _detachPos.x;
       double ddy = where.y - _detachPos.y;
       double d = sqrt(ddx * ddx + ddy * ddy);
       if (d > 10 && SetCursorPos(_hideX, _hideY))
       {
          _lastPos = _detachPos;
       }
   }
#endif

   return kMouseEventHandled;
}

double CCursorHidingControl::getMouseDeltaScaling(CPoint& where, const CButtonState& buttons)
{
   return 1.0;
}

void CCursorHidingControl::detachCursor(CPoint& where)
{
   if (!_isDetached && hideCursor)
   {
      doDetach(where);
   }
   if( ! hideCursor )
   {
      scaleAnyway = true;
   }
}

void CCursorHidingControl::attachCursor()
{
   if (_isDetached && hideCursor)
   {
      doAttach();
   }
   if( ! hideCursor )
   {
      scaleAnyway = false;
   }
}

void CCursorHidingControl::doDetach(CPoint& where)
{
   _isDetached = true;
   _detachPos = where;

#if WINDOWS
   if (hideCursor)
   {
      ShowCursor(false);

      POINT p;
      if (GetCursorPos(&p))
      {
         _hideX = p.x;
         _hideY = p.y;
      }
   }
#endif
}

void CCursorHidingControl::doAttach()
{
   _isDetached = false;

#if WINDOWS
   double x = _hideX + _sumDX;
   double y = _hideY + _sumDY;

   // get frame offset from top left of the screen space
   auto f = getFrame();
   CCoord fx, fy;
   f->getPosition(fx, fy);

      // rectangle of the control we were operating
   CRect widgetRect = getViewSize();

   // transform it to screen coordinates (apply zoom level etc.)
   f->getTransform().transform(widgetRect);

   // apply frame offset
   widgetRect.left += fx;
   widgetRect.right += fx;
   widgetRect.top += fy;
   widgetRect.bottom += fy;
      
   // restrict the cursor to the widget's rectangle
   if (x > widgetRect.right)
      x = widgetRect.right;
   if (x < widgetRect.left)
      x = widgetRect.left;
   if (y > widgetRect.bottom)
      y = widgetRect.bottom;
   if (y < widgetRect.top)
      y = widgetRect.top;

   SetCursorPos((int)x, (int)y);

   ShowCursor(true);
#endif
}
