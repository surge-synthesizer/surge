//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#pragma once

#if MAC || LINUX
#else
#include <windows.h>
#include <mmreg.h>
#endif

#if WINDOWS

#define SamplerID 'smpl' /* chunk ID for Sampler Chunk */

typedef struct
{
   long dwIdentifier;
   long dwType;
   long dwStart;
   long dwEnd;
   long dwFraction;
   long dwPlayCount;
} SampleLoop;

typedef struct
{
   // int32             chunkID;
   // long           chunkSize;

   long dwManufacturer;
   long dwProduct;
   long dwSamplePeriod;
   long dwMIDIUnityNote;
   long dwMIDIPitchFraction;
   long dwSMPTEFormat;
   long dwSMPTEOffset;
   long cSampleLoops;
   long cbSamplerData;
   // SampleLoop Loops[];
} SamplerChunk;

/* end steal */

class Sample
{
public:
   /// constructor
   Sample();
   /// deconstructor
   virtual ~Sample();
   bool load(const char* filename);

private:
   bool load_riff_wave_mk2(const char* filename);

public:
   // public data
   float* sample_data;
   int channels;
   int sample_length;
   int sample_rate;
   float inv_sample_rate;
   char filename[256];

   void remember()
   {
      refcount++;
   }
   bool forget()
   {
      refcount--;
      return (!refcount);
   }
   // meta data

   // tag_s_RIFFWAVE_inst inst_tag;
   SamplerChunk smpl_chunk;
   SampleLoop smpl_loop;

   bool inst_present, smpl_present, loop_present;

private:
   bool sample_loaded;
   unsigned int refcount;
};

#endif
