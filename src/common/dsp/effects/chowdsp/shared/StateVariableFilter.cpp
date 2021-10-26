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
