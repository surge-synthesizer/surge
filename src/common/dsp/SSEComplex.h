/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

/*
 * You would hope you could specialize std::complex for SSE, but alas, you cannot
 * because it assumes (in the spec) that you only do float double or longdouble and
 * that shows up in how it is coded. So instead write a little SSEComplex class
 * with limited capabilities
 */

#ifndef SURGE_SSECOMPLEX_H
#define SURGE_SSECOMPLEX_H

#include "globals.h"
#include <complex>
#include <functional>
#include "FastMath.h"

struct SSEComplex
{
    typedef __m128 T;
    T _r, _i;
    constexpr SSEComplex(const T &r = _mm_setzero_ps(), const T &i = _mm_setzero_ps())
        : _r(r), _i(i)
    {
    }

    SSEComplex(float r[4], float i[4])
    {
        _r = _mm_loadu_ps(r);
        _i = _mm_loadu_ps(i);
    }

    inline __m128 real() const { return _r; }
    inline __m128 imag() const { return _i; }

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

        _r = _mm_load_ps(rfl);
        _i = _mm_load_ps(ifl);
    }
    inline SSEComplex &operator+=(const SSEComplex &o)
    {
        _r = _mm_add_ps(_r, o._r);
        _i = _mm_add_ps(_i, o._i);
        return *this;
    }

    std::complex<float> atIndex(int i) const
    {
        float rfl alignas(16)[4], ifl alignas(16)[4];
        _mm_store_ps(rfl, _r);
        _mm_store_ps(ifl, _i);
        return std::complex<float>{rfl[i], ifl[i]};
    }

    inline static SSEComplex fastExp(__m128 angle)
    {
        angle = Surge::DSP::clampToPiRangeSSE(angle);
        return {Surge::DSP::fastcosSSE(angle), Surge::DSP::fastsinSSE(angle)};
    }

    inline SSEComplex map(std::function<std::complex<float>(const std::complex<float> &)> f)
    {
        float rfl alignas(16)[4], ifl alignas(16)[4];
        _mm_store_ps(rfl, _r);
        _mm_store_ps(ifl, _i);

        float rflR alignas(16)[4], iflR alignas(16)[4];
        for (int i = 0; i < 4; ++i)
        {
            auto a = std::complex<float>{rfl[i], ifl[i]};
            auto b = f(a);
            rflR[i] = b.real();
            iflR[i] = b.imag();
        }
        return {_mm_load_ps(rflR), _mm_load_ps(iflR)};
    }

    inline __m128 map_float(std::function<float(const std::complex<float> &)> f)
    {
        float rfl alignas(16)[4], ifl alignas(16)[4];
        _mm_store_ps(rfl, _r);
        _mm_store_ps(ifl, _i);

        float out alignas(16)[4];
        for (int i = 0; i < 4; ++i)
        {
            auto a = std::complex<float>{rfl[i], ifl[i]};
            out[i] = f(a);
        }
        return _mm_load_ps(out);
    }
};

inline SSEComplex operator+(const SSEComplex &a, const SSEComplex &b)
{
    return {_mm_add_ps(a._r, b._r), _mm_add_ps(a._i, b._i)};
}

inline __m128 SSEComplexMulReal(const SSEComplex &a, const SSEComplex &b)
{
    return _mm_sub_ps(_mm_mul_ps(a._r, b._r), _mm_mul_ps(a._i, b._i));
}

inline __m128 SSEComplexMulImag(const SSEComplex &a, const SSEComplex &b)
{
    return _mm_add_ps(_mm_mul_ps(a._r, b._i), _mm_mul_ps(a._i, b._r));
}

inline SSEComplex operator*(const SSEComplex &a, const SSEComplex &b)
{
    return {SSEComplexMulReal(a, b), SSEComplexMulImag(a, b)};
}

inline SSEComplex operator*(const SSEComplex &a, const float &b)
{
    const __m128 scalar = _mm_set1_ps(b);
    return {_mm_mul_ps(a._r, scalar), _mm_mul_ps(a._i, scalar)};
}

#endif // SURGE_SSECOMPLEX_H
