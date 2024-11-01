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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_HYSTERESISPROCESSING_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_HYSTERESISPROCESSING_H

#include "HysteresisOps.h"

enum SolverType
{
    RK2 = 0,
    RK4,
    NR4,
    NR8,
    NUM_SOLVERS
};

/*
    Hysteresis processing for a model of an analog tape machine.
    For more information on the DSP happening here, see:
    https://ccrma.stanford.edu/~jatin/420/tape/TapeModel_DAFx.pdf
*/
class HysteresisProcessing
{
  public:
    HysteresisProcessing();

    void reset();
    void setSampleRate(double newSR);

    void cook(float drive, float width, float sat, bool v1);

    /* Process a single sample */
    template <SolverType solver, typename Float> inline Float process(Float H) noexcept
    {
#if CHOWTAPE_HYSTERESIS_USE_SIMD
        auto H_d = HysteresisOps::deriv(H, H_n1, H_d_n1, SIMD_MM(set1_pd)(T));
#else
        auto H_d = HysteresisOps::deriv(H, H_n1, H_d_n1, T);
#endif

        Float M;
        switch (solver)
        {
        case RK2:
            M = RK2Solver(H, H_d);
            break;
        case RK4:
            M = RK4Solver(H, H_d);
            break;
        case NR4:
            M = NRSolver<4>(H, H_d);
            break;
        case NR8:
            M = NRSolver<8>(H, H_d);
            break;
        default:
#if CHOWTAPE_HYSTERESIS_USE_SIMD
            M = SIMD_MM(set1_pd)(0.0);
#else
            M = 0.0;
#endif
        };

        // check for instability
#if CHOWTAPE_HYSTERESIS_USE_SIMD
        auto illCondition = SIMD_MM(or_pd)(SIMD_MM(cmpunord_pd)(M, M),
                                           SIMD_MM(cmpgt_pd)(M, SIMD_MM(set1_pd)(upperLim)));
        M = SIMD_MM(andnot_pd)(illCondition, M);
        H_d = SIMD_MM(andnot_pd)(illCondition, H_d);
#else
        bool illCondition = std::isnan(M) || M > upperLim;
        M = illCondition ? 0.0 : M;
        H_d = illCondition ? 0.0 : H_d;
#endif

        M_n1 = M;
        H_n1 = H;
        H_d_n1 = H_d;

        return M;
    }

  private:
    // runge-kutta solvers
    template <typename Float> inline Float RK2Solver(Float H, Float H_d) noexcept
    {
#if CHOWTAPE_HYSTERESIS_USE_SIMD
#define F(a) SIMD_MM(set1_pd)(a)
#define M(a, b) SIMD_MM(mul_pd)(a, b)
#define A(a, b) SIMD_MM(add_pd)(a, b)
        const auto k1 = M(HysteresisOps::hysteresisFunc(M_n1, H_n1, H_d_n1, hpState), F(T));

        const auto k2 =
            M(HysteresisOps::hysteresisFunc(A(M_n1, M(k1, F(0.5))), M(A(H, H_n1), F(0.5)),
                                            M(A(H_d, H_d_n1), F(0.5)), hpState),
              F(T));
        return A(M_n1, k2);
#undef F
#undef M
#undef A
#else
        const Float k1 = HysteresisOps::hysteresisFunc(M_n1, H_n1, H_d_n1, hpState) * T;
        const Float k2 = HysteresisOps::hysteresisFunc(M_n1 + (k1 * 0.5), (H + H_n1) * 0.5,
                                                       (H_d + H_d_n1) * 0.5, hpState) *
                         T;
        return M_n1 + k2;
#endif
    }

