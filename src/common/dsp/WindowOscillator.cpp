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

#include "Oscillator.h"
#if !MAC && !LINUX
#include <intrin.h>
#endif
#if LINUX
#include <stdint.h>
#endif

/* wt2 osc */

WindowOscillator::WindowOscillator(SurgeStorage* storage,
                                   OscillatorStorage* oscdata,
                                   pdata* localcopy)
   : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{}

void WindowOscillator::init(float pitch, bool is_display)
{
   memset(&Sub, 0, sizeof(Sub));

   ActiveSubOscs = limit_range(oscdata->p[6].val.i, 1, wt2_suboscs - 1);
   if (is_display)
      ActiveSubOscs = 1;

   float out_attenuation_inv = sqrt((float)ActiveSubOscs);
   OutAttenuation = 1.0f / (out_attenuation_inv * 16777216.f);

   if (ActiveSubOscs == 1)
   {
      DetuneBias = 1;
      DetuneOffset = 0;

      Sub.Gain[0][0] = 128;
      Sub.Gain[0][1] = 128; // unity gain
      Sub.Pos[0] = (storage->WindowWT.size << 16);
   }
   else
   {
      DetuneBias = (float)2.f / ((float)ActiveSubOscs - 1.f);
      DetuneOffset = -1.f;

      bool odd = ActiveSubOscs & 1;
      float mid = ActiveSubOscs * 0.5 - 0.5;
      int half = ActiveSubOscs >> 1;
      for (int i = 0; i < ActiveSubOscs; i++)
      {
         float d = fabs((float)i - mid) / mid;
         if (odd && (i >= half))
            d = -d;
         if (i & 1)
            d = -d;

         Sub.Gain[i][0] = limit_range((int)(float)(128.f * megapanL(d)), 0, 255);
         Sub.Gain[i][1] = limit_range((int)(float)(128.f * megapanR(d)), 0, 255);

         if (oscdata->retrigger.val.b)
            Sub.Pos[i] = (storage->WindowWT.size + ((storage->WindowWT.size * i) / ActiveSubOscs))
                         << 16;
         else
            Sub.Pos[i] = (storage->WindowWT.size + (rand() & (storage->WindowWT.size - 1))) << 16;
      }
   }

   hp.coeff_instantize();
   lp.coeff_instantize();

   hp.coeff_HP(hp.calc_omega(oscdata->p[3].val.f / 12.0 ) / OSC_OVERSAMPLING, 0.707);
   lp.coeff_LP2B(lp.calc_omega(oscdata->p[4].val.f / 12.0 ) / OSC_OVERSAMPLING, 0.707);

}

WindowOscillator::~WindowOscillator()
{}

void WindowOscillator::init_ctrltypes()
{
   oscdata->p[0].set_name("Morph");
   oscdata->p[0].set_type(ct_countedset_percent);
   oscdata->p[0].set_user_data(oscdata);

   oscdata->p[1].set_name("Formant");
   oscdata->p[1].set_type(ct_pitch);
   oscdata->p[2].set_name("Window");
   oscdata->p[2].set_type(ct_wt2window);

   oscdata->p[3].set_name("Low Cut");
   oscdata->p[3].set_type(ct_freq_audible_deactivatable);
   oscdata->p[4].set_name("High Cut");
   oscdata->p[4].set_type(ct_freq_audible_deactivatable);

   oscdata->p[5].set_name("Unison Detune");
   oscdata->p[5].set_type(ct_oscspread);
   oscdata->p[6].set_name("Unison Voices");
   oscdata->p[6].set_type(ct_osccountWT);
}
void WindowOscillator::init_default_values()
{
   oscdata->p[0].val.f = 0.0f;
   oscdata->p[1].val.f = 0.0f;
   oscdata->p[2].val.i = 0;

   oscdata->p[3].val.f = oscdata->p[3].val_min.f; // high cut at the bottom
   oscdata->p[3].deactivated = true;
   oscdata->p[4].val.f = oscdata->p[4].val_max.f; // low cut at the top
   oscdata->p[4].deactivated = true;

   oscdata->p[5].val.f = 0.2f;
   oscdata->p[6].val.i = 1;
}

inline unsigned int BigMULr16(unsigned int a, unsigned int b)
{
   // 64-bit unsigned multiply with right shift by 16 bits
#if _M_X64 && ! TARGET_RACK
   unsigned __int64 c = __emulu(a, b);
   return c >> 16;
#elif LINUX || TARGET_RACK
   uint64_t c = (uint64_t)a * (uint64_t)b;
   return c >> 16;
#else
   unsigned int result;
   __asm
   {
		mov eax,a
		mov ecx,b
		mul ecx
		shl edx, 16
		shr eax, 16
		or eax,edx
         // TODO: Fix return for GCC ASM
		mov result, eax
   }
   return result;
#endif
}

#if MAC || LINUX
inline bool _BitScanReverse(unsigned long* result, unsigned long bits)
{
   *result = __builtin_ctz(bits);
   return true;
}
#endif

