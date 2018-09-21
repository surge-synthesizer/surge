//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "sub3_osc.h"
#include "dsputils.h"

const float hpf_cycle_loss = 0.99f;

osc_wavetable::osc_wavetable(sub3_storage* storage, sub3_osc* oscdata, pdata* localcopy)
    : oscillator_BLIT(storage, oscdata, localcopy)
{
   // FMfilter.storage = storage;
}

osc_wavetable::~osc_wavetable()
{}

void osc_wavetable::init(float pitch, bool is_display)
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

   id_shape = oscdata->p[0].param_id_in_scene;
   id_vskew = oscdata->p[1].param_id_in_scene;
   id_clip = oscdata->p[2].param_id_in_scene;
   id_formant = oscdata->p[3].param_id_in_scene;
   id_hskew = oscdata->p[4].param_id_in_scene;
   id_detune = oscdata->p[5].param_id_in_scene;

   float rate = 0.05;
   l_shape.setRate(rate);
   l_clip.setRate(rate);
   l_vskew.setRate(rate);
   l_hskew.setRate(rate);

   n_unison = limit_range(oscdata->p[6].val.i, 1, max_unison);
   if (oscdata->wt.flags & wtf_is_sample)
   {
      sampleloop = n_unison;
      n_unison = 1;
   }
   if (is_display)
      n_unison = 1;

   prepare_unison(n_unison);

   memset(oscbuffer, 0, sizeof(float) * (ob_length + FIRipol_N));
   memset(oscbufferR, 0, sizeof(float) * (ob_length + FIRipol_N));
   memset(last_level, 0, max_unison * sizeof(float));

   pitch_last = pitch;
   pitch_t = pitch;
   update_lagvals<true>();

   float shape = oscdata->p[0].val.f;
   float intpart;
   shape *= ((float)oscdata->wt.n_tables - 1.f) * 0.99999f;
   tableipol = modff(shape, &intpart);
   tableid = limit_range((int)intpart, 0, (int)oscdata->wt.n_tables - 2);
   last_tableipol = tableipol;
   last_tableid = tableid;
   hskew = 0.f;
   last_hskew = 0.f;
   if (oscdata->wt.flags & wtf_is_sample)
   {
      tableipol = 0.f;
      tableid -= 1;
   }

   int i;
   for (i = 0; i < n_unison; i++)
   {
      {
         float s = 0.f;
         oscstate[i] = 0;
         if (oscdata->retrigger.val.b)
            s = 0.f; //(oscdata->startphase.val.f) * (float)oscdata->wt.size;
         else if (!is_display)
         {
            float drand = (float)rand() / RAND_MAX;
            oscstate[i] = drand; // * (float)oscdata->wt.size;
         }

         state[i] = 0; //((int)s) & (oscdata->wt.size-1);
      }
      last_level[i] = 0.0;
      mipmap[i] = 0;
      mipmap_ofs[i] = 0;
      driftlfo2[i] = 0.f;
      driftlfo[i] = 0.f;
   }
}

void osc_wavetable::init_ctrltypes()
{
   oscdata->p[0].set_name("Shape");
   oscdata->p[0].set_type(ct_percent);
   oscdata->p[1].set_name("Skew V");
   oscdata->p[1].set_type(ct_percent_bidirectional);
   oscdata->p[2].set_name("Saturate");
   oscdata->p[2].set_type(ct_percent);
   oscdata->p[3].set_name("Formant");
   oscdata->p[3].set_type(ct_syncpitch);
   oscdata->p[4].set_name("Skew H");
   oscdata->p[4].set_type(ct_percent_bidirectional);
   oscdata->p[5].set_name("Uni Spread");
   oscdata->p[5].set_type(ct_oscspread);
   oscdata->p[6].set_name("Uni Count");
   oscdata->p[6].set_type(ct_osccountWT);
}
void osc_wavetable::init_default_values()
{
   oscdata->p[0].val.f = 0.0f;
   oscdata->p[1].val.f = 0.0f;
   oscdata->p[2].val.f = 0.f;
   oscdata->p[3].val.f = 0.f;
   oscdata->p[4].val.f = 0.f;
   oscdata->p[5].val.f = 0.2f;
   oscdata->p[6].val.i = 1;
}

float osc_wavetable::distort_level(float x)
{
   float a = l_vskew.v * 0.5;
   float clip = l_clip.v;

   x = x - a * x * x + a;

   // x = limit_range(x*(1+3*clip),-1,1);
   x = limit_range(x * (1 - clip) + clip * x * x * x, -1.f, 1.f);

   return x;
}

