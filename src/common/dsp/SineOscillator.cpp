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

#include "SineOscillator.h"
#include "FastMath.h"
#include <algorithm>

SineOscillator::SineOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
   : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{}

void SineOscillator::prepare_unison(int voices)
{
   out_attenuation_inv = sqrt((float)voices);
   out_attenuation = 0.8 / out_attenuation_inv + 0.2 / voices;
   dplaying = 1.0 / 50.0 * 44100 / samplerate; // normalize to be sample rate independent amount of time for 50 441k samples
   
   if (voices == 1)
   {
      detune_bias = 1;
      detune_offset = 0;
      panL[0] = 1.f;
      panR[0] = 1.f;
      playingramp[0] = 1;
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
      
         playingramp[i] = 0;
      }
      playingramp[0] = 1;
   }
}

void SineOscillator::init(float pitch, bool is_display)
{
   n_unison = limit_range(oscdata->p[sin_unison_voices].val.i, 1, MAX_UNISON);
   if (is_display)
      n_unison = 1;
   prepare_unison(n_unison);

   for (int i = 0; i < n_unison; i++)
   {
      if (i > 0)
         phase[i] = 2.0 * M_PI * rand() / RAND_MAX - M_PI; // phase in range -PI to PI
      else
         phase[i] = 0.f;
      lastvalue[i] = 0.f;
      driftlfo[i] = 0.f;
      driftlfo2[i] = 0.f;
      sinus[i].set_phase(phase[i]);
   }

   fb_val = 0.f;

   id_mode = oscdata->p[sin_shape].param_id_in_scene;
   id_fb = oscdata->p[sin_feedback].param_id_in_scene;
   id_fmlegacy = oscdata->p[sin_FMmode].param_id_in_scene;
   id_detune = oscdata->p[sin_unison_detune].param_id_in_scene;

   hp.coeff_instantize();
   lp.coeff_instantize();

   hp.coeff_HP(hp.calc_omega(oscdata->p[sin_lowcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
   lp.coeff_LP2B(lp.calc_omega(oscdata->p[sin_highcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
}

SineOscillator::~SineOscillator()
{}

void SineOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   if (localcopy[id_fmlegacy].i == 0)
   {
      process_block_legacy(pitch, drift, stereo, FM, fmdepth);
      applyFilter();
      return;
   }

   fb_val = oscdata->p[sin_feedback].get_extended(localcopy[id_fb].f);

   double detune;
   double omega[MAX_UNISON];

   for (int l = 0; l < n_unison; l++)
   {
      driftlfo[l] = drift_noise(driftlfo2[l]);
      detune = drift * driftlfo[l];

      if (n_unison > 1)
      {
         if (oscdata->p[sin_unison_detune].absolute)
         {
            detune += oscdata->p[sin_unison_detune].get_extended(localcopy[oscdata->p[sin_unison_detune].param_id_in_scene].f) *
                      storage->note_to_pitch_inv_ignoring_tuning(std::min(148.f, pitch)) * 16 / 0.9443 * (detune_bias * float(l) + detune_offset);
         }
         else
         {
            detune += oscdata->p[sin_unison_detune].get_extended(localcopy[id_detune].f) * (detune_bias * float(l) + detune_offset);
         }
      }

      omega[l] = std::min(M_PI, (double)pitch_to_omega(pitch + detune));
   }

   float fv = 32.0 * M_PI * fmdepth * fmdepth * fmdepth;

   /*
   ** See issue 2619. At worst case we move phase by fv * 1. Since we
   ** need phase to be in -Pi,Pi, that means if fv / Pi > 1e5 or so
   ** we have float precision problems. So lets clamp fv.
   */
   fv = limit_range(fv, -1.0e6f, 1.0e6f);

   FMdepth.newValue(fv);
   FB.newValue(abs(fb_val));

   for (int k = 0; k < BLOCK_SIZE_OS; k++)
   {
      float outL = 0.f, outR = 0.f;

      for (int u = 0; u < n_unison; u++)
      {
         // Replicate FM2 exactly
         float p = phase[u] + lastvalue[u];

         if (FM)
            p += FMdepth.v * master_osc[k];

         // Unlike ::sin and ::cos we are limited in accurate range
         p = Surge::DSP::clampToPiRange(p);

         float out_local = valueFromSinAndCos(Surge::DSP::fastsin(p), Surge::DSP::fastcos(p));

         outL += (panL[u] * out_local) * playingramp[u] * out_attenuation;
         outR += (panR[u] * out_local) * playingramp[u] * out_attenuation;

         if (playingramp[u] < 1)
            playingramp[u] += dplaying;
         if (playingramp[u] > 1)
            playingramp[u] = 1;

         phase[u] += omega[u];
         // phase in range -PI to PI
         while (phase[u] > M_PI)
         {
            phase[u] -= 2.0 * M_PI;
         }

         lastvalue[u] = (fb_val < 0) ? out_local * out_local * FB.v : out_local * FB.v;
      }

      FMdepth.process();
      FB.process();

      if (stereo)
      {
         output[k] = outL;
         outputR[k] = outR;
      }
      else
         output[k] = (outL + outR) / 2;
   }
   applyFilter();
}

void SineOscillator::applyFilter()
{
   if (!oscdata->p[sin_lowcut].deactivated)
   {
      auto par = &(oscdata->p[sin_lowcut]);
      auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
      hp.coeff_HP(hp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
   }

   if (!oscdata->p[sin_highcut].deactivated)
   {
      auto par = &(oscdata->p[sin_highcut]);
      auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
      lp.coeff_LP2B(lp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
   }

   for (int k = 0; k < BLOCK_SIZE_OS; k += BLOCK_SIZE)
   {
      if (!oscdata->p[sin_lowcut].deactivated)
         hp.process_block(&(output[k]), &(outputR[k]));
      if (!oscdata->p[sin_highcut].deactivated)
         lp.process_block(&(output[k]), &(outputR[k]));
   }
}

void SineOscillator::process_block_legacy(
    float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   double detune;
   double omega[MAX_UNISON];

   if (FM)
   {
      for (int l = 0; l < n_unison; l++)
      {
         driftlfo[l] = drift_noise(driftlfo2[l]);
         detune = drift * driftlfo[l];

         if (n_unison > 1)
         {
            if (oscdata->p[sin_unison_detune].absolute)
            {
               detune += oscdata->p[sin_unison_detune].get_extended(localcopy[oscdata->p[sin_unison_detune].param_id_in_scene].f) *
                         storage->note_to_pitch_inv_ignoring_tuning(std::min(148.f, pitch)) * 16 / 0.9443 * (detune_bias * float(l) + detune_offset);
            }
            else
            {
               detune += oscdata->p[sin_unison_detune].get_extended(localcopy[id_detune].f) * (detune_bias * float(l) + detune_offset);
            }
         }

         omega[l] = std::min(M_PI, (double)pitch_to_omega(pitch + detune));
      }

      FMdepth.newValue(fmdepth);

      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         float outL = 0.f, outR = 0.f;

         for (int u = 0; u < n_unison; u++)
         {
            float out_local =
                valueFromSinAndCos(Surge::DSP::fastsin(phase[u]), Surge::DSP::fastcos(phase[u]));

            outL += (panL[u] * out_local) * out_attenuation * playingramp[u];
            outR += (panR[u] * out_local) * out_attenuation * playingramp[u];

            if (playingramp[u] < 1)
               playingramp[u] += dplaying;
            if (playingramp[u] > 1)
               playingramp[u] = 1;

            phase[u] += omega[u] + master_osc[k] * FMdepth.v;
            phase[u] = Surge::DSP::clampToPiRange(phase[u]);
         }

         FMdepth.process();

         if (stereo)
         {
            output[k] = outL;
            outputR[k] = outR;
         }
         else
            output[k] = (outL + outR) / 2;
      }
   }
   else
   {
      for (int l = 0; l < n_unison; l++)
      {
         driftlfo[l] = drift_noise(driftlfo2[l]);

         detune = drift * driftlfo[l];

         if (n_unison > 1)
            detune += oscdata->p[sin_unison_detune].get_extended(localcopy[id_detune].f) * (detune_bias * float(l) + detune_offset);

         omega[l] = std::min(M_PI, (double)pitch_to_omega(pitch + detune));
         sinus[l].set_rate(omega[l]);
      }

      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         float outL = 0.f, outR = 0.f;

         for (int u = 0; u < n_unison; u++)
         {
            sinus[u].process();

            float sinx = sinus[u].r;
            float cosx = sinus[u].i;

            float out_local = valueFromSinAndCos(sinx, cosx);

            outL += (panL[u] * out_local) * out_attenuation * playingramp[u];
            outR += (panR[u] * out_local) * out_attenuation * playingramp[u];

            if (playingramp[u] < 1)
               playingramp[u] += dplaying;
            if (playingramp[u] > 1)
               playingramp[u] = 1;
         }

         if (stereo)
         {
            output[k] = outL;
            outputR[k] = outR;
         }
         else
            output[k] = (outL + outR) / 2;
      }
   }
}

float SineOscillator::valueFromSinAndCos(float sinx, float cosx, int wfMode)
{
   float pvalue = sinx;

   float sin2x = 2 * sinx * cosx;
   float cos2x = 1 - (2 * sinx * sinx);

   int quadrant;
   int octant;

   if (sinx > 0)
      if (cosx > 0)
         quadrant = 1;
      else
         quadrant = 2;
   else if (cosx < 0)
      quadrant = 3;
   else
      quadrant = 4;

   switch (wfMode)
   {
   case 1:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - cosx;
         break;
      case 2:
         pvalue = 1 + cosx;
         break;
      case 3:
         pvalue = -1 - cosx;
         break;
      case 4:
         pvalue = -1 + cosx;
         break;
      }
      break;
   case 2:
      if (quadrant > 2)
         pvalue = 0;
      break;
   case 3:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - cosx;
         break;
      case 2:
         pvalue = 1 + cosx;
         break;
      default:
         pvalue = 0;
         break;
      }
      break;
   case 4:
      if (quadrant <= 2)
         pvalue = sin2x;
      else
         pvalue = 0;
      break;
   case 5:
   case 7:
      if (sin2x > 0)
         if (cos2x > 0)
            octant = 1;
         else
            octant = 2;
      else if (cos2x < 0)
         octant = 3;
      else
         octant = 4;

      if (quadrant > 2)
         octant += 4;

      switch (octant)
      {
      case 1:
      case 5:
         pvalue = 1 - cos2x;
         break;
      case 2:
      case 6:
         pvalue = 1 + cos2x;
         break;
      case 3:
      case 7:
         pvalue = -1 - cos2x;
         break;
      case 4:
      case 8:
         pvalue = -1 + cos2x;
         break;
      }

      if (wfMode == 7)
         pvalue = abs(pvalue);

      if (quadrant > 2)
         pvalue = 0;

      break;
   case 6:
      pvalue = abs(sin2x);
      if (quadrant > 2)
         pvalue = 0;
      break;
   case 8:
      if (quadrant > 2)
         pvalue = 0;
      pvalue = 2 * pvalue - 1;
      break;
   case 9:
      if (quadrant == 1 || quadrant == 3)
         pvalue = 0;
      break;
   case 10:
      if (quadrant == 2 || quadrant == 4)
         pvalue = 0;
      break;
   case 11:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - cosx;
         break;
      case 2:
         pvalue = 1 + cosx;
         break;
      default:
         pvalue = 0;
         break;
      }
      pvalue = 2 * pvalue - 1;
      break;
   case 12:
      pvalue = sin2x;
      if (quadrant == 2 || quadrant == 3)
         pvalue = -pvalue;
      break;
   case 13:
      pvalue = sin2x;
      if (quadrant == 2 || quadrant == 4)
         pvalue = 0;
      if (quadrant == 3)
         pvalue = -pvalue;
      break;
   case 14:
      pvalue = abs(cos2x);
      if (quadrant > 2)
         pvalue = 0;
      break;
   case 15:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - sinx;
         break;
      case 4:
         pvalue = -1 - sinx;
         break;
      default:
         pvalue = 0;
         break;
      }
      break;
   case 16:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - sinx;
         break;
      case 4:
         pvalue = -1 + cosx;
         break;
      default:
         pvalue = 0;
         break;
      }
      break;
   case 17:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - sinx;
         break;
      case 2:
         pvalue = abs(-1 + sinx);
         break;
      case 3:
         pvalue = -1 - sinx;
         break;
      case 4:
         pvalue = -1 * abs(1 + sinx);
         break;
      }
      pvalue *= 0.85;
      break;
   case 18:
      switch (quadrant)
      {
      case 1:
         pvalue = sin2x;
         break;
      case 2:
      case 3:
         pvalue = cosx;
         break;
      case 4:
         pvalue = -sin2x;
         break;
      }
      break;
   case 19:
   {
      float sin4x = 2 * sin2x * cos2x;

      switch (quadrant)
      {
      case 1:
         pvalue = sin4x;
         break;
      case 2:
         pvalue = -sin2x;
         break;
      default:
         pvalue = sinx;
         break;
      }

      break;
   }
   case 20:
      switch (quadrant)
      {
      case 1:
      case 3:
         pvalue = sinx;
         break;
      case 2:
         pvalue = 1;
         break;
      case 4:
         pvalue = -1;
         break;
      }
      break;
   case 21:
      switch (quadrant)
      {
      case 2:
      case 4:
         pvalue = sinx;
         break;
      case 1:
         pvalue = 1;
         break;
      case 3:
         pvalue = -1;
         break;
      }
      break;
   case 22:
      if (quadrant == 2 || quadrant == 3)
      {
         pvalue = 0;
      }
      break;
   case 23:
      if (quadrant == 1 || quadrant == 4)
      {
         pvalue = 0;
      }
      break;

   default:
      break;
   }
   return pvalue;
}

