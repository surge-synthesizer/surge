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

#include <vembertech/lipol.h>

#include "shared/SmoothedValue.h"
#include "shared/Oversampling.h"

namespace chowdsp
{

/*
** CHOW is a truculent distortion effect, with similar
** characteristic and controls as a compressor. The original
** implementation can be found at: https://github.com/Chowdhury-DSP/CHOW
*/
class CHOWEffect : public Effect
{
  public:
    enum chow_params
    {
        chow_thresh = 0,
        chow_ratio,
        chow_flip,

        chow_mix,

        chow_num_ctrls,
    };

    CHOWEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~CHOWEffect();

    virtual const char *get_effectname() override { return "CHOW"; }

    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;

    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    void set_params();
    void process_block(float *dataL, float *dataR);
    void process_block_os(float *dataL, float *dataR);

    enum
    {
        SmoothSteps = 200,
    };

    bool cur_os = true;
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];
    lipol_ps makeup alignas(16), mix alignas(16);
    Oversampling<2, BLOCK_SIZE> os;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> thresh_smooth, ratio_smooth;
};

} // namespace chowdsp
