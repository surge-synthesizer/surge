#include "aulayer.h"

//-----------------------------------------------------------------------------
// The following defines and implements CoreFoundation-like handling of
// an AUPreset container object:  CFAUPreset
//-----------------------------------------------------------------------------

const UInt32 kCFAUPreset_CurrentVersion = 0;

//-----------------------------------------------------------------------------
// create an instance of a CFAUPreset object
CFAUPresetRef CFAUPresetCreate(CFAllocatorRef inAllocator, SInt32 inPresetNumber,
                               CFStringRef inPresetName)
{
    CFAUPreset *newPreset = (CFAUPreset *)CFAllocatorAllocate(inAllocator, sizeof(CFAUPreset), 0);
    if (newPreset != NULL)
    {
        newPreset->auPreset.presetNumber = inPresetNumber;
        newPreset->auPreset.presetName = NULL;
        // create our own a copy rather than retain the string, in case the input string is mutable,
        // this will keep it from changing under our feet
        if (inPresetName != NULL)
            newPreset->auPreset.presetName = CFStringCreateCopy(inAllocator, inPresetName);
        newPreset->version = kCFAUPreset_CurrentVersion;
        newPreset->allocator = inAllocator;
        newPreset->retainCount = 1;
    }
    return (CFAUPresetRef)newPreset;
}

//-----------------------------------------------------------------------------
// retain a reference of a CFAUPreset object
CFAUPresetRef CFAUPresetRetain(CFAUPresetRef inPreset)
{
    if (inPreset != NULL)
    {
        CFAUPreset *incomingPreset = (CFAUPreset *)inPreset;
        // retain the input AUPreset's name string for this reference to the preset
        if (incomingPreset->auPreset.presetName != NULL)
            CFRetain(incomingPreset->auPreset.presetName);
        incomingPreset->retainCount += 1;
    }
    return inPreset;
}

//-----------------------------------------------------------------------------
// release a reference of a CFAUPreset object
void CFAUPresetRelease(CFAUPresetRef inPreset)
{
    CFAUPreset *incomingPreset = (CFAUPreset *)inPreset;
    // these situations shouldn't happen
    if (inPreset == NULL)
        return;
    if (incomingPreset->retainCount <= 0)
        return;

    // first release the name string, CF-style, since it's a CFString
    if (incomingPreset->auPreset.presetName != NULL)
        CFRelease(incomingPreset->auPreset.presetName);
    incomingPreset->retainCount -= 1;
    // check if this is the end of this instance's life
    if (incomingPreset->retainCount == 0)
    {
        // wipe out the data so that, if anyone tries to access stale memory later, it will be
        // (semi)invalid
        incomingPreset->auPreset.presetName = NULL;
        incomingPreset->auPreset.presetNumber = 0;
        // and finally, free the memory for the CFAUPreset struct
        CFAllocatorDeallocate(incomingPreset->allocator, (void *)inPreset);
    }
}

//-----------------------------------------------------------------------------
// The following 4 functions are CFArray callbacks for use when creating
// an AU's factory presets array to support kAudioUnitProperty_FactoryPresets.
//-----------------------------------------------------------------------------

const void *CFAUPresetArrayRetainCallBack(CFAllocatorRef inAllocator, const void *inPreset);
void CFAUPresetArrayReleaseCallBack(CFAllocatorRef inAllocator, const void *inPreset);
Boolean CFAUPresetArrayEqualCallBack(const void *inPreset1, const void *inPreset2);
CFStringRef CFAUPresetArrayCopyDescriptionCallBack(const void *inPreset);
void CFAUPresetArrayCallBacks_Init(CFArrayCallBacks *outArrayCallBacks);

//-----------------------------------------------------------------------------
// This function is called when an item (an AUPreset) is added to the CFArray,
// or when a CFArray containing an AUPreset is retained.
const void *CFAUPresetArrayRetainCallBack(CFAllocatorRef inAllocator, const void *inPreset)
{
    return CFAUPresetRetain((CFAUPresetRef)inPreset);
}

//-----------------------------------------------------------------------------
// This function is called when an item (an AUPreset) is removed from the CFArray
// or when the array is released.
// Since a reference to the data belongs to the array, we need to release that here.
void CFAUPresetArrayReleaseCallBack(CFAllocatorRef inAllocator, const void *inPreset)
{
    CFAUPresetRelease((CFAUPresetRef)inPreset);
}

