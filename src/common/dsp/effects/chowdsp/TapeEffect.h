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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPEEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPEEFFECT_H

#include <Effect.h>
#include "tape/HysteresisProcessor.h"
#include "tape/LossFilter.h"
#include "tape/DegradeProcessor.h"
#include "tape/ChewProcessor.h"
#include "tape/ToneControl.h"

#include <vembertech/lipol.h>

namespace chowdsp
{

/*
** Tape emulation effect, a port of the
** CHOW Tape Model plugin. For more information,
** see https://github.com/jatinchowdhury18/AnalogTapeModel
** and http://dafx2019.bcu.ac.uk/papers/DAFx2019_paper_3.pdf
*/
class TapeEffect : public Effect
{
    lipol_ps_blocksz mix alignas(16), makeup alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum tape_params
    {
        tape_drive = 0,
        tape_saturation,
        tape_bias,
        tape_tone,

        tape_speed,
        tape_gap,
        tape_spacing,
        tape_thickness,

        tape_degrade_depth,
        tape_degrade_amount,
        tape_degrade_variance,

        tape_mix,

        tape_num_ctrls,
    };

    TapeEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~TapeEffect();

    virtual const char *get_effectname() override { return "Tape"; }

    virtual void init() override;
    virtual void sampleRateReset() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    virtual int get_ringout_decay() override { return -1; };

    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    HysteresisProcessor hysteresis;
    ToneControl toneControl;
    LossFilter lossFilter;
    DegradeProcessor degrade;
    ChewProcessor chew;
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPEEFFECT_H
