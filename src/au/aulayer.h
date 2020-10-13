/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include "AUInstrumentBase.h"
#include "surge_auversion.h"
#include "SurgeStorage.h"
#include <vector>

class SurgeGUIEditor;
class SurgeSynthesizer;
typedef SurgeSynthesizer plugin;

//-------------------------------------------------------------------------------------------------------
const CFStringRef rawchunkname = CFSTR("VmbA_chunk");

enum
{
   kVmbAAudioUnitProperty_GetPluginCPPInstance = 70000,
   kVmbAAudioUnitProperty_GetEditPointer = 70001,
};

// event
struct AuMIDIEvent
{
   UInt8 status, channel, data1, data2;
   long inStartFrame;
};

class aulayer : public AUInstrumentBase
{
public:
   aulayer(AudioUnit au);
   virtual ~aulayer();

   virtual int GetNumCustomUIComponents();
   virtual void GetUIComponentDescs(ComponentDescription* inDescArray);

   virtual ComponentResult Initialize();
   virtual void Cleanup();
   virtual ComponentResult Version();
   virtual ComponentResult Reset(AudioUnitScope inScope, AudioUnitElement inElement);

   virtual ComponentResult ChangeStreamFormat(AudioUnitScope inScope,
                                              AudioUnitElement inElement,
                                              const CAStreamBasicDescription& inPrevFormat,
                                              const CAStreamBasicDescription& inNewFormat);

   virtual ComponentResult Render(AudioUnitRenderActionFlags& ioActionFlags,
                                  const AudioTimeStamp& inTimeStamp,
                                  UInt32 inNumberFrames);

   virtual UInt32 SupportedNumChannels(const AUChannelInfo** outInfo);
   virtual bool StreamFormatWritable(AudioUnitScope scope, AudioUnitElement element);
   virtual bool ValidFormat(AudioUnitScope inScope,
                            AudioUnitElement inElement,
                            const CAStreamBasicDescription& inNewFormat);

   virtual ComponentResult StartNote(MusicDeviceInstrumentID inInstrument,
                                     MusicDeviceGroupID inGroupID,
                                     NoteInstanceID* outNoteInstanceID,
                                     UInt32 inOffsetSampleFrame,
                                     const MusicDeviceNoteParams& inParams);

   virtual ComponentResult StopNote(MusicDeviceGroupID inGroupID,
                                    NoteInstanceID inNoteInstanceID,
                                    UInt32 inOffsetSampleFrame);

   virtual OSStatus
   HandleNoteOn(UInt8 inChannel, UInt8 inNoteNumber, UInt8 inVelocity, UInt32 inStartFram);
   virtual OSStatus
   HandleNoteOff(UInt8 inChannel, UInt8 inNoteNumber, UInt8 inVelocity, UInt32 inStartFrame);
   virtual OSStatus
   HandleControlChange(UInt8 inChannel, UInt8 inController, UInt8 inValue, UInt32 inStartFrame);
   virtual OSStatus HandleChannelPressure(UInt8 inChannel, UInt8 inValue, UInt32 inStartFrame);
   virtual OSStatus
   HandlePitchWheel(UInt8 inChannel, UInt8 inPitch1, UInt8 inPitch2, UInt32 inStartFrame);
   virtual OSStatus
   HandlePolyPressure(UInt8 inChannel, UInt8 inKey, UInt8 inValue, UInt32 inStartFrame);
   virtual OSStatus HandleProgramChange(UInt8 inChannel, UInt8 inValue);

   virtual ComponentResult GetProperty(AudioUnitPropertyID inID,
                                       AudioUnitScope inScope,
                                       AudioUnitElement inElement,
                                       void* outData);
   virtual ComponentResult GetPropertyInfo(AudioUnitPropertyID inID,
                                           AudioUnitScope inScope,
                                           AudioUnitElement inElement,
                                           UInt32& outDataSize,
                                           Boolean& outWritable);

   virtual ComponentResult SaveState(CFPropertyListRef* outData);
   virtual ComponentResult RestoreState(CFPropertyListRef inData);

