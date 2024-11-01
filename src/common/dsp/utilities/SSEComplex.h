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

/*
 * You would hope you could specialize std::complex for SSE, but alas, you cannot
 * because it assumes (in the spec) that you only do float double or longdouble and
 * that shows up in how it is coded. So instead write a little SSEComplex class
 * with limited capabilities
 */

#ifndef SURGE_SRC_COMMON_DSP_UTILITIES_SSECOMPLEX_H
#define SURGE_SRC_COMMON_DSP_UTILITIES_SSECOMPLEX_H

#include "globals.h"
#include <complex>
#include <functional>
#include "sst/basic-blocks/dsp/FastMath.h"

struct SSEComplex
{
    typedef SIMD_M128 T;
    T _r, _i;
    constexpr SSEComplex(const T &r = SIMD_MM(setzero_ps)(), const T &i = SIMD_MM(setzero_ps)())
        : _r(r), _i(i)
    {
    }

    SSEComplex(float r[4], float i[4])
    {
        _r = SIMD_MM(loadu_ps)(r);
        _i = SIMD_MM(loadu_ps)(i);
    }

    inline SIMD_M128 real() const { return _r; }
    inline SIMD_M128 imag() const { return _i; }

    SSEComplex(std::initializer_list<float> r, std::initializer_list<float> i)
    {
        if (r.size() != 4 && i.size() != 4)
        {
            throw std::invalid_argument("Initialize lists must be of size 4");
        }
        float rfl alignas(16)[4], ifl alignas(16)[4];
        for (int q = 0; q < 4; ++q)
        {
            rfl[q] = *(r.begin() + q);
            ifl[q] = *(i.begin() + q);
        }

        _r = SIMD_MM(load_ps)(rfl);
        _i = SIMD_MM(load_ps)(ifl);
    }
    inline SSEComplex &operator+=(const SSEComplex &o)
    {
        _r = SIMD_MM(add_ps)(_r, o._r);
        _i = SIMD_MM(add_ps)(_i, o._i);
        return *this;
    }

    std::complex<float> atIndex(int i) const
    {
        float rfl alignas(16)[4], ifl alignas(16)[4];
        SIMD_MM(store_ps)(rfl, _r);
        SIMD_MM(store_ps)(ifl, _i);
        return std::complex<float>{rfl[i], ifl[i]};
    }

    inline static SSEComplex fastExp(SIMD_M128 angle)
    {
        angle = sst::basic_blocks::dsp::clampToPiRangeSSE(angle);
        return {sst::basic_blocks::dsp::fastcosSSE(angle),
                sst::basic_blocks::dsp::fastsinSSE(angle)};
    }

    inline SSEComplex map(std::function<std::complex<float>(const std::complex<float> &)> f)
    {
        float rfl alignas(16)[4], ifl alignas(16)[4];
        SIMD_MM(store_ps)(rfl, _r);
        SIMD_MM(store_ps)(ifl, _i);

        float rflR alignas(16)[4], iflR alignas(16)[4];
        for (int i = 0; i < 4; ++i)
        {
            auto a = std::complex<float>{rfl[i], ifl[i]};
            auto b = f(a);
            rflR[i] = b.real();
            iflR[i] = b.imag();
        }
        return {SIMD_MM(load_ps)(rflR), SIMD_MM(load_ps)(iflR)};
    }

    inline SIMD_M128 map_float(std::function<float(const std::complex<float> &)> f)
    {
        float rfl alignas(16)[4], ifl alignas(16)[4];
        SIMD_MM(store_ps)(rfl, _r);
        SIMD_MM(store_ps)(ifl, _i);

        float out alignas(16)[4];
        for (int i = 0; i < 4; ++i)
        {
            auto a = std::complex<float>{rfl[i], ifl[i]};
            out[i] = f(a);
        }
        return SIMD_MM(load_ps)(out);
    }
};

inline SSEComplex operator+(const SSEComplex &a, const SSEComplex &b)
{
    return {SIMD_MM(add_ps)(a._r, b._r), SIMD_MM(add_ps)(a._i, b._i)};
}

inline SIMD_M128 SSEComplexMulReal(const SSEComplex &a, const SSEComplex &b)
{
    return SIMD_MM(sub_ps)(SIMD_MM(mul_ps)(a._r, b._r), SIMD_MM(mul_ps)(a._i, b._i));
}

inline SIMD_M128 SSEComplexMulImag(const SSEComplex &a, const SSEComplex &b)
{
    return SIMD_MM(add_ps)(SIMD_MM(mul_ps)(a._r, b._i), SIMD_MM(mul_ps)(a._i, b._r));
}

inline SSEComplex operator*(const SSEComplex &a, const SSEComplex &b)
{
    return {SSEComplexMulReal(a, b), SSEComplexMulImag(a, b)};
}

inline SSEComplex operator*(const SSEComplex &a, const float &b)
{
    const auto scalar = SIMD_MM(set1_ps)(b);
    return {SIMD_MM(mul_ps)(a._r, scalar), SIMD_MM(mul_ps)(a._i, scalar)};
}

#endif // SURGE_SSECOMPLEX_H
