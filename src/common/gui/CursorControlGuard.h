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

#pragma once
#include "vstgui/vstgui.h"

namespace Surge
{
namespace UI
{

/*
 * CursorControlGuard is the primary guard class we use to hide, show,
 * and move a cursor. It uses the RAII style pattern in a way, in that
 * constructing it hides the cursor and destroying it shows the cursor.
 *
 * A common usage would be to have a std::shared_ptr<CursorControlGuard> member
 * of your class and onMouseDown initialize it with a std::make_shared of this
 * and onMouseUp reset it.
 *
 * The guard will also allow you to change the coordinates at which the mouse
 * re-appears usign the various APIs in the class.
 */
struct CursorControlGuard
{
   enum MotionMode {
      SHOW_AT_LOCATION, // Use the where from construction or setShowLocation when you show
      SHOW_AT_MOUSE_MOTION_POINT // Use the point of construction when you show
   } motionMode = SHOW_AT_LOCATION;
   VSTGUI::CPoint showLocation; // In physical screen cordinates

   CursorControlGuard();
   CursorControlGuard(VSTGUI::CFrame *, const VSTGUI::CPoint &where);
   ~CursorControlGuard();

   void setShowLocationFromFrameLocation( VSTGUI::CFrame *, const VSTGUI::CPoint &where );

private:
   void doHide();

};

}
}