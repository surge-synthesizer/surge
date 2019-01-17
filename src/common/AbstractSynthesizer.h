//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "globals.h"

struct timedata
{
   double ppqPos, tempo;
};

struct parametermeta
{
   float fmin, fmax, fdefault;
   unsigned int flags, clump;
   bool hide, expert, meta;
};

const int n_outputs = 2;
const int n_inputs = 2;

void initDllGlobals();

class alignas(16) AbstractSynthesizer
{
public:
   virtual void playNote(char channel, char key, char velocity, char detune) = 0;
   virtual void pitchBend(char channel, int value) = 0;
   virtual void channelAftertouch(char channel, int value)
   {}
   virtual void programChange(char channel, int value)
   {}
   virtual void polyAftertouch(char channel, int key, int value)
   {}
   virtual void channelController(char channel, int cc, int value)
   {}
   virtual void releaseNote(char channel, char key, char velocity)
   {}
   virtual bool
   setParameter01(long index, float value, bool external = false, bool force_integer = false) = 0;
   virtual float getParameter01(long index) = 0;
   virtual int remapExternalApiToInternalId(unsigned int x)
   {
      return x;
   }
   virtual void getParameterDisplay(long index, char* text) = 0;
   virtual void getParameterDisplay(long index, char* text, float x) = 0;
   virtual void getParameterName(long index, char* text) = 0;
   virtual void getParameterMeta(long index, parametermeta& pm) = 0;
   virtual void allNotesOff() = 0;
   virtual void setSamplerate(float sr) = 0;
   virtual int getNumInputs()
   {
      return n_inputs;
   }
   virtual int getNumOutputs()
   {
      return n_outputs;
   }
   virtual int getBlockSize()
   {
      return BLOCK_SIZE;
   }
   virtual void process() = 0;
   virtual void loadRaw(const void* data, int size, bool preset = false) = 0;
   virtual unsigned int saveRaw(void** data) = 0;

public:
   float output alignas(16)[n_outputs][BLOCK_SIZE];
   float input alignas(16)[n_inputs][BLOCK_SIZE];
   timedata time_data;
   bool audio_processing_active;
};
