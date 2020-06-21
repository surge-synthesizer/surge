
#include "aulayer.h"
#include <gui/SurgeGUIEditor.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#include <AudioUnit/AudioUnitCarbonView.h>
#include "aulayer_cocoaui.h"

typedef SurgeSynthesizer sub3_synth;

//----------------------------------------------------------------------------------------------------

aulayer::aulayer(AudioUnit au) : AUInstrumentBase(au, 1, 1)
{
   plugin_instance = 0;
   editor_instance = 0;
}

//----------------------------------------------------------------------------------------------------

aulayer::~aulayer()
{
   if (plugin_instance)
   {
      plugin_instance->~plugin();
      _aligned_free(plugin_instance);
   }

   if (editor_instance)
   {
      delete editor_instance;
   }
}

//----------------------------------------------------------------------------------------------------

int aulayer::GetNumCustomUIComponents()
{
   return 1;
}

//----------------------------------------------------------------------------------------------------

UInt32 aulayer::SupportedNumChannels(const AUChannelInfo** outInfo)
{
   if (!outInfo)
      return 2;
   cinfo[0].inChannels = 0;
   cinfo[0].outChannels = 2;
   cinfo[1].inChannels = 2;
   cinfo[1].outChannels = 2;
   *outInfo = cinfo;
   return 2;
}

//----------------------------------------------------------------------------------------------------

#define kComponentSubType 'Srge'

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::Version()
{
   return kVersionNumber;
}

//----------------------------------------------------------------------------------------------------

void aulayer::GetUIComponentDescs(ComponentDescription* inDescArray)
{
   inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
   inDescArray[0].componentSubType = kComponentSubType;
   inDescArray[0].componentManufacturer = kComponentManuf;
   inDescArray[0].componentFlags = 0;
   inDescArray[0].componentFlagsMask = 0;
}

//----------------------------------------------------------------------------------------------------

bool aulayer::HasIcon()
{
   return true;
}

//----------------------------------------------------------------------------------------------------

CFURLRef aulayer::GetIconLocation()
{
   CFURLRef icon = 0;
   CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.vemberaudio.audiounit.surge"));

   if (bundle)
   {
      CFURLRef resources = CFBundleCopyResourcesDirectoryURL(bundle);
      if (resources)
      {
         icon =
             CFURLCreateCopyAppendingPathComponent(NULL, resources, CFSTR("surgeicon.icns"), false);
         CFRelease(resources);
      }
   }
   return icon;
}

//----------------------------------------------------------------------------------------------------

void aulayer::InitializePlugin()
{
   if (!plugin_instance)
   {
      // sub3_synth* synth = (sub3_synth*)_aligned_malloc(sizeof(sub3_synth),16);
      // new(synth) sub3_synth(this);
      // FIXME: The VST uses a std::unique_ptr<> and we probably should here also
      plugin_instance = new SurgeSynthesizer(this);

      // This allows us standalone performance mode. See issue #146 and comment below tagged with
      // issue number
      plugin_instance->time_data.ppqPos = 0;

         sub3_synth* s = (sub3_synth*)plugin_instance;

         // Generate preset list in an ordered fashion
         presetOrderToPatchList.clear();
         for (int i = 0; i < s->storage.firstThirdPartyCategory; i++)
         {
            // Remap index to the corresponding category in alphabetical order.
            int c = s->storage.patchCategoryOrdering[i];
            for (auto p : s->storage.patchOrdering)
            {
               if (s->storage.patch_list[p].category == c)
               {
                  presetOrderToPatchList.push_back(p);
               }
            }
         }
   }
   assert(plugin_instance);
}

//----------------------------------------------------------------------------------------------------

