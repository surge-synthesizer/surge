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
#include "DSPUtils.h"
#include "AllpassFilter.h"

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

#include "sst/waveshapers.h"

class DistortionEffect : public Effect
{
    HalfRateFilter hr_a alignas(16), hr_b alignas(16);
    lipol_ps drive alignas(16), outgain alignas(16);
    sst::waveshapers::QuadWaveshaperState wsState alignas(16);

  public:
    DistortionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~DistortionEffect();
    virtual const char *get_effectname() override { return "distortion"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;

    static constexpr int ringout_time = 1600, ringout_end = 320;

    virtual int get_ringout_decay() override { return ringout_time; }
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    enum dist_params
    {
        dist_preeq_gain = 0,
        dist_preeq_freq,
        dist_preeq_bw,
        dist_preeq_highcut,
        dist_drive,
        dist_feedback,
        dist_posteq_gain,
        dist_posteq_freq,
        dist_posteq_bw,
        dist_posteq_highcut,
        dist_gain,
        dist_model,
    };

  private:
    BiquadFilter band1, band2, lp1, lp2;
    int bi; // block increment (to keep track of events not occurring every n blocks)
    float L, R;
};
