//
//  aulayer_cocoaui.mm
//  surge-au
//
//  Created by Paul Walker on 12/10/18.
//

#include <objc/runtime.h>
#import <CoreFoundation/CoreFoundation.h>
#import <AudioUnit/AUCocoaUIView.h>
#include "aulayer.h"
#include "aulayer_cocoaui.h"

@interface SurgeNSView : NSView
{

}

- (id) initWithSurge: (id) cont preferredSize: (NSSize) size;
@end

@interface SurgeCocoaUI : NSObject<AUCocoaUIBase>
{
}

- (NSString *) description;
@end

@implementation SurgeCocoaUI
- (NSView *) uiViewForAudioUnit: (AudioUnit) inAudioUnit
                       withSize: (NSSize) inPreferredSize
{
    // Remember we end up being called here because that's what AUCocoaUIView does in the initiation collaboration with hosts
    AULOG::log( "uiViewForAudioUnit %s on %s\n", __TIME__, __DATE__ );
#if 0
    // OK so here's what happens in VST land. Basically we get the controller pointer from the AU class and then wrap it in this thingy. Lots to unpick still.
    Vst::IEditController* editController = 0;
    UInt32 size = sizeof (Vst::IEditController*);
    if (AudioUnitGetProperty (inAU, 64000, kAudioUnitScope_Global, 0, &editController, &size) != noErr)
        return nil;
    return [[[SMTGCocoa_NSViewWrapperForAU alloc] initWithEditController:editController audioUnit:inAU preferredSize:inPreferredSize] autorelease];
#endif
    return [[[SurgeNSView alloc] initWithSurge:nil preferredSize:inPreferredSize] autorelease];
    // return nil;
}

- (unsigned int)interfaceVersion {
    AULOG::log( "interfaceVersion\n" );
    return 0;
}

- (NSString *) description {
    return [NSString stringWithUTF8String: "Surge View"];
}

@end

@implementation SurgeNSView
- (id) initWithSurge: (id) cont preferredSize: (NSSize) size
{
    self = [super initWithFrame: NSMakeRect (0, 0, size.width, size.height)];
    return self;
}

// Just for now
- (void) drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    // This next line sets the the current fill color parameter of the Graphics Context
    [[NSColor redColor] setFill];
    // This next function fills a rect the same as dirtyRect with the current fill color of the Graphics Context.
    NSRectFill(dirtyRect);
    // You might want to use _bounds or self.bounds if you want to be sure to fill the entire bounds rect of the view.
}
@end


static CFBundleRef GetBundleFromExecutable (const char* filepath)
{
    @autoreleasepool {
        NSString* execStr = [NSString stringWithCString:filepath encoding:NSUTF8StringEncoding];
        NSString* macOSStr = [execStr stringByDeletingLastPathComponent];
        NSString* contentsStr = [macOSStr stringByDeletingLastPathComponent];
        NSString* bundleStr = [contentsStr stringByDeletingLastPathComponent];
        return CFBundleCreate (0, (CFURLRef)[NSURL fileURLWithPath:bundleStr isDirectory:YES]);
    }
    
}

ComponentResult aulayer::GetProperty(AudioUnitPropertyID iID, AudioUnitScope iScope, AudioUnitElement iElem, void* outData)
{
    if( iScope == kAudioUnitScope_Global )
    {
        switch( iID )
        {
            case kAudioUnitProperty_CocoaUI:
                // FIXME - this autorelease over-releases so leak a little (we are infrequently called)
                // but definitely come back and fix this!
                //@autoreleasepool
                {
                    AULOG::log( "Asking for kAudioUnitProperty_CocoaUI\n" );
                    auto surgeclass = objc_getClass( "SurgeCocoaUI" );
                    const char* image = class_getImageName ( surgeclass );
                    CFBundleRef bundle = GetBundleFromExecutable (image);
                    CFURLRef url = CFBundleCopyBundleURL (bundle);
                    CFRelease (bundle);

                    
                    AudioUnitCocoaViewInfo* info = static_cast<AudioUnitCocoaViewInfo*> (outData);
                    info->mCocoaAUViewClass[0] = (__bridge CFStringRef)NSStringFromClass( surgeclass );
                    info->mCocoaAUViewBundleLocation = url;

                    return noErr;
                }
        }
    }

    return AUInstrumentBase::GetProperty(iID, iScope, iElem, outData);
}

