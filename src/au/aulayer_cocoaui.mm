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
#include <gui/SurgeGUIEditor.h>

@interface SurgeNSView : NSView
{
    SurgeGUIEditor *editController;
}

- (id) initWithSurge: (SurgeGUIEditor *) cont preferredSize: (NSSize) size;
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
    return [[[SMTGCocoa_NSViewWrapperForAU alloc] initWithEditController:editController audioUnit:inAU preferredSize:inPreferredSize] autorelease];
#endif
    
    SurgeGUIEditor* editController = 0;
    UInt32 size = sizeof (SurgeGUIEditor *);
    if (AudioUnitGetProperty (inAudioUnit, kVmbAAudioUnitProperty_GetEditPointer, kAudioUnitScope_Global, 0, &editController, &size) != noErr)
        return nil;
    
    return [[[SurgeNSView alloc] initWithSurge:editController preferredSize:inPreferredSize] autorelease];
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
- (id) initWithSurge: (SurgeGUIEditor *) cont preferredSize: (NSSize) size
{
    self = [super initWithFrame: NSMakeRect (0, 0, size.width / 2, size.height / 2)];
    
    if (self)
    {
        AULOG::log( "About to poen with %d\n", cont );
        cont->open( self );
/*
        editController = cont;
        editController->addRef ();
        audioUnit = au;
        // WHAT WE ARE GETTING is a plugView not an editController
        plugView = editController->createView (Vst::ViewType::kEditor);
        if (!plugView || plugView->isPlatformTypeSupported (kPlatformTypeNSView) != kResultTrue)
        {
            [self dealloc];
            return nil;
        }
     
        plugFrame = NEW AUPlugFrame (self);
        plugView->setFrame (plugFrame);
        
        if (plugView->attached (self, kPlatformTypeNSView) != kResultTrue)
        {
            [self dealloc];
            return nil;
        }
        ViewRect vr;
        if (plugView->getSize (&vr) == kResultTrue)
        {
            NSRect newSize = NSMakeRect (0, 0, vr.right - vr.left, vr.bottom - vr.top);
            [self setFrame:newSize];
        }
        
        isAttached = YES;
        UInt32 size = sizeof (FObject*);
        if (AudioUnitGetProperty (audioUnit, 64001, kAudioUnitScope_Global, 0, &dynlib, &size) == noErr)
            dynlib->addRef ();
*/
    }
    
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

//----------------------------------------------------------------------------------------------------

const char* getclamptxt(int id)
{
    switch(id)
    {
        case 1: return "Macro Parameters";
        case 2: return "Global / FX";
        case 3: return "Scene A Common";
        case 4: return "Scene A Osc";
        case 5: return "Scene A Osc Mixer";
        case 6: return "Scene A Filters";
        case 7: return "Scene A Envelopes";
        case 8: return "Scene A LFOs";
        case 9: return "Scene B Common";
        case 10: return "Scene B Osc";
        case 11: return "Scene B Osc Mixer";
        case 12: return "Scene B Filters";
        case 13: return "Scene B Envelopes";
        case 14: return "Scene B LFOs";
    }
    return "";
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
            case kVmbAAudioUnitProperty_GetEditPointer:
            {
                AULOG::log( "Asking for the edit pointer\n" );
                if( editor_instance == NULL )
                {
                    editor_instance = new SurgeGUIEditor( this, plugin_instance );
                }
                void** pThis = (void**)(outData);
                *pThis = (void*)editor_instance;

                return noErr;
            }
            case kAudioUnitProperty_ParameterValueName:
            {
                if(!IsInitialized()) return kAudioUnitErr_Uninitialized;
                AudioUnitParameterValueName *aup = (AudioUnitParameterValueName*)outData;
                char tmptxt[64];
                float f;
                if(aup->inValue) f = *(aup->inValue);
                else f = plugin_instance->getParameter01(plugin_instance->remapExternalApiToInternalId(aup->inParamID));
                plugin_instance->getParameterDisplay(plugin_instance->remapExternalApiToInternalId(aup->inParamID),tmptxt,f);
                aup->outName = CFStringCreateWithCString(NULL,tmptxt,kCFStringEncodingUTF8);
                return noErr;
            }
            case kAudioUnitProperty_ParameterClumpName:
            {
                AudioUnitParameterNameInfo *aupn = (AudioUnitParameterNameInfo*)outData;
                aupn->outName = CFStringCreateWithCString(NULL,getclamptxt(aupn->inID),kCFStringEncodingUTF8);
                return noErr;
            }
            case kVmbAAudioUnitProperty_GetPluginCPPInstance:
            {
                void** pThis = (void**)(outData);
                *pThis = (void*)plugin_instance;
                return noErr;
            }
        }
    }

    return AUInstrumentBase::GetProperty(iID, iScope, iElem, outData);
}

