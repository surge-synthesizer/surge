#include "DisplayInfo.h"
#include "UserInteractions.h"

#include <X11/Xlib.h>
#include <iostream>
#include <iomanip>

namespace Surge
{
namespace GUI
{

using namespace VSTGUI;

float getDisplayBackingScaleFactor(CFrame *) { return 2.0; }

/*
** We make a static single screen assumption for now so cache the screen size and only
** query Xlib for it once.
*/
int dispinfoScreenW = -1;
int dispinfoScreenH = -1;

CRect getScreenDimensions(CFrame *)
{
    if (dispinfoScreenW < 0 || dispinfoScreenH < 0)
    {
        /*
        ** For now make the assumption of single screen to at least avoid
        ** clip and bound errors like shown in #594
        **
        ** FIXME: Deal with multi-display linux
        */
        Display *d = XOpenDisplay(nullptr);
        if (d)
        {
            Screen *s = DefaultScreenOfDisplay(d);
            dispinfoScreenW = s->width;
            dispinfoScreenH = s->height;
            XCloseDisplay(d);
        }
        else
        {
            dispinfoScreenW = 0;
            dispinfoScreenH = 0;
        }
    }

    return CRect(CPoint(0, 0), CPoint(dispinfoScreenW, dispinfoScreenH));
}

} // namespace GUI
} // namespace Surge
