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

#include "AudioInputOscillator.h"

/* audio input osc */

/* add controls:
input L/R
gain
limiter?
*/

AudioInputOscillator::AudioInputOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{
   // in case of more scenes being added, design a solution for audio in oscillator extra input!
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

void AudioInputOscillator::init(float pitch, bool is_display)
{
   hp.coeff_instantize();
   lp.coeff_instantize();

   hp.coeff_HP(hp.calc_omega(oscdata->p[5].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
   lp.coeff_LP2B(lp.calc_omega(oscdata->p[6].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
}

AudioInputOscillator::~AudioInputOscillator()
{
   if( storage )
      storage->otherscene_clients --;
}

void AudioInputOscillator::init_ctrltypes(int scene, int osc)
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

void AudioInputOscillator::init_default_values()
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

void AudioInputOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
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

void AudioInputOscillator::applyFilter()
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

void AudioInputOscillator::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision <= 12)
   {
      oscdata->p[5].val.f = oscdata->p[5].val_min.f; // high cut at the bottom
      oscdata->p[5].deactivated = true;
      oscdata->p[6].val.f = oscdata->p[6].val_max.f; // low cut at the top
      oscdata->p[6].deactivated = true;
   }
}

