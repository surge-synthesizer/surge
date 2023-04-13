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

class InputBlenderEffect : public Effect
{
  public:
    enum ibp_params{
        ibp_channel = 0,
        ibp_audio_level,
        ibp_upstream_level,

        ibp_num_params
    };
    InputBlenderEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    ~InputBlenderEffect() override;
    void init_ctrltypes() override;
    void init_default_values() override;
    void process(float *dataL, float *dataR) override;
};

