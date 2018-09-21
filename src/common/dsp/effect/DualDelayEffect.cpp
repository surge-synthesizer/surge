#include "effect_defs.h"

/* dualdelay			*/

DualDelayEffect::DualDelayEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), timeL(0.0001), timeR(0.0001), lp(storage), hp(storage)
{
   mix.set_blocksize(block_size);
   pan.set_blocksize(block_size);
   feedback.set_blocksize(block_size);
   crossfeed.set_blocksize(block_size);
}

DualDelayEffect::~DualDelayEffect()
{}

void DualDelayEffect::init()
{
   memset(buffer[0], 0, (max_delay_length + FIRipol_N) * sizeof(float));
   memset(buffer[1], 0, (max_delay_length + FIRipol_N) * sizeof(float));
   wpos = 0;
   lfophase = 0;
   ringout_time = 100000;
   envf = 0;
   LFOval = 0;
   LFOdirection = true;
   lp.suspend();
   hp.suspend();
   setvars(true);
}

void DualDelayEffect::setvars(bool init)
{
   float fb = amp_to_linear(*f[2]);
   float cf = amp_to_linear(*f[3]);

   feedback.set_target_smoothed(fb);
   crossfeed.set_target_smoothed(cf);

   float lforate =
       envelope_rate_linear(-*f[6]) * (fxdata->p[6].temposync ? storage->temposyncratio : 1.f);
   lfophase += lforate;
   if (lfophase > 0.5)
   {
      lfophase -= 1;
      LFOdirection = !LFOdirection;
   }

   float lfo_increment = (0.00000000001f + powf(2, *f[7] * (1.f / 12.f)) - 1.f) * block_size;
   // small bias to avoid denormals

   const float ca = 0.99f;
   if (LFOdirection)
      LFOval = ca * LFOval + lfo_increment;
   else
      LFOval = ca * LFOval - lfo_increment;

   if (init)
   {
      timeL.newValue(samplerate * ((fxdata->p[0].temposync ? storage->temposyncratio_inv : 1.f) *
                                   note_to_pitch(12 * fxdata->p[0].val.f)) +
                     LFOval - FIRoffset);
      timeR.newValue(samplerate * ((fxdata->p[1].temposync ? storage->temposyncratio_inv : 1.f) *
                                   note_to_pitch(12 * fxdata->p[1].val.f)) -
                     LFOval - FIRoffset);
   }
   else
   {
      timeL.newValue(samplerate * ((fxdata->p[0].temposync ? storage->temposyncratio_inv : 1.f) *
                                   note_to_pitch(12 * *f[0])) +
                     LFOval - FIRoffset);
      timeR.newValue(samplerate * ((fxdata->p[1].temposync ? storage->temposyncratio_inv : 1.f) *
                                   note_to_pitch(12 * *f[1])) -
                     LFOval - FIRoffset);
   }

   const float db96 = powf(10.f, 0.05f * -96.f);
   float maxfb = max(db96, fb + cf);
   if (maxfb < 1.f)
   {
      float f = block_size_inv * max(timeL.v, timeR.v) * (1.f + log(db96) / log(maxfb));
      ringout_time = (int)f;
   }
   else
   {
      ringout_time = -1;
      ringout = 0;
   }

   mix.set_target_smoothed(*f[10]);
   width.set_target_smoothed(db_to_linear(*f[11]));
   pan.set_target_smoothed(limit_range(*f[8], -1.f, 1.f));

   hp.coeff_HP(hp.calc_omega(*f[4] / 12.0), 0.707);
   lp.coeff_LP2B(lp.calc_omega(*f[5] / 12.0), 0.707);
   if (init)
   {
      timeL.instantize();
      timeR.instantize();
      feedback.instantize();
      crossfeed.instantize();
      mix.instantize();
      width.instantize();
      pan.instantize();
      hp.coeff_instantize();
      lp.coeff_instantize();
   }
}

