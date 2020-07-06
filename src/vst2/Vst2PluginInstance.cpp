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

#include "globals.h"
#include "Vst2PluginInstance.h"
#include "SurgeSynthesizer.h"
#include "SurgeGUIEditor.h"
#include "SurgeError.h"
//#include "sub3_editor2.h"
#include <float.h>
#include "public.sdk/source/vst2.x/aeffeditor.h"
#include "public.sdk/source/vst2.x/audioeffectx.h"
#include "version.h"

#if MAC
#include <fenv.h>
#include <AvailabilityMacros.h>
#pragma STDC FENV_ACCESS on
#endif

using namespace std;

#if LINUX
namespace VSTGUI { void* soHandle = nullptr; }
#endif

#include "UserInteractions.h"
#include "CScalableBitmap.h"

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
   setNumOutputs(6); // stereo out, scene A out, scene B out

#if MAC || LINUX
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

   state = UNINITIALIZED;
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
   delete _instance;
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
   if (state == INITIALIZED)
   {
      _instance->allNotesOff();
      _instance->audio_processing_active = false;
   }
}

VstInt32 Vst2PluginInstance::stopProcess()
{
   if (state == INITIALIZED)
      _instance->allNotesOff();
   return 1;
}

void Vst2PluginInstance::open()
{
   if (!tryInit())
      return;

   updateDisplay();
}

void Vst2PluginInstance::close()
{}

void Vst2PluginInstance::resume()
{
   if (_instance)
   {
       _instance->setSamplerate(this->getSampleRate());
       _instance->audio_processing_active = true;
   }

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

   if (!strcmp(text, "2in6out"))
      return 1;

   if (!strcmp(text, "plugAsChannelInsert"))
      return 1;

   if (!strcmp(text, "plugAsSend"))
      return 1;

   if (!strcmp(text, "MPE"))
      return 1;

   if (!strcmp(text, "sizeWindow"))
       return 1;

   return -1;
}

VstInt32 Vst2PluginInstance::processEvents(VstEvents* ev)
{
   if (state == INITIALIZED)
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
   if (state != INITIALIZED)
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
   getProgramNameIndexed(0, 0, name);
}
bool Vst2PluginInstance::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
   if (tryInit())
   {
       SurgeSynthesizer* s = (SurgeSynthesizer*)_instance;
       strncpy(text, s->storage.getPatch().name.c_str(), kVstMaxProgNameLen);
       text[kVstMaxProgNameLen - 1] = '\0';
   }
   return true;
}

void Vst2PluginInstance::setParameter(VstInt32 index, float value)
{
   if (!tryInit())
      return;

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
   if (!tryInit())
      return 0;

   return _instance->getParameter01(_instance->remapExternalApiToInternalId(index));
}

void Vst2PluginInstance::getParameterName(VstInt32 index, char* label)
{
   if (!tryInit())
      return;

   _instance->getParameterName(_instance->remapExternalApiToInternalId(index), label);
}

void Vst2PluginInstance::getParameterDisplay(VstInt32 index, char* text)
{
   if (!tryInit())
      return;

   _instance->getParameterDisplay(_instance->remapExternalApiToInternalId(index), text);
}

void Vst2PluginInstance::getParameterLabel(VstInt32 index, char* label)
{
   strcpy(label, "");
}

bool Vst2PluginInstance::getEffectName(char* name)
{
   strcpy(name, stringProductName);
   return true;
}

bool Vst2PluginInstance::getProductString(char* name)
{
   strcpy(name, stringProductName);
   return true;
}

bool Vst2PluginInstance::getVendorString(char* text)
{
   strcpy(text, stringCompanyName);
   return true;
}