//-----------------------------------------------------------------------------
// This function is called when someone wants to compare to items (AUPresets)
// in the CFArray to see if they are equal or not.
// For our AUPresets, we will compare based on the preset number and the name string.
Boolean CFAUPresetArrayEqualCallBack(const void *inPreset1, const void *inPreset2)
{
    AUPreset *preset1 = (AUPreset *)inPreset1;
    AUPreset *preset2 = (AUPreset *)inPreset2;
    // the two presets are only equal if they have the same preset number and
    // if the two name strings are the same (which we rely on the CF function to compare)
    return (preset1->presetNumber == preset2->presetNumber) &&
           (CFStringCompare(preset1->presetName, preset2->presetName, 0) == kCFCompareEqualTo);
}

//-----------------------------------------------------------------------------
// This function is called when someone wants to get a description of
// a particular item (an AUPreset) as though it were a CF type.
// That happens, for example, when using CFShow().
// This will create and return a CFString that indicates that
// the object is an AUPreset and tells the preset number and preset name.
CFStringRef CFAUPresetArrayCopyDescriptionCallBack(const void *inPreset)
{
    AUPreset *preset = (AUPreset *)inPreset;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                    CFSTR("AUPreset:\npreset number = %d\npreset name = %@"),
                                    preset->presetNumber, preset->presetName);
}

//-----------------------------------------------------------------------------
// this will initialize a CFArray callbacks structure to use the above callback functions
void CFAUPresetArrayCallBacks_Init(CFArrayCallBacks *outArrayCallBacks)
{
    if (outArrayCallBacks == NULL)
        return;
    // wipe the struct clean
    memset(outArrayCallBacks, 0, sizeof(*outArrayCallBacks));
    // set all of the values and function pointers in the callbacks struct
    outArrayCallBacks->version = 0; // currently, 0 is the only valid version value for this
    outArrayCallBacks->retain = CFAUPresetArrayRetainCallBack;
    outArrayCallBacks->release = CFAUPresetArrayReleaseCallBack;
    outArrayCallBacks->copyDescription = CFAUPresetArrayCopyDescriptionCallBack;
    outArrayCallBacks->equal = CFAUPresetArrayEqualCallBack;
}

//-----------------------------------------------------------------------------

const CFArrayCallBacks kCFAUPresetArrayCallBacks = {
    0, CFAUPresetArrayRetainCallBack, CFAUPresetArrayReleaseCallBack,
    CFAUPresetArrayCopyDescriptionCallBack, CFAUPresetArrayEqualCallBack};

