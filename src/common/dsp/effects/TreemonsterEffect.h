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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_TREEMONSTEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_TREEMONSTEREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"
#include "AllpassFilter.h"

#include <vembertech/lipol.h>
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

class TreemonsterEffect : public Effect
{
    lipol_ps_blocksz rm alignas(16), width alignas(16), mix alignas(16);

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc oscL alignas(16), oscR alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum tm_params
    {
        tm_threshold = 0,
        tm_speed,
        tm_hp,
        tm_lp,

        tm_pitch,
        tm_ring_mix,

        tm_width,
        tm_mix,

        tm_num_ctrls,
    };

    TreemonsterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~TreemonsterEffect();
    virtual const char *get_effectname() override { return "Treemonster"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    // These are outputs which you can optionally grab from outside
    // the main processing loop. The Rack module does this.
    float smoothedPitch[2][BLOCK_SIZE], envelopeOut[2][BLOCK_SIZE];

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
    float length[2], lastval[2], length_target[2], length_smooth[2];
    bool first_thresh[2];
    BiquadFilter lp, hp;

    double envA, envR;
    float envV[2];
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_TREEMONSTEREFFECT_H
