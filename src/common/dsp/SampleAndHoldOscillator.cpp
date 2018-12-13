//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "Oscillator.h"
#include "DspUtilities.h"

// const float integrator_hpf = 0.99999999f;
// const float integrator_hpf = 0.9992144f;		// 44.1 kHz
// const float integrator_hpf = 0.9964f;		// 44.1 kHz
// const float integrator_hpf = 0.9982f;		// 44.1 kHz	 Magic Moog freq
const float integrator_hpf = 0.999f;
// 290 samples to fall 50% (British) (is probably a 2-pole HPF)
// 202 samples (American)
// const float integrator_hpf = 0.999f;
// pow(ln(0.5)/(samplerate/50hz)
const float hpf_cycle_loss = 0.995f;

SampleAndHoldOscillator::SampleAndHoldOscillator(SurgeStorage* storage,
                                                 OscillatorStorage* oscdata,
                                                 pdata* localcopy)
    : AbstractBlitOscillator(storage, oscdata, localcopy)
{}

SampleAndHoldOscillator::~SampleAndHoldOscillator()
{}

void SampleAndHoldOscillator::init(float pitch, bool is_display)
{
   assert(storage);
   first_run = true;
#if PPC
   osc_out = 0.f;
   osc_outR = 0.f;
#else
   osc_out = _mm_set1_ps(0.f);
   osc_outR = _mm_set1_ps(0.f);
#endif
   bufpos = 0;
   dc = 0;

   // init här
   id_shape = oscdata->p[0].param_id_in_scene;
   id_pw = oscdata->p[1].param_id_in_scene;
   id_smooth = oscdata->p[2].param_id_in_scene;
   id_sub = oscdata->p[3].param_id_in_scene;
   id_sync = oscdata->p[4].param_id_in_scene;
   id_detune = oscdata->p[5].param_id_in_scene;

   float rate = 0.05;
   l_pw.setRate(rate);

   l_shape.setRate(rate);
   l_sub.setRate(rate);
   l_sync.setRate(rate);

   n_unison = limit_range(oscdata->p[6].val.i, 1, max_unison);
   if (is_display)
   {
      n_unison = 1;
      srand(2);
   }
   prepare_unison(n_unison);

   memset(oscbuffer, 0, sizeof(float) * (ob_length + FIRipol_N));
   memset(oscbufferR, 0, sizeof(float) * (ob_length + FIRipol_N));
   memset(last_level, 0, max_unison * sizeof(float));
   memset(last_level2, 0, max_unison * sizeof(float));
   memset(elapsed_time, 0, max_unison * sizeof(float));

   this->pitch = pitch;
   update_lagvals<true>();

   int i;
   for (i = 0; i < n_unison; i++)
   {
      if (oscdata->retrigger.val.b || is_display)
      {
         oscstate[i] = 0;
         syncstate[i] = 0;
      }
      else
      {
         double drand = (double)rand() / RAND_MAX;
         double detune = localcopy[id_detune].f * (detune_bias * float(i) + detune_offset);
         // double t = drand * max(2.0,dsamplerate_os / (16.35159783 *
         // pow((double)1.05946309435,(double)pitch)));
         double st = drand * note_to_pitch(detune - 12);
         drand = (double)rand() / RAND_MAX;
         double ot = drand * note_to_pitch(detune + l_sync.v);
         oscstate[i] = st;
         syncstate[i] = st;
      }
      state[i] = 0;
      last_level[i] = 0.0;
      pwidth[i] = limit_range(l_pw.v, 0.001, 0.999);
      driftlfo2[i] = 0.f;
      driftlfo[i] = 0.f;
   }
}

