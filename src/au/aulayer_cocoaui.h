//
//  aulayer_cocoaui.h
//  surge-au
//
//  Created by Paul Walker on 12/10/18.
//

#ifndef aulayer_cocoaui_h
#define aulayer_cocoaui_h

#import <AudioToolbox/AudioUnit.h>

ComponentResult SurgeAUGetPropertyCocoaDelegate (AudioUnitPropertyID inID,
                                                 AudioUnitScope inScope,
                                                 AudioUnitElement inElement,
                                                 void* outData);

#endif /* aulayer_cocoaui_h */
