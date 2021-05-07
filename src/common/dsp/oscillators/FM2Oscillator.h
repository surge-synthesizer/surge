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

class FM2Oscillator : public Oscillator
{
  public:
    enum fm2_params
    {
        fm2_m1amount = 0,
        fm2_m1ratio,
        fm2_m2amount,
        fm2_m2ratio,
        fm2_m12offset,
        fm2_m12phase,
        fm2_feedback,
    };

    FM2Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~FM2Oscillator();
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    double phase, lastoutput;
    quadr_osc RM1, RM2;
    Surge::Oscillator::DriftLFO driftLFO;
    float fb_val;
    lag<double> FMdepth, RelModDepth1, RelModDepth2, FeedbackDepth, PhaseOffset;
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;
};
