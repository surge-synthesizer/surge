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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_RESONATOREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_RESONATOREFFECT_H
#include "Effect.h"
#include "DSPUtils.h"
#include <sst/filters/HalfRateFilter.h>

#include <vembertech/lipol.h>

class ResonatorEffect : public Effect
{
    lipol_ps_blocksz gain alignas(16);
    lipol_ps_blocksz mix alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum resonator_params
    {
        resonator_freq1 = 0,
        resonator_res1,
        resonator_gain1,

        resonator_freq2,
        resonator_res2,
        resonator_gain2,

        resonator_freq3,
        resonator_res3,
        resonator_gain3,

        resonator_mode,
        resonator_gain,
        resonator_mix,

        resonator_num_ctrls,
    };

    enum resonator_modes
    {
        rm_lowpass = 0,
        rm_bandpass,
        rm_bandpass_n,
        rm_highpass,

        rm_num_modes,
    };

    ResonatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~ResonatorEffect();
    virtual const char *get_effectname() override { return "Resonator"; }
    virtual void init() override;
    virtual void sampleRateReset() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual int get_ringout_decay() override { return -1; }
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    sst::filters::QuadFilterUnitState *qfus = nullptr;
    sst::filters::HalfRate::HalfRateFilter halfbandOUT, halfbandIN;
    sst::filters::FilterCoefficientMaker<SurgeStorage> coeff[3][2];
    lag<float, true> cutoff[3], resonance[3], bandGain[3];
    // float filterDelay[3][2][MAX_FB_COMB + FIRipol_N];
    // float WP[3][2];
    float Reg[3][2][sst::filters::n_filter_registers];

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_RESONATOREFFECT_H
