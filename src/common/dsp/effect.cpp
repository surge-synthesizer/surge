#include "effect_defs.h"

baseeffect* spawn_effect(int id, sub3_storage* storage, sub3_fx* fxdata, pdata* pd)
{
   baseeffect* t = 0;
   switch (id)
   {
   case fxt_delay:
      t = (baseeffect*)_aligned_malloc(sizeof(dualdelay), 16);
      new (t) dualdelay(storage, fxdata, pd);
      break;
   case fxt_eq:
      t = (baseeffect*)_aligned_malloc(sizeof(eq3band), 16);
      new (t) eq3band(storage, fxdata, pd);
      break;
   case fxt_phaser:
      t = (baseeffect*)_aligned_malloc(sizeof(phaser), 16);
      new (t) phaser(storage, fxdata, pd);
      break;
   case fxt_rotaryspeaker:
      t = (baseeffect*)_aligned_malloc(sizeof(rotary_speaker), 16);
      new (t) rotary_speaker(storage, fxdata, pd);
      break;
   case fxt_distortion:
      t = (baseeffect*)_aligned_malloc(sizeof(distortion), 16);
      new (t) distortion(storage, fxdata, pd);
      break;
   case fxt_reverb:
      t = (baseeffect*)_aligned_malloc(sizeof(reverb1), 16);
      new (t) reverb1(storage, fxdata, pd);
      break;
   case fxt_reverb2:
      t = (baseeffect*)_aligned_malloc(sizeof(reverb2), 16);
      new (t) reverb2(storage, fxdata, pd);
      break;
   case fxt_freqshift:
      t = (baseeffect*)_aligned_malloc(sizeof(freqshift), 16);
      new (t) freqshift(storage, fxdata, pd);
      break;
   case fxt_conditioner:
      t = (baseeffect*)_aligned_malloc(sizeof(conditioner), 16);
      new (t) conditioner(storage, fxdata, pd);
      break;
   case fxt_chorus4:
      t = (baseeffect*)_aligned_malloc(sizeof(chorus<4>), 16);
      new (t) chorus<4>(storage, fxdata, pd);
      break;
   case fxt_vocoder:
      t = (baseeffect*)_aligned_malloc(sizeof(vocoder), 16);
      new (t) vocoder(storage, fxdata, pd);
      break;
   /*case fxt_emphasize:
           t = (baseeffect*) _aligned_malloc(sizeof(emphasize),16);
           new(t) emphasize(storage,fxdata,pd);
           break;*/
   default:
      t = 0;
      break;
   };
   return t;
}

baseeffect::baseeffect(sub3_storage* storage, sub3_fx* fxdata, pdata* pd)
{
   assert(storage);
   this->fxdata = fxdata;
   this->storage = storage;
   this->pd = pd;
   ringout = 10000000;
   for (int i = 0; i < n_fx_params; i++)
      f[i] = &pd[fxdata->p[i].id].f;
}

bool baseeffect::process_ringout(float* dataL, float* dataR, bool indata_present)
{
   if (indata_present)
      ringout = 0;
   else
      ringout++;

   int d = get_ringout_decay();
   if ((d < 0) || (ringout < d) || (ringout == 0))
   {
      process(dataL, dataR);
      return true;
   }
   else
      process_only_control();
   return false;
}

void baseeffect::init_ctrltypes()
{
   for (int j = 0; j < n_fx_params; j++)
   {
      fxdata->p[j].modulateable = true;
      fxdata->p[j].set_type(ct_none);
   }
}

/* eq3band			*/

eq3band::eq3band(sub3_storage* storage, sub3_fx* fxdata, pdata* pd)
    : baseeffect(storage, fxdata, pd), band1(storage), band2(storage), band3(storage)
{
   gain.set_blocksize(block_size);

   band1.setBlockSize(block_size * slowrate); // spelar ingen roll eftersom de är lagbaserade
   band2.setBlockSize(block_size * slowrate);
   band3.setBlockSize(block_size * slowrate);
}

eq3band::~eq3band()
{}

void eq3band::init()
{
   setvars(true);
   band1.suspend();
   band2.suspend();
   band3.suspend();
   bi = 0;
}

