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
#include "StateVariableFilter.h"

namespace chowdsp
{
//==============================================================================
template <typename SampleType> StateVariableFilter<SampleType>::StateVariableFilter() { update(); }

template <typename SampleType>
void StateVariableFilter<SampleType>::setCutoffFrequency(NumericType newCutoffFrequencyHz)
{
    cutoffFrequency = newCutoffFrequencyHz;
    update();
}

template <typename SampleType>
void StateVariableFilter<SampleType>::setResonance(NumericType newResonance)
{
    resonance = newResonance;
    update();
}

//==============================================================================
template <typename SampleType>
void StateVariableFilter<SampleType>::prepare(float sr, int numChannels)
{
    sampleRate = sr;

    s1.resize(numChannels);
    s2.resize(numChannels);

    reset();
    update();
}

template <typename SampleType> void StateVariableFilter<SampleType>::reset()
{
    reset(static_cast<SampleType>(0));
}

template <typename SampleType> void StateVariableFilter<SampleType>::reset(SampleType newValue)
{
    for (auto v : {&s1, &s2})
        std::fill(v->begin(), v->end(), newValue);
}

//==============================================================================
template <typename SampleType> void StateVariableFilter<SampleType>::update()
{
    g = static_cast<NumericType>(std::tan(M_PI * cutoffFrequency / sampleRate));
    R2 = static_cast<NumericType>((NumericType)1.0 / resonance);
    h = static_cast<NumericType>((NumericType)1.0 / ((NumericType)1.0 + R2 * g + g * g));

    gh = g * h;
    g2 = static_cast<NumericType>(2) * g;
    gpR2 = g + R2;
}

//==============================================================================
template class StateVariableFilter<float>;
template class StateVariableFilter<double>;

} // namespace chowdsp
