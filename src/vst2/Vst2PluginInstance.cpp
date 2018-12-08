//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "globals.h"
#include "Vst2PluginInstance.h"
#include "SurgeSynthesizer.h"
#include "SurgeGUIEditor.h"
//#include "sub3_editor2.h"
#include <float.h>
#include "public.sdk/source/vst2.x/aeffeditor.h"
#include "public.sdk/source/vst2.x/audioeffectx.h"

#if PPC
// For Carbon, use CoreServices.h instead
#include <CoreServices/CoreServices.h>
#else
#include "CpuArchitecture.h"
#endif

#if MAC
#include <fenv.h>
#include <AvailabilityMacros.h>
#pragma STDC FENV_ACCESS on
#endif

using namespace std;

//-------------------------------------------------------------------------------------------------------

AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
   return new Vst2PluginInstance(audioMaster);
}

AEffect* VSTPluginMain(audioMasterCallback audioMaster)
{
   // Get VST Version of the Host
   if (!audioMaster(0, audioMasterVersion, 0, 0, 0, 0))
      return 0; // old version

   // Create the AudioEffect
   AudioEffect* effect = createEffectInstance(audioMaster);
   if (!effect)
      return 0;

   // Return the VST AEffect structure
   return effect->getAeffect();
}

#if WIN32
#include <windows.h>
void* hInstance;

extern "C" {
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpvReserved)
{
   hInstance = hInst;
   return 1;
}
} // extern "C"
#elif __linux__
namespace VSTGUI { void* soHandle = nullptr; }
#endif

//-------------------------------------------------------------------------------------------------------
Vst2PluginInstance::Vst2PluginInstance(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, 1, n_total_params + n_customcontrollers)
{
   setNumInputs(2);  // stereo in
   setNumOutputs(2); // stereo out

#if MAC || __linux__
   isSynth();
   plug_is_synth = true;
   setUniqueID('cjs3'); // identify
#else
   char path[1024];
   extern void* hInstance;
   GetModuleFileName((HMODULE)hInstance, path, 1024);
   _strlwr(path);

   if (strstr(path, "_fx") || strstr(path, "_FX"))
   {
      plug_is_synth = false;
      setUniqueID('cjsx'); // identify
   }
   else
   {
      isSynth();
      plug_is_synth = true;
      setUniqueID('cjs3'); // identify
   }
#endif

   input_connected = 3;

   programsAreChunks(true);

   canProcessReplacing(); // supports both accumulating and replacing output

   strcpy(programName, "Default"); // default program name

   initialized = false;
   // NULL objects
   _instance = NULL;
   events_this_block = 0;
   events_processed = 0;
   oldblokkosize = 0;
}

VstPlugCategory Vst2PluginInstance::getPlugCategory()
{
   if (plug_is_synth)
      return kPlugCategSynth;
   else
      return kPlugCategEffect;
}

void Vst2PluginInstance::inputConnected(VstInt32 index, bool state)
{
   input_connected = (input_connected & (~(1 << index))) | (state ? (1 << index) : 0);
}

Vst2PluginInstance::~Vst2PluginInstance()
{
   // delete objects
   if (_instance)
   {
      _instance->~SurgeSynthesizer();
      _aligned_free(_instance);
   }
}

VstInt32
Vst2PluginInstance::vendorSpecific(VstInt32 lArg1, VstInt32 lArg2, void* ptrArg, float floatArg)
{
   if (lArg1 == 'MPEC')
   {
      if (lArg2 == 7)
      {
         strcpy((char*)ptrArg, "Gain");
         return 1;
      }
      else if (lArg2 == 10)
      {
         strcpy((char*)ptrArg, "Pan");
         return 1;
      }
      else if (lArg2 == 74)
      {
         strcpy((char*)ptrArg, "Timbre");
         return 1;
      }

      return 0;
   }
   else if (editor && lArg1 == 'stCA' && lArg2 == 'Whee')
   {
      // support mouse wheel!
      return editor->onWheel(floatArg) == true ? 1 : 0;
   }
   else
   {
      return AudioEffectX::vendorSpecific(lArg1, lArg2, ptrArg, floatArg);
   }
}

void Vst2PluginInstance::suspend()
{
   if (initialized)
   {
      _instance->allNotesOff();
      _instance->audio_processing_active = false;
   }
}

VstInt32 Vst2PluginInstance::stopProcess()
{
   if (initialized)
      _instance->allNotesOff();
   return 1;
}
void Vst2PluginInstance::init()
{
   char host[256], product[256];

   getHostVendorString(host);
   getHostProductString(product);
   VstInt32 hostversion = getHostVendorVersion();

   SurgeSynthesizer* synth = (SurgeSynthesizer*)_aligned_malloc(sizeof(SurgeSynthesizer), 16);
   if (!synth)
   {
      // out of memory
#if WIN32
      MessageBox(::GetActiveWindow(), "Could not allocate memory.", "Out of memory",
                 MB_OK | MB_ICONERROR);
#endif

      return;
   }
   new (synth) SurgeSynthesizer(this);
   _instance = (SurgeSynthesizer*)synth;

   _instance->setSamplerate(this->getSampleRate());
   editor = new SurgeGUIEditor(this, _instance);

   blockpos = 0;
   initialized = true;
   // numPrograms = ((sub3_synth*)plugin_instance)->storage.patch_list.size();
}