void DualDelayEffect::process(float* dataL, float* dataR)
{
   setvars(false);

   _MM_ALIGN16 float tbufferL[block_size], wbL[block_size]; // wb = write-buffer
   _MM_ALIGN16 float tbufferR[block_size], wbR[block_size];
   int k;

   for (k = 0; k < block_size; k++)
   {
      timeL.process();
      timeR.process();

      int i_dtimeL = max(block_size, min((int)timeL.v, max_delay_length - FIRipol_N - 1));
      int i_dtimeR = max(block_size, min((int)timeR.v, max_delay_length - FIRipol_N - 1));

      int rpL = ((wpos - i_dtimeL + k) - FIRipol_N) & (max_delay_length - 1);
      int rpR = ((wpos - i_dtimeR + k) - FIRipol_N) & (max_delay_length - 1);

      int sincL = FIRipol_N *
                  limit_range((int)(FIRipol_M * (float(i_dtimeL + 1) - timeL.v)), 0, FIRipol_M - 1);
      int sincR = FIRipol_N *
                  limit_range((int)(FIRipol_M * (float(i_dtimeR + 1) - timeR.v)), 0, FIRipol_M - 1);

#if PPC
      const vFloat zero = (vFloat)0.f;
      vFloat L, R, buf_in[3], abuf[4];
      vector unsigned char mask;

      float* target = &buffer[0][rpL];
      abuf[0] = vec_ld(0, target);
      abuf[1] = vec_ld(16, target);
      abuf[2] = vec_ld(32, target);
      abuf[3] = vec_ld(47, target);
      mask = vec_lvsl(0, target);
      buf_in[0] = vec_perm(abuf[0], abuf[1], mask);
      buf_in[1] = vec_perm(abuf[1], abuf[2], mask);
      buf_in[2] = vec_perm(abuf[2], abuf[3], mask);

      L = vec_madd(vec_ld(0, &sinctable1X[sincL]), buf_in[0], zero);
      L = vec_madd(vec_ld(0, &sinctable1X[sincL + 4]), buf_in[1], L);
      L = vec_madd(vec_ld(0, &sinctable1X[sincL + 8]), buf_in[2], L);

      target = &buffer[0][rpR];
      abuf[0] = vec_ld(0, target);
      abuf[1] = vec_ld(16, target);
      abuf[2] = vec_ld(32, target);
      abuf[3] = vec_ld(48, target);
      mask = vec_lvsl(0, target);
      buf_in[0] = vec_perm(abuf[0], abuf[1], mask);
      buf_in[1] = vec_perm(abuf[1], abuf[2], mask);
      buf_in[2] = vec_perm(abuf[2], abuf[3], mask);

      R = vec_madd(vec_ld(0, &sinctable1X[sincR]), buf_in[0], zero);
      R = vec_madd(vec_ld(0, &sinctable1X[sincR + 4]), buf_in[1], R);
      R = vec_madd(vec_ld(0, &sinctable1X[sincR + 8]), buf_in[2], R);

      // summera ner vector till scalar och spara

      L = vec_add(L, vec_sld(L, L, 8));
      L = vec_add(L, vec_sld(L, L, 4));
      vec_ste(L, k << 2, tbufferL);
      R = vec_add(R, vec_sld(R, R, 8));
      R = vec_add(R, vec_sld(R, R, 4));
      vec_ste(R, k << 2, tbufferR);

#else
      __m128 L, R;
      L = _mm_mul_ps(_mm_load_ps(&sinctable1X[sincL]), _mm_loadu_ps(&buffer[0][rpL]));
      L = _mm_add_ps(
          L, _mm_mul_ps(_mm_load_ps(&sinctable1X[sincL + 4]), _mm_loadu_ps(&buffer[0][rpL + 4])));
      L = _mm_add_ps(
          L, _mm_mul_ps(_mm_load_ps(&sinctable1X[sincL + 8]), _mm_loadu_ps(&buffer[0][rpL + 8])));
      L = sum_ps_to_ss(L);
      R = _mm_mul_ps(_mm_load_ps(&sinctable1X[sincR]), _mm_loadu_ps(&buffer[1][rpR]));
      R = _mm_add_ps(
          R, _mm_mul_ps(_mm_load_ps(&sinctable1X[sincR + 4]), _mm_loadu_ps(&buffer[1][rpR + 4])));
      R = _mm_add_ps(
          R, _mm_mul_ps(_mm_load_ps(&sinctable1X[sincR + 8]), _mm_loadu_ps(&buffer[1][rpR + 8])));
      R = sum_ps_to_ss(R);
      _mm_store_ss(&tbufferL[k], L);
      _mm_store_ss(&tbufferR[k], R);
#endif
   }

   softclip_block(tbufferL, block_size_quad);
   softclip_block(tbufferR, block_size_quad);

   lp.process_block(tbufferL, tbufferR);
   hp.process_block(tbufferL, tbufferR);

   pan.trixpan_blocks(dataL, dataR, wbL, wbR, block_size_quad);

   feedback.MAC_2_blocks_to(tbufferL, tbufferR, wbL, wbR, block_size_quad);
   crossfeed.MAC_2_blocks_to(tbufferL, tbufferR, wbR, wbL, block_size_quad);

   if (wpos + block_size >= max_delay_length)
   {
      for (k = 0; k < block_size; k++)
      {
         buffer[0][(wpos + k) & (max_delay_length - 1)] = wbL[k];
         buffer[1][(wpos + k) & (max_delay_length - 1)] = wbR[k];
      }
   }
   else
   {
      copy_block(wbL, &buffer[0][wpos], block_size_quad);
      copy_block(wbR, &buffer[1][wpos], block_size_quad);
   }

   if (wpos == 0)
   {
      for (k = 0; k < FIRipol_N; k++)
      {
         buffer[0][k + max_delay_length] =
             buffer[0][k]; // copy buffer so FIR-core doesn't have to wrap
         buffer[1][k + max_delay_length] = buffer[1][k];
      }
   }

   // scale width
   _MM_ALIGN16 float M[block_size], S[block_size];
   encodeMS(tbufferL, tbufferR, M, S, block_size_quad);
   width.multiply_block(S, block_size_quad);
   decodeMS(M, S, tbufferL, tbufferR, block_size_quad);

   mix.fade_2_blocks_to(dataL, tbufferL, dataR, tbufferR, dataL, dataR, block_size_quad);

   wpos += block_size;
   wpos = wpos & (max_delay_length - 1);
}

