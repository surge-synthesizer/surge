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

class FrequencyShifterEffect : public Effect
{
  public:
    HalfRateFilter fr alignas(16), fi alignas(16);
    lipol_ps mix alignas(16);
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
    quadr_osc o1L, o2L, o1R, o2R;
    int ringout_time;
};