    template <typename Float> inline Float RK4Solver(Float H, Float H_d) noexcept
    {
#if CHOWTAPE_HYSTERESIS_USE_SIMD
#define F(a) SIMD_MM(set1_pd)(a)
#define M(a, b) SIMD_MM(mul_pd)(a, b)
#define A(a, b) SIMD_MM(add_pd)(a, b)
        const auto H_1_2 = M(A(H, H_n1), F(0.5));
        const auto H_d_1_2 = M(A(H_d, H_d_n1), F(0.5));

        const auto k1 = M(HysteresisOps::hysteresisFunc(M_n1, H_n1, H_d_n1, hpState), F(T));
        const auto k2 =
            M(HysteresisOps::hysteresisFunc(A(M_n1, M(k1, F(0.5))), H_1_2, H_d_1_2, hpState), F(T));
        const auto k3 =
            M(HysteresisOps::hysteresisFunc(A(M_n1, M(k2, F(0.5))), H_1_2, H_d_1_2, hpState), F(T));
        const auto k4 = M(HysteresisOps::hysteresisFunc(A(M_n1, k3), H, H_d, hpState), F(T));

        const static auto oneSixth = F(1.0 / 6.0);
        const static auto oneThird = F(1.0 / 3.0);
        return A(M_n1, A(M(k1, oneSixth), A(M(k2, oneThird), A(M(k3, oneThird), M(k4, oneSixth)))));
#undef F
#undef M
#undef A
#else
        const Float H_1_2 = (H + H_n1) * 0.5;
        const Float H_d_1_2 = (H_d + H_d_n1) * 0.5;

        const Float k1 = HysteresisOps::hysteresisFunc(M_n1, H_n1, H_d_n1, hpState) * T;
        const Float k2 =
            HysteresisOps::hysteresisFunc(M_n1 + (k1 * 0.5), H_1_2, H_d_1_2, hpState) * T;
        const Float k3 =
            HysteresisOps::hysteresisFunc(M_n1 + (k2 * 0.5), H_1_2, H_d_1_2, hpState) * T;
        const Float k4 = HysteresisOps::hysteresisFunc(M_n1 + k3, H, H_d, hpState) * T;

        constexpr double oneSixth = 1.0 / 6.0;
        constexpr double oneThird = 1.0 / 3.0;
        return M_n1 + k1 * oneSixth + k2 * oneThird + k3 * oneThird + k4 * oneSixth;
#endif
    }

    // newton-raphson solvers
    template <int nIterations, typename Float> inline Float NRSolver(Float H, Float H_d) noexcept
    {
#if CHOWTAPE_HYSTERESIS_USE_SIMD
#define F(a) SIMD_MM(set1_pd)(a)
#define M(a, b) SIMD_MM(mul_pd)(a, b)
#define D(a, b) SIMD_MM(div_pd)(a, b)
#define A(a, b) SIMD_MM(add_pd)(a, b)
#define S(a, b) SIMD_MM(sub_pd)(a, b)
        auto _M = M_n1;
        const auto last_dMdt = HysteresisOps::hysteresisFunc(M_n1, H_n1, H_d_n1, hpState);

        SIMD_M128D dMdt, dMdtPrime, deltaNR, num, den;
        for (int n = 0; n < nIterations; ++n)
        {
            dMdt = HysteresisOps::hysteresisFunc(_M, H, H_d, hpState);
            dMdtPrime = HysteresisOps::hysteresisFuncPrime(H_d, dMdt, hpState);

            num = S(S(_M, M_n1), M(F(Talpha), A(dMdt, last_dMdt)));
            den = S(F(1.0), M(F(Talpha), dMdtPrime));
            deltaNR = D(num, den);
            _M = S(_M, deltaNR);
        }

        return _M;
#undef F
#undef M
#undef D
#undef A
#undef S
#else
        Float M = M_n1;
        const Float last_dMdt = HysteresisOps::hysteresisFunc(M_n1, H_n1, H_d_n1, hpState);

        Float dMdt;
        Float dMdtPrime;
        Float deltaNR;
        for (int n = 0; n < nIterations; ++n)
        {
            dMdt = HysteresisOps::hysteresisFunc(M, H, H_d, hpState);
            dMdtPrime = HysteresisOps::hysteresisFuncPrime(H_d, dMdt, hpState);
            deltaNR = (M - M_n1 - (Float)Talpha * (dMdt + last_dMdt)) /
                      (Float(1.0) - (Float)Talpha * dMdtPrime);
            M -= deltaNR;
        }

        return M;
#endif
    }

    // parameter values
    double fs = 48000.0;
    double T = 1.0 / fs;
    double Talpha = T / 1.9;
    double upperLim = 20.0;

    // state variables
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    SIMD_M128D M_n1;
    SIMD_M128D H_n1;
    SIMD_M128D H_d_n1;
#else
    double M_n1 = 0.0;
    double H_n1 = 0.0;
    double H_d_n1 = 0.0;
#endif

    HysteresisOps::HysteresisState hpState;
};

#endif