void DualDelayEffect::suspend()
{
   init();
}

const char* DualDelayEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Input";
   case 1:
      return "Delay time";
   case 2:
      return "Feedback/EQ";
   case 3:
      return "Modulation";
   case 4:
      return "Mix";
   }
   return 0;
}
int DualDelayEffect::group_label_ypos(int id)
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
      return 21;
   case 4:
      return 27;
   }
   return 0;
}

void DualDelayEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[0].set_name("Left");
   fxdata->p[0].set_type(ct_envtime);
   fxdata->p[1].set_name("Right");
   fxdata->p[1].set_type(ct_envtime);
   fxdata->p[2].set_name("Feedback");
   fxdata->p[2].set_type(ct_amplitude);
   fxdata->p[3].set_name("Crossfeed");
   fxdata->p[3].set_type(ct_amplitude);
   fxdata->p[4].set_name("Low Cut");
   fxdata->p[4].set_type(ct_freq_audible);
   fxdata->p[5].set_name("High Cut");
   fxdata->p[5].set_type(ct_freq_audible);
   fxdata->p[6].set_name("Rate");
   fxdata->p[6].set_type(ct_lforate);
   fxdata->p[7].set_name("Depth");
   fxdata->p[7].set_type(ct_detuning);
   // fxdata->p[8].set_name("ducking");
   // fxdata->p[8].set_type(ct_decibel_extra_narrow); fxdata->p[9].set_name("rate");
   // fxdata->p[9].set_type(ct_envtime);
   fxdata->p[8].set_name("Pan");
   fxdata->p[8].set_type(ct_percent_bidirectional);

   fxdata->p[10].set_name("Mix");
   fxdata->p[10].set_type(ct_percent);
   fxdata->p[11].set_name("Width");
   fxdata->p[11].set_type(ct_decibel_narrow);

   fxdata->p[0].posy_offset = 5;
   fxdata->p[1].posy_offset = 5;

   fxdata->p[2].posy_offset = 7;
   fxdata->p[3].posy_offset = 7;
   fxdata->p[4].posy_offset = 7;
   fxdata->p[5].posy_offset = 7;

   fxdata->p[6].posy_offset = 9;
   fxdata->p[7].posy_offset = 9;

   // fxdata->p[8].posy_offset = 5;
   // fxdata->p[9].posy_offset = 5;

   fxdata->p[8].posy_offset = -15;

   fxdata->p[10].posy_offset = 7;
   fxdata->p[11].posy_offset = 7;
}
void DualDelayEffect::init_default_values()
{
   fxdata->p[0].val.f = -2.f;
   fxdata->p[1].val.f = -2.f;
   fxdata->p[2].val.f = 0.0f;
   fxdata->p[3].val.f = 0.0f;
   fxdata->p[4].val.f = -24.f;
   fxdata->p[5].val.f = 30.f;
   fxdata->p[6].val.f = -2.f;
   fxdata->p[7].val.f = 0.f;
   fxdata->p[8].val.f = 0.f;
   // fxdata->p[9].val.f = 0.f;
   fxdata->p[10].val.f = 1.f;
   fxdata->p[11].val.f = 0.f;
}