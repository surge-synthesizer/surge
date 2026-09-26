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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_OSCILLATORCOMMONFUNCTIONS_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_OSCILLATORCOMMONFUNCTIONS_H

#include "sst/basic-blocks/dsp/OscillatorDriftUnisonCharacter.h"
#include "DSPUtils.h"
#include "SurgeStorage.h"

namespace Surge
{
namespace Oscillator
{
using DriftLFO = sst::basic_blocks::dsp::DriftLFO;

template <typename valtype>
using CharacterFilter = sst::basic_blocks::dsp::CharacterFilter<valtype, SurgeStorage>;

template <typename valtype> using UnisonSetup = sst::basic_blocks::dsp::UnisonSetup<valtype>;

/*
 * The Wavetable and Window oscillators share a unison voices parameter whose meaning flips
 * with wtf_unison_is_loop_count: when that flag is set on a sample, the count is a play
 * count and the oscillator collapses to one voice. Both the parameter label and the unison
 * detune control have to follow the flag, and neither is a parameter, so the two hooks below
 * read the oscillator's wavetable flags directly. They are shared rather than duplicated per
 * oscillator because the two behave identically here; the instances live in Oscillator.cpp.
 */
struct SampleUnisonDynamicName : public ParameterDynamicNameFunction
{
    const char *getName(const Parameter *p) const override;
};

struct SampleUnisonDetuneDeact : public ParameterDynamicDeactivationFunction
{
    bool getValue(const Parameter *p) const override;
};

extern SampleUnisonDynamicName sampleUnisonDynamicName;
extern SampleUnisonDetuneDeact sampleUnisonDetuneDeact;

} // namespace Oscillator
} // namespace Surge

#endif // SURGE_OSCILLATORCOMMONFUNCTIONS_H
