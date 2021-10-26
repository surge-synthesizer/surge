#pragma once

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
