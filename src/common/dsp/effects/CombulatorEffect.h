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

class CombulatorEffect : public Effect
{
    lipol_ps input alignas(16), mix alignas(16), negone alignas(16);

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
    HalfRateFilter halfbandOUT, halfbandIN;
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
