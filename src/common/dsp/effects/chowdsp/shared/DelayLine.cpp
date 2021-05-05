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

#include <algorithm>
#include <cmath>
#include <basic_dsp.h>
#include "DelayLine.h"

namespace chowdsp
{

//==============================================================================
template <typename SampleType, typename InterpolationType>
DelayLine<SampleType, InterpolationType>::DelayLine() : DelayLine(0, 1)
{
}

template <typename SampleType, typename InterpolationType>
DelayLine<SampleType, InterpolationType>::DelayLine(size_t maximumDelayInSamples, size_t nChannels)
{
    totalSize = std::max((size_t)4, maximumDelayInSamples + 1);

    this->bufferData.resize(nChannels);
    for (size_t ch = 0; ch < nChannels; ++ch)
        this->bufferData[ch] = std::vector<SampleType>(totalSize);
}

//==============================================================================
template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::setDelay(SampleType newDelayInSamples)
{
    auto upperLimit = (SampleType)(totalSize - 1);

    delay = limit_range(newDelayInSamples, (SampleType)0, upperLimit);
    delayInt = static_cast<int>(std::floor(delay));
    delayFrac = delay - (SampleType)delayInt;

    interpolator.updateInternalVariables(delayInt, delayFrac);
}

template <typename SampleType, typename InterpolationType>
SampleType DelayLine<SampleType, InterpolationType>::getDelay() const
{
    return delay;
}

//==============================================================================
template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::prepare(double sampleRate, size_t blockSize)
{
    const auto numChannels = this->bufferData.size();

    this->writePos.resize(numChannels);
    this->readPos.resize(numChannels);

    this->v.resize(numChannels);
    interpolator.reset(totalSize);

    reset();
}

template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::reset()
{
    for (auto vec : {&this->writePos, &this->readPos})
        std::fill(vec->begin(), vec->end(), 0);

    std::fill(this->v.begin(), this->v.end(), static_cast<SampleType>(0));

    for (size_t ch = 0; ch < this->bufferData.size(); ++ch)
        std::fill(this->bufferData[ch].begin(), this->bufferData[ch].end(),
                  static_cast<SampleType>(0));
}

//==============================================================================
template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::pushSample(size_t channel, SampleType sample)
{
    this->bufferData[channel][this->writePos[channel]] = sample;
    this->writePos[channel] = (this->writePos[channel] + totalSize - 1) % totalSize;
}

template <typename SampleType, typename InterpolationType>
SampleType DelayLine<SampleType, InterpolationType>::popSample(size_t channel,
                                                               SampleType delayInSamples,
                                                               bool updateReadPointer)
{
    if (delayInSamples >= 0)
        setDelay(delayInSamples);

    auto result = interpolateSample(channel);

    if (updateReadPointer)
        this->readPos[channel] = (this->readPos[channel] + totalSize - 1) % totalSize;

    return result;
}

//==============================================================================
template class DelayLine<float, DelayLineInterpolationTypes::None>;
template class DelayLine<double, DelayLineInterpolationTypes::None>;
template class DelayLine<float, DelayLineInterpolationTypes::Linear>;
template class DelayLine<double, DelayLineInterpolationTypes::Linear>;
template class DelayLine<float, DelayLineInterpolationTypes::Lagrange3rd>;
template class DelayLine<double, DelayLineInterpolationTypes::Lagrange3rd>;
template class DelayLine<float, DelayLineInterpolationTypes::Lagrange5th>;
template class DelayLine<double, DelayLineInterpolationTypes::Lagrange5th>;
template class DelayLine<float, DelayLineInterpolationTypes::Thiran>;
template class DelayLine<double, DelayLineInterpolationTypes::Thiran>;

} // namespace chowdsp
