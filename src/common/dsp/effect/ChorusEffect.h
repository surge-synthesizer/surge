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


template <int v> class ChorusEffect : public Effect
{
   lipol_ps feedback alignas(16),
            mix alignas(16),
            width alignas(16);
   __m128 voicepanL4 alignas(16)[v],
          voicepanR4 alignas(16)[v];
   float buffer alignas(16)[max_delay_length + FIRipol_N]; // Includes padding so we can use SSE
                                                           // interpolation without wrapping
public:
   ChorusEffect<v>(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~ChorusEffect();
   virtual const char* get_effectname() override
   {
      return "chorus";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

private:
   lag<float, true> time[v];
   float voicepan[v][2];
   float envf;
   int wpos;
   BiquadFilter lp, hp;
   double lfophase[v];
};

