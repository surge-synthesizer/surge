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
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"
#include "OscillatorCommonFunctions.h"

class FM3Oscillator : public Oscillator
{
  public:
    enum fm3_params
    {
        fm3_m1amount = 0,
        fm3_m1ratio,
        fm3_m2amount,
        fm3_m2ratio,
        fm3_m3amount,
        fm3_m3freq,
        fm3_feedback,
    };

    FM3Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~FM3Oscillator();
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    double phase, lastoutput;
    quadr_osc RM1, RM2, AM;
    Surge::Oscillator::DriftLFO driftLFO;
    float fb_val;
    lag<double> FMdepth, AbsModDepth, RelModDepth1, RelModDepth2, FeedbackDepth;
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;
};
