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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_FREQUENCYSHIFTEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_FREQUENCYSHIFTEREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>
#include <sst/filters/HalfRateFilter.h>
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

class FrequencyShifterEffect : public Effect
{
  public:
    sst::filters::HalfRate::HalfRateFilter fr alignas(16), fi alignas(16);
    lipol_ps_blocksz mix alignas(16);
    FrequencyShifterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~FrequencyShifterEffect();
    virtual const char *get_effectname() override { return "freqshift"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;
    virtual int get_ringout_decay() override { return ringout_time; }

    enum freqshift_params
    {
        freq_shift = 0,
        freq_rmult,
        freq_delay,
        freq_feedback,
        freq_mix,

        freq_num_params,
    };

  private:
    lipol<float, true> feedback;
    lag<float, true> time, shiftL, shiftR;
    bool inithadtempo;
    float buffer[2][max_delay_length];
    int wpos;
    // CHalfBandFilter<6> frL,fiL,frR,fiR;

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc o1L, o2L, o1R, o2R;
    int ringout_time;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_FREQUENCYSHIFTEREFFECT_H
