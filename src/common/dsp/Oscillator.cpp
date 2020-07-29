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
#include "DspUtilities.h"
#include "FastMath.h"
#include <cmath>

using namespace std;

Oscillator*
spawn_osc(int osctype, SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
{
   Oscillator* osc = 0;
   switch (osctype)
   {
   case ot_classic:
      return new SurgeSuperOscillator(storage, oscdata, localcopy);
   case ot_wavetable:
      return new WavetableOscillator(storage, oscdata, localcopy);
   case ot_WT2:
   {
      // In the event we are misconfigured, window will segv. If you still play
      // after clicking through 100 warnings, lets jsut give youa sin
      if( storage && storage->WindowWT.size == 0 )
         return new osc_sine( storage, oscdata, localcopy );
 
      return new WindowOscillator(storage, oscdata, localcopy);
   }
   case ot_shnoise:
      return new SampleAndHoldOscillator(storage, oscdata, localcopy);
   case ot_audioinput:
      return new osc_audioinput(storage, oscdata, localcopy);
   case ot_FM:
      return new FMOscillator(storage, oscdata, localcopy);
   case ot_FM2:
      return new FM2Oscillator(storage, oscdata, localcopy);
   case ot_sinus:
   default:
      return new osc_sine(storage, oscdata, localcopy);
   }
   return osc;
}

Oscillator::Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : master_osc(0)
{
   // assert(storage);
   assert(oscdata);
   this->storage = storage;
   this->oscdata = oscdata;
   this->localcopy = localcopy;
   ticker = 0;
}

Oscillator::~Oscillator()
{}

/* sine osc */

osc_sine::osc_sine(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
   : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{}

void osc_sine::prepare_unison(int voices)
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

void osc_sine::init(float pitch, bool is_display)
{
   n_unison = limit_range(oscdata->p[6].val.i, 1, MAX_UNISON);
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

   id_mode = oscdata->p[0].param_id_in_scene;
   id_fb = oscdata->p[1].param_id_in_scene;
   id_fmlegacy = oscdata->p[2].param_id_in_scene;
   id_detune = oscdata->p[5].param_id_in_scene;

   hp.coeff_instantize();
   lp.coeff_instantize();

   hp.coeff_HP(hp.calc_omega(oscdata->p[3].val.f / 12.0 ) / OSC_OVERSAMPLING, 0.707);
   lp.coeff_LP2B(lp.calc_omega(oscdata->p[4].val.f / 12.0 ) / OSC_OVERSAMPLING, 0.707);
}

osc_sine::~osc_sine()
{}

void osc_sine::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   if( localcopy[id_fmlegacy].i == 0 )
   {
       process_block_legacy(pitch, drift, stereo, FM, fmdepth );
       applyFilter();
       return;
   }
   
   fb_val = oscdata->p[1].get_extended(localcopy[id_fb].f);

   double detune;
   double omega[MAX_UNISON];

   for (int l = 0; l < n_unison; l++)
   {
      driftlfo[l] = drift_noise(driftlfo2[l]);
      detune = drift * driftlfo[l];

      if (n_unison > 1)
      {
         if( oscdata->p[5].absolute )
         {
            detune += oscdata->p[5].get_extended(localcopy[oscdata->p[5].param_id_in_scene].f)
               * storage->note_to_pitch_inv_ignoring_tuning( std::min( 148.f, pitch ) ) * 16 / 0.9443  * (detune_bias * float(l) + detune_offset);;
         }
         else
         {
            detune += oscdata->p[5].get_extended(localcopy[id_detune].f) * (detune_bias * float(l) + detune_offset);
         }
      }

      omega[l] = min(M_PI, (double)pitch_to_omega(pitch + detune));
   }
   
   FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);
   FB.newValue(abs(fb_val));

   for (int k = 0; k < BLOCK_SIZE_OS; k++)
   {
      float outL = 0.f, outR = 0.f;

      for (int u = 0; u < n_unison; u++)
      {
          // Replicate FM2 exactly
          auto p = phase[u] + lastvalue[u];
            
          if (FM)
             p += FMdepth.v * master_osc[k];

          // Unlike ::sin and ::cos we are limited in accurate range
          p = Surge::DSP::clampToPiRange(p);
          
          float out_local = valueFromSinAndCos(Surge::DSP::fastsin(p), Surge::DSP::fastcos(p));

          outL += (panL[u] * out_local) * playingramp[u] * out_attenuation;
          outR += (panR[u] * out_local) * playingramp[u] * out_attenuation;

          if( playingramp[u] < 1 )
             playingramp[u] += dplaying;
          if( playingramp[u] > 1 )
             playingramp[u] = 1;
          
          phase[u] += omega[u];
          // phase in range -PI to PI
          while ( phase[u] > M_PI )
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
         outputR[k]= outR;
      }
      else
         output[k] = (outL + outR) / 2;
   }
   applyFilter();
}

void osc_sine::applyFilter()
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