void SineOscillator::handleStreamingMismatches(int streamingRevision,
                                              int currentSynthStreamingRevision)
{
   if (streamingRevision <= 9)
   {
      oscdata->p[sin_shape].val.i = oscdata->p[sin_shape].val_min.i;
   }
   if (streamingRevision <= 10)
   {
      oscdata->p[sin_feedback].val.f = 0;
      oscdata->p[sin_FMmode].val.i = 0;
   }
   if (streamingRevision <= 12)
   {
      oscdata->p[sin_lowcut].val.f = oscdata->p[sin_lowcut].val_min.f; // high cut at the bottom
      oscdata->p[sin_lowcut].deactivated = true;
      oscdata->p[sin_highcut].val.f = oscdata->p[sin_highcut].val_max.f; // low cut at the top
      oscdata->p[sin_highcut].deactivated = true;
      oscdata->p[sin_feedback].set_type(ct_osc_feedback);

      int wave_remap[] = {0, 8, 9, 10, 1, 11, 4, 12, 13, 2, 3, 5, 6, 7, 14, 15, 16, 17, 18, 19};

      // range checking for garbage data
      if (oscdata->p[sin_shape].val.i < 0 || (oscdata->p[sin_shape].val.i >= (sizeof wave_remap) / sizeof *wave_remap))
         oscdata->p[sin_shape].val.i = oscdata->p[sin_shape].val_min.i;
      else
      {
         // make sure old patches still point to the correct waveforms
         oscdata->p[sin_shape].val.i = wave_remap[oscdata->p[sin_shape].val.i];
      }
   }
}