void Vst2PluginInstance::open()
{
   if (!initialized)
   {
      init();
      updateDisplay();
   }
}

void Vst2PluginInstance::close()
{}

void Vst2PluginInstance::resume()
{

   _instance->setSamplerate(this->getSampleRate());
   _instance->audio_processing_active = true;

   //	wantEvents ();
   AudioEffectX::resume();
}

VstInt32 Vst2PluginInstance::canDo(char* text)
{
   if (!strcmp(text, "receiveVstEvents"))
      return 1;

   if (!strcmp(text, "receiveVstMidiEvent"))
      return 1;

   if (!strcmp(text, "receiveVstTimeInfo"))
      return 1;

   if (!strcmp(text, "receiveVstSysexEvent"))
      return 1;

   if (!strcmp(text, "midiProgramNames"))
      return -1;

   if (!strcmp(text, "2in2out"))
      return 1;

   if (!strcmp(text, "plugAsChannelInsert"))
      return 1;

   if (!strcmp(text, "plugAsSend"))
      return 1;

   if (!strcmp(text, "MPE"))
      return 1;

   return -1;
}

VstInt32 Vst2PluginInstance::processEvents(VstEvents* ev)
{
   if (initialized)
   {
      events_this_block = min<int>((int)(ev->numEvents), MAX_EVENTS);
      events_processed = 0;

      int of = 0;
      int i;
      for (i = 0; i < this->events_this_block; i++)
      {
         VstEvent* inEvent = (ev->events[i]);
         size_t size = inEvent->byteSize + 2 * sizeof(VstInt32);

         char* dst = _eventbufferdata + of;

         of += size;

         if (of > EVENTBUFFER_SIZE)
         {
            events_this_block = i;
            break;
         }

         memcpy(dst, inEvent, size);
         _eventptr[i] = (VstEvent*)dst;
      }
   }
   return 1; // want more
}

