
#include "cocoa_minimal_main.h"
#include <Cocoa/Cocoa.h>
#include "vstgui/lib/platform/mac/macglobals.h"
#include "vstgui/lib/platform/mac/cocoa/nsviewframe.h"
#include "vstgui/lib/cframe.h"

namespace VSTGUI
{
    void* gBundleRef = NULL; // FIXME - set this to the bundle!
}

void cocoa_minimal_main(int w, int h, std::function<void(VSTGUI::CFrame *f)> frameCb)
{
    // Thanks http://www.cocoawithlove.com/2010/09/minimalist-cocoa-programming.html
    [NSAutoreleasePool new];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    
    id menubar = [[NSMenu new] autorelease];
    id appMenuItem = [[NSMenuItem new] autorelease];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];
    id appMenu = [[NSMenu new] autorelease];
    id appName = [[NSProcessInfo processInfo] processName];
    
    id quitTitle = [@"Quit " stringByAppendingString:appName];
    id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
        action:@selector(terminate:) keyEquivalent:@"q"] autorelease];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];

      

    id window = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, h)
                                             styleMask:NSTitledWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:NO]
            autorelease];
    [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
    [window setTitle:appName];
    [window makeKeyAndOrderFront:nil];


    VSTGUI::CFrame *f = new VSTGUI::CFrame(VSTGUI::CRect(VSTGUI::CPoint(0,0),VSTGUI::CPoint(w, h)), NULL);
    f->open( [window contentView] );
    frameCb( f );
    
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}
