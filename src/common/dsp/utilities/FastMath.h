#pragma once

#include "globals.h"
#include <cmath>

/*
** Fast Math Approximations to various Functions
**
** Documentation on each one
*/

/*
** These are directly copied from JUCE6 modules/juce_dsp/mathos/juce_FastMathApproximations.h
**
** Since JUCE6 is GPL3 and Surge is GPL3 that copy is fine, but I did want to explicitly
** acknowledge it
*/

namespace Surge
{
namespace DSP
{

// JUCE6 Pade approximation of sin valid from -PI to PI with max error of 1e-5 and average error of
// 5e-7
inline float fastsin(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = -x * (-(float)11511339840 +
                           x2 * ((float)1640635920 + x2 * (-(float)52785432 + x2 * (float)479249)));
    auto denominator =
        (float)11511339840 + x2 * ((float)277920720 + x2 * ((float)3177720 + x2 * (float)18361));
    return numerator / denominator;
}

inline __m128 fastsinSSE(__m128 x) noexcept
{
#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define S(a, b) _mm_sub_ps(a, b)
#define F(a) _mm_set_ps1(a)
#define C(x) __m128 m##x = F((float)x)

    /*
    auto numerator = -x * (-(float)11511339840 +
                           x2 * ((float)1640635920 + x2 * (-(float)52785432 + x2 * (float)479249)));
    auto denominator =
        (float)11511339840 + x2 * ((float)277920720 + x2 * ((float)3177720 + x2 * (float)18361));
        */
    C(11511339840);
    C(1640635920);
    C(52785432);
    C(479249);
    C(277920720);
    C(3177720);
    C(18361);
    auto mnegone = F(-1);

    auto x2 = M(x, x);

    auto num = M(mnegone,
                 M(x, S(M(x2, A(m1640635920, M(x2, S(M(x2, m479249), m52785432)))), m11511339840)));
    auto den = A(m11511339840, M(x2, A(m277920720, M(x2, A(m3177720, M(x2, m18361))))));

#undef C
#undef M
#undef A
#undef S
#undef F
    return _mm_div_ps(num, den);
}

// JUCE6 Pade approximation of cos valid from -PI to PI with max error of 1e-5 and average error of
// 5e-7
inline float fastcos(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = -(-(float)39251520 + x2 * ((float)18471600 + x2 * (-1075032 + 14615 * x2)));
    auto denominator = (float)39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
    return numerator / denominator;
}

inline __m128 fastcosSSE(__m128 x) noexcept
{
#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define S(a, b) _mm_sub_ps(a, b)
#define F(a) _mm_set_ps1(a)
#define C(x) __m128 m##x = F((float)x)

    // auto x2 = x * x;
    auto x2 = M(x, x);

    C(39251520);
    C(18471600);
    C(1075032);
    C(14615);
    C(1154160);
    C(16632);
    C(127);

    // auto numerator = -(-(float)39251520 + x2 * ((float)18471600 + x2 * (-1075032 + 14615 * x2)));
    auto num = S(m39251520, M(x2, A(m18471600, M(x2, S(M(m14615, x2), m1075032)))));

    // auto denominator = (float)39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
    auto den = A(m39251520, M(x2, A(m1154160, M(x2, A(m16632, M(x2, m127))))));
#undef C
#undef M
#undef A
#undef S
#undef F
    return _mm_div_ps(num, den);
}

/*
** Push x into -PI, PI periodically. There is probably a faster way to do this
*/
inline float clampToPiRange(float x)
{
    if (x <= M_PI && x >= -M_PI)
        return x;
    float y = x + M_PI; // so now I am 0,2PI

    // float p = fmod( y, 2.0 * M_PI );

    constexpr float oo2p = 1.0 / (2.0 * M_PI);
    float p = y - 2.0 * M_PI * (int)(y * oo2p);

    if (p < 0)
        p += 2.0 * M_PI;
    return p - M_PI;
}

inline __m128 clampToPiRangeSSE(__m128 x)
{
    const auto mpi = _mm_set1_ps(M_PI);
    const auto m2pi = _mm_set1_ps(2.0 * M_PI);
    const auto oo2p = _mm_set1_ps(1.0 / (2.0 * M_PI));
    const auto mz = _mm_setzero_ps();

    auto y = _mm_add_ps(x, mpi);
    auto yip = _mm_cvtepi32_ps(_mm_cvttps_epi32(_mm_mul_ps(y, oo2p)));
    auto p = _mm_sub_ps(y, _mm_mul_ps(m2pi, yip));
    auto off = _mm_and_ps(_mm_cmplt_ps(p, mz), m2pi);
    p = _mm_add_ps(p, off);

    return _mm_sub_ps(p, mpi);
}

/*
** Valid in range -5, 5
*/
inline float fasttanh(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = x * (135135 + x2 * (17325 + x2 * (378 + x2)));
    auto denominator = 135135 + x2 * (62370 + x2 * (3150 + 28 * x2));
    return numerator / denominator;
}

// Valid in range (-PI/2, PI/2)
inline float fasttan(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = x * (-135135 + x2 * (17325 + x2 * (-378 + x2)));
    auto denominator = -135135 + x2 * (62370 + x2 * (-3150 + 28 * x2));
    return numerator / denominator;
}

inline __m128 fasttanhSSE(__m128 x)
{
    static const __m128 m135135 = _mm_set_ps1(135135), m17325 = _mm_set_ps1(17325),
                        m378 = _mm_set_ps1(378), m62370 = _mm_set_ps1(62370),
                        m3150 = _mm_set_ps1(3150), m28 = _mm_set_ps1(28);

#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)

    auto x2 = M(x, x);
    auto num = M(x, A(m135135, M(x2, A(m17325, M(x2, A(m378, x2))))));
    auto den = A(m135135, M(x2, A(m62370, M(x2, A(m3150, M(m28, x2))))));

#undef M
#undef A

    return _mm_div_ps(num, den);
}

inline __m128 fasttanhSSEclamped(__m128 x)
{
    auto xc = _mm_min_ps(_mm_set_ps1(5), _mm_max_ps(_mm_set_ps1(-5), x));
    return fasttanhSSE(xc);
}

/*
** Valid in range -6, 4
*/
inline float fastexp(float x) noexcept
{
    auto numerator = 1680 + x * (840 + x * (180 + x * (20 + x)));
    auto denominator = 1680 + x * (-840 + x * (180 + x * (-20 + x)));
    return numerator / denominator;
}

inline __m128 fastexpSSE(__m128 x) noexcept
{
#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define F(a) _mm_set_ps1(a)

    static const __m128 m1680 = F(1680), m840 = F(840), mneg840 = F(-840), m180 = F(180),
                        m20 = F(20), mneg20 = F(-20);

    auto num = A(m1680, M(x, A(m840, M(x, A(m180, M(x, A(m20, x)))))));
    auto den = A(m1680, M(x, A(mneg840, M(x, A(m180, M(x, A(mneg20, x)))))));

    return _mm_div_ps(num, den);

#undef M
#undef A
#undef F
}

} // namespace DSP
} // namespace Surge
