/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
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
#include <vembertech/lipol.h>

class MSToolEffect : public Effect
{
    lipol_ps ampM alignas(16), ampS alignas(16), width alignas(16), postampL alignas(16),
        postampR alignas(16);

  public:
    MSToolEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~MSToolEffect();
    virtual const char *get_effectname() override { return "Mid-Side Tool"; }
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
    enum mstl_params
    {
        mstl_matrix = 0,
        mstl_hpm,
        mstl_pqm,
        mstl_freqm,
        mstl_lpm,
        mstl_hps,
        mstl_pqs,
        mstl_freqs,
        mstl_lps,
        mstl_mgain,
        mstl_sgain,
        mstl_outgain,
    };

  private:
    BiquadFilter hpm, hps, lpm, lps, bandm, bands;
};
