#ifndef HYSTERESISPROCESSING_H_INCLUDED
#define HYSTERESISPROCESSING_H_INCLUDED

#include <cmath>
#include <numeric>

namespace chowdsp
{

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

    void cook(float drive, float width, float sat);

    /* Process a single sample */
    inline double process(double H) noexcept
    {
        double H_d = deriv(H, H_n1, H_d_n1);
        double M = NRSolver(H, H_d);

        // check for instability
        bool illCondition = std::isnan(M) || M > upperLim;
        M = illCondition ? 0.0 : M;
        H_d = illCondition ? 0.0 : H_d;

        M_n1 = M;
        H_n1 = H;
        H_d_n1 = H_d;

        return M;
    }

  private:
    static constexpr double ONE_THIRD = 1.0 / 3.0;
    static constexpr double NEG_TWO_OVER_15 = -2.0 / 15.0;

    inline int sign(double x) const noexcept { return (x > 0.0) - (x < 0.0); }

    inline double langevin(double x) const noexcept // Langevin function
    {
        if (!nearZero)
            return (coth) - (1.0 / x);
        else
            return x / 3.0;
    }

    inline double langevinD(double x) const noexcept // Derivative of Langevin function
    {
        if (!nearZero)
            return (1.0 / (x * x)) - (coth * coth) + 1.0;
        else
            return ONE_THIRD;
    }

    inline double langevinD2(double x) const noexcept // 2nd derivative of Langevin function
    {
        if (!nearZero)
            return 2.0 * coth * (coth * coth - 1.0) - (2.0 / (x * x * x));
        else
            return NEG_TWO_OVER_15 * x;
        ;
    }

    inline double deriv(double x_n, double x_n1,
                        double x_d_n1) const noexcept // Derivative by alpha transform
    {
        constexpr double dAlpha = 0.75;
        return (((1.0 + dAlpha) / T) * (x_n - x_n1)) - dAlpha * x_d_n1;
    }

    // hysteresis function dM/dt
    inline double hysteresisFunc(double M, double H, double H_d) noexcept
    {
        Q = (H + alpha * M) / a;
        coth = 1.0 / std::tanh(Q);
        nearZero = Q < 0.001 && Q > -0.001;

        M_diff = M_s * langevin(Q) - M;

        delta = (double)((H_d >= 0.0) - (H_d < 0.0));
        delta_M = (double)(sign(delta) == sign(M_diff));

        L_prime = langevinD(Q);

        kap1 = nc * delta_M;
        f1Denom = nc * delta * k - alpha * M_diff;
        f1 = kap1 * M_diff / f1Denom;
        f2 = M_s_oa_tc * L_prime;
        f3 = 1.0 - (M_s_oa_tc_talpha * L_prime);

        return H_d * (f1 + f2) / f3;
    }

    // derivative of hysteresis func w.r.t M (depends on cached values from computing
    // hysteresisFunc)
    inline double hysteresisFuncPrime(double H_d, double dMdt) const noexcept
    {
        const double L_prime2 = langevinD2(Q);
        const double M_diff2 = M_s_oa_talpha * L_prime - 1.0;

        const double f1_p =
            kap1 * ((M_diff2 / f1Denom) + M_diff * alpha * M_diff2 / (f1Denom * f1Denom));
        const double f2_p = M_s_oaSq_tc_talpha * L_prime2;
        const double f3_p = -M_s_oaSq_tc_talphaSq * L_prime2;

        return H_d * (f1_p + f2_p) / f3 - dMdt * f3_p / f3;
    }

    // newton-raphson solvers
    inline double NRSolver(double H, double H_d) noexcept
    {
        double M = M_n1;
        const double last_dMdt = hysteresisFunc(M_n1, H_n1, H_d_n1);

        double dMdt, dMdtPrime, deltaNR;

        // loop #1
        dMdt = hysteresisFunc(M, H, H_d);
        dMdtPrime = hysteresisFuncPrime(H_d, dMdt);
        deltaNR = (M - M_n1 - Talpha * (dMdt + last_dMdt)) / (1.0 - Talpha * dMdtPrime);
        M -= deltaNR;

        // loop #2
        dMdt = hysteresisFunc(M, H, H_d);
        dMdtPrime = hysteresisFuncPrime(H_d, dMdt);
        deltaNR = (M - M_n1 - Talpha * (dMdt + last_dMdt)) / (1.0 - Talpha * dMdtPrime);
        M -= deltaNR;

        // loop #3
        dMdt = hysteresisFunc(M, H, H_d);
        dMdtPrime = hysteresisFuncPrime(H_d, dMdt);
        deltaNR = (M - M_n1 - Talpha * (dMdt + last_dMdt)) / (1.0 - Talpha * dMdtPrime);
        M -= deltaNR;

        return M;
    }

    // parameter values
    double fs = 48000.0;
    double T = 1.0 / fs;
    double Talpha = T / 1.9;
    double M_s = 1.0;
    double a = M_s / 4.0;
    const double alpha = 1.6e-3;
    double k = 0.47875;
    double c = 1.7e-1;
    double upperLim = 20.0;

    // Save calculations
    double nc = 1 - c;
    double M_s_oa = M_s / a;
    double M_s_oa_talpha = alpha * M_s / a;
    double M_s_oa_tc = c * M_s / a;
    double M_s_oa_tc_talpha = alpha * c * M_s / a;
    double M_s_oaSq_tc_talpha = alpha * c * M_s / (a * a);
    double M_s_oaSq_tc_talphaSq = alpha * alpha * c * M_s / (a * a);

    // state variables
    double M_n1 = 0.0;
    double H_n1 = 0.0;
    double H_d_n1 = 0.0;

    // temp vars
    double Q, M_diff, delta, delta_M, L_prime, kap1, f1Denom, f1, f2, f3;
    double coth = 0.0;
    bool nearZero = false;
};

} // namespace chowdsp

#endif
