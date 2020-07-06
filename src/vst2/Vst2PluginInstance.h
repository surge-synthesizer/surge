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

#include "public.sdk/source/vst2.x/audioeffectx.h"
#include "SurgeSynthesizer.h"
#include <util/FpuState.h>

class SurgeGUIEditor;

//-------------------------------------------------------------------------------------------------------
class Vst2PluginInstance : public AudioEffectX
{
public:
   enum
   {
      MAX_EVENTS = 1024,
      EVENTBUFFER_SIZE = 65536,
   };
   Vst2PluginInstance(audioMasterCallback audioMaster);
   ~Vst2PluginInstance();

   // Processes
   template <bool replacing> void processT(float** inputs, float** outputs, VstInt32 sampleFrames);
   virtual void process(float** inputs, float** outputs, VstInt32 sampleFrames);
   virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
   virtual VstInt32 processEvents(VstEvents* ev);

   // Host->Client calls
   virtual bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text);
   virtual void setProgramName(char* name);
   virtual void getProgramName(char* name);
   /*virtual bool getProgramNameIndexed (long category, long index, char *text);
   virtual void setProgram (long program);
   virtual long getProgram ();*/

   virtual void open();
   virtual void close();

   virtual void resume();
   virtual void suspend();
   virtual VstInt32 stopProcess();
   virtual void setParameter(VstInt32 index, float value);
   virtual float getParameter(VstInt32 index);
   virtual void getParameterLabel(VstInt32 index, char* label);
   virtual void getParameterDisplay(VstInt32 index, char* text);
   virtual void getParameterName(VstInt32 index, char* text);
   // virtual bool getParameterProperties (long index, VstParameterProperties *p);
   virtual bool getEffectName(char* name);
   virtual bool getVendorString(char* text);
   virtual bool getProductString(char* text);
   // virtual VstInt32 getVendorVersion () { return 1000; }
   virtual VstInt32 canDo(char* text);
   virtual VstPlugCategory getPlugCategory();
   /*virtual bool hasMidiProgramsChanged (long channel);
   virtual long getMidiProgramName (long channel, MidiProgramName* midiProgramName);
   virtual long getCurrentMidiProgram (long channel, MidiProgramName* currentProgram);*/
   // virtual bool getMidiKeyName (long channel, MidiKeyName* keyName);
   // virtual bool getProgramNameIndexed (long category, long index, char *text);
   // virtual long getNumCategories ();

   /*virtual long getMidiProgramCategory(long channel,MidiProgramCategory *category);
   virtual bool getProgramNameIndexed (long category, long index, char *text);
   virtual long getNumCategories ();*/

   virtual void inputConnected(VstInt32 index, bool state);
   virtual VstInt32 vendorSpecific(VstInt32 lArg1, VstInt32 lArg2, void* ptrArg, float floatArg);
   virtual bool getInputProperties(VstInt32 index, VstPinProperties* properties);
   virtual bool getOutputProperties(VstInt32 index, VstPinProperties* properties);
   virtual void setSampleRate(float sampleRate);
   virtual VstInt32 getChunk(void** data, bool isPreset = false);
   virtual VstInt32 setChunk(void* data, VstInt32 byteSize, bool isPreset = false);

   virtual bool beginEdit( VstInt32 index );
   virtual bool endEdit( VstInt32 index );
   
   virtual VstInt32 getNumMidiInputChannels()
   {
      return 3;
   } ///< Return number of MIDI input channels
   virtual VstInt32 getNumMidiOutputChannels()
   {
      return 0;
   } ///< Return number of MIDI output channels

protected:
   // internal calls
   void handleEvent(VstEvent*);
   int oldblokkosize;
   int blockpos;

   void handleZoom(SurgeGUIEditor *e);
   
public:
   enum State {
      UNINITIALIZED  = 0,
      INITIALIZED    = 1,
      DEAD           = 2,
   };

   SurgeSynthesizer* _instance;
   VstEvent* _eventptr[MAX_EVENTS];
   char _eventbufferdata[EVENTBUFFER_SIZE];
   int events_this_block, events_processed;
   enum State state;
   char programName[32];
   bool plug_is_synth;
   int input_connected;
   FpuState _fpuState;

   bool tryInit();
};
