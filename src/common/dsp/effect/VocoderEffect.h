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
#include "Effect.h"
#include "BiquadFilter.h"
#include "DspUtilities.h"
#include "AllpassFilter.h"

#include "VectorizedSvfFilter.h"

#include <vt_dsp/halfratefilter.h>
#include <vt_dsp/lipol.h>

const int n_vocoder_bands = 20;
const int NVocoderVec = n_vocoder_bands >> 2;

enum VocoderModInput
 {
    VOCODER_MODULATOR_MONO,
    VOCODER_MODULATOR_L,
    VOCODER_MODULATOR_R,
    VOCODER_MODULATOR_STEREO
 };

class VocoderEffect : public Effect
{
public:
 

   enum Params
   {
      KGain,
      KGateLevel,
      KRate,
      KQuality,
      KShift,

      kNumBands,
      kFreqLo,
      kFreqHi,
      kModulatorMode,
      kModExpand,
      kModCenter,
      kMix,
      kNumParams
      
      // KUnvoicedThreshold,
   };

   VocoderEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~VocoderEffect();
   virtual const char* get_effectname() override
   {
      return "vocoder";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override; 
   virtual void suspend() override;
   virtual int get_ringout_decay() override
   {
      return 500;
   }
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;

private:
   VectorizedSvfFilter mCarrierL alignas(16)[NVocoderVec];
   VectorizedSvfFilter mCarrierR alignas(16)[NVocoderVec];
   VectorizedSvfFilter mModulator alignas(16)[NVocoderVec];
   VectorizedSvfFilter mModulatorR alignas(16)[NVocoderVec];
   vFloat mEnvF alignas(16)[NVocoderVec];
   vFloat mEnvFR alignas(16)[NVocoderVec];
   lipol_ps mGain alignas(16);
   lipol_ps mGainR alignas(16);
   int modulator_mode;
   float wet;
   int mBI; // block increment (to keep track of events not occurring every n blocks)
   int active_bands;
   
   /*
   float mVoicedLevel;
   float mUnvoicedLevel;

   BiquadFilter
           mVoicedDetect,
           mUnvoicedDetect;
   */
};
