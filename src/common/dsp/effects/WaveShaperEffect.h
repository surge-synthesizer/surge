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
#include "AllpassFilter.h"

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

#include "sst/waveshapers.h"

class WaveShaperEffect : public Effect
{
  public:
    static const int MAX_UNISON = 16;

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
    HalfRateFilter halfbandOUT, halfbandIN;
    BiquadFilter lpPre, hpPre, lpPost, hpPost;
    lipol_ps mix alignas(16), boost alignas(16);
    lag<float> drive, bias;
};
