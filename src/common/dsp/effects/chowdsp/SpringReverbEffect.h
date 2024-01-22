/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRINGREVERBEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRINGREVERBEFFECT_H

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

    lipol_ps_blocksz mix alignas(16), makeup alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];
};
} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRINGREVERBEFFECT_H