void Vst2PluginInstance::handleEvent(VstEvent* ev)
{
   if (!initialized)
      return;

   if (ev->type == kVstMidiType)
   {
      VstMidiEvent* event = (VstMidiEvent*)ev;
      char* midiData = event->midiData;
      int status = midiData[0] & 0xf0;
      int channel = midiData[0] & 0x0f;
      if (status == 0x90 || status == 0x80) // we only look at notes
      {
         int newnote = midiData[1] & 0x7f;
         int velo = midiData[2] & 0x7f;
         if ((status == 0x80) || (velo == 0))
         {
            _instance->releaseNote((char)channel, (char)newnote, (char)velo);
         }
         else
         {
            _instance->playNote((char)channel, (char)newnote, (char)velo, event->detune);
         }
      }
      else if (status == 0xe0) // pitch bend
      {
         long value = (midiData[1] & 0x7f) + ((midiData[2] & 0x7f) << 7);
         _instance->pitchBend((char)channel, value - 8192);
      }
      else if (status == 0xB0) // controller
      {
         if (midiData[1] == 0x7b || midiData[1] == 0x7e)
            _instance->allNotesOff(); // all notes off
         else
            _instance->channelController((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
      }
      else if (status == 0xC0) // program change
      {
         _instance->programChange((char)channel, midiData[1] & 0x7f);
      }
      else if (status == 0xD0) // channel aftertouch
      {
         _instance->channelAftertouch((char)channel, midiData[1] & 0x7f);
      }
      else if (status == 0xA0) // poly aftertouch
      {
         _instance->polyAftertouch((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
      }
      else if ((status == 0xfc) || (status == 0xff)) //  MIDI STOP or reset
      {
         _instance->allNotesOff();
      }
   }
}

void Vst2PluginInstance::setProgramName(char* name)
{
   strcpy(programName, name);
}

void Vst2PluginInstance::getProgramName(char* name)
{
   strcpy(name, programName);
}

void Vst2PluginInstance::setParameter(VstInt32 index, float value)
{
   if (!initialized)
      init();

   _instance->setParameter01(_instance->remapExternalApiToInternalId(index), value, true);

   // flyttat till sub3_synth
   /*if(editor)
   {
           VstInt32 index_remapped = ((sub3_synth*)plugin_instance)->remap_param_id(index);
           ((AEffGUIEditor*)editor)->setParameter(index_remapped,value);
   }*/
}

float Vst2PluginInstance::getParameter(VstInt32 index)
{
   if (!initialized)
      init();
   return _instance->getParameter01(_instance->remapExternalApiToInternalId(index));
}

void Vst2PluginInstance::getParameterName(VstInt32 index, char* label)
{
   if (!initialized)
      init();
   _instance->getParameterName(_instance->remapExternalApiToInternalId(index), label);
}

void Vst2PluginInstance::getParameterDisplay(VstInt32 index, char* text)
{
   if (!initialized)
      init();
   _instance->getParameterDisplay(_instance->remapExternalApiToInternalId(index), text);
}

void Vst2PluginInstance::getParameterLabel(VstInt32 index, char* label)
{
   strcpy(label, "");
}

bool Vst2PluginInstance::getEffectName(char* name)
{
   strcpy(name, "Surge");
   return true;
}

bool Vst2PluginInstance::getProductString(char* name)
{
   strcpy(name, "Surge");
   return true;
}

bool Vst2PluginInstance::getVendorString(char* text)
{
   strcpy(text, "Vember Audio");
   return true;
}

template <bool replacing>
void Vst2PluginInstance::processT(float** inputs, float** outputs, VstInt32 sampleFrames)
{
   if (!initialized)
      return;

   _fpuState.set();

   SurgeSynthesizer* s = (SurgeSynthesizer*)_instance;
   s->process_input = (!plug_is_synth || input_connected);

   // do each buffer
   VstTimeInfo* timeinfo = getTimeInfo(kVstPpqPosValid | kVstTempoValid | kVstTransportPlaying);
   if (timeinfo)
   {
      if (timeinfo->flags & kVstTempoValid)
         _instance->time_data.tempo = timeinfo->tempo;
      if (timeinfo->flags & kVstTransportPlaying)
      {
         if (timeinfo->flags & kVstPpqPosValid)
            _instance->time_data.ppqPos = timeinfo->ppqPos;
      }
   }
   else
   {
      _instance->time_data.tempo = 120;
   }

   int i;
   int n_outputs = _instance->getNumOutputs();
   int n_inputs = _instance->getNumInputs();
   for (i = 0; i < sampleFrames; i++)
   {
      if (blockpos == 0)
      {
         // move clock
         timedata* td = &(_instance->time_data);
         _instance->time_data.ppqPos +=
             (double)block_size * _instance->time_data.tempo / (60. * sampleRate);

         // process events for the current block
         while (events_processed < events_this_block)
         {
            if (i >= _eventptr[events_processed]->deltaFrames)
            {
               handleEvent(_eventptr[events_processed]);
               events_processed++;
            }
            else
               break;
         }

         // run sampler engine for the current block
         _instance->process();
      }

      if (s->process_input)
      {
         int inp;
         for (inp = 0; inp < n_inputs; inp++)
         {
            _instance->input[inp][blockpos] = inputs[inp][i];
         }
      }

      int outp;
      for (outp = 0; outp < n_outputs; outp++)
      {
         if (replacing)
            outputs[outp][i] = (float)_instance->output[outp][blockpos]; // replacing
         else
            outputs[outp][i] += (float)_instance->output[outp][blockpos]; // adding
      }

      blockpos++;
      if (blockpos >= block_size)
         blockpos = 0;
   }

   // process out old events that didn't finish last block
   while (events_processed < events_this_block)
   {
      handleEvent(_eventptr[events_processed]);
      events_processed++;
   }

   _fpuState.restore();
}

void Vst2PluginInstance::process(float** inputs, float** outputs, VstInt32 sampleFrames)
{
   processT<false>(inputs, outputs, sampleFrames);
}

void Vst2PluginInstance::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
   processT<true>(inputs, outputs, sampleFrames);
}

bool Vst2PluginInstance::getOutputProperties(VstInt32 index, VstPinProperties* properties)
{
   if (index < 2)
   {
      sprintf(properties->label, "SURGE"); // , (index & 0xfe) + 1, (index & 0xfe) + 2);
      properties->flags = kVstPinIsStereo | kVstPinIsActive;
      return true;
   }
   return false;
}

bool Vst2PluginInstance::getInputProperties(VstInt32 index, VstPinProperties* properties)
{
   bool returnCode = false;
   if (index < 2)
   {
      sprintf(properties->label, "SURGE in");
      properties->flags = kVstPinIsStereo | kVstPinIsActive;
      returnCode = true;
   }
   return returnCode;
}

void Vst2PluginInstance::setSampleRate(float sampleRate)
{
   AudioEffectX::setSampleRate(sampleRate);

   if (_instance)
      _instance->setSamplerate(sampleRate);
}

const int dummydata = 'OMED';
VstInt32 Vst2PluginInstance::getChunk(void** data, bool isPreset)
{
   if (!initialized)
      init();

   return _instance->saveRaw(data);
   //#endif
}

VstInt32 Vst2PluginInstance::setChunk(void* data, VstInt32 byteSize, bool isPreset)
{
   if (!initialized)
      init();

   if (byteSize <= 4)
      return 0;

   _instance->loadRaw(data, byteSize, false);

   return 1;
}
