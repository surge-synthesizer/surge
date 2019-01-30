//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "Oscillator.h"
#include "DspUtilities.h"

using namespace std;

// const float integrator_hpf = 0.99999999f;
// const float integrator_hpf = 0.9992144f;		// 44.1 kHz
// const float integrator_hpf = 0.9964f;		// 44.1 kHz
// const float integrator_hpf = 0.9982f;		// 44.1 kHz	 Magical Moog frequency

// 290 samples to fall by 50% (British)  (Is probably a 2-pole HPF)
// 202 samples (American)
// const float integrator_hpf = 0.999f;
// pow(ln(0.5)/(samplerate/50hz)
const float hpf_cycle_loss = 0.995f;

AbstractBlitOscillator::AbstractBlitOscillator(SurgeStorage* storage,
                                               OscillatorStorage* oscdata,
                                               pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy)
{
   integrator_hpf = (1.f - 2.f * 20.f * samplerate_inv);
   integrator_hpf *= integrator_hpf;
}

void AbstractBlitOscillator::prepare_unison(int voices)
{
   out_attenuation_inv = sqrt((float)n_unison);
   out_attenuation = 1.0f / out_attenuation_inv;

   if (voices == 1)
   {
      detune_bias = 1;
      detune_offset = 0;
      panL[0] = 1.f;
      panR[0] = 1.f;
   }
   else
   {
      detune_bias = (float)2.f / (n_unison - 1.f);
      detune_offset = -1.f;

      bool odd = voices & 1;
      float mid = voices * 0.5 - 0.5;
      int half = voices >> 1;
      for (int i = 0; i < voices; i++)
      {
         float d = fabs((float)i - mid) / mid;
         if (odd && (i >= half))
            d = -d;
         if (i & 1)
            d = -d;

         panL[i] = (1.f - d);
         panR[i] = (1.f + d);
      }
   }
}

SurgeSuperOscillator::SurgeSuperOscillator(SurgeStorage* storage,
                                           OscillatorStorage* oscdata,
                                           pdata* localcopy)
    : AbstractBlitOscillator(storage, oscdata, localcopy)
{}

SurgeSuperOscillator::~SurgeSuperOscillator()
{}

