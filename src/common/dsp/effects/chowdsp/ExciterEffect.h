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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_EXCITEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_EXCITEREFFECT_H
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

    lipol_ps_blocksz wet_gain alignas(16), drive_gain alignas(16);
    float dryL alignas(16)[BLOCK_SIZE], dryR alignas(16)[BLOCK_SIZE];
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_EXCITEREFFECT_H