void SampleAndHoldOscillator::init_ctrltypes()
{
   oscdata->p[0].set_name("Correlation");
   oscdata->p[0].set_type(ct_percent_bidirectional);
   oscdata->p[1].set_name("Width");
   oscdata->p[1].set_type(ct_percent);
   oscdata->p[1].val_default.f = 0.5f;
   oscdata->p[2].set_name("-");
   oscdata->p[2].set_type(ct_none);
   oscdata->p[3].set_name("-");
   oscdata->p[3].set_type(ct_none);
   oscdata->p[4].set_name("Sync");
   oscdata->p[4].set_type(ct_syncpitch);
   oscdata->p[5].set_name("Uni Spread");
   oscdata->p[5].set_type(ct_oscspread);
   oscdata->p[6].set_name("Uni Count");
   oscdata->p[6].set_type(ct_osccount);
}
void SampleAndHoldOscillator::init_default_values()
{
   oscdata->p[0].val.f = 0.f;
   oscdata->p[1].val.f = 0.5f;
   oscdata->p[2].val.f = 0.f;
   oscdata->p[3].val.f = 0.f;
   oscdata->p[4].val.f = 0.f;
   oscdata->p[5].val.f = 0.2f;
   oscdata->p[6].val.i = 1;
}

void SampleAndHoldOscillator::convolute(int voice, bool FM, bool stereo)
{
   float detune = drift * driftlfo[voice];
   if (n_unison > 1)
      detune += localcopy[id_detune].f * (detune_bias * float(voice) + detune_offset);

   float sub = l_sub.v;

   const float p24 = (1 << 24);
   unsigned int ipos;
   if (FM)
      ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv * FMmul_inv));
   else
      ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv));

   float invertcorrelation = 1.f;
   if (syncstate[voice] < oscstate[voice])
   {
      ipos = (unsigned int)((float)p24 * (syncstate[voice] * pitchmult_inv));
      float t = note_to_pitch_inv(detune);
      if (state[voice] == 1)
         invertcorrelation = -1.f;
      state[voice] = 0;
      oscstate[voice] = syncstate[voice];
      syncstate[voice] += t;
   }

   unsigned int delay = ((ipos >> 24) & 0x3f);
   if (FM)
      delay = FMdelay;
   unsigned int m = ((ipos >> 16) & 0xff) * (FIRipol_N << 1);
   unsigned int lipolui16 = (ipos & 0xffff);
#if PPC
   float flipol = ((float)((unsigned int)(lipolui16)));
#else
   __m128 lipol128 = _mm_cvtsi32_ss(lipol128, lipolui16);
   lipol128 = _mm_shuffle_ps(lipol128, lipol128, _MM_SHUFFLE(0, 0, 0, 0));
#endif

   int k;
   const float s = 0.99952f;
   // add time until next statechange
   float t;
   if (oscdata->p[5].absolute)
      t = note_to_pitch_inv(detune * pitchmult_inv * (1.f / 440.f) + l_sync.v);
   else
      t = note_to_pitch_inv(detune + l_sync.v);

   float g, gR;

   float wf = l_shape.v * 0.8 * invertcorrelation;
   float wfabs = fabs(wf);
   float smooth = l_smooth.v;
   float rand11 = (((float)rand() * rcp(RAND_MAX)) * 2.f - 1.f);
   float randt = rand11 * (1 - wfabs) - wf * last_level[voice];

   randt = randt * rcp(1.0f - wfabs);
   randt = min(0.5f, max(-0.5f, randt));

   if (state[voice] == 0)
      pwidth[voice] = l_pw.v;

   g = randt - last_level[voice];
   last_level[voice] = randt;

   g *= out_attenuation;
   if (stereo)
   {
      gR = g * panR[voice];
      g *= panL[voice];
   }

