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
#include "DspUtilities.h"
#include "AllpassFilter.h"

#include "VectorizedSvfFilter.h"

#include <vt_dsp/halfratefilter.h>
#include <vt_dsp/lipol.h>
#include "ModControl.h"
#include "SSESincDelayLine.h"

class BBDEnsembleEffect : public Effect
{
    lipol_ps width alignas(16), gain alignas(16), mix alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum ens_params
    {
        ens_input_gain = 0,

        ens_bbd_stages,
        ens_bbd_aa_cutoff,

        ens_lfo_freq1,
        ens_lfo_depth1,
        ens_lfo_freq2,
        ens_lfo_depth2,

        ens_width,
        ens_gain,
        ens_mix,

        ens_num_ctrls,
    };

    BBDEnsembleEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~BBDEnsembleEffect();
    virtual const char *get_effectname() override { return "Ensemble"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    Surge::ModControl modlfos[2][3]; // 2 LFOs differening by 120 degree in phase at outputs
    SSESincDelayLine<8192> delL, delR;
};
