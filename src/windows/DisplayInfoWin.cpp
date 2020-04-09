#include "DisplayInfo.h"
#include "UserInteractions.h"

#include "wtypes.h"
#include "vstgui/lib/platform/win32/win32frame.h" // actually 64 bit too!

namespace Surge
{
namespace GUI
{

using namespace VSTGUI;
    
float getDisplayBackingScaleFactor(CFrame *f)
{
    /*
    ** This API isn't needed until windows supports the scalable bitmaps
    ** so rough it out but don't include it yet
    */
#if 0
    VSTGUI::Win32Frame *wf = dynamic_cast<VSTGUI::Win32Frame *>(f);

    if (wf && wf->getHWND() )
    {
        HMONITOR hMon = MonitorFromWindow(wf->getHWND(), MONITOR_DEFAULTTONEAREST);
        DEVICE_SCALE_FACTOR pScale; // https://docs.microsoft.com/en-us/windows/desktop/api/shtypes/ne-shtypes-device_scale_factor
        GetScaleFactorForMonitor(hMon,&pScale);
        // At this point we would convert this painfully to a float with a big switch. But until
        // we implement scalable bitmaps it doesn't matter, so:
        return 2.0;
    }
#endif

    return 2.0;
}
    
CRect getScreenDimensions(CFrame *)
{
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    return CRect(CPoint(0,0), CPoint(desktop.right,desktop.bottom));
}

}
}
