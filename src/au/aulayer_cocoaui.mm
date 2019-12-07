//
//  aulayer_cocoaui.mm
//  surge-au
//
//  Created by Paul Walker on 12/10/18.
//
/*
    OK so how do cocoa uis work in AU2? There's two things
 
 1: You return a valid AudioUnitViewInfo which contains basically a bundle URL and class name. You implement this in GetProperty in
 response to kAudioUnitPropert_CocoaUI
 
 2: You make that class that you return match the AUCococUIBase protocol which requires it to be able to create a frame
 
 Now the question is how does that frame get a reference to your audio unit? Well there's trick three, which is the uiForAudioUnit has to
 use the property mechanism to get a reference to an editor. In this case I do that by sending the kVmBAudioUnitPropert_GetEditPointer
 which I basically just made up. That returns (in this case) a pointer to a SurgeGUIEditor which uses a subset of the VST api
 (only the public API) to give you a drawable element.
 
 The other trick to make this all work is that Surge itself works by having parameter changes which imapct other parameter changes
 (like when you change patch number it changes patch name) just update parameters and not redraw. This makes sense for all
 the obvious reasons (thread locality; bunched drawing; etc...). You can see the refresh_param_quuee and refresh_control_queue
 implementation which just keep getting stacked up. But to unstack them you need to call a redraw, basically, and that's what the
 editor ::idle method does. So the final thing to make it all work is to spin up a CFRunLoopTimer (not an NSTImer, note; I tried and
 that's not the way to go) to call ::idle every 50ms.
 
 And that's what this code does.
 
 */

#include <objc/runtime.h>
#import <CoreFoundation/CoreFoundation.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <AppKit/AppKit.h>
#include "aulayer.h"
#include "aulayer_cocoaui.h"

#include <gui/SurgeGUIEditor.h>
#include <gui/CScalableBitmap.h>

using namespace VSTGUI;

@interface SurgeNSView : NSView
{
    SurgeGUIEditor *editController;
    CFRunLoopTimerRef idleTimer;
    float lastScale;
    NSSize underlyingUISize;
    bool setSizeByZoom; // use this flag to see if resize comes from here or from external
    
}

- (id) initWithSurge: (SurgeGUIEditor *) cont preferredSize: (NSSize) size;
- (void) doIdle;
- (void) dealloc;
- (void) setFrame:(NSRect)newSize;

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
    SurgeGUIEditor* editController = 0;
    UInt32 size = sizeof (SurgeGUIEditor *);
    if (AudioUnitGetProperty (inAudioUnit, kVmbAAudioUnitProperty_GetEditPointer, kAudioUnitScope_Global, 0, &editController, &size) != noErr)
        return nil;
    
    return [[[SurgeNSView alloc] initWithSurge:editController preferredSize:inPreferredSize] autorelease];
    // return nil;
}

- (unsigned int)interfaceVersion {
    return 0;
}

- (NSString *) description {
    return [NSString stringWithUTF8String: "Surge View"];
}

@end

void timerCallback( CFRunLoopTimerRef timer, void *info )
{
    SurgeNSView *view = (SurgeNSView *)info;
    [view doIdle];
}