#if PPC

   if (stereo)
   {
      vFloat ob_inL[3], abufL[4], edgesL;
      vFloat ob_inR[3], abufR[4], edgesR;
      vFloat g128L = vec_loadAndSplatScalar(&g);
      vFloat g128R = vec_loadAndSplatScalar(&gR);
      vFloat st[3];
      vFloat lipol128 = vec_loadAndSplatScalar(&flipol);
      vector unsigned char mask, maskstore; // since both buffers are aligned and read the same
                                            // position, the same mask can be used

      // load & align oscbuffer (left)
      float* targetL = &oscbuffer[bufpos + delay];
      abufL[0] = vec_ld(0, targetL);
      abufL[1] = vec_ld(16, targetL);
      abufL[2] = vec_ld(32, targetL);
      abufL[3] = vec_ld(48, targetL);
      mask = vec_lvsl(0, targetL);
      maskstore = vec_lvsr(0, targetL);
      ob_inL[0] = vec_perm(abufL[0], abufL[1], mask);
      ob_inL[1] = vec_perm(abufL[1], abufL[2], mask);
      ob_inL[2] = vec_perm(abufL[2], abufL[3], mask);
      edgesL = vec_perm(abufL[3], abufL[0], mask);
      // (right)
      float* targetR = &oscbufferR[bufpos + delay];
      abufR[0] = vec_ld(0, targetR);
      abufR[1] = vec_ld(16, targetR);
      abufR[2] = vec_ld(32, targetR);
      abufR[3] = vec_ld(48, targetR);
      ob_inR[0] = vec_perm(abufR[0], abufR[1], mask);
      ob_inR[1] = vec_perm(abufR[1], abufR[2], mask);
      ob_inR[2] = vec_perm(abufR[2], abufR[3], mask);
      edgesR = vec_perm(abufR[3], abufR[0], mask);

      float* target2 = &sinctable[m];
      st[0] = vec_madd(lipol128, vec_ld(48, target2), vec_ld(0, target2));
      st[1] = vec_madd(lipol128, vec_ld(64, target2), vec_ld(16, target2));
      st[2] = vec_madd(lipol128, vec_ld(80, target2), vec_ld(32, target2));

      ob_inL[0] = vec_madd(g128L, st[0], ob_inL[0]);
      ob_inL[1] = vec_madd(g128L, st[1], ob_inL[1]);
      ob_inL[2] = vec_madd(g128L, st[2], ob_inL[2]);

      ob_inR[0] = vec_madd(g128R, st[0], ob_inR[0]);
      ob_inR[1] = vec_madd(g128R, st[1], ob_inR[1]);
      ob_inR[2] = vec_madd(g128R, st[2], ob_inR[2]);

      // re-unalign oscbuffer and store (left)
      abufL[0] = vec_perm(edgesL, ob_inL[0], maskstore);
      abufL[1] = vec_perm(ob_inL[0], ob_inL[1], maskstore);
      abufL[2] = vec_perm(ob_inL[1], ob_inL[2], maskstore);
      abufL[3] = vec_perm(ob_inL[2], edgesL, maskstore);
      vec_st(abufL[0], 0, targetL);
      vec_st(abufL[1], 16, targetL);
      vec_st(abufL[2], 32, targetL);
      vec_st(abufL[3], 48, targetL);

      // re-unalign oscbuffer and store (right)
      abufR[0] = vec_perm(edgesR, ob_inR[0], maskstore);
      abufR[1] = vec_perm(ob_inR[0], ob_inR[1], maskstore);
      abufR[2] = vec_perm(ob_inR[1], ob_inR[2], maskstore);
      abufR[3] = vec_perm(ob_inR[2], edgesR, maskstore);
      vec_st(abufR[0], 0, targetR);
      vec_st(abufR[1], 16, targetR);
      vec_st(abufR[2], 32, targetR);
      vec_st(abufR[3], 48, targetR);
   }
   else
   {
      vFloat ob_in[3], abuf[4], st[3], edges;
      vFloat g128 = vec_loadAndSplatScalar(&g);
      vFloat lipol128 = vec_loadAndSplatScalar(&flipol);
      vector unsigned char mask, maskstore;

      // load & align oscbuffer
      float* target = &oscbuffer[bufpos + delay];
      abuf[0] = vec_ld(0, target);
      abuf[1] = vec_ld(16, target);
      abuf[2] = vec_ld(32, target);
      abuf[3] = vec_ld(48, target);
      mask = vec_lvsl(0, target);
      maskstore = vec_lvsr(0, target);
      ob_in[0] = vec_perm(abuf[0], abuf[1], mask);
      ob_in[1] = vec_perm(abuf[1], abuf[2], mask);
      ob_in[2] = vec_perm(abuf[2], abuf[3], mask);
      edges = vec_perm(abuf[3], abuf[0], mask);

      float* target2 = &sinctable[m];
      st[0] = vec_madd(lipol128, vec_ld(48, target2), vec_ld(0, target2));
      st[1] = vec_madd(lipol128, vec_ld(64, target2), vec_ld(16, target2));
      st[2] = vec_madd(lipol128, vec_ld(80, target2), vec_ld(32, target2));

      ob_in[0] = vec_madd(g128, st[0], ob_in[0]);
      ob_in[1] = vec_madd(g128, st[1], ob_in[1]);
      ob_in[2] = vec_madd(g128, st[2], ob_in[2]);

      // re-unalign oscbuffer and store
      abuf[0] = vec_perm(edges, ob_in[0], maskstore);
      abuf[1] = vec_perm(ob_in[0], ob_in[1], maskstore);
      abuf[2] = vec_perm(ob_in[1], ob_in[2], maskstore);
      abuf[3] = vec_perm(ob_in[2], edges, maskstore);
      vec_st(abuf[0], 0, target);
      vec_st(abuf[1], 16, target);
      vec_st(abuf[2], 32, target);
      vec_st(abuf[3], 48, target);
   }