void osc_sine::process_block_legacy(float pitch, float drift, bool stereo, bool FM, float fmdepth)
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
            if( oscdata->p[5].absolute )
            {
               detune += oscdata->p[5].get_extended(localcopy[oscdata->p[5].param_id_in_scene].f)
                  * storage->note_to_pitch_inv_ignoring_tuning( std::min( 148.f, pitch ) ) * 16 / 0.9443  * (detune_bias * float(l) + detune_offset);;
            }
            else
            {
               detune += oscdata->p[5].get_extended(localcopy[id_detune].f) * (detune_bias * float(l) + detune_offset);
            }
         }

         omega[l] = min(M_PI, (double)pitch_to_omega(pitch + detune));
      }

      FMdepth.newValue(fmdepth);

      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         float outL = 0.f, outR = 0.f;

         for (int u = 0; u < n_unison; u++)
         {
             float out_local = valueFromSinAndCos(Surge::DSP::fastsin(phase[u]), Surge::DSP::fastcos(phase[u]));

             outL += (panL[u] * out_local) * out_attenuation * playingramp[u];
             outR += (panR[u] * out_local) * out_attenuation * playingramp[u];


             if( playingramp[u] < 1 )
                playingramp[u] += dplaying;
             if( playingramp[u] > 1 )
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
            detune += oscdata->p[5].get_extended(localcopy[id_detune].f) * (detune_bias * float(l) + detune_offset);

         omega[l] = min(M_PI, (double)pitch_to_omega(pitch + detune));
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

             
             if( playingramp[u] < 1 )
                playingramp[u] += dplaying;
             if( playingramp[u] > 1 )
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

float osc_sine::valueFromSinAndCos(float sinx, float cosx, int wfMode)
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

   //   0, 4, 9, 10, 6, 11, 12, 13,  1, 2,  3,  5,  7,  8, 14, 15, 16, 17, 18, 19

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

void osc_sine::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if( streamingRevision <= 9 )
   {
      oscdata->p[0].val.i = oscdata->p[0].val_min.i;
   }
   if( streamingRevision <= 10 )
   {
      oscdata->p[1].val.f = 0;
      oscdata->p[2].val.i = 0;
   }
   if( streamingRevision <= 12 )
   {
      oscdata->p[3].val.f = oscdata->p[3].val_min.f; // high cut at the bottom
      oscdata->p[3].deactivated = true;
      oscdata->p[4].val.f = oscdata->p[4].val_max.f; // low cut at the top
      oscdata->p[4].deactivated = true;
      oscdata->p[1].set_type(ct_osc_feedback);

      int wave_remap[] = {0, 8, 9, 10, 1, 11, 4, 12, 13, 2, 3, 5, 6, 7, 14, 15, 16, 17, 18, 19};

      // range checking for garbage data
      if (oscdata->p[0].val.i < 0 || (oscdata->p[0].val.i >= (sizeof wave_remap) / sizeof *wave_remap))
         oscdata->p[0].val.i = oscdata->p[0].val_min.i;
      else
      {
         // make sure old patches still point to the correct waveforms
         oscdata->p[0].val.i = wave_remap[oscdata->p[0].val.i];
      }
   }
}

void osc_sine::init_ctrltypes()
{
   oscdata->p[0].set_name("Shape");
   oscdata->p[0].set_type(ct_sineoscmode);

   oscdata->p[1].set_name("Feedback");
   oscdata->p[1].set_type(ct_osc_feedback_negative);

   oscdata->p[2].set_name("FM Behaviour");
   oscdata->p[2].set_type(ct_sinefmlegacy);

   oscdata->p[3].set_name("Low Cut");
   oscdata->p[3].set_type(ct_freq_audible_deactivatable);

   oscdata->p[4].set_name("High Cut");
   oscdata->p[4].set_type(ct_freq_audible_deactivatable);

   oscdata->p[5].set_name("Unison Detune");
   oscdata->p[5].set_type(ct_oscspread);

   oscdata->p[6].set_name("Unison Voices");
   oscdata->p[6].set_type(ct_osccount);
}

void osc_sine::init_default_values()
{
   oscdata->p[0].val.i = 0;
   oscdata->p[1].val.f = 0;
   oscdata->p[2].val.i = 1;
   
   oscdata->p[3].val.f = oscdata->p[3].val_min.f; // high cut at the bottom
   oscdata->p[3].deactivated = true;
   oscdata->p[4].val.f = oscdata->p[4].val_max.f; // low cut at the top
   oscdata->p[4].deactivated = true;
   
   oscdata->p[5].val.f = 0.2;
   oscdata->p[6].val.i = 1;
}
/* audio input osc */

/* add controls:
input L/R
gain
limiter?
*/