void SineOscillator::init_ctrltypes()
{
   oscdata->p[sin_shape].set_name("Shape");
   oscdata->p[sin_shape].set_type(ct_sineoscmode);

   oscdata->p[sin_feedback].set_name("Feedback");
   oscdata->p[sin_feedback].set_type(ct_osc_feedback_negative);

   oscdata->p[sin_FMmode].set_name("FM Behaviour");
   oscdata->p[sin_FMmode].set_type(ct_sinefmlegacy);

   oscdata->p[sin_lowcut].set_name("Low Cut");
   oscdata->p[sin_lowcut].set_type(ct_freq_audible_deactivatable);

   oscdata->p[sin_highcut].set_name("High Cut");
   oscdata->p[sin_highcut].set_type(ct_freq_audible_deactivatable);

   oscdata->p[sin_unison_detune].set_name("Unison Detune");
   oscdata->p[sin_unison_detune].set_type(ct_oscspread);

   oscdata->p[sin_unison_voices].set_name("Unison Voices");
   oscdata->p[sin_unison_voices].set_type(ct_osccount);
}

void SineOscillator::init_default_values()
{
   oscdata->p[sin_shape].val.i = 0;
   oscdata->p[sin_feedback].val.f = 0;
   oscdata->p[sin_FMmode].val.i = 1;

   oscdata->p[sin_lowcut].val.f = oscdata->p[sin_lowcut].val_min.f; // high cut at the bottom
   oscdata->p[sin_lowcut].deactivated = true;
   oscdata->p[sin_highcut].val.f = oscdata->p[sin_highcut].val_max.f; // low cut at the top
   oscdata->p[sin_highcut].deactivated = true;
   
   oscdata->p[sin_unison_detune].val.f = 0.2;
   oscdata->p[sin_unison_voices].val.i = 1;
}