#else

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
#endif

   if (state[voice] & 1)
      rate[voice] = t * (1.0 - pwidth[voice]);
   else
      rate[voice] = t * pwidth[voice];

   oscstate[voice] += rate[voice];
   oscstate[voice] = max(0.f, oscstate[voice]);
   state[voice] = (state[voice] + 1) & 1;
}

template <bool is_init> void SampleAndHoldOscillator::update_lagvals()
{
   l_sync.newValue(max(0.f, localcopy[id_sync].f));
   l_pw.newValue(limit_range(localcopy[id_pw].f, 0.001f, 0.999f));
   l_shape.newValue(localcopy[id_shape].f);
   l_smooth.newValue(localcopy[id_smooth].f);
   l_sub.newValue(localcopy[id_sub].f);

   float invt =
       4.f * min(1.0, (8.175798915 * note_to_pitch(pitch + l_sync.v)) * dsamplerate_os_inv);
   float hpf2 = min(integrator_hpf, powf(hpf_cycle_loss, invt)); 
    // ACHTUNG! gör lookup-table
    // ACHTUNG/WARNING! Make a lookup-table

   li_hpf.set_target(hpf2);

   if (is_init)
   {
      hpf_coeff.instantize();
      integrator_mult.instantize();
      l_pw.instantize();
      l_shape.instantize();
      l_smooth.instantize();
      l_sub.instantize();
      l_sync.instantize();
   }
}

