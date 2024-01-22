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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_COMBULATOREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_COMBULATOREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"
#include <sst/filters/HalfRateFilter.h>

#include <vembertech/lipol.h>

class CombulatorEffect : public Effect
{
    lipol_ps_blocksz input alignas(16), mix alignas(16), negone alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum combulator_params
    {
        combulator_noise_mix = 0,

        combulator_freq1,
        combulator_freq2,
        combulator_freq3,
        combulator_feedback,
        combulator_tone,

        combulator_gain1,
        combulator_gain2,
        combulator_gain3,

        combulator_pan2,
        combulator_pan3,

        combulator_mix,

        combulator_num_ctrls,
    };

    CombulatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~CombulatorEffect();
    virtual const char *get_effectname() override { return "Combulator"; }
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
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    sst::filters::QuadFilterUnitState *qfus = nullptr;
    sst::filters::HalfRate::HalfRateFilter halfbandOUT, halfbandIN;
    sst::filters::FilterCoefficientMaker<SurgeStorage> coeff[3][2];
    BiquadFilter lp, hp;
    lag<float, true> freq[3], feedback, gain[3], pan2, pan3, tone, noisemix;
    float filterDelay[3][2][MAX_FB_COMB_EXTENDED + FIRipol_N];
    float WP[3][2];
    float Reg[3][2][sst::filters::n_filter_registers];

    static constexpr int PANLAW_SIZE = 4096; // power of 2 please
    float panL[PANLAW_SIZE], panR[PANLAW_SIZE];

    float envA, envR, envV[2];
    float noiseGen[2][2];

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_COMBULATOREFFECT_H
