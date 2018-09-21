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

class AbstractSynthesizer
{
public:
   virtual void play_note(char channel, char key, char velocity, char detune) = 0;
   virtual void pitch_bend(char channel, int value) = 0;
   virtual void channel_aftertouch(char channel, int value)
   {}
   virtual void program_change(char channel, int value)
   {}
   virtual void poly_aftertouch(char channel, int key, int value)
   {}
   virtual void channel_controller(char channel, int cc, int value)
   {}
   virtual void release_note(char channel, char key, char velocity)
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
   virtual void all_notes_off() = 0;
   virtual void set_samplerate(float sr) = 0;
   virtual int get_n_inputs()
   {
      return n_inputs;
   }
   virtual int get_n_outputs()
   {
      return n_outputs;
   }
   virtual int get_block_size()
   {
      return block_size;
   }
   virtual void process() = 0;
   virtual void load_raw(const void* data, int size, bool preset = false) = 0;
   virtual unsigned int save_raw(void** data) = 0;

public:
   _MM_ALIGN16 float output[n_outputs][block_size];
   _MM_ALIGN16 float input[n_inputs][block_size];
   timedata time_data;
   bool audio_processing_active;
};