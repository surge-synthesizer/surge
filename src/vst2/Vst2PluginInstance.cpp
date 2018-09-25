//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "globals.h"
#include "Vst2PluginInstance.h"
#include "SurgeSynthesizer.h"
#include "SurgeGUIEditor.h"
//#include "sub3_editor2.h"
#include <float.h>
#include <public.sdk/source/vst2.x/aeffeditor.h>

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

//-------------------------------------------------------------------------------------------------------
Vst2PluginInstance::Vst2PluginInstance(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, 1, n_total_params + n_customcontrollers)
{
   setNumInputs(2);  // stereo in
   setNumOutputs(2); // stereo out

#if MAC
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
   plugin_instance = NULL;
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
   if (plugin_instance)
   {
      plugin_instance->~SurgeSynthesizer();
      _aligned_free(plugin_instance);
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
      plugin_instance->all_notes_off();
      plugin_instance->audio_processing_active = false;
   }
}

VstInt32 Vst2PluginInstance::stopProcess()
{
   if (initialized)
      plugin_instance->all_notes_off();
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
   plugin_instance = (SurgeSynthesizer*)synth;

   plugin_instance->set_samplerate(this->getSampleRate());
   editor = new SurgeGUIEditor(this, plugin_instance);

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

   plugin_instance->set_samplerate(this->getSampleRate());
   plugin_instance->audio_processing_active = true;

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

         char* dst = eventbufferdata + of;

         of += size;

         if (of > EVENTBUFFER_SIZE)
         {
            events_this_block = i;
            break;
         }

         memcpy(dst, inEvent, size);
         eventptr[i] = (VstEvent*)dst;
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
      int status = midiData[0] & 0xf0;      // ignoring channel
      int channel = midiData[0] & 0x0f;     // force to channel 1
      if (status == 0x90 || status == 0x80) // we only look at notes
      {
         int newnote = midiData[1] & 0x7f;
         int velo = midiData[2] & 0x7f;
         if ((status == 0x80) || (velo == 0))
         {
            plugin_instance->release_note((char)channel, (char)newnote, (char)velo);
         }
         else
         {
            plugin_instance->play_note((char)channel, (char)newnote, (char)velo, event->detune);
         }
      }
      else if (status == 0xe0) // pitch bend
      {
         long value = (midiData[1] & 0x7f) + ((midiData[2] & 0x7f) << 7);
         plugin_instance->pitch_bend((char)channel, value - 8192);
      }
      else if (status == 0xB0) // controller
      {
         if (midiData[1] == 0x7b || midiData[1] == 0x7e)
            plugin_instance->all_notes_off(); // all notes off
         else
            plugin_instance->channel_controller((char)channel, midiData[1] & 0x7f,
                                                midiData[2] & 0x7f);
      }
      else if (status == 0xC0) // program change
      {
         plugin_instance->program_change((char)channel, midiData[1] & 0x7f);
      }
      else if (status == 0xD0) // channel aftertouch
      {
         plugin_instance->channel_aftertouch((char)channel, midiData[1] & 0x7f);
      }
      else if (status == 0xA0) // poly aftertouch
      {
         plugin_instance->poly_aftertouch((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
      }
      else if ((status == 0xfc) || (status == 0xff)) //  MIDI STOP or reset
      {
         plugin_instance->all_notes_off();
      }
   }
   else if (ev->type == kVstSysExType)
   {
      VstMidiSysexEvent* event = (VstMidiSysexEvent*)ev;
      plugin_instance->sysex(event->dumpBytes, (unsigned char*)event->sysexDump);
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

   plugin_instance->setParameter01(plugin_instance->remapExternalApiToInternalId(index), value,
                                   true);

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
   return plugin_instance->getParameter01(plugin_instance->remapExternalApiToInternalId(index));
}

void Vst2PluginInstance::getParameterName(VstInt32 index, char* label)
{
   if (!initialized)
      init();
   plugin_instance->getParameterName(plugin_instance->remapExternalApiToInternalId(index), label);
}

void Vst2PluginInstance::getParameterDisplay(VstInt32 index, char* text)
{
   if (!initialized)
      init();
   plugin_instance->getParameterDisplay(plugin_instance->remapExternalApiToInternalId(index), text);
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

   SurgeSynthesizer* s = (SurgeSynthesizer*)plugin_instance;
   s->process_input = (!plug_is_synth || input_connected);

   // do each buffer
   VstTimeInfo* timeinfo = getTimeInfo(kVstPpqPosValid | kVstTempoValid | kVstTransportPlaying);
   if (timeinfo)
   {
      if (timeinfo->flags & kVstTempoValid)
         plugin_instance->time_data.tempo = timeinfo->tempo;
      if (timeinfo->flags & kVstTransportPlaying)
      {
         if (timeinfo->flags & kVstPpqPosValid)
            plugin_instance->time_data.ppqPos = timeinfo->ppqPos;
      }
   }
   else
   {
      plugin_instance->time_data.tempo = 120;
   }

   int i;
   int n_outputs = plugin_instance->get_n_outputs();
   int n_inputs = plugin_instance->get_n_inputs();
   for (i = 0; i < sampleFrames; i++)
   {
      if (blockpos == 0)
      {
         // move clock
         timedata* td = &(plugin_instance->time_data);
         plugin_instance->time_data.ppqPos +=
             (double)block_size * plugin_instance->time_data.tempo / (60. * sampleRate);

         // process events for the current block
         while (events_processed < events_this_block)
         {
            if (i >= eventptr[events_processed]->deltaFrames)
            {
               handleEvent(eventptr[events_processed]);
               events_processed++;
            }
            else
               break;
         }

         // run sampler engine for the current block
         plugin_instance->process();
      }

      if (s->process_input)
      {
         int inp;
         for (inp = 0; inp < n_inputs; inp++)
         {
            plugin_instance->input[inp][blockpos] = inputs[inp][i];
         }
      }

      int outp;
      for (outp = 0; outp < n_outputs; outp++)
      {
         if (replacing)
            outputs[outp][i] = (float)plugin_instance->output[outp][blockpos]; // replacing
         else
            outputs[outp][i] += (float)plugin_instance->output[outp][blockpos]; // adding
      }

      blockpos++;
      if (blockpos >= block_size)
         blockpos = 0;
   }

   // process out old events that didn't finish last block
   while (events_processed < events_this_block)
   {
      handleEvent(eventptr[events_processed]);
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

   if (plugin_instance)
      plugin_instance->set_samplerate(sampleRate);
}

const int dummydata = 'OMED';
VstInt32 Vst2PluginInstance::getChunk(void** data, bool isPreset)
{
   if (!initialized)
      init();

   return plugin_instance->save_raw(data);
   //#endif
}

VstInt32 Vst2PluginInstance::setChunk(void* data, VstInt32 byteSize, bool isPreset)
{
   if (!initialized)
      init();

   if (byteSize <= 4)
      return 0;

   plugin_instance->load_raw(data, byteSize, false);

   return 1;
}
