/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_MSTOOLEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_MSTOOLEFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>

class MSToolEffect : public Effect
{
    lipol_ps_blocksz ampM alignas(16), ampS alignas(16), width alignas(16), postampL alignas(16),
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

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_MSTOOLEFFECT_H
