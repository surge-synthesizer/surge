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
#include <dsp/effect/Effect.h>
#include "HysteresisProcessor.h"
#include "LossFilter.h"

namespace chowdsp
{

/*
** Tape emulation effect, a port of the
** CHOW Tape Model plugin. For more information,
** see https://github.com/jatinchowdhury18/AnalogTapeModel
** and http://dafx2019.bcu.ac.uk/papers/DAFx2019_paper_3.pdf
*/
class Tape : public Effect
{
  public:
    enum tape_params
    {
        tape_drive = 0,
        tape_saturation,
        tape_bias,
        tape_speed,
        tape_gap,
        tape_spacing,
        tape_thick,

        tape_num_ctrls,
    };

    Tape(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~Tape();

    virtual const char *get_effectname() override { return "Tape"; }

    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;

    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    HysteresisProcessor hysteresis;
    LossFilter lossFilter;
};

} // namespace chowdsp
