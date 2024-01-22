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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRING_REVERB_SCHROEDERALLPASS_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRING_REVERB_SCHROEDERALLPASS_H

#include "../shared/chowdsp_DelayLine.h"

#ifdef __GNUC__ // GCC doesn't like "ignored-attributes"...
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

namespace chowdsp
{

template <typename T = float, int order = 1> class SchroederAllpass
{
  public:
    SchroederAllpass() = default;

    void prepare(float sampleRate)
    {
        delay.prepare({(double)sampleRate, 256, 1});
        nestedAllpass.prepare(sampleRate);
    }

    void reset()
    {
        delay.reset();
        nestedAllpass.reset();
    }

    void setParams(float delaySamp, T feedback)
    {
        delay.setDelay(delaySamp);
        g = feedback;
        nestedAllpass.setParams(delaySamp, feedback);
    }

    inline T processSample(T x) noexcept
    {
        auto delayOut = nestedAllpass.processSample(delay.popSample(0));
        x += g * delayOut;
        delay.pushSample(0, x);
        return delayOut - g * x;
    }

  private:
    DelayLine<T, DelayLineInterpolationTypes::Thiran> delay{1 << 18};
    SchroederAllpass<T, order - 1> nestedAllpass;
    T g;
};

template <typename T> class SchroederAllpass<T, 1>
{
  public:
    SchroederAllpass() = default;

    void prepare(float sampleRate) { delay.prepare({(double)sampleRate, 256, 1}); }

    void reset() { delay.reset(); }

    void setParams(float delaySamp, T feedback)
    {
        delay.setDelay(delaySamp);
        g = feedback;
    }

    inline T processSample(T x) noexcept
    {
        auto delayOut = delay.popSample(0);
        x += g * delayOut;
        delay.pushSample(0, x);
        return delayOut - g * x;
    }

  private:
    DelayLine<T, DelayLineInterpolationTypes::Thiran> delay{1 << 18};
    T g;
};
} // namespace chowdsp

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRING_REVERB_SCHROEDERALLPASS_H
