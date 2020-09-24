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

#include "SurgeStorage.h"

class alignas(16) Oscillator
{
public:
   // The data blocks processed by the SIMD instructions (e.g. SSE2), which must
   // always be before any other variables in the class, in order to be properly
   // aligned to 16 bytes.
   float output alignas(16)[BLOCK_SIZE_OS];
   float outputR alignas(16)[BLOCK_SIZE_OS];

   Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual ~Oscillator();
   virtual void init(float pitch, bool is_display = false){};
   virtual void init_ctrltypes(int scene, int oscnum){ init_ctrltypes(); };
   virtual void init_ctrltypes(){};
   virtual void init_default_values(){};
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f)
   {}
   virtual void assign_fm(float* master_osc)
   {
      this->master_osc = master_osc;
   }
   virtual bool allow_display()
   {
      return true;
   }
   inline float pitch_to_omega(float x)
   {
      // Wondering about that constant 16.35? It is the twice the frequency of C0 (since we have a 2 pi here)
      return (float)(M_PI * (16.35159783) * storage->note_to_pitch(x) * dsamplerate_os_inv);
   }

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
   {
       // No-op here.
   }
   
protected:
   SurgeStorage* storage;
   OscillatorStorage* oscdata;
   pdata* localcopy;
   float* __restrict master_osc;
   float drift;
   int ticker;
};

class AbstractBlitOscillator : public Oscillator
{
public:
   AbstractBlitOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);

protected:
   float oscbuffer alignas(16)[OB_LENGTH + FIRipol_N];
   float oscbufferR alignas(16)[OB_LENGTH + FIRipol_N];
   float dcbuffer alignas(16)[OB_LENGTH + FIRipol_N];
   __m128 osc_out, osc_out2, osc_outR, osc_out2R;
   void prepare_unison(int voices);
   float integrator_hpf;
   float pitchmult, pitchmult_inv;
   int bufpos;
   int n_unison;
   float out_attenuation, out_attenuation_inv, detune_bias, detune_offset;
   float oscstate[MAX_UNISON], syncstate[MAX_UNISON], rate[MAX_UNISON];
   float driftlfo[MAX_UNISON], driftlfo2[MAX_UNISON];
   float panL[MAX_UNISON], panR[MAX_UNISON];
   int state[MAX_UNISON];
};


