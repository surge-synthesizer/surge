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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CONDITIONEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CONDITIONEREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>

const int lookahead_bits = 7;
const int lookahead = 1 << 7;

class ConditionerEffect : public Effect
{
    lipol_ps_blocksz ampL alignas(16), ampR alignas(16), width alignas(16), postamp alignas(16);

  public:
    ConditionerEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~ConditionerEffect();
    virtual const char *get_effectname() override { return "conditioner"; }
    virtual void init() override;
    virtual void process_only_control() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual int get_ringout_decay() override { return 100; }
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual Surge::ParamConfig::VUType vu_type(int id) override;
    virtual int vu_ypos(int id) override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;
    enum cond_params
    {
        cond_bass = 0,
        cond_treble,
        cond_width,
        cond_balance,
        cond_threshold,
        cond_attack,
        cond_release,
        cond_gain,
        cond_hpwidth,
    };

  private:
    BiquadFilter band1, band2, hp;
    float ef;
    lipol<float, true> a_rate, r_rate;
    float lamax[lookahead << 1];
    float delayed[2][lookahead];
    int bufpos;
    float filtered_lamax, filtered_lamax2, gain;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CONDITIONEREFFECT_H