void SampleAndHoldOscillator::process_block(
    float pitch0, float drift, bool stereo, bool FM, float depth)
{
   this->pitch = min(148.f, pitch0);
   this->drift = drift;
   pitchmult_inv = max(1.0, dsamplerate_os * (1 / 8.175798915) * note_to_pitch_inv(pitch));
   pitchmult =
       1.f /
       pitchmult_inv; 
      // This must be a real division, reciprocal-approximation is not precise enough
   int k, l;

   // if (FM) FMdepth.newValue(depth);
   update_lagvals<false>();

   l_pw.process();
   l_shape.process();
   l_smooth.process();
   l_sub.process();
   l_sync.process();

   // hpf_coeff.process();
   // integrator_mult.process();

   if (FM)
   {
      for (l = 0; l < n_unison; l++)
         driftlfo[l] = drift_noise(driftlfo2[l]);
      for (int s = 0; s < block_size_os; s++)
      {
         float fmmul = limit_range(1.f + depth * master_osc[s], 0.1f, 1.9f);
         float a = pitchmult * fmmul;
         FMdelay = s;

         for (l = 0; l < n_unison; l++)
         {
            while (oscstate[l] < a)
            {
               FMmul_inv = rcp(fmmul);
               convolute(l, true, stereo);
            }

            oscstate[l] -= a;
         }
      }
   }
   else
   {
      float a = (float)block_size_os * pitchmult;
      for (l = 0; l < n_unison; l++)
      {
         driftlfo[l] = drift_noise(driftlfo2[l]);

         while ((syncstate[l] < a) || (oscstate[l] < a))
            convolute(l, false, stereo);

         oscstate[l] -= a;
         if (l_sync.v > 0)
            syncstate[l] -= a;
      }
   }

   _MM_ALIGN16 float hpfblock[block_size_os];
   li_hpf.store_block(hpfblock, block_size_os_quad);

#if PPC
   float oa = out_attenuation * pitchmult;
   for (k = 0; k < block_size_os; k++)
   {
      float ob = oscbuffer[bufpos + k] - oa * dc;
      osc_out = osc_out * hpfblock[k] + ob;
      output[k] = osc_out;

      if (stereo)
      {
         float obR = oscbufferR[bufpos + k] - oa * dc;
         osc_outR = osc_outR * hpfblock[k] + obR;
         outputR[k] = osc_outR;
      }
   }
#else

   __m128 mdc = _mm_load_ss(&dc);
   __m128 oa = _mm_load_ss(&out_attenuation);
   oa = _mm_mul_ss(oa, _mm_load_ss(&pitchmult));

   for (k = 0; k < block_size_os; k++)
   {
      __m128 hpf = _mm_load_ss(&hpfblock[k]);
      __m128 ob = _mm_load_ss(&oscbuffer[bufpos + k]);
      __m128 a = _mm_mul_ss(osc_out, hpf);
      ob = _mm_sub_ss(ob, _mm_mul_ss(mdc, oa));
      osc_out = _mm_add_ss(a, ob);

      _mm_store_ss(&output[k], osc_out);

      if (stereo)
      {
         ob = _mm_load_ss(&oscbufferR[bufpos + k]);
         a = _mm_mul_ss(osc_outR, hpf);
         ob = _mm_sub_ss(ob, _mm_mul_ss(mdc, oa));
         osc_outR = _mm_add_ss(a, ob);
         _mm_store_ss(&outputR[k], osc_outR);
      }
   }
   _mm_store_ss(&dc, mdc);

#endif

   clear_block(&oscbuffer[bufpos], block_size_os_quad);
   if (stereo)
      clear_block(&oscbufferR[bufpos], block_size_os_quad);
   clear_block(&dcbuffer[bufpos], block_size_os_quad);

   bufpos = (bufpos + block_size_os) & (ob_length - 1);

   // each block overlap FIRipol_N samples into the next (due to impulses not being wrapped around
   // the block edges copy the overlapping samples to the new block position

   if (!bufpos) // only needed if the new bufpos == 0
   {
#if PPC
      vFloat overlap, dcoverlap, overlapR;
      const vFloat zero = (vFloat)0.f;
      for (k = 0; k < (FIRipol_N); k += 4)
      {
         overlap = vec_ld((ob_length + k) << 2, oscbuffer);
         vec_st(overlap, (k) << 2, oscbuffer);
         vec_st(zero, (ob_length + k) << 2, oscbuffer);

         dcoverlap = vec_ld((ob_length + k) << 2, dcbuffer);
         vec_st(dcoverlap, (k) << 2, dcbuffer);
         vec_st(zero, (ob_length + k) << 2, dcbuffer);

         if (stereo)
         {
            overlapR = vec_ld((ob_length + k) << 2, oscbufferR);
            vec_st(overlapR, (k) << 2, oscbufferR);
            vec_st(zero, (ob_length + k) << 2, oscbufferR);
         }
      }
#else
      __m128 overlap[FIRipol_N >> 2], overlapR[FIRipol_N >> 2];
      const __m128 zero = _mm_setzero_ps();
      for (k = 0; k < (FIRipol_N); k += 4)
      {
         overlap[k >> 2] = _mm_load_ps(&oscbuffer[ob_length + k]);
         _mm_store_ps(&oscbuffer[k], overlap[k >> 2]);
         _mm_store_ps(&oscbuffer[ob_length + k], zero);
         if (stereo)
         {
            overlapR[k >> 2] = _mm_load_ps(&oscbufferR[ob_length + k]);
            _mm_store_ps(&oscbufferR[k], overlapR[k >> 2]);
            _mm_store_ps(&oscbufferR[ob_length + k], zero);
         }
      }
#endif
   }
}
