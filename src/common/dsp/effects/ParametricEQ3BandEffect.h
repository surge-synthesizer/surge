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

class ParametricEQ3BandEffect : public Effect
{
    lipol_ps gain alignas(16);
    lipol_ps mix alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum eq3_params
    {
        eq3_gain1 = 0,
        eq3_freq1,
        eq3_bw1,

        eq3_gain2,
        eq3_freq2,
        eq3_bw2,

        eq3_gain3,
        eq3_freq3,
        eq3_bw3,

        eq3_gain,
        eq3_mix,

        eq3_num_ctrls,
    };

    ParametricEQ3BandEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~ParametricEQ3BandEffect();
    virtual const char *get_effectname() override { return "EQ"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    BiquadFilter band1, band2, band3;
    int bi; // block increment (to keep track of events not occurring every n blocks)
};
