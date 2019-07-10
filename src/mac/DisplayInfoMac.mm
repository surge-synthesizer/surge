#include "DisplayInfo.h"
#include "UserInteractions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Cocoa/Cocoa.h>
#include "vstgui/vstgui.h"
#include "vstgui/lib/platform/mac/cocoa/nsviewframe.h"

namespace Surge
{
namespace GUI
{

using namespace VSTGUI;
    
NSView *getViewFromFrame(CFrame *f)
{
    IPlatformFrame *pf = f->getPlatformFrame();
    NSViewFrame *nf = dynamic_cast<NSViewFrame*>(pf);
    if (!nf)
    {
        Surge::UserInteractions::promptError("Platform frame is not an NSViewFrame. Please report this issue to developers.",
                                             "Software Error");
        return NULL;
    }
    return nf->getNSView();
}


float getDisplayBackingScaleFactor(CFrame *f)
{
    // If I don't know my resolution, guess retina.
    float defaultBackingScale = 2.0; 
    if (!f)
    {
        return defaultBackingScale;
    }
    
    NSView *v = getViewFromFrame(f);
    
    // So objective c would run these but lets be explicit
    if (!v || ![v window] || ![[v window] screen])
    {
        return defaultBackingScale;
    }
    
    float res = [[[v window] screen] backingScaleFactor];
    return res;
}

CRect getScreenDimensions(CFrame *f)
{
    CRect defaultDimension(CPoint(0,0),CPoint(0,0)); 
    
    if (!f)
    {
        return defaultDimension;
    }
    
    NSView *v = getViewFromFrame(f);
    if (!v || ![v window] || ![[v window] screen])
    {
        return defaultDimension;
    }
    
    NSRect screenRect = [[[v window] screen] visibleFrame];
    
    
    return CRect(CPoint(0,0), CPoint(screenRect.size.width,screenRect.size.height));;
}


} // end NS GUI
} // end NS Surge
