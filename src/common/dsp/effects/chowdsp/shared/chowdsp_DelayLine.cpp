/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace chowdsp
{
//==============================================================================
template <typename SampleType, typename InterpolationType>
DelayLine<SampleType, InterpolationType>::DelayLine() : DelayLine(0)
{
}

template <typename SampleType, typename InterpolationType>
DelayLine<SampleType, InterpolationType>::DelayLine(int maximumDelayInSamples)
{
    jassert(maximumDelayInSamples >= 0);

    totalSize = juce::jmax(4, maximumDelayInSamples + 1);
}

//==============================================================================
template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::setDelay(
    DelayLine<SampleType, InterpolationType>::NumericType newDelayInSamples)
{
    auto upperLimit = (NumericType)(totalSize - 1);
    jassert(juce::isPositiveAndNotGreaterThan(newDelayInSamples, upperLimit));

    delay = juce::jlimit((NumericType)0, upperLimit, newDelayInSamples);
    delayInt = static_cast<int>(std::floor(delay));
    delayFrac = delay - (NumericType)delayInt;

    interpolator.updateInternalVariables(delayInt, delayFrac);
}

template <typename SampleType, typename InterpolationType>
typename DelayLine<SampleType, InterpolationType>::NumericType
DelayLine<SampleType, InterpolationType>::getDelay() const
{
    return delay;
}

//==============================================================================
template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::prepare(const juce::dsp::ProcessSpec &spec)
{
    jassert(spec.numChannels > 0);

    this->bufferData =
        juce::dsp::AudioBlock<SampleType>(this->dataBlock, spec.numChannels, 2 * (size_t)totalSize);

    this->writePos.resize(spec.numChannels);
    this->readPos.resize(spec.numChannels);

    this->v.resize(spec.numChannels);
    interpolator.reset(totalSize);

    reset();

    bufferPtrs.resize(spec.numChannels);
    for (size_t ch = 0; ch < spec.numChannels; ++ch)
        bufferPtrs[ch] = this->bufferData.getChannelPointer(ch);
}

template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::reset()
{
    for (auto vec : {&this->writePos, &this->readPos})
        std::fill(vec->begin(), vec->end(), 0);

    std::fill(this->v.begin(), this->v.end(), static_cast<SampleType>(0));

    this->bufferData.clear();
}

} // namespace chowdsp
