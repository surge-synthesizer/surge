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