void eq3band::setvars(bool init)
{
   if (init)
   {
      gain.set_target(1.f); // db_to_linear(fxdata->p[9].val.f));
      gain.instantize();
      band1.coeff_peakEQ(band1.calc_omega(fxdata->p[1].val.f * (1.f / 12.f)), fxdata->p[2].val.f,
                         1.f); // sätt banden till 0dB så fades eqn in
      band2.coeff_peakEQ(band2.calc_omega(fxdata->p[4].val.f * (1.f / 12.f)), fxdata->p[5].val.f,
                         1.f);
      band3.coeff_peakEQ(band3.calc_omega(fxdata->p[7].val.f * (1.f / 12.f)), fxdata->p[8].val.f,
                         1.f);
      band1.coeff_instantize();
      band2.coeff_instantize();
      band3.coeff_instantize();
   }
   else
   {
      band1.coeff_peakEQ(band1.calc_omega(*f[1] * (1.f / 12.f)), *f[2], *f[0]);
      band2.coeff_peakEQ(band2.calc_omega(*f[4] * (1.f / 12.f)), *f[5], *f[3]);
      band3.coeff_peakEQ(band3.calc_omega(*f[7] * (1.f / 12.f)), *f[8], *f[6]);
   }
}

void eq3band::process(float* dataL, float* dataR)
{
   if (bi == 0)
      setvars(false);
   bi = (bi + 1) & slowrate_m1;

   band1.process_block(dataL, dataR);
   band2.process_block(dataL, dataR);
   band3.process_block(dataL, dataR);

   gain.set_target_smoothed(db_to_linear(*f[9]));
   gain.multiply_2_blocks(dataL, dataR, block_size_quad);
}

void eq3band::suspend()
{
   init();
}

const char* eq3band::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Band 1";
   case 1:
      return "Band 2";
   case 2:
      return "Band 3";
   case 3:
      return "Output";
   }
   return 0;
}
int eq3band::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 9;
   case 2:
      return 17;
   case 3:
      return 25;
   }
   return 0;
}

void eq3band::init_ctrltypes()
{
   baseeffect::init_ctrltypes();

   fxdata->p[0].set_name("L Gain");
   fxdata->p[0].set_type(ct_decibel);
   fxdata->p[1].set_name("L Freq");
   fxdata->p[1].set_type(ct_freq_audible);
   fxdata->p[2].set_name("L BW");
   fxdata->p[2].set_type(ct_bandwidth);

   fxdata->p[3].set_name("M Gain");
   fxdata->p[3].set_type(ct_decibel);
   fxdata->p[4].set_name("M Freq");
   fxdata->p[4].set_type(ct_freq_audible);
   fxdata->p[5].set_name("M BW");
   fxdata->p[5].set_type(ct_bandwidth);

   fxdata->p[6].set_name("H Gain");
   fxdata->p[6].set_type(ct_decibel);
   fxdata->p[7].set_name("H Freq");
   fxdata->p[7].set_type(ct_freq_audible);
   fxdata->p[8].set_name("H BW");
   fxdata->p[8].set_type(ct_bandwidth);

   fxdata->p[9].set_name("Gain");
   fxdata->p[9].set_type(ct_decibel);

   for (int i = 0; i < 10; i++)
      fxdata->p[i].posy_offset = 1 + 2 * (i / 3);
}
void eq3band::init_default_values()
{
   fxdata->p[0].val.f = 0.f;
   fxdata->p[1].val.f = -2.5f * 12.f;
   fxdata->p[2].val.f = 2.f;

   fxdata->p[3].val.f = 0.f;
   fxdata->p[4].val.f = 0.5f * 12.f;
   fxdata->p[5].val.f = 2.f;

   fxdata->p[6].val.f = 0.f;
   fxdata->p[7].val.f = 4.5f * 12.f;
   fxdata->p[8].val.f = 2.f;

   fxdata->p[9].val.f = 0.f;
}

/* chorus			*/

template <int v>
chorus<v>::chorus(sub3_storage* storage, sub3_fx* fxdata, pdata* pd)
    : baseeffect(storage, fxdata, pd), lp(storage), hp(storage)
{
   mix.set_blocksize(block_size);
   feedback.set_blocksize(block_size);
}

template <int v> chorus<v>::~chorus()
{}

template <int v> void chorus<v>::init()
{
   memset(buffer, 0, (max_delay_length + FIRipol_N) * sizeof(float));
   wpos = 0;
   envf = 0;
   const float gainscale = 1 / sqrt((float)v);

   for (int i = 0; i < v; i++)
   {
      time[i].setRate(0.001);
      float x = i;
      x /= (float)(v - 1);
      lfophase[i] = x;
      x = 2.f * x - 1.f;
      voicepan[i][0] = sqrt(0.5 - 0.5 * x) * gainscale;
      voicepan[i][1] = sqrt(0.5 + 0.5 * x) * gainscale;
#if PPC

#else
      voicepanL4[i] = _mm_set1_ps(voicepan[i][0]);
      voicepanR4[i] = _mm_set1_ps(voicepan[i][1]);
#endif
   }

   setvars(true);
}

