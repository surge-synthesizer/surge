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

class TreemonsterEffect : public Effect
{
    lipol_ps rm alignas(16), width alignas(16), mix alignas(16);
    quadr_osc oscL alignas(16), oscR alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum tm_params
    {
        tm_threshold = 0,
        tm_speed,
        tm_hp,
        tm_lp,

        tm_pitch,
        tm_ring_mix,

        tm_width,
        tm_mix,

        tm_num_ctrls,
    };

    TreemonsterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~TreemonsterEffect();
    virtual const char *get_effectname() override { return "Treemonster"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
    float length[2], lastval[2], length_target[2], length_smooth[2];
    bool first_thresh[2];
    BiquadFilter lp, hp;

    double envA, envR;
    float envV[2];
};