void WindowOscillator::ProcessSubOscs(bool stereo, bool FM)
{
   const unsigned int M0Mask = 0x07f8;
   unsigned int SizeMask = (oscdata->wt.size << 16) - 1;
   unsigned int SizeMaskWin = (storage->WindowWT.size << 16) - 1;

   
   unsigned char Window = limit_range(oscdata->p[2].val.i, 0, 8);

   int Table = limit_range(
       (int)(float)(oscdata->wt.n_tables * localcopy[oscdata->p[0].param_id_in_scene].f), 0,
       (int)oscdata->wt.n_tables - 1);
   int FormantMul =
       (int)(float)(65536.f * storage->note_to_pitch_tuningctr(localcopy[oscdata->p[1].param_id_in_scene].f));

   // We can actually get input tables bigger than the convolution table
   int WindowVsWavePO2 = storage->WindowWT.size_po2 - oscdata->wt.size_po2;
   if( WindowVsWavePO2 < 0 )
   {
       FormantMul = std::max(FormantMul << -WindowVsWavePO2, 1);
   }
   else
   {
       FormantMul = std::max(FormantMul >> WindowVsWavePO2, 1);
   }

   {
      // SSE2 path
      for (int so = 0; so < ActiveSubOscs; so++)
      {
         unsigned int Pos = Sub.Pos[so];
         unsigned int RatioA = Sub.Ratio[so];
         if( FM )
            RatioA = Sub.FMRatio[so][0];
         
         unsigned int MipMapA = 0;
         unsigned int MipMapB = 0;
         if (Sub.Table[so] >= oscdata->wt.n_tables)
            Sub.Table[so] = Table;
         // TableID may not be valid anymore if a new wavetable is loaded

         unsigned long MSBpos;
         unsigned int bs = BigMULr16(RatioA, 3 * FormantMul);
         if (_BitScanReverse(&MSBpos, bs))
            MipMapB = limit_range((int)MSBpos - 17, 0, oscdata->wt.size_po2 - 1);

         if (_BitScanReverse(&MSBpos, 3 * RatioA))
            MipMapA = limit_range((int)MSBpos - 17, 0, storage->WindowWT.size_po2 - 1);

         short* WaveAdr = oscdata->wt.TableI16WeakPointers[MipMapB][Sub.Table[so]];
         short* WinAdr = storage->WindowWT.TableI16WeakPointers[MipMapA][Window];

         for (int i = 0; i < BLOCK_SIZE_OS; i++)
         {
            if( FM )
            {
               Pos += Sub.FMRatio[so][i];
            }
            else
            {
               Pos += RatioA;
            }
            if (Pos & ~SizeMaskWin)
            {
               Sub.FormantMul[so] = FormantMul;
               Sub.Table[so] = Table;
               WaveAdr = oscdata->wt.TableI16WeakPointers[MipMapB][Table];
               Pos = Pos & SizeMaskWin;
            }

            unsigned int WinPos = Pos >> (16 + MipMapA);
            unsigned int WinSPos = (Pos >> (8 + MipMapA)) & 0xFF;

            unsigned int FPos = BigMULr16(Sub.FormantMul[so], Pos) & SizeMask;

            unsigned int MPos = FPos >> (16 + MipMapB);
            unsigned int MSPos = ((FPos >> (8 + MipMapB)) & 0xFF);

            __m128i Wave = _mm_madd_epi16(_mm_load_si128(((__m128i*)sinctableI16 + MSPos)),
                                          _mm_loadu_si128((__m128i*)&WaveAdr[MPos]));

            __m128i Win = _mm_madd_epi16(_mm_load_si128(((__m128i*)sinctableI16 + WinSPos)),
                                         _mm_loadu_si128((__m128i*)&WinAdr[WinPos]));

            // Sum
            int iWin alignas(16)[4],
                iWave alignas(16)[4];
#if MAC
            // this should be very fast on C2D/C1D (and there are no macs with K8's)
            iWin[0] = _mm_cvtsi128_si32(Win);
            iWin[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Win, _MM_SHUFFLE(1, 1, 1, 1)));
            iWin[2] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Win, _MM_SHUFFLE(2, 2, 2, 2)));
            iWin[3] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Win, _MM_SHUFFLE(3, 3, 3, 3)));
            iWave[0] = _mm_cvtsi128_si32(Wave);
            iWave[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Wave, _MM_SHUFFLE(1, 1, 1, 1)));
            iWave[2] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Wave, _MM_SHUFFLE(2, 2, 2, 2)));
            iWave[3] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Wave, _MM_SHUFFLE(3, 3, 3, 3)));
#else
            _mm_store_si128((__m128i*)&iWin, Win);
            _mm_store_si128((__m128i*)&iWave, Wave);
