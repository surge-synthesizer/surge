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
#include "shared/Oversampling.h"
#include "exciter/LevelDetector.h"
#include <vembertech/basic_dsp.h>
#include <vembertech/lipol.h>

namespace chowdsp
{

/*
** Exciter effect, based on the Aphex Aural Exciter.
** For more information, see here:
** https://jatinchowdhury18.medium.com/complex-nonlinearities-epsiode-2-harmonic-exciter-cd883d888a43
*/
class ExciterEffect : public Effect
{
  public:
    enum exciter_params
    {
        exciter_drive = 0,
        exciter_tone,
        exciter_att,
        exciter_rel,
        exciter_mix,

        exciter_num_ctrls,
    };

    ExciterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~ExciterEffect();

    virtual const char *get_effectname() override { return "Exciter"; }

    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;

    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    void set_params();

    inline void process_sample(float &l, float &r)
    {
        toneFilter.process_sample(l, r, l, r);
        auto levelL = levelDetector.process_sample(l, 0);
        auto levelR = levelDetector.process_sample(r, 0);
        l = std::tanh(l) * levelL;
        r = std::tanh(r) * levelR;
    }

    Oversampling<1, BLOCK_SIZE> os;
    BiquadFilter toneFilter;
    LevelDetector levelDetector;

    lipol_ps wet_gain alignas(16), drive_gain alignas(16);
    float dryL alignas(16)[BLOCK_SIZE], dryR alignas(16)[BLOCK_SIZE];
};

} // namespace chowdsp
