#include <Cocoa/Cocoa.h>
#include "cocoa_utils.h"
#include <iostream>
#include <iomanip>

#include <CoreText/CoreText.h>
#include "vstgui/lib/platform/mac/macglobals.h"
#include "UserInteractions.h"

double CocoaUtils::getDoubleClickInterval()
{
   return [NSEvent doubleClickInterval];
}

void CocoaUtils::miniedit_text_impl( char *c, int maxchars )
{
    @autoreleasepool
    {
        // Thanks Internet! https://stackoverflow.com/questions/28362472/is-there-a-simple-input-box-in-cocoa
        NSAlert *msg = [[NSAlert alloc] init];
        [msg addButtonWithTitle:@"OK"];
        [msg addButtonWithTitle:@"Cancel"];
        
        [msg setMessageText:@"Please provide the value"];
        
        NSTextField *txt = [[NSTextField alloc] init];
        [txt setFrame: NSMakeRect(0, 0, 200, 24 )];
        [txt setStringValue: [NSString stringWithCString:c encoding:NSASCIIStringEncoding] ];
        
        [msg setAccessoryView:txt];
        
        auto response = [msg runModal];
        
        if (response == NSAlertFirstButtonReturn) {
            strncpy( c, [[txt stringValue] UTF8String], maxchars );
            c[maxchars - 1] = 0;
        }
    }
}

void CocoaUtils::registerBundleFonts()
{
    /*
    ** MacFonts use the CoreText registry so we can just find them in the bundle
    ** and register them using the CoreText Font Manager API
    */
    CFURLRef url = nullptr;
    url = CFBundleCopyResourceURL(VSTGUI::getBundleRef(), CFSTR("Lato-Regular"), CFSTR("ttf"), CFSTR("fonts"));
    if (url)
    {
        CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, NULL );
        CFRelease(url);
    }
    else
    {
        Surge::UserInteractions::promptError("Cannot load lato font from bundle", "Software Error");
    }
}