void osc_wavetable::convolute(int voice, bool FM, bool stereo)
{
   float block_pos = oscstate[voice] * block_size_os_inv * pitchmult_inv;

   double detune = drift * driftlfo[voice];
   if (n_unison > 1)
      detune += localcopy[id_detune].f * (detune_bias * float(voice) + detune_offset);

   // int ipos = (large+oscstate[voice])>>16;
   const float p24 = (1 << 24);
   unsigned int ipos;
   if (FM)
      ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv * FMmul_inv));
   else
      ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv));

   if (state[voice] == 0)
   {
      formant_last = formant_t;
      last_hskew = hskew;
      hskew = l_hskew.v;

      if (oscdata->wt.flags & wtf_is_sample)
      {
         tableid++;
         if (tableid > oscdata->wt.n_tables - 3)
         {
            if (sampleloop < 7)
               sampleloop--;

            if (sampleloop > 0)
            {
               tableid = 0;
            }
            else
            {
               tableid = oscdata->wt.n_tables - 2;
               oscstate[voice] = 100000000000.f; // rather large number
               return;
            }
         }
      }

      int ts = oscdata->wt.size;
      float a = oscdata->wt.dt * pitchmult_inv;

      const float wtbias = 1.8f;

      mipmap[voice] = 0;

      if ((a < 0.015625 * wtbias) && (ts >= 128))
         mipmap[voice] = 6;
      else if ((a < 0.03125 * wtbias) && (ts >= 64))
         mipmap[voice] = 5;
      else if ((a < 0.0625 * wtbias) && (ts >= 32))
         mipmap[voice] = 4;
      else if ((a < 0.125 * wtbias) && (ts >= 16))
         mipmap[voice] = 3;
      else if ((a < 0.25 * wtbias) && (ts >= 8))
         mipmap[voice] = 2;
      else if ((a < 0.5 * wtbias) && (ts >= 4))
         mipmap[voice] = 1;

      // wt_inc = (1<<mipmap[i]);
      mipmap_ofs[voice] = 0;
      for (int i = 0; i < mipmap[voice]; i++)
         mipmap_ofs[voice] += (ts >> i);
   }

   // generate pulse
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

   float g, gR;
   int wt_inc = (1 << mipmap[voice]);
   // int wt_ofs = mipmap_ofs[voice];
   float dt = (oscdata->wt.dt) * wt_inc;

   // add time until next statechange
   float tempt;
   if (oscdata->p[5].absolute)
      tempt = note_to_pitch_inv(detune * pitchmult_inv * (1.f / 440.f));
   else
      tempt = note_to_pitch_inv(detune);
   float t;
   float xt = ((float)state[voice] + 0.5f) * dt;
   // xt = (1 - hskew + 2*hskew*xt);
   // xt = (1 + hskew *sin(xt*2.0*M_PI));
   // 1 + a.*(1 - 2.*x + (2.*x-1).^3).*sqrt(27/4) = 1 + 4*x*a*(x-1)*(2x-1)
   const float taylorscale = sqrt((float)27.f / 4.f);
   xt = 1.f + hskew * 4.f * xt * (xt - 1.f) * (2.f * xt - 1.f) * taylorscale;

   // t = dt * tempt;
   /*while (t<0.5 && (wt_inc < wavetable_steps))
   {
           wt_inc = wt_inc << 1;
           t = dt * tempt * wt_inc;
   }	*/
   float ft = block_pos * formant_t + (1.f - block_pos) * formant_last;
   float formant = note_to_pitch(-ft);
   dt *= formant * xt;

   // if(state[voice] >= (oscdata->wt.size-wt_inc)) dt += (1-formant);
   int wtsize = oscdata->wt.size >> mipmap[voice];
   if (state[voice] >= (wtsize - 1))
      dt += (1 - formant);
   t = dt * tempt;

   state[voice] = state[voice] & (wtsize - 1);
   float tblip_ipol = (1 - block_pos) * last_tableipol + block_pos * tableipol;
   // float newlevel = distort_level(
   // oscdata->wt.table[tableid][wt_ofs+state[voice]]*(1.f-tblip_ipol) +
   // oscdata->wt.table[tableid+1][wt_ofs+state[voice]]*tblip_ipol );
   float newlevel = distort_level(
       oscdata->wt.TableF32WeakPointers[mipmap[voice]][tableid][state[voice]] * (1.f - tblip_ipol) +
       oscdata->wt.TableF32WeakPointers[mipmap[voice]][tableid + 1][state[voice]] * tblip_ipol);

   g = newlevel - last_level[voice];
   last_level[voice] = newlevel;

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
         //		assert((void*)obf < (void*)(storage-3));
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

   rate[voice] = t;

   oscstate[voice] += rate[voice];
   oscstate[voice] = max(0.f, oscstate[voice]);
   // state[voice] = (state[voice]+wt_inc)&(oscdata->wt.size-wt_inc);
   state[voice] = (state[voice] + 1) & ((oscdata->wt.size >> mipmap[voice]) - 1);
}

