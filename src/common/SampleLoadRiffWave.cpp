//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------

#include "Sample.h"
#include "globals.h"
#include "SurgeStorage.h"
#include <assert.h>

#if WINDOWS

#include <windows.h>
#include <mmreg.h>

void write_log(char* c)
{}

bool Sample::load_riff_wave_mk2(const char* fname)
{
   assert(fname);

   unsigned int wave_channels = 0;
   unsigned int wave_samplerate = 0;
   unsigned int wave_blockalignment = 0;
   unsigned int wave_bitdepth = 0;
   unsigned int wave_length = 0;

   HMMIO hmmio;

   /* Open the file for reading with buffered I/O. Let Windows use its default internal buffer */
   hmmio = mmioOpen((LPSTR)filename, NULL, MMIO_READ | MMIO_ALLOCBUF);
   if (!hmmio)
   {
      char msg[256];
      sprintf(msg, "file io error: File [%s] not found!", fname);
      write_log(msg);
      mmioClose(hmmio, 0);
      return false;
   }

   MMCKINFO mmckinfoParent; /* for the Group Header */

   /* Tell Windows to locate a WAVE Group header somewhere in the file, and read it in.
   This marks the start of any embedded WAVE format within the file */
   mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
   if (mmioDescend(hmmio, (LPMMCKINFO)&mmckinfoParent, 0, MMIO_FINDRIFF))
   {
      /* Oops! No embedded WAVE format within this file */
      write_log("file	io:	This file doesn't contain a WAVE!");
      mmioClose(hmmio, 0);
      return false;
   }

   MMCKINFO mmckinfoSubchunk; /* for finding chunks within the Group */

   /* Tell Windows to locate the WAVE's "fmt " chunk and read in its header */
   mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
   {
      /* Oops! The required fmt chunk was not found! */
      write_log("file	io:	Required fmt chunk was not found!");
      mmioClose(hmmio, 0);
      return false;
   }

   // store to the position of the ftm-chunk for seeking later
   long startpos = mmckinfoSubchunk.dwDataOffset - 8;

   WAVEFORMATEXTENSIBLE waveFormat; /* for reading a fmt chunk's data */

   /* Tell Windows to read in the "fmt " chunk into a WAVEFORMATEX structure */
   if (mmioRead(hmmio, (HPSTR)&waveFormat, mmckinfoSubchunk.cksize) !=
       (LRESULT)mmckinfoSubchunk.cksize)
   {
      /* Oops! */
      write_log("file	io: error reading the fmt chunk!");
      mmioClose(hmmio, 0);
      return false;
   }

   /* check that the file is in a format we can read, abort otherwise */

   if ((waveFormat.Format.nChannels != 1) && (waveFormat.Format.nChannels != 2))
   {
      write_log("file	io: only mono & stereo-files are supported!");
      mmioClose(hmmio, 0);
      return false;
   }
   if (!((waveFormat.Format.wFormatTag == WAVE_FORMAT_PCM) ||
         (waveFormat.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)))
   {
      write_log(
          "file	io: only WAVE_FORMAT_PCM and WAVE_FORMAT_IEEE_FLOAT wave-files are supported!");
      mmioClose(hmmio, 0);
      return false;
   }

   /*	Ascend out of the "fmt " subchunk. If you plan to parse any other chunks in the file, you
   need to "ascend" out of any chunk that you've mmioDescend()'ed into */

   mmioAscend(hmmio, &mmckinfoSubchunk, 0);

   /*	Tell Windows to locate the data chunk. Upon return, the file
   pointer will be ready to read in the actual waveform data within
   the data chunk */
   mmckinfoSubchunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
   {
      /* Oops! */
      write_log("file	io: Data chunk not found!");
      mmioClose(hmmio, 0);
      return false;
   }

   /* Get the size of the data chunk (ie, the size of the waveform data) */
   int WaveDataSize = mmckinfoSubchunk.cksize;
   int WaveDataSamples =
       8 * WaveDataSize / (waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels);

   /* load sample */
   /* Tell Windows to read in the "data" chunk into a buffer */
   unsigned char* loaddata = (unsigned char*)malloc(WaveDataSize);
   if (!loaddata)
   {
      write_log("file	io: could not allocate memory!");
      mmioClose(hmmio, 0);
      return false;
   }

   if (mmioRead(hmmio, (HPSTR)loaddata, mmckinfoSubchunk.cksize) !=
       (LRESULT)mmckinfoSubchunk.cksize)
   {
      /* Oops! */
      write_log("file	io: error reading the data chunk!");
      mmioClose(hmmio, 0);
      free(loaddata);
      return false;
   }

   this->inst_present = false;
   /* does not seem to be in general use
   
   mmioAscend(hmmio, &mmckinfoSubchunk, 0);
   mmioSeek(hmmio,startpos,SEEK_SET);
   // read instrument chunk
   mmckinfoSubchunk.ckid = RIFFWAVE_inst;

   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
   {
           this->inst_present = false;
   } else {
           this->inst_present = true;
   
           if (mmioRead(hmmio, (HPSTR)&inst_tag, mmckinfoSubchunk.cksize) !=
   (LRESULT)mmckinfoSubchunk.cksize)
           {

                   write_log("file	io: error reading the inst chunk!");
                   mmioClose(hmmio, 0);
                   return false;
           }
   }	*/

   mmioAscend(hmmio, &mmckinfoSubchunk, 0);

   // seek to beginning of file since the smpl chunk could be both prior and past the data chunk
   mmioSeek(hmmio, startpos, SEEK_SET);

   // read sampler chunk
   mmckinfoSubchunk.ckid = mmioFOURCC('s', 'm', 'p', 'l');
   // mmckinfoSubchunk.dwDataOffset = 0;

   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
   {
      smpl_present = false;
   }
   else
   {
      smpl_present = true;

      if (mmioRead(hmmio, (HPSTR)&smpl_chunk, sizeof(smpl_chunk)) != (LRESULT)sizeof(smpl_chunk))
      {
         free(loaddata);
         write_log("file	io: error reading the smpl chunk!");
         mmioClose(hmmio, 0);
         return false;
      }

      if (smpl_chunk.cSampleLoops > 0)
      {
         loop_present = true;
         mmioRead(hmmio, (HPSTR)&smpl_loop, sizeof(smpl_loop));
         smpl_loop.dwEnd++; // SC wants the loop end point to be the first sample AFTER the loop
      }
      else
      {
         loop_present = false;
      }
   }

   /* if the read has been succesfull this far, we can delete the old sample (if it exists) and
    * replace it with a new one */

   if (sample_loaded)
      delete sample_data;

   // strcpy(this->filename, filename);

   this->sample_length = WaveDataSamples;
   this->sample_rate = waveFormat.Format.nSamplesPerSec;
   this->inv_sample_rate = 1.0f / this->sample_rate;
   //	this->sample_start = 0;
   this->channels = (unsigned char)waveFormat.Format.nChannels;

   unsigned int samplesizewithmargin = WaveDataSamples + 2 * FIRipol_N + block_size + FIRoffset;
   sample_data = new float[samplesizewithmargin];
   unsigned int i;
   for (i = 0; i < samplesizewithmargin; i++)
      sample_data[i] = 0.0f;

   this->sample_loaded = true;

   if (waveFormat.Format.wFormatTag == WAVE_FORMAT_PCM)
   {
      if (waveFormat.Format.wBitsPerSample == 8)
      {
         unsigned int i;
         for (i = 0; i < sample_length; i++)
         {
            int value = loaddata[(i * channels)] - 128;
            sample_data[i + FIRoffset] = 0.007813f * float(value);
         }
      }
      else if (waveFormat.Format.wBitsPerSample == 16)
      {
         unsigned int i;
         for (i = 0; i < sample_length; i++)
         {
            int value =
                (loaddata[((i * channels) << 1) + 1] << 8) + loaddata[((i * channels) << 1)];
            value -= (value & 0x8000) << 1;
            sample_data[i + FIRoffset] = 0.000030517578125f * float(value);
         }
      }
      else if (waveFormat.Format.wBitsPerSample == 24)
      {
         unsigned int i;
         {
            for (i = 0; i < sample_length; i++)
            {
               int* ld24 = (int*)&loaddata[3 * i];
               int value = (*ld24) & 0x00ffffff;
               // int value = (loaddata[(3*i*channels)+2] << 16) + (loaddata[(3*i*channels)+1] << 8)
               // + loaddata[(3*i*channels)];
               value -= (value & 0x800000) << 1;
               sample_data[i + FIRoffset] = 0.00000011920928955078f * float(value);
            }
         }
      }
      else if (waveFormat.Format.wBitsPerSample == 32)
      {
         unsigned int i;
         int* load32data = (int*)loaddata;
         for (i = 0; i < sample_length; i++)
         {
            int value = load32data[(i * channels)];
            sample_data[i + FIRoffset] = (4.6566128730772E-10f) * (float)value;
         }
      }
   }
   else if (waveFormat.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
   {
      if (waveFormat.Format.wBitsPerSample == 32)
      {
         unsigned int i;
         float* floatdata = (float*)loaddata;
         for (i = 0; i < sample_length; i++)
         {
            float value = floatdata[(i * channels)];
            sample_data[i + FIRoffset] = value;
         }
      }
   }

   free(loaddata);

   /* Close the file */
   mmioClose(hmmio, 0);
   return true;
}

#endif
