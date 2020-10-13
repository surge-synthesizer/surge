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

const int wt2_suboscs = 16;

class WindowOscillator : public Oscillator
{
private:
   int IOutputL alignas(16)[BLOCK_SIZE_OS];
   int IOutputR alignas(16)[BLOCK_SIZE_OS];
   struct
   {
      unsigned int Pos[wt2_suboscs];
      unsigned int SubPos[wt2_suboscs];
      unsigned int Ratio[wt2_suboscs];
      unsigned int Table[wt2_suboscs];
      unsigned int FormantMul[wt2_suboscs];
      unsigned int DispatchDelay[wt2_suboscs]; // samples until playback should start (for
                                               // per-sample scheduling)
      unsigned char Gain[wt2_suboscs][2];
      float DriftLFO[wt2_suboscs][2];

      int FMRatio[wt2_suboscs][BLOCK_SIZE_OS];
   } Sub alignas(16);

public:
   WindowOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual ~WindowOscillator();
   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;

private:
   BiquadFilter lp, hp;
   void applyFilter();

   void ProcessSubOscs(bool stereo, bool FM);
   lag<double> FMdepth[wt2_suboscs];

   float OutAttenuation;
   float DetuneBias, DetuneOffset;
   int ActiveSubOscs;
};