/*
obsolete code below
 //-----------------------------------------------------------------------------
// The following 4 functions are CFArray callbacks for use when creating
// an AU's factory presets array to support kAudioUnitProperty_FactoryPresets.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// This function is called when an item (an AUPreset) is added to the CFArray.
// At this time, we need to make a fully independent copy of the AUPreset so that
// the data will be guaranteed to be valid regardless of what the AU does, and
// regardless of whether the AU instance is still open or not.
// This means that we need to allocate memory for an AUPreset struct just for the
// array and create a copy of the preset name string.
const void * auPresetCFArrayRetainCallback(CFAllocatorRef inAllocator, const void * inPreset)
{
        AUPreset * preset = (AUPreset*) inPreset;
        // first allocate new memory for the array's copy of this AUPreset
        AUPreset * newPreset = (AUPreset*) CFAllocatorAllocate(inAllocator, sizeof(AUPreset), 0);
        // set the number of the new copy to that of the input AUPreset
        newPreset->presetNumber = preset->presetNumber;
        // create a copy of the input AUPreset's name string for this new copy of the preset
        newPreset->presetName = CFStringCreateCopy(kCFAllocatorDefault, preset->presetName);
        // return the pointer to the new memory, which belongs to the array, rather than the input
pointer return newPreset;
}

//-----------------------------------------------------------------------------
// This function is called when an item (an AUPreset) is removed from the CFArray
// or when the array is released.
// Since the memory for the data belongs to the array, we need to release it all here.
void auPresetCFArrayReleaseCallback(CFAllocatorRef inAllocator, const void * inPreset)
{
        AUPreset * preset = (AUPreset*) inPreset;
        // first release the name string, CF-style, since it's a CFString
        if (preset->presetName != NULL)
                CFRelease(preset->presetName);
        // wipe out the data so that, if anyone tries to access stale memory later, it will be
invalid preset->presetName = NULL; preset->presetNumber = 0;
        // and finally, free the memory for the AUPreset struct
        CFAllocatorDeallocate(inAllocator, (void*)inPreset);
}

//-----------------------------------------------------------------------------
// This function is called when someone wants to compare to items (AUPresets)
// in the CFArray to see if they are equal or not.
// For our AUPresets, we will compare based on the preset number and the name string.
Boolean auPresetCFArrayEqualCallback(const void * inPreset1, const void * inPreset2)
{
        AUPreset * preset1 = (AUPreset*) inPreset1;
        AUPreset * preset2 = (AUPreset*) inPreset2;
        // the two presets are only equal if they have the same preset number and
        // if the two name strings are the same (which we rely on the CF function to compare)
        return (preset1->presetNumber == preset2->presetNumber) &&
                (CFStringCompare(preset1->presetName, preset2->presetName, 0) == kCFCompareEqualTo);
}

//-----------------------------------------------------------------------------
// This function is called when someone wants to get a description of
// a particular item (an AUPreset) as though it were a CF type.
// That happens, for example, when using CFShow().
// This will create and return a CFString that indicates that
// the object is an AUPreset and tells the preset number and preset name.
CFStringRef auPresetCFArrayCopyDescriptionCallback(const void * inPreset)
{
        AUPreset * preset = (AUPreset*) inPreset;
        return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                                        CFSTR("AUPreset:\npreset
number = %d\npreset name = %@"), preset->presetNumber, preset->presetName);
}

//-----------------------------------------------------------------------------
// this will initialize a CFArray callbacks structure to use the above callback functions
void AUPresetCFArrayCallbacks_Init(CFArrayCallBacks * outArrayCallbacks)
{
        if (outArrayCallbacks == NULL)
                return;
        // wipe the struct clean
        memset(outArrayCallbacks, 0, sizeof(*outArrayCallbacks));
        // set all of the values and function pointers in the callbacks struct
        outArrayCallbacks->version = 0;	// currently, 0 is the only valid version value for this
        outArrayCallbacks->retain = auPresetCFArrayRetainCallback;
        outArrayCallbacks->release = auPresetCFArrayReleaseCallback;
        outArrayCallbacks->copyDescription = auPresetCFArrayCopyDescriptionCallback;
        outArrayCallbacks->equal = auPresetCFArrayEqualCallback;
}

//-----------------------------------------------------------------------------
#ifdef __GNUC__
// XXX what is the best way to do this?
#if 1
const CFArrayCallBacks kAUPresetCFArrayCallbacks = {
version: 0,
retain: auPresetCFArrayRetainCallback,
release: auPresetCFArrayReleaseCallback,
copyDescription: auPresetCFArrayCopyDescriptionCallback,
equal: auPresetCFArrayEqualCallback
};
#elif 0
const CFArrayCallBacks kAUPresetCFArrayCallbacks = {
        .version = 0,
        .retain = auPresetCFArrayRetainCallback,
        .release = auPresetCFArrayReleaseCallback,
        .copyDescription = auPresetCFArrayCopyDescriptionCallback,
        .equal = auPresetCFArrayEqualCallback
};
#else
// this seems to be the only way of the 3 that is fully valid C, but unfortunately only for GCC
const CFArrayCallBacks kAUPresetCFArrayCallbacks;
static void kAUPresetCFArrayCallbacks_constructor() __attribute__((constructor));
static void kAUPresetCFArrayCallbacks_constructor()
{
        AUPresetCFArrayCallbacks_Init( (CFArrayCallBacks*) &kAUPresetCFArrayCallbacks );
}
#endif
#else
// XXX I'll use this for other compilers, even though I hate initializing structs with all arguments
at once
// (cuz what if I ever decided to change the order of the struct members or something like that?)
const CFArrayCallBacks kAUPresetCFArrayCallbacks = {
        0,
        auPresetCFArrayRetainCallback,
        auPresetCFArrayReleaseCallback,
        auPresetCFArrayCopyDescriptionCallback,
        auPresetCFArrayEqualCallback
};
#endif


*/