osc_audioinput::osc_audioinput(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{
   isInSceneB = false;
   if( storage )
   {
      storage->otherscene_clients ++;
      bool isSB = false;
      for( int i=0; i<n_oscs; ++i )
         if( &(storage->getPatch().scene[1].osc[i]) == oscdata )
            isSB = true;

      isInSceneB = isSB;
   }
}

void osc_audioinput::init(float pitch, bool is_display)
{
   hp.coeff_instantize();
   lp.coeff_instantize();

   hp.coeff_HP(hp.calc_omega(oscdata->p[5].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
   lp.coeff_LP2B(lp.calc_omega(oscdata->p[6].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
}

osc_audioinput::~osc_audioinput()
{
   if( storage )
      storage->otherscene_clients --;
}

void osc_audioinput::init_ctrltypes(int scene, int osc)
{
   oscdata->p[0].set_name("Audio In L/R Channel");
   oscdata->p[0].set_type(ct_percent_bidirectional_stereo);
   oscdata->p[1].set_name("Audio In Gain");
   oscdata->p[1].set_type(ct_decibel);
   if( scene == 1 )
   {
      oscdata->p[2].set_name("Scene A L/R Channel");
      oscdata->p[2].set_type(ct_percent_bidirectional_stereo);
      oscdata->p[3].set_name("Scene A Gain");
      oscdata->p[3].set_type(ct_decibel);
      oscdata->p[4].set_name("Audio In<>Scene A Mix");
      oscdata->p[4].set_type(ct_percent);
   }
   oscdata->p[5].set_name("Low Cut");
   oscdata->p[5].set_type(ct_freq_audible_deactivatable);
   oscdata->p[6].set_name("High Cut");
   oscdata->p[6].set_type(ct_freq_audible_deactivatable);
}

void osc_audioinput::init_default_values()
{
   oscdata->p[0].val.f = 0.0f;
   oscdata->p[1].val.f = 0.0f;
   if( isInSceneB )
   {
      oscdata->p[2].val.f = 0.0f;
      oscdata->p[3].val.f = 0.0f;
      oscdata->p[4].val.f = 0.0f;
   }
   oscdata->p[5].val.f = oscdata->p[5].val_min.f; // high cut at the bottom
   oscdata->p[5].deactivated = true;
   oscdata->p[6].val.f = oscdata->p[6].val_max.f; // low cut at the top
   oscdata->p[6].deactivated = true;
}

void osc_audioinput::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
   bool useOtherScene = false;
   if( isInSceneB && localcopy[oscdata->p[4].param_id_in_scene].f > 0.f )
   {
      useOtherScene = true;
   }

   float inGain = db_to_linear(localcopy[oscdata->p[1].param_id_in_scene].f);
   float inChMix = limit_range(localcopy[oscdata->p[0].param_id_in_scene].f, -1.f, 1.f);
   float sceneGain = db_to_linear(localcopy[oscdata->p[3].param_id_in_scene].f);
   float sceneChMix = limit_range(localcopy[oscdata->p[2].param_id_in_scene].f, -1.f, 1.f);
   float sceneMix = localcopy[oscdata->p[4].param_id_in_scene].f;
   float inverseMix = 1.f - sceneMix;

   float l = inGain * (1.f - inChMix);
   float r = inGain * (1.f + inChMix);
 
            
   float sl = sceneGain * (1.f - sceneChMix);
   float sr = sceneGain * (1.f + sceneChMix);

   if (stereo)
   {
      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         if( useOtherScene )
         {
            output[k] = (l * storage->audio_in[0][k] * inverseMix) + (sl * storage->audio_otherscene[0][k] * sceneMix);
            outputR[k] = (r * storage->audio_in[1][k] * inverseMix) + (sr * storage->audio_otherscene[1][k] * sceneMix);
         }
         else
         {
            output[k] = l * storage->audio_in[0][k];
            outputR[k] = r * storage->audio_in[1][k];
         }
      }
   }
   else
   {
      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         if( useOtherScene )
         {
            output[k] = (((l * storage->audio_in[0][k]) + (r * storage->audio_in[1][k])) * inverseMix) +
                        (((sl * storage->audio_otherscene[0][k]) + (sr * storage->audio_otherscene[1][k])) * sceneMix);
         }
         else
         {
            output[k] = l * storage->audio_in[0][k] + r * storage->audio_in[1][k];
         }
      }
   }

   applyFilter();
}

void osc_audioinput::applyFilter()
{
   if (!oscdata->p[5].deactivated)
      hp.coeff_HP(hp.calc_omega(localcopy[oscdata->p[5].param_id_in_scene].f / 12.0) / OSC_OVERSAMPLING, 0.707);
   if (!oscdata->p[6].deactivated)
      lp.coeff_LP2B(lp.calc_omega(localcopy[oscdata->p[6].param_id_in_scene].f / 12.0) / OSC_OVERSAMPLING, 0.707);

   for (int k = 0; k < BLOCK_SIZE_OS; k += BLOCK_SIZE)
   {
      if (!oscdata->p[5].deactivated)
         hp.process_block(&(output[k]), &(outputR[k]));
      if (!oscdata->p[6].deactivated)
         lp.process_block(&(output[k]), &(outputR[k]));
   }
}

void osc_audioinput::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision <= 12)
   {
      oscdata->p[5].val.f = oscdata->p[5].val_min.f; // high cut at the bottom
      oscdata->p[5].deactivated = true;
      oscdata->p[6].val.f = oscdata->p[6].val_max.f; // low cut at the top
      oscdata->p[6].deactivated = true;
   }
}
