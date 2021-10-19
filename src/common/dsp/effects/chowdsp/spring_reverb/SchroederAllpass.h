#pragma once

#include "../shared/DelayLine.h"

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
        delay.prepare((double)sampleRate, 256);
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

    template <typename C = T>
    inline typename std::enable_if<std::is_same<C, __m128>::value, T>::type
    processSample(T x) noexcept
    {
        auto delayOut = nestedAllpass.processSample(delay.popSample(0));
        x = _mm_add_ps(x, _mm_mul_ps(g, delayOut));
        delay.pushSample(0, x);
        return _mm_sub_ps(delayOut, _mm_mul_ps(g, x));
    }

    template <typename C = T>
    inline typename std::enable_if<std::is_floating_point<C>::value, T>::type
    processSample(T x) noexcept
    {
        auto delayOut = nestedAllpass.processSample(delay.popSample(0));
        x += g * delayOut;
        delay.pushSample(0, x);
        return delayOut - g * x;
    }

  private:
    DelayLine<T, DelayLineInterpolationTypes::Thiran> delay{1 << 18, 1};
    SchroederAllpass<T, order - 1> nestedAllpass;
    T g;
};

template <typename T> class SchroederAllpass<T, 1>
{
  public:
    SchroederAllpass() = default;

    void prepare(float sampleRate) { delay.prepare((double)sampleRate, 256); }

    void reset() { delay.reset(); }

    void setParams(float delaySamp, T feedback)
    {
        delay.setDelay(delaySamp);
        g = feedback;
    }

    template <typename C = T>
    inline typename std::enable_if<std::is_same<C, __m128>::value, C>::type
    processSample(T x) noexcept
    {
        auto delayOut = delay.popSample(0);
        x = _mm_add_ps(x, _mm_mul_ps(g, delayOut));
        delay.pushSample(0, x);
        return _mm_sub_ps(delayOut, _mm_mul_ps(g, x));
    }

    template <typename C = T>
    inline typename std::enable_if<std::is_floating_point<C>::value, C>::type
    processSample(T x) noexcept
    {
        auto delayOut = delay.popSample(0);
        x += g * delayOut;
        delay.pushSample(0, x);
        return delayOut - g * x;
    }

  private:
    DelayLine<T, DelayLineInterpolationTypes::Thiran> delay{1 << 18, 1};
    T g;
};
} // namespace chowdsp

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