template <bool is_init> void osc_wavetable::update_lagvals()
{
   l_vskew.newValue(limit_range(localcopy[id_vskew].f, -1.f, 1.f));
   l_hskew.newValue(limit_range(localcopy[id_hskew].f, -1.f, 1.f));
   float a = limit_range(localcopy[id_clip].f, 0.f, 1.f);
   l_clip.newValue(-8 * a * a * a);
   l_shape.newValue(limit_range(localcopy[id_shape].f, 0.f, 1.f));
   formant_t = max(0.f, localcopy[id_formant].f);

   float invt = min(1.0, (8.175798915 * note_to_pitch(pitch_t)) * dsamplerate_os_inv);
   float hpf2 =
       min(integrator_hpf, powf(hpf_cycle_loss, 4 * invt)); // TODO !!! ACHTUNG! gör lookup-table

   hpf_coeff.newValue(hpf2);
   integrator_mult.newValue(invt);

   li_hpf.set_target(hpf2);

   if (is_init)
   {
      hpf_coeff.instantize();
      integrator_mult.instantize();
      l_shape.instantize();
      l_vskew.instantize();
      l_hskew.instantize();
      l_clip.instantize();

      formant_last = formant_t;
   }
}

void osc_wavetable::process_block(float pitch0, float drift, bool stereo, bool FM, float depth)
{
   pitch_last = pitch_t;
   pitch_t = min(148.f, pitch0);
   pitchmult_inv = max(1.0, dsamplerate_os * (1 / 8.175798915) * note_to_pitch_inv(pitch_t));
   pitchmult =
       1.f /
       pitchmult_inv; // denna måste vara en riktig division, reciprocal-approx är inte precis nog
   this->drift = drift;
   int k, l;

   update_lagvals<false>();
   l_shape.process();
   l_vskew.process();
   l_hskew.process();
   l_clip.process();

   if ((oscdata->wt.n_tables == 1) ||
       (tableid >= oscdata->wt.n_tables)) // tableid-range kan ha ändrats under tiden, gör koll
   {
      tableipol = 0.f;
      tableid = 0;
      last_tableid = 0;
      last_tableipol = 0.f;
   }
   else if (oscdata->wt.flags & wtf_is_sample)
   {
      tableipol = 0.f;
      last_tableipol = 0.f;
   }
   else
   {
      last_tableipol = tableipol;
      last_tableid = tableid;

      float shape = l_shape.v;
      float intpart;
      shape *= ((float)oscdata->wt.n_tables - 1.f) * 0.99999f;
      tableipol = modff(shape, &intpart);
      tableid = limit_range((int)intpart, 0, (int)oscdata->wt.n_tables - 2);

      if (tableid > last_tableid)
      {
         if (last_tableipol != 1.f)
         {
            tableid = last_tableid;
            tableipol = 1.f;
         }
         else
            last_tableipol = 0.0f;
      }
      else if (tableid < last_tableid)
      {
         if (last_tableipol != 0.f)
         {
            tableid = last_tableid;
            tableipol = 0.f;
         }
         else
            last_tableipol = 1.0f;
      }
   }

   /*wt_inc = 1;
   float a = oscdata->wt.dt * pitchmult_inv;
   if(a < 0.125) wt_inc = 8;
   else if(a < 0.25) wt_inc = 4;
   else if(a < 0.5) wt_inc = 2;*/

   if (FM)
   {
      /*FMfilter.coeff_HP(FMfilter.calc_omega(pitch_t - 24.f - 48.f *
      oscdata->p[id_detune].val.f),0.707); FMfilter.process_block(&master_osc[0]);
      FMfilter.process_block(&master_osc[block_size]);*/

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
         while (oscstate[l] < a)
            convolute(l, false, stereo);
         oscstate[l] -= a;
      }
      // li_DC.set_target(dc);
   }

   _MM_ALIGN16 float hpfblock[block_size_os];
   li_hpf.store_block(hpfblock, block_size_os_quad);

#if PPC
   for (k = 0; k < block_size_os; k++)
   {
      osc_out = osc_out * hpfblock[k] + oscbuffer[bufpos + k];
      output[k] = osc_out;
      if (stereo)
      {
         osc_outR = osc_outR * hpfblock[k] + oscbufferR[bufpos + k];
         outputR[k] = osc_outR;
      }
   }
#else
   for (k = 0; k < block_size_os; k++)
   {
      __m128 hpf = _mm_load_ss(&hpfblock[k]);
      __m128 ob = _mm_load_ss(&oscbuffer[bufpos + k]);
      __m128 a = _mm_mul_ss(osc_out, hpf);
      osc_out = _mm_add_ss(a, ob);
      _mm_store_ss(&output[k], osc_out);
      if (stereo)
      {
         __m128 ob = _mm_load_ss(&oscbufferR[bufpos + k]);
         __m128 a = _mm_mul_ss(osc_outR, hpf);
         osc_outR = _mm_add_ss(a, ob);
         _mm_store_ss(&outputR[k], osc_outR);
      }
   }
#endif

   clear_block(&oscbuffer[bufpos], block_size_os_quad);
   if (stereo)
      clear_block(&oscbufferR[bufpos], block_size_os_quad);

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