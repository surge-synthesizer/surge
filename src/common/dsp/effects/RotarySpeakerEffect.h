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

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

#include "sst/waveshapers.h"

class RotarySpeakerEffect : public Effect
{
  public:
    lipol_ps width alignas(16), mix alignas(16);
    sst::waveshapers::QuadWaveshaperState wsState alignas(16);

    RotarySpeakerEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~RotarySpeakerEffect();
    virtual void process_only_control() override;
    virtual const char *get_effectname() override { return "rotary"; }
    virtual void process(float *dataL, float *dataR) override;
    virtual int get_ringout_decay() override { return max_delay_length >> 5; }
    void setvars(bool init);
    virtual void suspend() override;
    virtual void init() override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    enum rotary_params
    {
        rot_horn_rate = 0,
        rot_doppler,
        rot_tremolo,
        rot_rotor_rate,
        rot_drive,
        rot_waveshape,
        rot_width,
        rot_mix,

        rot_num_params,
    };

  protected:
    float buffer[max_delay_length];
    int wpos;
    // filter *lp[2],*hp[2];
    // biquadunit rotor_lpL,rotor_lpR;
    BiquadFilter xover, lowbass;
    // float
    // f_rotor_lp[2][n_filter_parameters],f_xover[n_filter_parameters],f_lowbass[n_filter_parameters];
    quadr_osc lfo;
    quadr_osc lf_lfo;
    lipol<float> dL, dR, hornamp[2];
    lag<float, true> drive;
    bool first_run;
};
