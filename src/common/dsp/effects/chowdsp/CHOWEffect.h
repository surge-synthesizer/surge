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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_CHOWEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_CHOWEFFECT_H
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
    lipol_ps_blocksz makeup alignas(16), mix alignas(16);
    Oversampling<2, BLOCK_SIZE> os;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> thresh_smooth, ratio_smooth;
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_CHOWEFFECT_H
