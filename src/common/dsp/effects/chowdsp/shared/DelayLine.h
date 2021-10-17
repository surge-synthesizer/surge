#pragma once

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

#include <vector>
#include "DelayInterpolation.h"

namespace chowdsp
{
namespace SampleTypeHelpers // Internal classes needed for handling sample type classes
{
template <typename T, bool = std::is_floating_point<T>::value> struct ElementType
{
    using Type = T;
};

template <typename T> struct ElementType<T, false>
{
    using Type = float;
};
} // namespace SampleTypeHelpers

/** Base class for delay lines with any interpolation type */
template <typename SampleType> class DelayLineBase
{
  public:
    using NumericType = typename SampleTypeHelpers::ElementType<SampleType>::Type;

    DelayLineBase() = default;

    virtual void setDelay(NumericType /* newDelayInSamples */) = 0;
    virtual NumericType getDelay() const = 0;

    virtual void prepare(double /*sampleRate*/, size_t /*blockSize*/) = 0;
    virtual void reset(){};

    virtual void pushSample(size_t /* channel */, SampleType /* sample */) noexcept = 0;
    virtual SampleType popSample(size_t /* channel */, SampleType /* delayInSamples */,
                                 bool /* updateReadPointer */) noexcept
    {
        return {};
    }

    void copyState(const DelayLineBase<SampleType> &other)
    {
        for (size_t ch = 0; ch < bufferData.size(); ++ch)
            std::copy(other.bufferData[ch].begin(), other.bufferData[ch].end(),
                      bufferData[ch].begin());

        std::copy(other.v.begin(), other.v.end(), v.begin());
        std::copy(other.writePos.begin(), other.writePos.end(), writePos.begin());
        std::copy(other.readPos.begin(), other.readPos.end(), readPos.begin());
    }

  protected:
    std::vector<std::vector<SampleType>> bufferData;
    std::vector<SampleType> v;
    std::vector<size_t> writePos, readPos;
};

//==============================================================================
/**
    A delay line processor featuring several algorithms for the fractional delay
    calculation, block processing, and sample-by-sample processing useful when
    modulating the delay in real time or creating a standard delay effect with
    feedback.

    This implementation has been modified from the original JUCE implementation
    to include 5th-order Lagrange Interpolation.

    Note: If you intend to change the delay in real time, you may want to smooth
    changes to the delay systematically using either a ramp or a low-pass filter.

    @see SmoothedValue, FirstOrderTPTFilter
*/
template <typename SampleType, typename InterpolationType = DelayLineInterpolationTypes::Linear>
class DelayLine : public DelayLineBase<SampleType>
{
  public:
    //==============================================================================
    /** Default constructor. */
    DelayLine();

    /** Constructor. */
    explicit DelayLine(size_t maximumDelayInSamples, size_t nChannels);

    //==============================================================================
    /** Sets the delay in samples. */
    void setDelay(NumericType newDelayInSamples) override;

    /** Returns the current delay in samples. */
    NumericType getDelay() const override;

    //==============================================================================
    /** Initialises the processor. */
    void prepare(double sampleRate, size_t blockSize) override;

    /** Resets the internal state variables of the processor. */
    template <typename C = SampleType>
    typename std::enable_if<std::is_floating_point<C>::value, void>::type reset()
    {
        for (auto vec : {&this->writePos, &this->readPos})
            std::fill(vec->begin(), vec->end(), 0);

        std::fill(this->v.begin(), this->v.end(), static_cast<SampleType>(0));

        for (size_t ch = 0; ch < this->bufferData.size(); ++ch)
            std::fill(this->bufferData[ch].begin(), this->bufferData[ch].end(),
                      static_cast<SampleType>(0));
    }

    /** Resets the internal state variables of the processor. */
    template <typename C = SampleType>
    typename std::enable_if<std::is_same<C, __m128>::value, void>::type reset()
    {
        for (auto vec : {&this->writePos, &this->readPos})
            std::fill(vec->begin(), vec->end(), 0);

        std::fill(this->v.begin(), this->v.end(), _mm_setzero_ps());

        for (size_t ch = 0; ch < this->bufferData.size(); ++ch)
            std::fill(this->bufferData[ch].begin(), this->bufferData[ch].end(), _mm_setzero_ps());
    }

    //==============================================================================
    /** Pushes a single sample into one channel of the delay line.

        Use this function and popSample instead of process if you need to modulate
        the delay in real time instead of using a fixed delay value, or if you want
        to code a delay effect with a feedback loop.

        @see setDelay, popSample, process
    */
    inline void pushSample(size_t channel, SampleType sample) noexcept override
    {
        const auto writePtr = this->writePos[channel];
        this->bufferData[channel][writePtr] = sample;
        this->bufferData[channel][writePtr + totalSize] = sample;
        this->writePos[channel] = (writePtr + totalSize - 1) % totalSize;
    }

    /** Pops a single sample from one channel of the delay line.

        Use this function to modulate the delay in real time or implement standard
        delay effects with feedback.

        @param channel              the target channel for the delay line.

        @param delayInSamples       sets the wanted fractional delay in samples, or -1
                                    to use the value being used before or set with
                                    setDelay function.

        @param updateReadPointer    should be set to true if you use the function
                                    once for each sample, or false if you need
                                    multi-tap delay capabilities.

        @see setDelay, pushSample, process
    */
    template <typename C = SampleType>
    inline typename std::enable_if<std::is_floating_point<C>::value, SampleType>::type
    popSample(size_t channel, SampleType delayInSamples = -1,
              bool updateReadPointer = true) noexcept
    {
        if (delayInSamples >= 0)
            setDelay(delayInSamples);

        auto result = interpolateSample(channel);

        if (updateReadPointer)
            this->readPos[channel] = (this->readPos[channel] + totalSize - 1) % totalSize;

        return result;
    }

    template <typename C = SampleType>
    inline typename std::enable_if<std::is_same<C, __m128>::value, SampleType>::type
    popSample(size_t channel) noexcept
    {
        auto result = interpolateSample(channel);

        // update read pointer
        this->readPos[channel] = (this->readPos[channel] + totalSize - 1) % totalSize;

        return result;
    }

    //==============================================================================
    /** Processes the input and output samples supplied in the processing context.

        Can be used for block processing when the delay is not going to change
        during processing. The delay must first be set by calling setDelay.

        @see setDelay
    */
    void process(const SampleType *inputSamples, SampleType *outputSamples, const size_t numSamples,
                 size_t channel)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            pushSample(channel, inputSamples[i]);
            outputSamples[i] = popSample(channel);
        }
    }

  private:
    inline SampleType interpolateSample(size_t channel) noexcept
    {
        auto index = (this->readPos[channel] + delayInt);
        return interpolator.call(this->bufferData[channel].data(), index, delayFrac,
                                 this->v[channel]);
    }

    //==============================================================================
    InterpolationType interpolator;
    NumericType delay = 0.0, delayFrac = 0.0;
    int delayInt = 0;
    size_t totalSize = 4;
};

} // namespace chowdsp