bool aulayer::IsPluginInitialized()
{
   return plugin_instance != 0;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::Initialize()
{
   if (!IsPluginInitialized())
   {
      InitializePlugin();
   }

   double samplerate = GetOutput(0)->GetStreamFormat().mSampleRate;
   plugin_instance->setSamplerate(samplerate);
   plugin_instance->audio_processing_active = true;
   plugin_instance->allNotesOff();
   sampleRateCache = samplerate;

   blockpos = 0;
   events_this_block = 0;
   // init parameters
   for (UInt32 i = 0; i < n_total_params + num_metaparameters; i++)
   {
      parameterIDlist[i] = i;
      parameterIDlist_CFString[i] = 0;
   }

   AUInstrumentBase::Initialize();
   return noErr;
}

//----------------------------------------------------------------------------------------------------

void aulayer::Cleanup()
{
   AUInstrumentBase::Cleanup();
}

//----------------------------------------------------------------------------------------------------

bool aulayer::StreamFormatWritable(AudioUnitScope scope, AudioUnitElement element)
{

   return true; // IsInitialized() ? false : true;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::ChangeStreamFormat(AudioUnitScope inScope,
                                            AudioUnitElement inElement,
                                            const CAStreamBasicDescription& inPrevFormat,
                                            const CAStreamBasicDescription& inNewFormat)
{
   double samplerate = inNewFormat.mSampleRate;
   ComponentResult result =
       AUBase::ChangeStreamFormat(inScope, inElement, inPrevFormat, inNewFormat);

   if (plugin_instance)
      plugin_instance->setSamplerate(samplerate);
   sampleRateCache = samplerate;

   return result;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::Reset(AudioUnitScope inScope, AudioUnitElement inElement)
{
   if ((inScope == kAudioUnitScope_Global) && (inElement == 0) && plugin_instance)
   {
      double samplerate = GetOutput(0)->GetStreamFormat().mSampleRate;
      if( samplerate != sampleRateCache )
      {
          plugin_instance->setSamplerate(samplerate);
          sampleRateCache = samplerate;
      }
      plugin_instance->allNotesOff();
   }
   return noErr;
}

//----------------------------------------------------------------------------------------------------

bool aulayer::ValidFormat(AudioUnitScope inScope,
                          AudioUnitElement inElement,
                          const CAStreamBasicDescription& inNewFormat)
{

   if (!FormatIsCanonical(inNewFormat))
      return false;
   if (inElement > 0)
      return false; // only 1 input/output element supported
   if (inScope == kAudioUnitScope_Input)
   {
      if (inNewFormat.NumberChannels() == 0)
         return true;
      else if (inNewFormat.NumberChannels() == 2)
      {
         return true;
      }
   }
   else if (inScope == kAudioUnitScope_Output)
   {
      if (inNewFormat.NumberChannels() == 2)
         return true;
   }
   else if (inScope == kAudioUnitScope_Global)
   {
      return true;
   }
   return false;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::StartNote(MusicDeviceInstrumentID inInstrument,
                                   MusicDeviceGroupID inGroupID,
                                   NoteInstanceID* outNoteInstanceID,
                                   UInt32 inOffsetSampleFrame,
                                   const MusicDeviceNoteParams& inParams)
{
   return 0;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::StopNote(MusicDeviceGroupID inGroupID,
                                  NoteInstanceID inNoteInstanceID,
                                  UInt32 inOffsetSampleFrame)
{
   return 0;
}

//----------------------------------------------------------------------------------------------------

OSStatus
aulayer::HandleNoteOn(UInt8 inChannel, UInt8 inNoteNumber, UInt8 inVelocity, UInt32 inStartFrame)
{
   plugin_instance->playNote(inChannel, inNoteNumber, inVelocity, 0);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

OSStatus
aulayer::HandleNoteOff(UInt8 inChannel, UInt8 inNoteNumber, UInt8 inVelocity, UInt32 inStartFrame)
{
   plugin_instance->releaseNote(inChannel, inNoteNumber, inVelocity);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

OSStatus aulayer::HandleControlChange(UInt8 inChannel,
                                      UInt8 inController,
                                      UInt8 inValue,
                                      UInt32 inStartFrame)
{
   plugin_instance->channelController(inChannel, inController, inValue);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

OSStatus aulayer::HandleChannelPressure(UInt8 inChannel, UInt8 inValue, UInt32 inStartFrame)
{
   plugin_instance->channelAftertouch(inChannel, inValue);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

OSStatus
aulayer::HandlePitchWheel(UInt8 inChannel, UInt8 inPitch1, UInt8 inPitch2, UInt32 inStartFrame)
{
   long value = (inPitch1 & 0x7f) + ((inPitch2 & 0x7f) << 7);
   plugin_instance->pitchBend(inChannel, value - 8192);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

OSStatus
aulayer::HandlePolyPressure(UInt8 inChannel, UInt8 inKey, UInt8 inValue, UInt32 inStartFrame)
{
   plugin_instance->polyAftertouch(inChannel, inKey, inValue);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

OSStatus aulayer::HandleProgramChange(UInt8 inChannel, UInt8 inValue)
{
   plugin_instance->programChange(inChannel, inValue);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::Render(AudioUnitRenderActionFlags& ioActionFlags,
                                const AudioTimeStamp& inTimeStamp,
                                UInt32 inNumberFrames)
{
   assert(IsPluginInitialized());
   assert(IsInitialized());
   SurgeSynthesizer* s = (SurgeSynthesizer*)plugin_instance;
   s->process_input = 0;

   float sampleRate = sampleRateCache;
   float* outputs[N_OUTPUTS];
   float* inputs[N_INPUTS];

   AudioUnitRenderActionFlags xflags = 0;
   if (HasInput(0))
   {
      ComponentResult result = PullInput(0, xflags, inTimeStamp, inNumberFrames);

      if (result == noErr)
      {
         for (long i = 0; i < N_INPUTS; i++)
         {
            inputs[i] = GetInput(0)->GetChannelData(i);
         }
         s->process_input = inputs[0] != 0;
      }
   }
   // Get output buffer list and extract the i/o buffer pointers.
   // if (N_OUTPUTS>0)
   {
      AudioBufferList& asOutBufs = GetOutput(0)->GetBufferList();
      for (long o = 0; o < N_OUTPUTS; ++o)
      {
         outputs[o] = (float*)(asOutBufs.mBuffers[o].mData);
         if (asOutBufs.mBuffers[o].mDataByteSize <= 0 || o >= asOutBufs.mNumberBuffers)
            outputs[o] = nil;
      }
   }

   // Get the transport status
   Boolean isPlaying;

   if (CallHostTransportState(&isPlaying,
                              NULL, // &isTransportStateChanged,
                              NULL, // &currentSampleInTimeline,
                              NULL, // &isCycling,
                              NULL, // &cycleStartBeat,
                              NULL  // &cycleEndBeat
                              ) < 0)
   {
      isPlaying = false;
   }

   // do each buffer
   Float64 CurrentBeat, CurrentTempo;
   if (CallHostBeatAndTempo(&CurrentBeat, &CurrentTempo) >= 0)
   {
      plugin_instance->time_data.tempo = CurrentTempo;

      // If the engine isn't playing, so CurrentBeat is a constant, then result here is
      // resetting time_data.ppqPos to the same thing over and over. That means the lfo_freerun
      // mode basically acts like keypress mode since time_data.ppqPos - which maps to songpos
      // is always reset to zero or a frame or so beyond. If instead we only reset it when
      // playing then exactly what we want occurs. In playback mode we get perfectly predictable
      // oscillator start and stop; but in performance mode we get freerun working off the internal
      // clock which we synthesize by running the "move clock" block below.
      //
      // See github issue #146
      if (isPlaying)
         plugin_instance->time_data.ppqPos = CurrentBeat;
   }
   else
   {
      plugin_instance->time_data.tempo = 120;
   }

   UInt32 oDS;
   Float32 otsNum;
   UInt32 otsDen;
   Float64 ocmD;
   if( CallHostMusicalTimeLocation(&oDS, &otsNum, &otsDen, &ocmD ) >= 0 )
   {
      plugin_instance->time_data.timeSigNumerator = (int)otsNum;
      plugin_instance->time_data.timeSigDenominator = (int)otsDen;
   }

   unsigned int events_processed = 0;

   unsigned int i;
   for (i = 0; i < inNumberFrames; i++)
   {
      if (blockpos == 0)
      {
         // move clock
         plugin_instance->time_data.ppqPos +=
             (double)BLOCK_SIZE * plugin_instance->time_data.tempo / (60. * sampleRate);

         // process events for the current block
         while (events_processed < events_this_block)
         {
            if ((i + blockpos) > eventbuffer[events_processed].inStartFrame)
            {
               AuMIDIEvent* e = &eventbuffer[events_processed];
               AUMIDIBase::HandleMidiEvent(e->status, e->channel, e->data1, e->data2,
                                           e->inStartFrame);
               events_processed++;
            }
            else
               break;
         }

         // run synth engine for the current block
         plugin_instance->process();
      }

      if (s->process_input)
      {
         int inp;
         for (inp = 0; inp < N_INPUTS; inp++)
         {
            plugin_instance->input[inp][blockpos] = inputs[inp][i];
         }
      }

      int outp;
      for (outp = 0; outp < N_OUTPUTS; outp++)
      {
         outputs[outp][i] = (float)plugin_instance->output[outp][blockpos]; // replacing
      }

      blockpos++;
      if (blockpos >= BLOCK_SIZE)
         blockpos = 0;
   }

   // process remaining events (there shouldn't be any)
   while (events_processed < events_this_block)
   {
      AuMIDIEvent* e = &eventbuffer[events_processed];
      AUMIDIBase::HandleMidiEvent(e->status, e->channel, e->data1, e->data2, e->inStartFrame);
      events_processed++;
   }
   events_this_block = 0; // reset eventbuffer

   return noErr;
}

//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::GetPropertyInfo(AudioUnitPropertyID iID,
                                         AudioUnitScope iScope,
                                         AudioUnitElement iElem,
                                         UInt32& iSize,
                                         Boolean& fWritable)
{
   // OK so big todo here and in GetProperty
   // 1: Switch these all to have an inScope--kAudioUnitScope_Global guard
   // 2: Switch these to be a switch
   // 3: Make Cocoa UI make the datasize the size of the view info. Take a look in
   // juce_audio_plugin_client/AU/juce_AU_Wrapper.mm 4: That will probably core out since the calss
   // isn't defined. That's OK! MOve on from there.
   if (iScope == kAudioUnitScope_Global)
   {
      switch (iID)
      {
      case kAudioUnitProperty_CocoaUI:
         iSize = sizeof(AudioUnitCocoaViewInfo);
         fWritable = true;
         return noErr;
         break;
      case kVmbAAudioUnitProperty_GetEditPointer:
         iSize = sizeof(SurgeGUIEditor*);
         fWritable = true;
         return noErr;
         break;
      case kAudioUnitProperty_ParameterValueName:
         iSize = sizeof(AudioUnitParameterValueName);
         return noErr;
         break;
      case kAudioUnitProperty_ParameterClumpName:
         iSize = sizeof(AudioUnitParameterNameInfo);
         return noErr;
         break;
      case kVmbAAudioUnitProperty_GetPluginCPPInstance:
         iSize = sizeof(void*);
         return noErr;
         break;
      }
   }

   return AUInstrumentBase::GetPropertyInfo(iID, iScope, iElem, iSize, fWritable);
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::RestoreState(CFPropertyListRef plist)
{
   ComponentResult result = AUBase::RestoreState(plist);
   if (result != noErr)
      return result;

   CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);

   CFDataRef data = reinterpret_cast<CFDataRef>(CFDictionaryGetValue(dict, rawchunkname));
   if (data != NULL)
   {
      if (!IsPluginInitialized())
      {
         InitializePlugin();
      }
      const UInt8* p;
      p = CFDataGetBytePtr(data);
      size_t psize = CFDataGetLength(data);
      plugin_instance->loadRaw(p, psize, false);

      plugin_instance->loadFromDawExtraState();
      if( editor_instance )
          editor_instance->loadFromDAWExtraState(plugin_instance);

      // This stops any prior factory loads from clobbering me. #2102
      plugin_instance->patchid_queue = -1;
   }
   return noErr;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::SaveState(CFPropertyListRef* plist)
{
   // store AUBase class data
   ComponentResult result = AUBase::SaveState(plist);
   if (result != noErr)
      return result;
   if (!IsInitialized())
      return kAudioUnitErr_Uninitialized;

   // append raw chunk data
   // TODO It is here that we can find memory leaks!!!

   CFMutableDictionaryRef dict = (CFMutableDictionaryRef)*plist;
   void* data;
   
   plugin_instance->populateDawExtraState();
   if( editor_instance )
       editor_instance->populateDawExtraState(plugin_instance);
   
   CFIndex size = plugin_instance->saveRaw(&data);
   CFDataRef dataref =
       CFDataCreateWithBytesNoCopy(NULL, (const UInt8*)data, size, kCFAllocatorNull);
   CFDictionarySetValue(dict, rawchunkname, dataref);
   CFRelease(dataref);

   return noErr;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::GetPresets(CFArrayRef* outData) const
{
   // kAudioUnitProperty_FactoryPresets

   // Type: CFArrayRef containing AUPreset's
   // Returns an array of AUPreset that contain a number and name for each of the presets.
   // The number of each preset must be greater (or equal to) zero, and the numbers need not be
   // ordered or contiguous. The name of each preset can be presented to the user as a means of
   // identifying each preset.
   // The CFArrayRef should be released by the caller.

   if (!IsInitialized())
      return kAudioUnitErr_Uninitialized;

   if (outData == NULL)
      return noErr;

   sub3_synth* s = (sub3_synth*)plugin_instance;

   UInt32 n_presets = presetOrderToPatchList.size();

   CFMutableArrayRef newArray =
       CFArrayCreateMutable(kCFAllocatorDefault, n_presets, &kCFAUPresetArrayCallBacks);
   if (newArray == NULL)
      return coreFoundationUnknownErr;

   for (long i = 0; i < n_presets; i++)
   {
      auto patch = s->storage.patch_list[presetOrderToPatchList[i]];
      std::string nm = s->storage.patch_category[patch.category].name + " / " + patch.name;
      CFAUPresetRef newPreset =
          CFAUPresetCreate(kCFAllocatorDefault, i,
                           CFStringCreateWithCString(NULL, nm.c_str(), kCFStringEncodingUTF8));
      if (newPreset != NULL)
      {
         CFArrayAppendValue(newArray, newPreset);
         CFAUPresetRelease(newPreset);
      }
   }

   *outData = (CFArrayRef)newArray;
   return noErr;
   // return coreFoundationUnknownErr; // this isn't OK so tell the host that
}

//----------------------------------------------------------------------------------------------------

OSStatus aulayer::NewFactoryPresetSet(const AUPreset& inNewFactoryPreset)
{
   if (!IsInitialized())
      return false;
   if (inNewFactoryPreset.presetNumber < 0)
      return false;
   SetAFactoryPresetAsCurrent( inNewFactoryPreset );
   sub3_synth* s = (sub3_synth*)plugin_instance;
   s->patchid_queue = presetOrderToPatchList[inNewFactoryPreset.presetNumber];
   s->processThreadunsafeOperations();
   return true;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::GetParameterList(AudioUnitScope inScope,
                                          AudioUnitParameterID* outParameterList,
                                          UInt32& outNumParameters)
{
   if ((inScope != kAudioUnitScope_Global) || !IsInitialized())
   {
      outNumParameters = 0;
      return noErr;
   }

   outNumParameters = n_total_params + num_metaparameters;
   if (outParameterList)
      memcpy(outParameterList, parameterIDlist, sizeof(AudioUnitParameterID) * ( n_total_params + num_metaparameters ));
   return noErr;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::GetParameterInfo(AudioUnitScope inScope,
                                          AudioUnitParameterID inParameterID,
                                          AudioUnitParameterInfo& outParameterInfo)
{
   // FIXME: I think this code can be a bit tighter and cleaner, but it works OK for now
   outParameterInfo.unit = kAudioUnitParameterUnit_Generic;
   outParameterInfo.minValue = 0.f;
   outParameterInfo.maxValue = 1.f;
   outParameterInfo.defaultValue = 0.f;

   outParameterInfo.flags = kAudioUnitParameterFlag_IsWritable | kAudioUnitParameterFlag_IsReadable;

   // kAudioUnitParameterFlag_IsGlobalMeta

   sprintf(outParameterInfo.name, "-");
   // outParameterInfo.unitName

   inParameterID = plugin_instance->remapExternalApiToInternalId(inParameterID);

   if (inScope != kAudioUnitScope_Global)
      return kAudioUnitErr_InvalidParameter;
   if (!IsInitialized())
      return kAudioUnitErr_Uninitialized; // return defaults

   plugin_instance->getParameterName(inParameterID, outParameterInfo.name);
   outParameterInfo.flags |= kAudioUnitParameterFlag_HasCFNameString |
                             kAudioUnitParameterFlag_CFNameRelease |
                             kAudioUnitParameterFlag_HasClump | kAudioUnitParameterFlag_HasName;
   outParameterInfo.cfNameString =
       CFStringCreateWithCString(NULL, outParameterInfo.name, kCFStringEncodingUTF8);

   parametermeta pm;
   plugin_instance->getParameterMeta(inParameterID, pm);
   outParameterInfo.minValue = pm.fmin;
   outParameterInfo.maxValue = pm.fmax;
   outParameterInfo.defaultValue = pm.fdefault;
   if (pm.expert)
      outParameterInfo.flags |= kAudioUnitParameterFlag_ExpertMode;
   if (pm.meta)
      outParameterInfo.flags |= kAudioUnitParameterFlag_IsGlobalMeta;

   outParameterInfo.clumpID = pm.clump;

   return noErr;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::GetParameter(AudioUnitParameterID inID,
                                      AudioUnitScope inScope,
                                      AudioUnitElement inElement,
                                      Float32& outValue)
{
   if (inScope != kAudioUnitScope_Global)
      return kAudioUnitErr_InvalidParameter;
   if (inID >= n_total_params + num_metaparameters)
      return kAudioUnitErr_InvalidParameter;
   if (!IsInitialized())
      return kAudioUnitErr_Uninitialized;
   outValue = plugin_instance->getParameter01(plugin_instance->remapExternalApiToInternalId(inID));
   return noErr;
}

//----------------------------------------------------------------------------------------------------

ComponentResult aulayer::SetParameter(AudioUnitParameterID inID,
                                      AudioUnitScope inScope,
                                      AudioUnitElement inElement,
                                      Float32 inValue,
                                      UInt32 inBufferOffsetInFrames)
{
   if (inScope != kAudioUnitScope_Global)
      return kAudioUnitErr_InvalidParameter;
   if (inID >= n_total_params + num_metaparameters)
      return kAudioUnitErr_InvalidParameter;
   if (!IsInitialized())
      return kAudioUnitErr_Uninitialized;
   plugin_instance->setParameter01(plugin_instance->remapExternalApiToInternalId(inID), inValue,
                                   true);
   return noErr;
}

//----------------------------------------------------------------------------------------------------

bool aulayer::CanScheduleParameters() const
{
   return false; // TODO add support
}

//----------------------------------------------------------------------------------------------------

bool aulayer::ParameterBeginEdit(int p)
{
   AudioUnitEvent event;
   event.mEventType = kAudioUnitEvent_BeginParameterChangeGesture;
   event.mArgument.mParameter.mAudioUnit = GetComponentInstance();
   event.mArgument.mParameter.mParameterID = plugin_instance->remapInternalToExternalApiId(p);
   event.mArgument.mParameter.mScope = kAudioUnitScope_Global;
   event.mArgument.mParameter.mElement = 0;
   AUEventListenerNotify(NULL, NULL, &event);
   return true;
}

//----------------------------------------------------------------------------------------------------

bool aulayer::ParameterEndEdit(int p)
{
   AudioUnitEvent event;
   event.mEventType = kAudioUnitEvent_EndParameterChangeGesture;
   event.mArgument.mParameter.mAudioUnit = GetComponentInstance();
   event.mArgument.mParameter.mParameterID = plugin_instance->remapInternalToExternalApiId(p);
   event.mArgument.mParameter.mScope = kAudioUnitScope_Global;
   event.mArgument.mParameter.mElement = 0;
   AUEventListenerNotify(NULL, NULL, &event);
   return true;
}

//----------------------------------------------------------------------------------------------------

bool aulayer::ParameterUpdate(int p)
{
   AudioUnitEvent event;
   event.mEventType = kAudioUnitEvent_ParameterValueChange;
   event.mArgument.mParameter.mAudioUnit = GetComponentInstance();
   event.mArgument.mParameter.mParameterID = plugin_instance->remapInternalToExternalApiId(p);
   event.mArgument.mParameter.mScope = kAudioUnitScope_Global;
   event.mArgument.mParameter.mElement = 0;
   AUEventListenerNotify(NULL, NULL, &event);
   return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUMIDIBase::HandleMidiEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus
aulayer::HandleMidiEvent(UInt8 status, UInt8 channel, UInt8 data1, UInt8 data2, UInt32 inStartFrame)
{
   if (!IsInitialized())
      return kAudioUnitErr_Uninitialized;

   if (events_this_block >= 1024)
      return -1; // buffer full

   eventbuffer[events_this_block].status = status;
   eventbuffer[events_this_block].channel = channel;
   eventbuffer[events_this_block].data1 = data1;
   eventbuffer[events_this_block].data2 = data2;
   eventbuffer[events_this_block].inStartFrame = inStartFrame;

   events_this_block++;

   return noErr;
}

AUDIOCOMPONENT_ENTRY(AUMusicDeviceFactory, aulayer);
