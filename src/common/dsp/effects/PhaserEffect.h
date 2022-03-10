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
#include "ModControl.h"

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

class PhaserEffect : public Effect
{
    lipol_ps width alignas(16), mix alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    PhaserEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~PhaserEffect();
    virtual const char *get_effectname() override { return "phaser"; }
    virtual void init() override;
    virtual void process_only_control() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual int get_ringout_decay() override;
    virtual void suspend() override;
    void setvars();
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    enum phaser_params
    {
        ph_center = 0,
        ph_feedback,
        ph_sharpness,
        ph_mod_rate,
        ph_mod_depth,
        ph_stereo,
        ph_mix,
        ph_width,
        ph_stages,
        ph_spread,
        ph_mod_wave,
        ph_tone,

        ph_num_params,
    };

  private:
    lipol<float, true> feedback, tone;
    static const int max_stages = 16;
    static const int default_stages = 4;
    int n_stages = default_stages;
    int n_bq_units = default_stages << 1;
    int n_bq_units_initialised = 0;
    float dL, dR;
    BiquadFilter *biquad[max_stages * 2], lp, hp;
    int bi; // block increment (to keep track of events not occurring every n blocks)
    void init_stages();

    // before stages/spread added parameters we had 4 stages at fixed frequencies and modulation
    // depth span
    float legacy_freq[4] = {1.5 / 12, 19.5 / 12, 35 / 12, 50 / 12};
    float legacy_span[4] = {2.0, 1.5, 1.0, 0.5};

    Surge::ModControl modLFOL, modLFOR;
};
