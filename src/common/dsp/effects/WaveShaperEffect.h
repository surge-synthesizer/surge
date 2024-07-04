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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_WAVESHAPEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_WAVESHAPEREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>

#include "sst/waveshapers.h"
#include <sst/filters/HalfRateFilter.h>

class WaveShaperEffect : public Effect
{
  public:
    WaveShaperEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~WaveShaperEffect();
    virtual const char *get_effectname() override { return "WaveShaper"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual int get_ringout_decay() override { return -1; }

    enum wsfx_params
    {
        ws_prelowcut,
        ws_prehighcut,
        ws_shaper,
        ws_bias,
        ws_drive,
        ws_postlowcut,
        ws_posthighcut,
        ws_postboost,
        ws_mix
    };

  private:
    sst::waveshapers::WaveshaperType lastShape{sst::waveshapers::WaveshaperType::wst_none};
    sst::waveshapers::QuadWaveshaperState wss;
    sst::filters::HalfRate::HalfRateFilter halfbandOUT, halfbandIN;
    BiquadFilter lpPre, hpPre, lpPost, hpPost;
    lipol_ps_blocksz mix alignas(16), boost alignas(16);
    lag<float> drive, bias;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_WAVESHAPEREFFECT_H
