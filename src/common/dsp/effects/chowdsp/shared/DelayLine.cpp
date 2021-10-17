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
        this->bufferData[ch] = std::vector<SampleType>(2 * totalSize);
}

//==============================================================================
template <typename SampleType, typename InterpolationType>
void DelayLine<SampleType, InterpolationType>::setDelay(NumericType newDelayInSamples)
{
    auto upperLimit = (NumericType)(totalSize - 1);

    delay = limit_range(newDelayInSamples, (NumericType)0, upperLimit);
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
void DelayLine<SampleType, InterpolationType>::prepare(double sampleRate, size_t blockSize)
{
    const auto numChannels = this->bufferData.size();

    this->writePos.resize(numChannels);
    this->readPos.resize(numChannels);

    this->v.resize(numChannels);
    interpolator.reset(totalSize);

    reset();
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
template class DelayLine<__m128, DelayLineInterpolationTypes::Thiran>;

} // namespace chowdsp