@implementation SurgeNSView
- (id) initWithSurge: (SurgeGUIEditor *) cont preferredSize: (NSSize) size
{
    cont->setZoomCallback( []( SurgeGUIEditor *ed ) {} );
    self = [super initWithFrame: NSMakeRect (0, 0, size.width / 2, size.height / 2)];

    idleTimer = nil;
    editController = cont;
    lastScale = cont->getZoomFactor() / 100.0;
    if (self)
    {
        cont->open( self );
        
        ERect *vr;
        if (cont->getRect(&vr))
        {
            float zf = cont->getZoomFactor() / 100.0;
            NSRect newSize = NSMakeRect (0, 0,
                                         vr->right - vr->left,
                                         vr->bottom - vr->top ) ;
            underlyingUISize = newSize.size;
            newSize = NSMakeRect (0, 0,
                                  lastScale * ( vr->right - vr->left ),
                                  lastScale * ( vr->bottom - vr->top ) ) ;
            setSizeByZoom = true;
            [self setFrame:newSize];
            setSizeByZoom = false;

            VSTGUI::CFrame *frame = cont->getFrame();
            if(frame)
            {
               frame->setZoom( zf );
               VSTGUI::CBitmap *bg = frame->getBackground();
               if(bg != NULL)
               {
                  CScalableBitmap *sbm = dynamic_cast<CScalableBitmap *>(bg); // dynamic casts are gross but better safe
                  if (sbm)
                  {
                     sbm->setExtraScaleFactor(cont->getZoomFactor());
                  }
               }
               frame->setDirty(true);
               frame->invalid();
            }
        }

        cont->setZoomCallback( [self]( SurgeGUIEditor *ed ) {
            ERect *vr;
            float zf = ed->getZoomFactor() / 100.0;
            if (ed->getRect(&vr))
            {
                int newW = (int)( (vr->right - vr->left) * zf );
                int newH = (int)( (vr->bottom - vr->top) * zf );
                NSRect newSize = NSMakeRect (0, 0, newW, newH );
                lastScale = zf;

                setSizeByZoom = true;
                [self setFrame:newSize];
                setSizeByZoom = false;

                VSTGUI::CFrame *frame = ed->getFrame();
                if(frame)
                {
                   frame->setZoom( zf );
                   frame->setSize(newW, newH);
                   VSTGUI::CBitmap *bg = frame->getBackground();
                   if(bg != NULL)
                   {
                      CScalableBitmap *sbm = dynamic_cast<CScalableBitmap *>(bg); // dynamic casts are gross but better safe
                      if (sbm)
                      {
                         sbm->setExtraScaleFactor(ed->getZoomFactor());
                      }
                   }
                   frame->setDirty(true);
                   frame->invalid();
                }

            }
            
        }
                              );

        CFTimeInterval      TIMER_INTERVAL = .05; // In SurgeGUISynthesizer.h it uses 50 ms
        CFRunLoopTimerContext TimerContext = {0, self, NULL, NULL, NULL};
        CFAbsoluteTime             FireTime = CFAbsoluteTimeGetCurrent() + TIMER_INTERVAL;
        idleTimer = CFRunLoopTimerCreate(kCFAllocatorDefault,
                                              FireTime,
                                              TIMER_INTERVAL,
                                              0, 0,
                                              timerCallback,
                                              &TimerContext);
        if (idleTimer)
            CFRunLoopAddTimer (CFRunLoopGetMain (), idleTimer, kCFRunLoopCommonModes);
    }
    
    return self;
}

- (void) doIdle
{
    editController->idle();
}

- (void) dealloc
{
    editController->close();
    if( idleTimer )
    {
        CFRunLoopTimerInvalidate( idleTimer );
    }

    [super dealloc];
}

- (void) setFrame: (NSRect) newSize
{
    /*
     * I override setFrame because hosts have independent views of window sizes which are saved.
     * this needs to be found when the host resizes after creation to set the zoomFactor properly.
     * Teensy bit gross, but works. Seems AU Lab does this but Logic Pro does not.
     *
     * the other option is to make zoom a parameter but then its in a patch and that seems wrong.
     */
    NSSize targetSize = newSize.size;
   
    if( !setSizeByZoom && fabs( targetSize.width - underlyingUISize.width * lastScale ) > 2 )
    {
        // so what's my apparent ratio
        float apparentZoom = targetSize.width / ( underlyingUISize.width * lastScale );
        int azi = roundf( apparentZoom * 10 ) * 10; // this is a bit gross. I know the zoom is incremented by 10s
        editController->setZoomFactor( azi );
    }
    
    [super setFrame:newSize];
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
                {
                    auto surgeclass = objc_getClass( "SurgeCocoaUI" );
                    const char* image = class_getImageName ( surgeclass );
                    CFBundleRef bundle = GetBundleFromExecutable (image);
                    CFURLRef url = CFBundleCopyBundleURL (bundle);
                    CFRetain( url );
                    CFRelease (bundle);

                    
                    AudioUnitCocoaViewInfo* info = static_cast<AudioUnitCocoaViewInfo*> (outData);
                    info->mCocoaAUViewClass[0] = CFStringCreateWithCString(kCFAllocatorDefault, class_getName(surgeclass), kCFStringEncodingUTF8);
                    info->mCocoaAUViewBundleLocation = url;

                    return noErr;
                }
            case kVmbAAudioUnitProperty_GetEditPointer:
            {
                if( editor_instance == NULL )
                {
                    editor_instance = new SurgeGUIEditor( this, plugin_instance );
                    editor_instance->loadFromDAWExtraState(plugin_instance);
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

