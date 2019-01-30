//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "SurgeStorage.h"
#include "DspUtilities.h"

#if WINDOWS
#include <windows.h>
#include <assert.h>
#include <mmreg.h>
#endif

void error_msg(char* c)
{} // add messagebox?

#define uint32 unsigned int
#define int32 int

using namespace std;

void SurgeStorage::load_wt_wav(string filename, Wavetable* wt)
{
#if WINDOWS
   uint32 wave_channels = 0;
   uint32 wave_samplerate = 0;
   uint32 wave_blockalignment = 0;
   uint32 wave_bitdepth = 0;
   uint32 wave_length = 0;

   HMMIO hmmio;

   /* Open the file for reading with buffered I/O. Let Windows use its default internal buffer */
   hmmio = mmioOpen((LPSTR)filename.c_str(), NULL, MMIO_READ | MMIO_ALLOCBUF);
   if (!hmmio)
   {
      char msg[256];
      sprintf(msg, "file io error: File [%s] not found!", filename);
      error_msg(msg);
      mmioClose(hmmio, 0);
      return;
   }

   MMCKINFO mmckinfoParent; /* for the Group Header */

   /* Tell Windows to locate a WAVE Group header somewhere in the file, and read it in.
   This marks the start of any embedded WAVE format within the file */
   mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
   if (mmioDescend(hmmio, (LPMMCKINFO)&mmckinfoParent, 0, MMIO_FINDRIFF))
   {
      /* Oops! No embedded WAVE format within this file */
      error_msg("file	io:	This file doesn't contain a WAVE!");
      mmioClose(hmmio, 0);
      return;
   }

   MMCKINFO mmckinfoSubchunk; /* for finding chunks within the Group */

   /* Tell Windows to locate the WAVE's "fmt " chunk and read in its header */
   mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
   {
      /* Oops! The required fmt chunk was not found! */
      error_msg("file	io:	Required fmt chunk was not found!");
      mmioClose(hmmio, 0);
      return;
   }

   // store to the position of the ftm-chunk for seeking later
   int startpos = mmckinfoSubchunk.dwDataOffset - 8;

   WAVEFORMATEXTENSIBLE waveFormat; /* for reading a fmt chunk's data */

   /* Tell Windows to read in the "fmt " chunk into a WAVEFORMATEX structure */

   // if (mmioRead(hmmio, (HPSTR)&waveFormat, mmckinfoSubchunk.cksize) !=
   // (LRESULT)mmckinfoSubchunk.cksize)
   if (mmioRead(hmmio, (HPSTR)&waveFormat, sizeof(WAVEFORMATEXTENSIBLE)) !=
       (LRESULT)sizeof(WAVEFORMATEXTENSIBLE))
   {
      /* Oops! */
      error_msg("file	io: error reading the fmt chunk!");
      mmioClose(hmmio, 0);
      return;
   }

   /* check that the file is in a format we can read, abort otherwise */

   if ((waveFormat.Format.nChannels != 1) && (waveFormat.Format.nChannels != 2))
   {
      error_msg("file	io: only mono & stereo-files are supported!");
      mmioClose(hmmio, 0);
      return;
   }
   if (!((waveFormat.Format.wFormatTag == WAVE_FORMAT_PCM) ||
         (waveFormat.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)))
   {
      error_msg(
          "file	io: only WAVE_FORMAT_PCM and WAVE_FORMAT_IEEE_FLOAT wave-files are supported!");
      mmioClose(hmmio, 0);
      return;
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
      error_msg("file	io: Data chunk not found!");
      mmioClose(hmmio, 0);
      return;
   }

   /* Get the size of the data chunk (ie, the size of the waveform data) */
   int32 WaveDataSize = mmckinfoSubchunk.cksize;
   int32 WaveDataSamples =
       8 * WaveDataSize / (waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels);

   /* load sample */
   /* Tell Windows to read in the "data" chunk into a buffer */
   unsigned char* loaddata = (unsigned char*)malloc(WaveDataSize);
   if (!loaddata)
   {
      error_msg("file	io: could not allocate memory!");
      mmioClose(hmmio, 0);
      return;
   }

   if (mmioRead(hmmio, (HPSTR)loaddata, mmckinfoSubchunk.cksize) !=
       (LRESULT)mmckinfoSubchunk.cksize)
   {
      /* Oops! */
      error_msg("file	io: error reading the data chunk!");
      mmioClose(hmmio, 0);
      free(loaddata);
      return;
   }

   // this->inst_present = false;
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

                   error_msg("file	io: error reading the inst chunk!");
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

   bool smpl_present = false;
   bool loop_present = false;

   SampleLoop smpl_loop;
   SamplerChunk smpl_chunk;

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
         error_msg("file	io: error reading the smpl chunk!");
         mmioClose(hmmio, 0);
         return;
      }

      if (smpl_chunk.cSampleLoops > 0)
      {
         loop_present = true;
         mmioRead(hmmio, (HPSTR)&smpl_loop, sizeof(smpl_loop));
         // Dandruff's weird wavetable bug seems to be related to this?

         smpl_loop.dwEnd++; // We want the loop end point to be the first sample AFTER the loop
      }
      else
      {
         loop_present = false;
      }
   }

   wt_header wh;
   memset(&wh, 0, sizeof(wt_header));
   wh.flags = wtf_is_sample;
   int sh = 9;
   if (smpl_present && loop_present)
   {
      int looplength = smpl_loop.dwEnd - smpl_loop.dwStart;
      wh.flags = 0;
      switch (looplength)
      {
      case 1024:
         sh = 10;
         break;
      case 512:
         sh = 9;
         break;
      case 256:
         sh = 8;
         break;
      case 128:
         sh = 7;
         break;
      case 64:
         sh = 6;
         break;
      case 32:
         sh = 5;
         break;
      case 16:
         sh = 4;
         break;
      case 8:
         sh = 3;
         break;
      case 4:
         sh = 2;
         break;
      case 2:
         sh = 1;
         break;
      default:
         wh.flags = wtf_is_sample;
         break;
      }
   }
   wh.n_samples = 1 << sh;
   int mask = wt->size - 1;

   int sample_length = min(WaveDataSamples, max_wtable_size * max_subtables);

   wh.n_tables = min(max_subtables, (sample_length >> sh));

   int channels = 1;
   // float *sample_data = &wt->table[0][0];

   if ((waveFormat.Format.wFormatTag == WAVE_FORMAT_PCM) &&
       (waveFormat.Format.wBitsPerSample == 16))
   {
      assert(wh.n_samples * wh.n_tables * 2 <= WaveDataSize);
      wh.flags |= wtf_int16;
   }
   else if ((waveFormat.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) &&
            (waveFormat.Format.wBitsPerSample == 32))
   {
      assert(wh.n_samples * wh.n_tables * 4 <= WaveDataSize);
   }
   else
      goto abort;

   CS_WaveTableData.enter();

   wt->BuildWT(loaddata, wh, wh.flags & wtf_is_sample);

   CS_WaveTableData.leave();

abort:

   free(loaddata);

   /* Close the file */
   mmioClose(hmmio, 0);
#else
   // FIXME: Implement WAV file loading for macOS and Linux.
   fprintf(stderr, "%s: WAV file loading is not implemented.\n", __func__);
#endif
}
