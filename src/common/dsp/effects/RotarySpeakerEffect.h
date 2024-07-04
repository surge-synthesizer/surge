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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_ROTARYSPEAKEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_ROTARYSPEAKEREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>

#include "sst/waveshapers.h"
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

class RotarySpeakerEffect : public Effect
{
  public:
    lipol_ps_blocksz width alignas(16), mix alignas(16);
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

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc lfo;
    quadr_osc lf_lfo;
    lipol<float> dL, dR, hornamp[2];
    lag<float, true> drive;
    bool first_run;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_ROTARYSPEAKEREFFECT_H
