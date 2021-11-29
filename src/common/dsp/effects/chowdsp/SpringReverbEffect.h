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

#include "spring_reverb/SpringReverbProc.h"

namespace chowdsp
{

/*
** SpringReverb is a spring reverb emulation, based loosely
** on the reverb structures described in the following papers:
** -  V. Valimaki, J. Parker, and J. S. Abel, “Parametric spring
**    reverberation effect,” Journal of the Audio Engineering Society, 2010
**
** - Parker, Julian, "Efficient Dispersion Generation Structures for
**   Spring Reverb Emulation", EURASIP, 2011
**
** The original implementation can be found at:
** https://github.com/Chowdhury-DSP/BYOD/tree/main/src/processors/other/spring_reverb
*/
class SpringReverbEffect : public Effect
{
  public:
    enum spring_reverb_params
    {
        spring_reverb_size,
        spring_reverb_decay,
        spring_reverb_reflections,
        spring_reverb_damping,

        spring_reverb_spin,
        spring_reverb_chaos,
        spring_reverb_knock,

        spring_reverb_mix,

        spring_reverb_num_ctrls,
    };

    SpringReverbEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    ~SpringReverbEffect() override = default;

    const char *get_effectname() override { return "Spring Reverb"; }

    void init() override;
    void process(float *dataL, float *dataR) override;
    void suspend() override;

    void init_ctrltypes() override;
    void init_default_values() override;
    const char *group_label(int id) override;
    int group_label_ypos(int id) override;

  private:
    SpringReverbProc proc;

    lipol_ps mix alignas(16), makeup alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];
};
} // namespace chowdsp