#endif

            iWin[0] = (iWin[0] + iWin[1] + iWin[2] + iWin[3]) >> 13;
            iWave[0] = (iWave[0] + iWave[1] + iWave[2] + iWave[3]) >> 13;
            if (stereo)
            {
               int Out = (iWin[0] * iWave[0]) >> 7;
               IOutputL[i] += (Out * (int)Sub.Gain[so][0]) >> 6;
               IOutputR[i] += (Out * (int)Sub.Gain[so][1]) >> 6;
            }
            else
               IOutputL[i] += (iWin[0] * iWave[0]) >> 6;
         }
         Sub.Pos[so] = Pos;
      }
   }
}

void WindowOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   memset(IOutputL, 0, BLOCK_SIZE_OS * sizeof(int));
   if (stereo)
      memset(IOutputR, 0, BLOCK_SIZE_OS * sizeof(int));

   float Detune;
   if( oscdata->p[5].absolute )
   {
      // See comment in SurgeSuperOscillator
      Detune = oscdata->p[5].get_extended(localcopy[oscdata->p[5].param_id_in_scene].f)
         * storage->note_to_pitch_inv_ignoring_tuning( std::min( 148.f, pitch ) ) * 16 / 0.9443;
   }
   else
   {
      Detune = oscdata->p[5].get_extended(localcopy[oscdata->p[5].param_id_in_scene].f);
   }
   float fmstrength = 32 * M_PI * fmdepth * fmdepth * fmdepth;
   for (int l = 0; l < ActiveSubOscs; l++)
   {
      Sub.DriftLFO[l][0] = drift_noise(Sub.DriftLFO[l][1]);
      /*
      ** This original code uses note 57 as a center point with a frequency of 220.
      */
      
      float f = storage->note_to_pitch(pitch + drift * Sub.DriftLFO[l][0] +
                                       Detune * (DetuneOffset + DetuneBias * (float)l));
      int Ratio = Float2Int(8.175798915f * 32768.f * f * (float)(storage->WindowWT.size) *
                            samplerate_inv); // (65536.f*0.5f), 0.5 for oversampling

      Sub.Ratio[l] = Ratio;
      if( FM )
      {
         FMdepth[l].newValue( fmstrength );
         for( int i=0; i<BLOCK_SIZE_OS; ++i )
         {
            float fmadj = ( 1.0 + FMdepth[l].v * master_osc[i] );
            float f = storage->note_to_pitch(pitch + drift * Sub.DriftLFO[l][0] +
                                             Detune * (DetuneOffset + DetuneBias * (float)l));
            int Ratio = Float2Int(8.175798915f * 32768.f * f * fmadj * (float)(storage->WindowWT.size) *
                                  samplerate_inv); // (65536.f*0.5f), 0.5 for oversampling
            Sub.FMRatio[l][i] = Ratio;
            FMdepth[l].process();
         }
      }
   }

   ProcessSubOscs(stereo, FM);

   // int32 -> float conversion
   __m128 scale = _mm_load1_ps(&OutAttenuation);
   {
      // SSE2 path
      if (stereo)
      {
         for (int i = 0; i < BLOCK_SIZE_OS; i += 4)
         {
            _mm_store_ps(&output[i], _mm_mul_ps(_mm_cvtepi32_ps(*(__m128i*)&IOutputL[i]), scale));
            _mm_store_ps(&outputR[i], _mm_mul_ps(_mm_cvtepi32_ps(*(__m128i*)&IOutputR[i]), scale));
         }
      }
      else
      {
         for (int i = 0; i < BLOCK_SIZE_OS; i += 4)
         {
            _mm_store_ps(&output[i], _mm_mul_ps(_mm_cvtepi32_ps(*(__m128i*)&IOutputL[i]), scale));
         }
      }
   }

   applyFilter();
}

void WindowOscillator::applyFilter()
{
   if( ! oscdata->p[3].deactivated )
      hp.coeff_HP(hp.calc_omega(localcopy[oscdata->p[3].param_id_in_scene].f / 12.0 ) / OSC_OVERSAMPLING , 0.707);
   if( ! oscdata->p[4].deactivated )
      lp.coeff_LP2B(lp.calc_omega(localcopy[oscdata->p[4].param_id_in_scene].f / 12.0 ) / OSC_OVERSAMPLING, 0.707);

   for (int k = 0; k < BLOCK_SIZE_OS; k+= BLOCK_SIZE )
   {
      if( ! oscdata->p[3].deactivated )
         hp.process_block( &(output[k]), &(outputR[k]) );
      if( ! oscdata->p[4].deactivated )
         lp.process_block( &(output[k]), &(outputR[k]) );
   }
}


void WindowOscillator::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if( streamingRevision <= 12 )
   {
      oscdata->p[3].val.f = oscdata->p[3].val_min.f; // high cut at the bottom
      oscdata->p[3].deactivated = true;
      oscdata->p[4].val.f = oscdata->p[4].val_max.f; // low cut at the top
      oscdata->p[4].deactivated = true;
      oscdata->p[1].set_type(ct_osc_feedback);
   }
}
