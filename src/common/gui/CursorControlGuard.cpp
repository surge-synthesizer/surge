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

#include "CursorControlGuard.h"
#include <atomic>
#include "vstgui/lib/cframe.h"

#if MAC
#include <CoreGraphics/CoreGraphics.h>
#elif WINDOWS
#include <windows.h>
#elif LINUX
#endif


namespace Surge
{
namespace UI {
/*
 * I want to count shows and hides. macOS does this internally inside the API
 * but Windows doesn't.
 */
static std::atomic<int> hideCount(0);

CursorControlGuard::CursorControlGuard()
{
   motionMode = SHOW_AT_MOUSE_MOTION_POINT;
   if( hideCount == 0 )
   {
      doHide();
   }
   hideCount++;
}

CursorControlGuard::CursorControlGuard(VSTGUI::CFrame *f, const VSTGUI::CPoint& where)
{
   setShowLocationFromFrameLocation(f, where);

   motionMode = SHOW_AT_LOCATION;
   if( hideCount == 0 )
   {
      doHide();
   }
   hideCount++;
}
CursorControlGuard::~CursorControlGuard()
{
   hideCount--;
   if( hideCount == 0 )
   {
      resetToShowLocation();
#if MAC
      CGDisplayShowCursor(kCGDirectMainDisplay);
#elif WINDOWS
      ShowCursor( true );
#elif LINUX
#endif
   }
}

bool CursorControlGuard::resetToShowLocation()
{
#if MAC
   if( motionMode == SHOW_AT_LOCATION )
   {
      CGAssociateMouseAndMouseCursorPosition(false);
      CGWarpMouseCursorPosition(CGPointMake(showLocation.x, showLocation.y));
      CGAssociateMouseAndMouseCursorPosition(true);
      return true;
   }

#elif WINDOWS
   if( motionMode == SHOW_AT_LOCATION )
   {
      SetCursorPos( showLocation.x, showLocation.y );
     return true;
   }
#elif LINUX
#endif
   return false;
}

void CursorControlGuard::doHide()
{
#if MAC
   CGDisplayHideCursor(kCGDirectMainDisplay);
#elif WINDOWS
   ShowCursor( false );
#elif LINUX
#endif

}

void CursorControlGuard::setShowLocationFromFrameLocation(VSTGUI::CFrame* f, const VSTGUI::CPoint& where)
{
   showLocation = where;
   VSTGUI::CCoord px, py; f->getPosition( px, py );

   showLocation = f->getTransform().transform(showLocation);
   showLocation = showLocation.offset(px, py);
   motionMode = SHOW_AT_LOCATION;
}

void CursorControlGuard::setShowLocationFromViewLocation(VSTGUI::CView *v, const VSTGUI::CPoint& where)
{
   auto r = where;
   r = v->localToFrame(r);
   setShowLocationFromFrameLocation(v->getFrame(), r );
}

}
}