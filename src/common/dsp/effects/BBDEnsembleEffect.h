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
#include "ModControl.h"
#include "SSESincDelayLine.h"
#include "chowdsp/bbd_utils/BBDDelayLine.h"
#include "chowdsp/bbd_utils/BBDNonlin.h"

class BBDEnsembleEffect : public Effect
{
    lipol_ps width alignas(16), mix alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum ens_params
    {
        ens_input_filter = 0,

        ens_lfo_freq1,
        ens_lfo_depth1,
        ens_lfo_freq2,
        ens_lfo_depth2,

        ens_delay_type,
        ens_delay_clockrate,
        ens_delay_sat,
        ens_delay_feedback,

        ens_width,
        ens_mix,

        ens_num_ctrls,
    };

    enum EnsembleStages
    {
        ens_sinc = 0,
        ens_128,
        ens_256,
        ens_512,
        ens_1024,
        ens_2048,
        ens_4096,
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
    float getFeedbackGain(bool bbd) const noexcept;
    void process_sinc_delays(float *dataL, float *dataR, float delayCenterMs, float delayScale);

    Surge::ModControl modlfos[2][3]; // 2 LFOs differening by 120 degree in phase at outputs
    SSESincDelayLine<8192> delL, delR;
    BBDDelayLine<128> del_128L1, del_128L2, del_128R1, del_128R2;
    BBDDelayLine<256> del_256L1, del_256L2, del_256R1, del_256R2;
    BBDDelayLine<512> del_512L1, del_512L2, del_512R1, del_512R2;
    BBDDelayLine<1024> del_1024L1, del_1024L2, del_1024R1, del_1024R2;
    BBDDelayLine<2048> del_2048L1, del_2048L2, del_2048R1, del_2048R2;
    BBDDelayLine<4096> del_4096L1, del_4096L2, del_4096R1, del_4096R2;

    BBDNonlin bbd_saturation_sse;
    size_t block_counter;

    float fbStateL, fbStateR;
    BiquadFilter dc_blocker[2];
    BiquadFilter sincInputFilter;
};
