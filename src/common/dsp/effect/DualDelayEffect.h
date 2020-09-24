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


class DualDelayEffect : public Effect
{
   lipol_ps feedback alignas(16),
            crossfeed alignas(16),
            aligpan alignas(16),
            pan alignas(16),
            mix alignas(16),
            width alignas(16);
   float buffer alignas(16)[2][max_delay_length + FIRipol_N];

public:
   DualDelayEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~DualDelayEffect();
   virtual const char* get_effectname() override
   {
      return "dualdelay";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;
   virtual int get_ringout_decay() override
   {
      return ringout_time;
   }

private:
   lag<float, true> timeL, timeR;
   bool inithadtempo;
   float envf;
   int wpos;
   BiquadFilter lp, hp;
   double lfophase;
   float LFOval;
   bool LFOdirection;
   int ringout_time;
};