template <bool replacing>
void Vst2PluginInstance::processT(float** inputs, float** outputs, VstInt32 sampleFrames)
{
   if (!tryInit())
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
      if (timeinfo->flags & kVstTimeSigValid )
      {
         _instance->time_data.timeSigNumerator = timeinfo->timeSigNumerator;
         _instance->time_data.timeSigDenominator = timeinfo->timeSigDenominator;
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
             (double)BLOCK_SIZE * _instance->time_data.tempo / (60. * sampleRate);

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
         {
            outputs[outp][i] = (float)_instance->output[outp][blockpos];
            outputs[2 + outp][i] = (float)_instance->sceneout[0][outp][blockpos];
            outputs[4 + outp][i] = (float)_instance->sceneout[1][outp][blockpos];
         }
         else  // adding
         {
            outputs[outp][i] += (float)_instance->output[outp][blockpos]; 
            outputs[2 + outp][i] += (float)_instance->sceneout[0][outp][blockpos];
            outputs[4 + outp][i] += (float)_instance->sceneout[1][outp][blockpos];
         }
      }

      blockpos++;
      if (blockpos >= BLOCK_SIZE)
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
   if (index < 6)
   {
   	  switch (index)
   	  {
   	     case 0:
            sprintf(properties->label, "Audio Out L");
            break;
   	     case 1:
            sprintf(properties->label, "Audio Out R");
            break;
   	     case 2:
            sprintf(properties->label, "Scene A Out L");
            break;
   	     case 3:
            sprintf(properties->label, "Scene A Out R");
            break;
   	     case 4:
            sprintf(properties->label, "Scene B Out L");
            break;
   	     case 5:
            sprintf(properties->label, "Scene B Out R");
            break;
   	  }
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
   	  if (index == 0)
         sprintf(properties->label, "Audio In L");
      else
         sprintf(properties->label, "Audio In R");
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
   if (!tryInit())
      return 0;

   _instance->populateDawExtraState();
   if( editor )
       ((SurgeGUIEditor *)editor)->populateDawExtraState(_instance);

   return _instance->saveRaw(data);
   //#endif
}

VstInt32 Vst2PluginInstance::setChunk(void* data, VstInt32 byteSize, bool isPreset)
{
   if (!tryInit())
      return 0;

   if (byteSize <= 4)
      return 0;

   _instance->loadRaw(data, byteSize, false);

   _instance->loadFromDawExtraState();
   if( editor )
       ((SurgeGUIEditor *)editor)->loadFromDAWExtraState(_instance);

   return 1;
}

bool Vst2PluginInstance::beginEdit( VstInt32 index )
{
   return AudioEffectX::beginEdit(
       SurgeGUIEditor::applyParameterOffset(_instance->remapExternalApiToInternalId(index)));
}

bool Vst2PluginInstance::endEdit( VstInt32 index )
{
   return AudioEffectX::endEdit(
       SurgeGUIEditor::applyParameterOffset(_instance->remapExternalApiToInternalId(index)));
}

bool Vst2PluginInstance::tryInit()
{
   char host[256], product[256];

   if (state == DEAD)
      return false;

   if (state == INITIALIZED)
      return true;

   getHostVendorString(host);
   getHostProductString(product);
   VstInt32 hostversion = getHostVendorVersion();

   try {
      std::unique_ptr<SurgeSynthesizer> synthTmp(new SurgeSynthesizer(this));
      synthTmp->setSamplerate(this->getSampleRate());

      std::unique_ptr<SurgeGUIEditor> editorTmp(new SurgeGUIEditor(this, synthTmp.get()));

      _instance = synthTmp.release();
      _instance->audio_processing_active = true;

      editor = editorTmp.release();
   } catch (std::bad_alloc err) {
      Surge::UserInteractions::promptError(err.what(), "Out of memory");
      state = DEAD;
      return false;
   } catch (Surge::Error err) {
      Surge::UserInteractions::promptError(err);
      state = DEAD;
      return false;
   }

   static_cast<SurgeGUIEditor *>(editor)->setZoomCallback( [this](SurgeGUIEditor *e) { handleZoom(e); } );

   blockpos = 0;
   state = INITIALIZED;
   return true;
}

void Vst2PluginInstance::handleZoom(SurgeGUIEditor *e)
{
    ERect *vr;
    float fzf = e->getZoomFactor() / 100.0;
    int newW = WINDOW_SIZE_X * fzf;
    int newH = WINDOW_SIZE_Y * fzf;
    sizeWindow( newW, newH );

    VSTGUI::CFrame *frame = e->getFrame();
    if(frame)
    {
        frame->setZoom( e->getZoomFactor() / 100.0 );
        /*
        ** cframe::setZoom uses prior size and integer math which means repeated resets drift
        ** look at the "oroginWidth" math around in CFrame::setZoom. So rather than let those
        ** drifts accumulate, just reset the size here since we know it
        */
        frame->setSize(newW, newH);

        /*
        ** VST2 has an error which is that the background bitmap doesn't get the frame transform
        ** applied. Simply look at cviewcontainer::drawBackgroundRect. So we have to force the background
        ** scale up using a backdoor API.
        */

        VSTGUI::CBitmap *bg = frame->getBackground();
        if(bg != NULL)
        {
            CScalableBitmap *sbm = dynamic_cast<CScalableBitmap *>(bg); // dynamic casts are gross but better safe
            if (sbm)
            {
               sbm->setExtraScaleFactor(e->getZoomFactor());
            }
        }

        frame->setDirty(true);
        frame->invalid();
    }
}