void SurgeSuperOscillator::init(float pitch, bool is_display)
{
   assert(storage);
   first_run = true;

   switch (storage->getPatch().character.val.i)
   {
   case 0:
   {
      float filt = 1.f - 2.f * 5000.f * samplerate_inv;
      filt *= filt;
      CoefB0 = 1.f - filt;
      CoefB1 = 0.f;
      CoefA1 = filt;
   }
   break;
   case 1:
      CoefB0 = 1.f;
      CoefB1 = 0.f;
      CoefA1 = 0.f;
      break;
   case 2:
   {
      float filt = 1.f - 2.f * 5000.f * samplerate_inv;
      filt *= filt;
      float A0 = 1.f / (1.f - filt);
      CoefB0 = 1.f * A0;
      CoefB1 = -filt * A0;
      CoefA1 = 0.f;
   }
   break;
   }

   osc_out = _mm_set1_ps(0.f);
   osc_out2 = _mm_set1_ps(0.f);
   osc_outR = _mm_set1_ps(0.f);
   osc_out2R = _mm_set1_ps(0.f);
   bufpos = 0;
   dc = 0;

   // Init here
   id_shape = oscdata->p[0].param_id_in_scene;
   id_pw = oscdata->p[1].param_id_in_scene;
   id_pw2 = oscdata->p[2].param_id_in_scene;
   id_sub = oscdata->p[3].param_id_in_scene;
   id_sync = oscdata->p[4].param_id_in_scene;
   id_detune = oscdata->p[5].param_id_in_scene;

   float rate = 0.05;
   l_pw.setRate(rate);
   l_pw2.setRate(rate);
   l_shape.setRate(rate);
   l_sub.setRate(rate);
   l_sync.setRate(rate);

   n_unison = limit_range(oscdata->p[6].val.i, 1, MAX_UNISON);
   if (is_display)
      n_unison = 1;
   prepare_unison(n_unison);

   memset(oscbuffer, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
   memset(oscbufferR, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
   memset(dcbuffer, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
   memset(last_level, 0, MAX_UNISON * sizeof(float));
   memset(elapsed_time, 0, MAX_UNISON * sizeof(float));

   this->pitch = pitch;
   update_lagvals<true>();

   for (int i = 0; i < n_unison; i++)
   {
      if (oscdata->retrigger.val.b || is_display)
      {
         oscstate[i] = 0; //(float)i / (float)n_unison;
         syncstate[i] = 0;
         last_level[i] = -0.4;
      }
      else
      {
         double drand = (double)rand() / RAND_MAX;
         double detune = localcopy[id_detune].f * (detune_bias * float(i) + detune_offset);
         // double t = drand * max(2.0,dsamplerate_os / (16.35159783 *
         // pow((double)1.05946309435,(double)pitch)));
         double st = 0.25 * drand * note_to_pitch_inv(detune - 12);
         drand = (double)rand() / RAND_MAX;
         // double ot = 0.25 * drand * note_to_pitch_inv(detune + l_sync.v);
         // HACK test 0.2*
         oscstate[i] = st;
         syncstate[i] = st;
         last_level[i] = 0.0;
      }
      dc_uni[i] = 0;
      state[i] = 0;
      pwidth[i] = limit_range(l_pw.v, 0.001f, 0.999f);
      driftlfo2[i] = 0.f;
      driftlfo[i] = 0.f;
   }
}

void SurgeSuperOscillator::init_ctrltypes()
{
   oscdata->p[0].set_name("Shape");
   oscdata->p[0].set_type(ct_percent_bidirectional);
   oscdata->p[1].set_name("Width");
   oscdata->p[1].set_type(ct_percent);
   oscdata->p[1].val_default.f = 0.5f;
   oscdata->p[2].set_name("Sub Width");
   oscdata->p[2].set_type(ct_percent);
   oscdata->p[2].val_default.f = 0.5f;
   oscdata->p[3].set_name("Sub Level");
   oscdata->p[3].set_type(ct_percent);
   oscdata->p[4].set_name("Sync");
   oscdata->p[4].set_type(ct_syncpitch);
   oscdata->p[5].set_name("Uni Spread");
   oscdata->p[5].set_type(ct_oscspread);
   oscdata->p[6].set_name("Uni Count");
   oscdata->p[6].set_type(ct_osccount);
}
void SurgeSuperOscillator::init_default_values()
{
   oscdata->p[0].val.f = 0.f;
   oscdata->p[1].val.f = 0.5f;
   oscdata->p[2].val.f = 0.5f;
   oscdata->p[3].val.f = 0.f;
   oscdata->p[4].val.f = 0.f;
   oscdata->p[5].val.f = 0.2f;
   oscdata->p[6].val.i = 1;
}

template <bool FM> void SurgeSuperOscillator::convolute(int voice, bool stereo)
{
   const bool NODC = false;
   // assert(oscstate[voice]>=0.f);
   float detune = drift * driftlfo[voice];
   if (n_unison > 1)
      detune += localcopy[id_detune].f * (detune_bias * (float)voice + detune_offset);
   float wf = l_shape.v;
   float sub = l_sub.v;

   const float p24 = (1 << 24);
   unsigned int ipos;

   if ((l_sync.v > 0) && (syncstate[voice] < oscstate[voice]))
   {
      if (FM)
         ipos = (unsigned int)(p24 * (syncstate[voice] * pitchmult_inv * FMmul_inv));
      else
         ipos = (unsigned int)(p24 * (syncstate[voice] * pitchmult_inv));
      // double t = max(0.5,dsamplerate_os * (1/8.175798915) * note_to_pitch_inv(pitch + detune) *
      // 2);
      float t = note_to_pitch_inv(detune) * 2;
      state[voice] = 0;
      last_level[voice] += dc_uni[voice] * (oscstate[voice] - syncstate[voice]);

      oscstate[voice] = syncstate[voice];
      syncstate[voice] += t;
      syncstate[voice] = max(0.f, syncstate[voice]);
   }
   else
   {
      if (FM)
         ipos = (unsigned int)(p24 * (oscstate[voice] * pitchmult_inv * FMmul_inv));
      else
         ipos = (unsigned int)(p24 * (oscstate[voice] * pitchmult_inv));
   }

   unsigned int delay;
   if (FM)
      delay = FMdelay;
   else
      delay = ((ipos >> 24) & 0x3f);
   unsigned int m = ((ipos >> 16) & 0xff) * (FIRipol_N << 1);
   unsigned int lipolui16 = (ipos & 0xffff);
   __m128 lipol128 = _mm_cvtsi32_ss(lipol128, lipolui16);
   lipol128 = _mm_shuffle_ps(lipol128, lipol128, _MM_SHUFFLE(0, 0, 0, 0));

   int k;
   const float s = 0.99952f;
   float sync = min((float)l_sync.v, (12 + 72 + 72) - pitch);
   float t;
   if (oscdata->p[5].absolute)
      t = note_to_pitch_inv(detune * pitchmult_inv * (1.f / 440.f) + sync);
   else
      t = note_to_pitch_inv(detune + sync);

   float t_inv = rcp(t);
   float g, gR;

   switch (state[voice])
   {
   case 0:
   {
      pwidth[voice] = l_pw.v;
      pwidth2[voice] = 2.f * l_pw2.v;
      float tg =
          ((1 + wf) * 0.5f + (1 - pwidth[voice]) * (-wf)) * (1 - sub) +
          0.5f * sub *
              (2.f - pwidth2[voice]); // calculate the height of the first impulse of the cycle
      g = tg - last_level[voice];
      last_level[voice] = tg;
      if (!NODC)
         last_level[voice] -= (pwidth[voice]) * (pwidth2[voice]) * (1.f + wf) *
                              (1.f - sub); // calculate the level the sub-cycle will have at the end
                                           // of it's duration taking DC into account
      break;
   }
   case 1:
      g = wf * (1.f - sub) - sub;
      last_level[voice] += g;
      if (!NODC)
         last_level[voice] -= (1 - pwidth[voice]) * (2 - pwidth2[voice]) * (1 + wf) * (1.f - sub);
      break;
   case 2:
      g = 1.f - sub;
      last_level[voice] += g;
      if (!NODC)
         last_level[voice] -= (pwidth[voice]) * (2 - pwidth2[voice]) * (1 + wf) * (1.f - sub);
      break;
   case 3:
      g = wf * (1.f - sub) + sub;
      last_level[voice] += g;
      if (!NODC)
         last_level[voice] -= (1 - pwidth[voice]) * (pwidth2[voice]) * (1 + wf) * (1.f - sub);
      break;
   };

   g *= out_attenuation;
   if (stereo)
   {
      gR = g * panR[voice];
      g *= panL[voice];
   }

   if (stereo)
   {
      __m128 g128L = _mm_load_ss(&g);
      g128L = _mm_shuffle_ps(g128L, g128L, _MM_SHUFFLE(0, 0, 0, 0));
      __m128 g128R = _mm_load_ss(&gR);
      g128R = _mm_shuffle_ps(g128R, g128R, _MM_SHUFFLE(0, 0, 0, 0));

      for (k = 0; k < FIRipol_N; k += 4)
      {
         float* obfL = &oscbuffer[bufpos + k + delay];
         float* obfR = &oscbufferR[bufpos + k + delay];
         __m128 obL = _mm_loadu_ps(obfL);
         __m128 obR = _mm_loadu_ps(obfR);
         __m128 st = _mm_load_ps(&sinctable[m + k]);
         __m128 so = _mm_load_ps(&sinctable[m + k + FIRipol_N]);
         so = _mm_mul_ps(so, lipol128);
         st = _mm_add_ps(st, so);
         obL = _mm_add_ps(obL, _mm_mul_ps(st, g128L));
         _mm_storeu_ps(obfL, obL);
         obR = _mm_add_ps(obR, _mm_mul_ps(st, g128R));
         _mm_storeu_ps(obfR, obR);
      }
   }
   else
   {
      __m128 g128 = _mm_load_ss(&g);
      g128 = _mm_shuffle_ps(g128, g128, _MM_SHUFFLE(0, 0, 0, 0));

      for (k = 0; k < FIRipol_N; k += 4)
      {
         float* obf = &oscbuffer[bufpos + k + delay];
         __m128 ob = _mm_loadu_ps(obf);
         __m128 st = _mm_load_ps(&sinctable[m + k]);
         __m128 so = _mm_load_ps(&sinctable[m + k + FIRipol_N]);
         so = _mm_mul_ps(so, lipol128);
         st = _mm_add_ps(st, so);
         st = _mm_mul_ps(st, g128);
         ob = _mm_add_ps(ob, st);
         _mm_storeu_ps(obf, ob);
      }
   }

   if (!NODC)
   {
      float olddc = dc_uni[voice];
      dc_uni[voice] = t_inv * (1.f + wf) * (1 - sub); //*pitchmult;
      dcbuffer[(bufpos + FIRoffset + delay)] += (dc_uni[voice] - olddc);
   }

   if (state[voice] & 1)
      rate[voice] = t * (1.0 - pwidth[voice]);
   else
      rate[voice] = t * pwidth[voice];

   if ((state[voice] + 1) & 2)
      rate[voice] *= (2.0f - pwidth2[voice]);
   else
      rate[voice] *= pwidth2[voice];

   oscstate[voice] += rate[voice];
   oscstate[voice] = max(0.f, oscstate[voice]);
   state[voice] = (state[voice] + 1) & 3;
}

template <bool is_init> void SurgeSuperOscillator::update_lagvals()
{
   l_sync.newValue(max(0.f, localcopy[id_sync].f));
   l_pw.newValue(limit_range(localcopy[id_pw].f, 0.001f, 0.999f));
   l_pw2.newValue(limit_range(localcopy[id_pw2].f, 0.001f, 0.999f));
   l_shape.newValue(limit_range(localcopy[id_shape].f, -1.f, 1.f));
   l_sub.newValue(limit_range(localcopy[id_sub].f, 0.f, 1.f));

   float invt =
       4.f * min(1.0, (8.175798915 * note_to_pitch(pitch + l_sync.v)) * dsamplerate_os_inv);
   float hpf2 = min(integrator_hpf, powf(hpf_cycle_loss, invt)); // TODO ACHTUNG/WARNING! Make a lookup table

   li_hpf.set_target(hpf2);
   // li_integratormult.set_target(invt);

   if (is_init)
   {
      l_pw.instantize();
      l_pw2.instantize();
      l_shape.instantize();
      l_sub.instantize();
      l_sync.instantize();
      li_DC.instantize();

      li_hpf.instantize();
      li_integratormult.instantize();
   }
}

void SurgeSuperOscillator::process_block(
    float pitch0, float drift, bool stereo, bool FM, float depth)
{
   this->pitch = min(148.f, pitch0);
   this->drift = drift;
   pitchmult_inv = Max(1.0, dsamplerate_os * (1.f / 8.175798915f) * note_to_pitch_inv(pitch));

   pitchmult =
       1.f /
       pitchmult_inv; // This must be a real division, reciprocal-approximation is not precise enough

   int k, l;

   update_lagvals<false>();

   l_pw.process();
   l_pw2.process();
   l_shape.process();
   l_sub.process();
   l_sync.process();

   if (FM)
   {
      for (l = 0; l < n_unison; l++)
         driftlfo[l] = drift_noise(driftlfo2[l]);
      for (int s = 0; s < BLOCK_SIZE_OS; s++)
      {
         float fmmul = limit_range(1.f + depth * master_osc[s], 0.1f, 1.9f);
         float a = pitchmult * fmmul;
         FMdelay = s;

         for (l = 0; l < n_unison; l++)
         {
            while (((l_sync.v > 0) && (syncstate[l] < a)) || (oscstate[l] < a))
            {
               FMmul_inv = rcp(fmmul);
               // The division races with the growth of the oscstate so that it never comes out of/gets out of the loop
              // this becomes unsafe, don't fuck with the oscstate but make a division within the convolute instead.
               convolute<true>(l, stereo);
            }

            oscstate[l] -= a;
            if (l_sync.v > 0)
               syncstate[l] -= a;
         }
      }
   }
   else
   {
      float a = (float)BLOCK_SIZE_OS * pitchmult;
      for (l = 0; l < n_unison; l++)
      {
         driftlfo[l] = drift_noise(driftlfo2[l]);

         while (((l_sync.v > 0) && (syncstate[l] < a)) || (oscstate[l] < a))
         {
            convolute<false>(l, stereo);
         }

         oscstate[l] -= a;
         if (l_sync.v > 0)
            syncstate[l] -= a;
      }
   }

   float hpfblock alignas(16)[BLOCK_SIZE_OS];
   li_hpf.store_block(hpfblock, BLOCK_SIZE_OS_QUAD);

   __m128 mdc = _mm_load_ss(&dc);
   __m128 oa = _mm_load_ss(&out_attenuation);
   oa = _mm_mul_ss(oa, _mm_load_ss(&pitchmult));

   const __m128 mmone = _mm_set_ss(1.0f);
   __m128 char_b0 = _mm_load_ss(&CoefB0);
   __m128 char_b1 = _mm_load_ss(&CoefB1);
   __m128 char_a1 = _mm_load_ss(&CoefA1);

   for (k = 0; k < BLOCK_SIZE_OS; k++)
   {
      __m128 dcb = _mm_load_ss(&dcbuffer[bufpos + k]);
      __m128 hpf = _mm_load_ss(&hpfblock[k]);
      __m128 ob = _mm_load_ss(&oscbuffer[bufpos + k]);

      __m128 a = _mm_mul_ss(osc_out, hpf);
      mdc = _mm_add_ss(mdc, dcb);
      ob = _mm_sub_ss(ob, _mm_mul_ss(mdc, oa));
      __m128 LastOscOut = osc_out;
      osc_out = _mm_add_ss(a, ob);

      // character filter (hifalloff/neutral/boost)
      osc_out2 =
          _mm_add_ss(_mm_mul_ss(osc_out2, char_a1),
                     _mm_add_ss(_mm_mul_ss(osc_out, char_b0), _mm_mul_ss(LastOscOut, char_b1)));

      _mm_store_ss(&output[k], osc_out2);

      if (stereo)
      {
         ob = _mm_load_ss(&oscbufferR[bufpos + k]);

         a = _mm_mul_ss(osc_outR, hpf);

         ob = _mm_sub_ss(ob, _mm_mul_ss(mdc, oa));
         __m128 LastOscOutR = osc_outR;
         osc_outR = _mm_add_ss(a, ob);

         osc_out2R = _mm_add_ss(
             _mm_mul_ss(osc_out2R, char_a1),
             _mm_add_ss(_mm_mul_ss(osc_outR, char_b0), _mm_mul_ss(LastOscOutR, char_b1)));

         _mm_store_ss(&outputR[k], osc_out2R);
      }
   }
   _mm_store_ss(&dc, mdc);

   clear_block(&oscbuffer[bufpos], BLOCK_SIZE_OS_QUAD);
   if (stereo)
      clear_block(&oscbufferR[bufpos], BLOCK_SIZE_OS_QUAD);
   clear_block(&dcbuffer[bufpos], BLOCK_SIZE_OS_QUAD);

   bufpos = (bufpos + BLOCK_SIZE_OS) & (OB_LENGTH - 1);

   // each block overlap FIRipol_N samples into the next (due to impulses not being wrapped around
   // the block edges copy the overlapping samples to the new block position

   if (bufpos == 0) // only needed if the new bufpos == 0
   {
      __m128 overlap[FIRipol_N >> 2], dcoverlap[FIRipol_N >> 2], overlapR[FIRipol_N >> 2];
      const __m128 zero = _mm_setzero_ps();
      for (k = 0; k < (FIRipol_N); k += 4)
      {
         overlap[k >> 2] = _mm_load_ps(&oscbuffer[OB_LENGTH + k]);
         _mm_store_ps(&oscbuffer[k], overlap[k >> 2]);
         _mm_store_ps(&oscbuffer[OB_LENGTH + k], zero);

         dcoverlap[k >> 2] = _mm_load_ps(&dcbuffer[OB_LENGTH + k]);
         _mm_store_ps(&dcbuffer[k], dcoverlap[k >> 2]);
         _mm_store_ps(&dcbuffer[OB_LENGTH + k], zero);

         if (stereo)
         {
            overlapR[k >> 2] = _mm_load_ps(&oscbufferR[OB_LENGTH + k]);
            _mm_store_ps(&oscbufferR[k], overlapR[k >> 2]);
            _mm_store_ps(&oscbufferR[OB_LENGTH + k], zero);
         }
      }
   }
   first_run = false;
}
