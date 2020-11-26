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

#pragma once

#include "OscillatorBase.h"
#include "DspUtilities.h"
#include <vt_dsp/lipol.h>
#include "BiquadFilter.h"

class SineOscillator : public Oscillator
{
public:
   enum sine_params
   {
      sin_shape,
      sin_feedback,
      sin_FMmode,
      sin_lowcut,
      sin_highcut,
      sin_unison_detune,
      sin_unison_voices,
   };

   SineOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual void process_block_legacy(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~SineOscillator();
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;

   quadr_osc sinus[MAX_UNISON];
   double phase[MAX_UNISON];
   float driftlfo[MAX_UNISON], driftlfo2[MAX_UNISON];
   float fb_val;
   float playingramp[MAX_UNISON], dplaying;
   lag<double> FMdepth;
   lag<double> FB;
   void prepare_unison(int voices);
   int n_unison;
   float out_attenuation, out_attenuation_inv, detune_bias, detune_offset;
   float panL[MAX_UNISON], panR[MAX_UNISON];

   int id_mode, id_fb, id_fmlegacy, id_detune;
   float lastvalue[MAX_UNISON];

   BiquadFilter lp, hp;
   void applyFilter();
   
   inline float valueFromSinAndCos(float svalue, float cvalue ) {
      return valueFromSinAndCos(svalue, cvalue, localcopy[id_mode].i );
   }
   static float valueFromSinAndCos(float svalue, float cvalue, int mode);
   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;
};