   virtual bool HasIcon();
   virtual CFURLRef GetIconLocation();

   virtual ComponentResult GetPresets(CFArrayRef* outData) const;
   virtual OSStatus NewFactoryPresetSet(const AUPreset& inNewFactoryPreset);

   virtual OSStatus
   HandleMidiEvent(UInt8 status, UInt8 channel, UInt8 data1, UInt8 data2, UInt32 inStartFrame);

   virtual ComponentResult GetParameterList(AudioUnitScope inScope,
                                            AudioUnitParameterID* outParameterList,
                                            UInt32& outNumParameters);
   virtual ComponentResult GetParameterInfo(AudioUnitScope inScope,
                                            AudioUnitParameterID inParameterID,
                                            AudioUnitParameterInfo& outParameterInfo);
   virtual ComponentResult GetParameter(AudioUnitParameterID inID,
                                        AudioUnitScope inScope,
                                        AudioUnitElement inElement,
                                        Float32& outValue);
   virtual ComponentResult SetParameter(AudioUnitParameterID inID,
                                        AudioUnitScope inScope,
                                        AudioUnitElement inElement,
                                        Float32 inValue,
                                        UInt32 inBufferOffsetInFrames);

   virtual bool CanScheduleParameters() const;

   /*virtual OSStatus 	ScheduleParameter (const AudioUnitParameterEvent 	*inParameterEvent,
                                        UInt32
      inNumEvents);*/

   bool ParameterUpdate(int p);
   bool ParameterBeginEdit(int p);
   bool ParameterEndEdit(int p);

   // These methods initialize the plugin itself, and is seperate from the AU initialization
   // THis is needed sometimes, as the AU may ask to store a state before the AU has been
   // initialized
   void InitializePlugin();
   bool IsPluginInitialized();

   void setPresetByID( int id );
   
   // FIXME: Move to std::unique_ptr<>
   plugin* plugin_instance;
   SurgeGUIEditor* editor_instance;

protected:
   // void handleEvent(VstEvent*);
   AuMIDIEvent eventbuffer[1024];
   int blockpos, events_this_block;
   AUChannelInfo cinfo[2]; // stored output configs
   AudioUnitParameterID parameterIDlist[n_total_params + num_metaparameters];
   CFStringRef parameterIDlist_CFString[n_total_params + num_metaparameters];
   float sampleRateCache;
   std::vector<int> presetOrderToPatchList;

   int checkNamesEvery = 0;
};

struct CFAUPreset
{
   AUPreset auPreset;
   UInt32 version;
   CFAllocatorRef allocator;
   CFIndex retainCount;
};

// from DFX au lib
// måste lägga in updaten från 2006-08-18, fanns error-lalala innan
/* these are CFArray callbacks for use when creating an AU's FactoryPresets array */
extern const void* auPresetCFArrayRetainCallback(CFAllocatorRef inAllocator, const void* inPreset);
extern void auPresetCFArrayReleaseCallback(CFAllocatorRef inAllocator, const void* inPreset);
extern Boolean auPresetCFArrayEqualCallback(const void* inPreset1, const void* inPreset2);
extern CFStringRef auPresetCFArrayCopyDescriptionCallback(const void* inPreset);
/* and this will initialize a CFArray callbacks structure to use the above callback functions */
/*extern void AUPresetCFArrayCallbacks_Init(CFArrayCallBacks * outArrayCallbacks);
extern const CFArrayCallBacks kAUPresetCFArrayCallbacks;*/

// new DFX lib (2006-08-18)
/* these handle a CoreFoundation-like container object for AUPreset called CFAUPreset */
typedef const struct CFAUPreset* CFAUPresetRef;
extern CFAUPresetRef
CFAUPresetCreate(CFAllocatorRef inAllocator, SInt32 inPresetNumber, CFStringRef inPresetName);
extern CFAUPresetRef CFAUPresetRetain(CFAUPresetRef inPreset);
extern void CFAUPresetRelease(CFAUPresetRef inPreset);
extern const CFArrayCallBacks kCFAUPresetArrayCallBacks;
