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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_STATEVARIABLEFILTER_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_STATEVARIABLEFILTER_H

#include <cmath>
#include <type_traits>
#include <vector>

namespace chowdsp
{
/** Filter type options for State Variable Filters */
enum class StateVariableFilterType
{
    Lowpass,
    Bandpass,
    Highpass
};

//==============================================================================
/** An IIR filter that can perform low, band and high-pass filtering on an audio
    signal, with 12 dB of attenuation per octave, using a TPT structure, designed
    for fast modulation (see Vadim Zavalishin's documentation about TPT
    structures for more information). Its behaviour is based on the analog
    state variable filter circuit.

    Note: The bandpass here is not the one in the RBJ CookBook as its gain can be
    higher than 0 dB. For the classic 0 dB bandpass, we need to multiply the
    result by R2.

    Note 2: Using this class prevents some loud audio artefacts commonly encountered when
    changing the cutoff frequency using other filter simulation structures and IIR
    filter classes. However, this class may still require additional smoothing for
    cutoff frequency changes.

    see IIRFilter, SmoothedValue

    @tags{DSP}
*/
template <typename SampleType> class StateVariableFilter
{
  public:
    //==============================================================================
    using Type = StateVariableFilterType;
    using NumericType = SampleType; // typename SampleTypeHelpers::ElementType<SampleType>::Type;

    //==============================================================================
    /** Constructor. */
    StateVariableFilter();

    //==============================================================================
    /** Sets the cutoff frequency of the filter.

        @param newFrequencyHz the new cutoff frequency in Hz.
    */
    void setCutoffFrequency(NumericType newFrequencyHz);

    /** Sets the resonance of the filter.

        Note: The bandwidth of the resonance increases with the value of the
        parameter. To have a standard 12 dB / octave filter, the value must be set
        at 1 / sqrt(2).
    */
    void setResonance(NumericType newResonance);

    //==============================================================================
    /** Returns the cutoff frequency of the filter. */
    NumericType getCutoffFrequency() const noexcept { return cutoffFrequency; }

    /** Returns the resonance of the filter. */
    NumericType getResonance() const noexcept { return resonance; }

    //==============================================================================
    /** Initialises the filter. */
    void prepare(float sampleRate, int numChannels = 1);

    /** Resets the internal state variables of the filter. */
    void reset();

    /** Resets the internal state variables of the filter to a given value. */
    void reset(SampleType newValue);

    //==============================================================================
    /** Processes one sample at a time on a given channel. */
    template <Type type>
    inline SampleType processSample(int channel, SampleType inputValue) noexcept
    {
        auto &ls1 = s1[(size_t)channel];
        auto &ls2 = s2[(size_t)channel];

        auto yT = (inputValue - ls1 * gpR2 - ls2) * gh;
        ls1 += yT + yT;

        auto gA = (ls1 - yT) * g;
        ls2 += gA + gA;

        switch (type)
        {
        case Type::Lowpass:
            return ls2 - gA;
        case Type::Bandpass:
            return ls1 - yT;
        case Type::Highpass:
            return yT / g;
        default:
            return ls2 - gA; // lowpass
        }
    }

  private:
    //==============================================================================
    void update();

    //==============================================================================
    NumericType g, h, R2, gh, gpR2, g2;
    std::vector<SampleType> s1{2}, s2{2};

    double sampleRate = 44100.0;
    NumericType cutoffFrequency = static_cast<NumericType>(1000.0),
                resonance = static_cast<NumericType>(1.0 / std::sqrt(2.0));
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_STATEVARIABLEFILTER_H
