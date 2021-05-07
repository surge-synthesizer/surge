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

class RingModulatorEffect : public Effect
{
  public:
    static const int MAX_UNISON = 16;

    RingModulatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~RingModulatorEffect();
    virtual const char *get_effectname() override { return "ringmodulator"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual int get_ringout_decay() override { return ringout_value; }

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    float diode_sim(float x);

    enum ringmod_params
    {
        rm_carrier_shape = 0,
        rm_carrier_freq,
        rm_unison_detune,
        rm_unison_voices,

        rm_diode_fwdbias,
        rm_diode_linregion,

        rm_lowcut,
        rm_highcut,

        rm_mix,

        rm_num_params,
    };

  private:
    int ringout_value = -1;
    float phase[MAX_UNISON], detune_offset[MAX_UNISON], panL[MAX_UNISON], panR[MAX_UNISON];
    int last_unison = -1;

    HalfRateFilter halfbandOUT, halfbandIN;
    BiquadFilter lp, hp;
    lipol_ps mix alignas(16);
};
