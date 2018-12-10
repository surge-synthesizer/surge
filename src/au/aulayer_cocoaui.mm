//
//  aulayer_cocoaui.mm
//  surge-au
//
//  Created by Paul Walker on 12/10/18.
//

#import <CoreFoundation/CoreFoundation.h>
#include "aulayer_cocoaui.h"

ComponentResult SurgeAUGetPropertyCocoaDelegate (AudioUnitPropertyID inID,
                                                 AudioUnitScope inScope,
                                                 AudioUnitElement inElement,
                                                 void* outData)
{
    [[NSURL URLWithString: @"yo"] retain];
    fprintf( stderr, "Hey ma, I'm in objective c!\n" );
    return noErr;
}
