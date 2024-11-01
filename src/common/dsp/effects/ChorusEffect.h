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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHORUSEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHORUSEFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>

template <int v> class ChorusEffect : public Effect
{
    lipol_ps_blocksz feedback alignas(16), mix alignas(16), width alignas(16);
    SIMD_M128 voicepanL4 alignas(16)[v], voicepanR4 alignas(16)[v];
    float buffer alignas(16)[max_delay_length + FIRipol_N]; // Includes padding so we can use SSE
                                                            // interpolation without wrapping

  public:
    enum chorus_params
    {
        ch_time,
        ch_rate,
        ch_depth,
        ch_feedback,
        ch_lowcut,
        ch_highcut,
        ch_mix,
        ch_width,

        ch_num_params,
    };

    ChorusEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~ChorusEffect();
    virtual const char *get_effectname() override { return "chorus"; }
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
    lag<float, true> time[v];
    float voicepan[v][2];
    float envf;
    int wpos;
    BiquadFilter lp, hp;
    double lfophase[v];
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHORUSEFFECT_H
