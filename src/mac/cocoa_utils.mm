#include <Cocoa/Cocoa.h>
#include "cocoa_utils.h"

double CocoaUtils::getDoubleClickInterval()
{
   return [NSEvent doubleClickInterval];
}

void CocoaUtils::miniedit_text_impl( char *c, int maxchars, const char *prompt, const char *title )
{
    @autoreleasepool
    {
        // Thanks Internet! https://stackoverflow.com/questions/28362472/is-there-a-simple-input-box-in-cocoa
        NSAlert *msg = [[NSAlert alloc] init];
        [msg addButtonWithTitle:@"OK"];
        [msg addButtonWithTitle:@"Cancel"];

        [msg setInformativeText:[NSString stringWithUTF8String:prompt]];
        [msg setMessageText:[NSString stringWithUTF8String:title]];

        
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
