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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H

#include "Effect.h"

class ConvolutionEffect : public Effect
{
  public:
    enum convolution_params
    {
        convolution_file = 0,
        convolution_mix,
    };

    ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);

    virtual const char *get_effectname() override;
    virtual void init_ctrltypes() override;
};

#endif  // SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H