template <int v> void chorus<v>::setvars(bool init)
{
   if (init)
   {
      feedback.set_target(0.5f * amp_to_linear(fxdata->p[3].val.f));
      hp.coeff_HP(hp.calc_omega(fxdata->p[4].val.f / 12.0), 0.707);
      lp.coeff_LP2B(lp.calc_omega(fxdata->p[5].val.f / 12.0), 0.707);
      mix.set_target(fxdata->p[6].val.f);
      width.set_target(db_to_linear(fxdata->p[7].val.f));
   }
   else
   {
      feedback.set_target_smoothed(0.5f * amp_to_linear(*f[3]));
      float rate =
          envelope_rate_linear(-*f[1]) * (fxdata->p[1].temposync ? storage->temposyncratio : 1.f);
      float tm =
          note_to_pitch(12 * *f[0]) * (fxdata->p[0].temposync ? storage->temposyncratio_inv : 1.f);
      for (int i = 0; i < v; i++)
      {
         lfophase[i] += rate;
         if (lfophase[i] > 1)
            lfophase[i] -= 1;
         // float lfoout = 0.5*lookup_waveshape_warp(3,4.f*lfophase[i]-2.f) * *f[2];
         float lfoout = (2.f * fabs(2.f * lfophase[i] - 1.f) - 1.f) * *f[2];
         time[i].newValue(samplerate * tm * (1 + lfoout));
      }

      hp.coeff_HP(hp.calc_omega(*f[4] * (1.f / 12.f)), 0.707);
      lp.coeff_LP2B(lp.calc_omega(*f[5] * (1.f / 12.f)), 0.707);
      mix.set_target_smoothed(*f[6]);
      width.set_target_smoothed(db_to_linear(*f[7]));
   }
}

template <int v> void chorus<v>::process(float* dataL, float* dataR)
{
   setvars(false);

   _MM_ALIGN16 float tbufferL[block_size];
   _MM_ALIGN16 float tbufferR[block_size];
   _MM_ALIGN16 float fbblock[block_size];
   int k;

   clear_block(tbufferL, block_size_quad);
   clear_block(tbufferR, block_size_quad);
#if PPC
   for (k = 0; k < block_size; k++)
   {
      tbufferL[k] = 0;
      tbufferR[k] = 0;
      for (int j = 0; j < v; j++)
      {
         time[j].process();
         float vtime = time[j].v;
         int i_dtime = max(block_size, min((int)vtime, max_delay_length - FIRipol_N - 1));
         int rp = ((wpos - i_dtime + k) - FIRipol_N) & (max_delay_length - 1);
         int sinc = FIRipol_N *
                    limit_range((int)(FIRipol_M * (float(i_dtime + 1) - vtime)), 0, FIRipol_M - 1);

         float vo = 0.f;
         for (int i = 0; i < FIRipol_N; i++)
         {
            vo += buffer[(rp - i) & (max_delay_length - 1)] * sinctable1X[sinc + FIRipol_N - i - 1];
         }
         tbufferL[k] += vo * voicepan[j][0];
         tbufferR[k] += vo * voicepan[j][1];
      }
   }
#else
   for (k = 0; k < block_size; k++)
   {
      __m128 L = _mm_setzero_ps(), R = _mm_setzero_ps();

      for (int j = 0; j < v; j++)
      {
         time[j].process();
         float vtime = time[j].v;
         int i_dtime = max(block_size, min((int)vtime, max_delay_length - FIRipol_N - 1));
         int rp = ((wpos - i_dtime + k) - FIRipol_N) & (max_delay_length - 1);
         int sinc = FIRipol_N *
                    limit_range((int)(FIRipol_M * (float(i_dtime + 1) - vtime)), 0, FIRipol_M - 1);

         __m128 vo;
         vo = _mm_mul_ps(_mm_load_ps(&sinctable1X[sinc]), _mm_loadu_ps(&buffer[rp]));
         vo = _mm_add_ps(
             vo, _mm_mul_ps(_mm_load_ps(&sinctable1X[sinc + 4]), _mm_loadu_ps(&buffer[rp + 4])));
         vo = _mm_add_ps(
             vo, _mm_mul_ps(_mm_load_ps(&sinctable1X[sinc + 8]), _mm_loadu_ps(&buffer[rp + 8])));

         L = _mm_add_ps(L, _mm_mul_ps(vo, voicepanL4[j]));
         R = _mm_add_ps(R, _mm_mul_ps(vo, voicepanR4[j]));
      }
      L = sum_ps_to_ss(L);
      R = sum_ps_to_ss(R);
      _mm_store_ss(&tbufferL[k], L);
      _mm_store_ss(&tbufferR[k], R);
   }
#endif

   lp.process_block(tbufferL, tbufferR);
   hp.process_block(tbufferL, tbufferR);
   add_block(tbufferL, tbufferR, fbblock, block_size_quad);
   feedback.multiply_block(fbblock, block_size_quad);
   hardclip_block(fbblock, block_size_quad);
   accumulate_block(dataL, fbblock, block_size_quad);
   accumulate_block(dataR, fbblock, block_size_quad);

   if (wpos + block_size >= max_delay_length)
   {
      for (k = 0; k < block_size; k++)
      {
         buffer[(wpos + k) & (max_delay_length - 1)] = fbblock[k];
      }
   }
   else
   {
      /*for(k=0; k<block_size; k++)
      {
              buffer[wpos+k] = fbblock[k];
      }*/
      copy_block(fbblock, &buffer[wpos], block_size_quad);
   }

   if (wpos == 0)
      for (k = 0; k < FIRipol_N; k++)
         buffer[k + max_delay_length] = buffer[k]; // copy buffer so FIR-core doesn't have to wrap

   // scale width
   _MM_ALIGN16 float M[block_size], S[block_size];
   encodeMS(tbufferL, tbufferR, M, S, block_size_quad);
   width.multiply_block(S, block_size_quad);
   decodeMS(M, S, tbufferL, tbufferR, block_size_quad);

   mix.fade_2_blocks_to(dataL, tbufferL, dataR, tbufferR, dataL, dataR, block_size_quad);

   wpos += block_size;
   wpos = wpos & (max_delay_length - 1);
}

