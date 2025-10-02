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
#include "ConvolutionEffect.h"

ConvolutionEffect::ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
}

const char *ConvolutionEffect::get_effectname() { return "convolution"; }

void ConvolutionEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();
    fxdata->p[convolution_file].set_name("File");
    fxdata->p[convolution_file].set_type(ct_convolution_file);
    fxdata->p[convolution_mix].set_name("Mix");
    fxdata->p[convolution_mix].set_type(ct_percent);
}
