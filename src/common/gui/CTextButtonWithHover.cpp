//
// Created by Paul Walker on 12/3/20.
//

#include "CTextButtonWithHover.h"
#include <iostream>

template <typename T>
struct HoverGuard {
   HoverGuard() {}

   void activate( std::function<T()> getf,
              std::function<void(T &)> isetf,
              T &overr
              )
   {
      active = true;
      setf = isetf;
      orig = getf();
      setf(overr);
   }

   ~HoverGuard()
   {
      if( active )
         setf( orig );
   }

   bool active = false;
   std::function<void(T & )> setf;
   T orig;
};

void CTextButtonWithHover::draw(VSTGUI::CDrawContext* context)
{

#define DO_HOVER(x, T) \
   HoverGuard<T> hg_ ## x;                    \
   if( hc_ ## x .isSet) {                \
      hg_ ## x .activate( [this](){ return get ## x (); }, \
                    [this](T &c ) { set ## x (c); }, \
                    hc_ ## x .item \
                   );   \
   }

   if( isHovered )
   {
      DO_HOVER( Gradient, VSTGUI::CGradient * );
      DO_HOVER( FrameColor, VSTGUI::CColor );
      DO_HOVER( TextColor, VSTGUI::CColor );
      DO_HOVER( GradientHighlighted, VSTGUI::CGradient * );
      DO_HOVER( FrameColorHighlighted, VSTGUI::CColor );
      DO_HOVER( TextColorHighlighted, VSTGUI::CColor );
      CTextButton::draw(context);
   }
   else
   {
      CTextButton::draw(context);
   }
}
VSTGUI::CMouseEventResult CTextButtonWithHover::onMouseEntered(VSTGUI::CPoint& where,
                                                               const VSTGUI::CButtonState& buttons)
{
   isHovered = true;
   invalid();
   return CView::onMouseEntered(where, buttons);
}

VSTGUI::CMouseEventResult CTextButtonWithHover::onMouseExited(VSTGUI::CPoint& where,
                                                              const VSTGUI::CButtonState& buttons)
{
   isHovered = false;
   invalid();
   return CView::onMouseExited(where, buttons);
}