template <int v> void chorus<v>::suspend()
{
   init();
}

template <int v> void chorus<v>::init_ctrltypes()
{
   baseeffect::init_ctrltypes();

   fxdata->p[0].set_name("Time");
   fxdata->p[0].set_type(ct_delaymodtime);
   fxdata->p[1].set_name("Rate");
   fxdata->p[1].set_type(ct_lforate);
   fxdata->p[2].set_name("Depth");
   fxdata->p[2].set_type(ct_percent);
   fxdata->p[3].set_name("Feedback");
   fxdata->p[3].set_type(ct_amplitude);
   fxdata->p[4].set_name("Low Cut");
   fxdata->p[4].set_type(ct_freq_audible);
   fxdata->p[5].set_name("High Cut");
   fxdata->p[5].set_type(ct_freq_audible);
   fxdata->p[6].set_name("Mix");
   fxdata->p[6].set_type(ct_percent);
   fxdata->p[7].set_name("Width");
   fxdata->p[7].set_type(ct_decibel_narrow);

   fxdata->p[0].posy_offset = 1;
   fxdata->p[1].posy_offset = 3;
   fxdata->p[2].posy_offset = 3;
   fxdata->p[3].posy_offset = 5;
   fxdata->p[4].posy_offset = 7;
   fxdata->p[5].posy_offset = 7;
   fxdata->p[6].posy_offset = 9;
   fxdata->p[7].posy_offset = 9;
}
template <int v> const char* chorus<v>::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Delay";
   case 1:
      return "Modulation";
   case 2:
      return "Feedback";
   case 3:
      return "EQ";
   case 4:
      return "Output";
   }
   return 0;
}
template <int v> int chorus<v>::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 5;
   case 2:
      return 11;
   case 3:
      return 15;
   case 4:
      return 21;
   }
   return 0;
}

template <int v> void chorus<v>::init_default_values()
{
   fxdata->p[0].val.f = -6.f;
   fxdata->p[1].val.f = -2.f;
   fxdata->p[2].val.f = 0.3f;
   fxdata->p[3].val.f = 0.5f;
   fxdata->p[4].val.f = -3.f * 12.f;
   fxdata->p[5].val.f = 3.f * 12.f;
   fxdata->p[6].val.f = 1.f;
   fxdata->p[7].val.f = 0.f;
}
