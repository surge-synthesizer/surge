#pragma once

#include <cmath>
#include "FastMath.h"

#define CHOWTAPE_HYSTERESIS_USE_SIMD 1

namespace HysteresisOps
{
#define F(a) _mm_set1_pd(a)
#define M(a, b) _mm_mul_pd(a, b)
#define D(a, b) _mm_div_pd(a, b)
#define A(a, b) _mm_add_pd(a, b)
#define S(a, b) _mm_sub_pd(a, b)

struct HysteresisState
{
    // parameter values
    double M_s = 1.0;
    double a = M_s / 4.0;
    static constexpr double alpha = 1.6e-3;
    double k = 0.47875;
    double c = 1.7e-1;

    // Save calculations
    double nc = 1 - c;
    double M_s_oa = M_s / a;
    double M_s_oa_talpha = alpha * M_s / a;
    double M_s_oa_tc = c * M_s / a;
    double M_s_oa_tc_talpha = alpha * c * M_s / a;
    double M_s_oaSq_tc_talpha = alpha * c * M_s / (a * a);
    double M_s_oaSq_tc_talphaSq = alpha * alpha * c * M_s / (a * a);

    // temp vars
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    __m128d Q, M_diff, L_prime, kap1, f1Denom, f1, f2, f3;
    __m128d coth;
    __m128d nearZero;
#else
    double Q, M_diff, L_prime, kap1, f1Denom, f1, f2, f3;
    double coth = 0.0;
    bool nearZero = false;
#endif
};

constexpr double ONE_THIRD = 1.0 / 3.0;
constexpr double NEG_TWO_OVER_15 = -2.0 / 15.0;

constexpr inline int sign(double x) { return int(x > 0.0) - int(x < 0.0); }

static inline __m128d signumSIMD(__m128d val)
{
    auto positive = _mm_and_pd(F(1.0), _mm_cmplt_pd(F(0.0), val));
    auto negative = _mm_and_pd(F(1.0), _mm_cmplt_pd(val, F(0.0)));
    return S(positive, negative);
}

/**
 * FastMath.h has a SSE tanh approximation using Pade
 * continued fractions, but we need one here for
 * double-precision SIMD registers. Also uses a slightly
 * higher-order continued fraction.
 */
static inline __m128d tanhSIMD(__m128d x)
{
    auto xc = _mm_min_pd(F(5.7), _mm_max_pd(F(-5.7), x));

    static const __m128d v2027025 = F(2027025.0), v270270 = F(270270.0), v6930 = F(6930.0),
                         v36 = F(36.0), v945945 = F(945945.0), v51975 = F(51975.0), v630 = F(630.0);

    auto x2 = M(xc, xc);
    auto numerator = M(xc, A(v2027025, M(x2, A(v270270, M(x2, A(v6930, M(v36, x2)))))));
    auto denominator = A(v2027025, M(x2, A(v945945, M(x2, A(v51975, M(x2, A(v630, x2)))))));
    return D(numerator, denominator);
}

/** Langevin function */
template <typename Float, typename Bool>
static inline Float langevin(Float x, Float coth, Bool nearZero) noexcept
{
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    return A(_mm_andnot_pd(nearZero, S(coth, D(F(1.0), x))), _mm_and_pd(nearZero, D(x, F(3.0))));
#else
    return !nearZero ? (coth) - (1.0 / x) : x / 3.0;
#endif
}

/** Derivative of Langevin function */
template <typename Float, typename Bool>
static inline Float langevinD(Float x, Float coth, Bool nearZero) noexcept
{
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    return A(_mm_andnot_pd(nearZero, A(S(D(F(1.0), M(x, x)), M(coth, coth)), F(1.0))),
             _mm_and_pd(nearZero, F(ONE_THIRD)));
#else
    return !nearZero ? (1.0 / (x * x)) - (coth * coth) + 1.0 : ONE_THIRD;
#endif
}

/** 2nd derivative of Langevin function */
template <typename Float, typename Bool>
static inline Float langevinD2(Float x, Float coth, Bool nearZero) noexcept
{
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    auto x3 = D(F(2.0), M(x, M(x, x)));
    auto coth3 = M(F(2.0), M(coth, S(M(coth, coth), F(1.0))));
    return A(_mm_andnot_pd(nearZero, S(coth3, x3)), _mm_and_pd(nearZero, M(x, F(NEG_TWO_OVER_15))));
#else
    return !nearZero ? 2.0 * coth * (coth * coth - 1.0) - (2.0 / (x * x * x)) : NEG_TWO_OVER_15 * x;
#endif
}

/** Derivative by alpha transform */
template <typename Float>
static inline Float deriv(Float x_n, Float x_n1, Float x_d_n1, Float T) noexcept
{
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    const static __m128d dAlpha = F(0.75);
    return S(M(D(A(F(1.0), dAlpha), T), S(x_n, x_n1)), M(dAlpha, x_d_n1));
#else
    constexpr Float dAlpha = 0.75;
    return ((((Float)1.0 + dAlpha) / T) * (x_n - x_n1)) - dAlpha * x_d_n1;
#endif
}

#if CHOWTAPE_HYSTERESIS_USE_SIMD
/** hysteresis function dM/dt */
static inline __m128d hysteresisFunc(__m128d _M, __m128d H, __m128d H_d,
                                     HysteresisState &hp) noexcept
{
    hp.Q = M(A(H, M(_M, F(hp.alpha))), D(F(1.0), F(hp.a)));

    hp.coth = D(F(1.0), tanhSIMD(hp.Q));
    hp.nearZero = _mm_and_pd(_mm_cmplt_pd(hp.Q, F(0.001)), _mm_cmpgt_pd(hp.Q, F(-0.001)));

    hp.M_diff = S(M(langevin(hp.Q, hp.coth, hp.nearZero), F(hp.M_s)), _M);
    const auto delta = S(_mm_and_pd(F(1.0), _mm_cmpge_pd(H_d, F(0.0))),
                         _mm_and_pd(F(1.0), _mm_cmplt_pd(H_d, F(0.0))));
    const auto delta_M = _mm_cmpeq_pd(signumSIMD(delta), signumSIMD(hp.M_diff));
    hp.kap1 = _mm_and_pd(F(hp.nc), delta_M);

    hp.L_prime = langevinD(hp.Q, hp.coth, hp.nearZero);

    hp.f1Denom = S(M(M(F(hp.nc), delta), F(hp.k)), M(F(hp.alpha), hp.M_diff));
    hp.f1 = D(M(hp.kap1, hp.M_diff), hp.f1Denom);
    hp.f2 = M(hp.L_prime, F(hp.M_s_oa_tc));
    hp.f3 = S(F(1.0), M(hp.L_prime, F(hp.M_s_oa_tc_talpha)));

    return D(M(H_d, A(hp.f1, hp.f2)), hp.f3);
}

// derivative of hysteresis func w.r.t M (depends on cached values from computing hysteresisFunc)
static inline __m128d hysteresisFuncPrime(__m128d H_d, __m128d dMdt, HysteresisState &hp) noexcept
{
    const auto L_prime2 = langevinD2(hp.Q, hp.coth, hp.nearZero);
    const auto M_diff2 = S(M(hp.L_prime, F(hp.M_s_oa_talpha)), F(1.0));

    const auto f1_p = M(hp.kap1, A(D(M_diff2, hp.f1Denom), D(M(hp.M_diff, M(F(hp.alpha), M_diff2)),
                                                             M(hp.f1Denom, hp.f1Denom))));
    const auto f2_p = M(L_prime2, F(hp.M_s_oaSq_tc_talpha));
    const auto f3_p = M(L_prime2, F(-hp.M_s_oaSq_tc_talphaSq));

    return D(S(M(H_d, A(f1_p, f2_p)), M(dMdt, f3_p)), hp.f3);
}
#else
template <typename Float>
static inline double hysteresisFunc(Float _M, Float H, Float H_d, HysteresisState &hp) noexcept
{
    hp.Q = (H + _M * hp.alpha) * (1.0 / hp.a);
    hp.coth = 1.0 / std::tanh(hp.Q);
    hp.nearZero = hp.Q < 0.001 && hp.Q > -0.001;

    hp.M_diff = langevin(hp.Q, hp.coth, hp.nearZero) * hp.M_s - _M;
    const auto delta = (Float)((H_d >= 0.0) - (H_d < 0.0));
    const auto delta_M = (Float)(sign(delta) == sign(hp.M_diff));
    hp.kap1 = (Float)hp.nc * delta_M;

    hp.L_prime = langevinD(hp.Q, hp.coth, hp.nearZero);

    hp.f1Denom = ((Float)hp.nc * delta) * hp.k - (Float)hp.alpha * hp.M_diff;
    hp.f1 = hp.kap1 * hp.M_diff / hp.f1Denom;
    hp.f2 = hp.L_prime * hp.M_s_oa_tc;
    hp.f3 = (Float)1.0 - (hp.L_prime * hp.M_s_oa_tc_talpha);

    return H_d * (hp.f1 + hp.f2) / hp.f3;
}

// derivative of hysteresis func w.r.t M (depends on cached values from computing hysteresisFunc)
template <typename Float>
static inline Float hysteresisFuncPrime(Float H_d, Float dMdt, HysteresisState &hp) noexcept
{
    const Float L_prime2 = langevinD2(hp.Q, hp.coth, hp.nearZero);
    const Float M_diff2 = hp.L_prime * hp.M_s_oa_talpha - 1.0;

    const Float f1_p = hp.kap1 * ((M_diff2 / hp.f1Denom) +
                                  hp.M_diff * hp.alpha * M_diff2 / (hp.f1Denom * hp.f1Denom));
    const Float f2_p = L_prime2 * hp.M_s_oaSq_tc_talpha;
    const Float f3_p = L_prime2 * (-hp.M_s_oaSq_tc_talphaSq);

    return H_d * (f1_p + f2_p) / hp.f3 - dMdt * f3_p / hp.f3;
}
#endif

#undef M
#undef A
#undef S
#undef F

} // namespace HysteresisOps
