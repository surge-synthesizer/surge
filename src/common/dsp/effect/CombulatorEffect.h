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
#include "DspUtilities.h"
#include "QuadFilterUnit.h"

#include <vt_dsp/lipol.h>

class CombulatorEffect : public Effect
{
    lipol_ps width alignas(16);
    lipol_ps mix alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum combulator_params
    {
        combulator_input_gain = 0,
        combulator_noise_mix,

        combulator_freq1,
        combulator_freq2,
        combulator_freq3,
        combulator_feedback,
        
        combulator_gain1,
        combulator_gain2,
        combulator_gain3,
        combulator_lowpass,
        
        combulator_width,
        combulator_mix,

        combulator_num_ctrls,
    };

    CombulatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~CombulatorEffect();
    virtual const char *get_effectname() override { return "Combulator"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual int get_ringout_decay() override { return -1; }
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    QuadFilterUnitState *qfus = nullptr;
    HalfRateFilter halfbandOUT, halfbandIN;
    FilterCoefficientMaker coeff[3][2];
    BiquadFilter lp;
    lag<float, true> cutoff[3], resonance, bandGain[3];
    float filterDelay[3][2][MAX_FB_COMB_EXTENDED + FIRipol_N];
    float WP[3][2];
    float Reg[3][2][n_filter_registers];

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
};
