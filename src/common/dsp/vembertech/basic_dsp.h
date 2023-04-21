#pragma once
#include "shared.h"
#include <cstring>

template <typename T> inline T limit_range(const T &x, const T &low, const T &high)
{
    assert(low <= high);
    return std::clamp(x, low, high);
}

template <typename T> inline T limit01(const T &x) { return limit_range(x, (T)0, (T)1); }
template <typename T> inline T clamp01(const T &x) { return limit_range(x, (T)0, (T)1); }
template <typename T> inline T limitpm1(const T &x) { return limit_range(x, (T)-1, (T)1); }
template <typename T> inline T clamp1bp(const T &x) { return limit_range(x, (T)-1, (T)1); }

inline void float2i15_block(float *f, short *s, int n)
{
    for (int i = 0; i < n; i++)
    {
        s[i] = (short)(int)limit_range((int)((float)f[i] * 16384.f), -16384, 16383);
    }
}

inline void i152float_block(short *s, float *f, int n)
{
    const float scale = 1.f / 16384.f;
    for (int i = 0; i < n; i++)
    {
        f[i] = (float)s[i] * scale;
    }
}

inline void i16toi15_block(short *s, short *o, int n)
{
    for (int i = 0; i < n; i++)
    {
        o[i] = s[i] >> 1;
    }
}

// These pan laws are bad so don't port them
inline float megapanL(float pos) // valid range -2 .. 2 (> +- 1 is inverted phase)
{
    if (pos > 2.f)
        pos = 2.f;
    if (pos < -2.f)
        pos = -2.f;
    return (1 - 0.75f * pos - 0.25f * pos * pos);
}

inline float megapanR(float pos)
{
    if (pos > 2.f)
        pos = 2.f;
    if (pos < -2.f)
        pos = -2.f;
    return (1 + 0.75f * pos - 0.25f * pos * pos);
}
