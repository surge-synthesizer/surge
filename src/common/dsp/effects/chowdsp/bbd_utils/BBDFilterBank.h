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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDFILTERBANK_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDFILTERBANK_H

#include "SSEComplex.h"
#include "portable_intrinsics.h"

/**
 * Bucket-Bridage Device filters, as derived by
 * Martin Holters and Julian Parker:
 * http://dafx2018.web.ua.pt/papers/DAFx2018_paper_12.pdf
 */

/**
 * Anti-aliasing/reconstruction filters used by JUNO-60 chorus.
 * Root/Pole analysis borrowed from the above paper.
 */
namespace FilterSpec
{
constexpr size_t N_filt = 4;

constexpr std::complex<float> iFiltRoot[] = {{-10329.2715f, -329.848f},
                                             {-10329.2715f, +329.848f},
                                             {366.990557f, -1811.4318f},
                                             {366.990557f, +1811.4318f}};
constexpr std::complex<float> iFiltPole[] = {
    {-55482.0f, -25082.0f}, {-55482.0f, +25082.0f}, {-26292.0f, -59437.0f}, {-26292.0f, +59437.0f}};

constexpr std::complex<float> oFiltRoot[] = {
    {-11256.0f, -99566.0f}, {-11256.0f, +99566.0f}, {-13802.0f, -24606.0f}, {-13802.0f, +24606.0f}};
constexpr std::complex<float> oFiltPole[] = {
    {-51468.0f, -21437.0f}, {-51468.0f, +21437.0f}, {-26276.0f, -59699.0f}, {-26276.0f, +59699.0f}};
} // namespace FilterSpec

inline SSEComplex fast_complex_pow(SIMD_M128 angle, float b)
{
    const auto scalar = SIMD_MM(set1_ps)(b);
    auto angle_pow = SIMD_MM(mul_ps)(angle, scalar);
    return SSEComplex::fastExp(angle_pow);
}

struct InputFilterBank
{
  public:
    InputFilterBank(float sampleTime) : Ts(sampleTime)
    {
        float root_real alignas(16)[4];
        float root_imag alignas(16)[4];
        float pole_real alignas(16)[4];
        float pole_imag alignas(16)[4];
        for (size_t i = 0; i < FilterSpec::N_filt; ++i)
        {
            root_real[i] = FilterSpec::iFiltRoot[i].real();
            root_imag[i] = FilterSpec::iFiltRoot[i].imag();

            pole_real[i] = FilterSpec::iFiltPole[i].real();
            pole_imag[i] = FilterSpec::iFiltPole[i].imag();
        }
        roots = SSEComplex(root_real, root_imag);
        poles = SSEComplex(pole_real, pole_imag);
    }

    inline void set_freq(float freq)
    {
        constexpr float originalCutoff = 9900.0f;
        const float freqFactor = freq / originalCutoff;
        root_corr = roots * freqFactor;
        pole_corr = poles.map([&freqFactor, this](const std::complex<float> &f) {
            return std::exp(f * freqFactor * Ts);
        });

        pole_corr_angle =
            pole_corr.map_float([](const std::complex<float> &f) { return std::arg(f); });

        gCoef = root_corr * Ts;
    }

    inline void set_time(float tn)
    {
        Gcalc =
            gCoef * pole_corr.map([&tn](const std::complex<float> &f) { return std::pow(f, tn); });
    }

    inline void set_delta(float delta) { Aplus = fast_complex_pow(pole_corr_angle, delta); }

    inline void calcG() noexcept { Gcalc = Aplus * Gcalc; }

    inline void process(float u)
    {
        x = pole_corr * x + SSEComplex(SIMD_MM(set1_ps)(u), SIMD_MM(set1_ps)(0.0f));
    }

    SSEComplex x;
    SSEComplex Gcalc{{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};

  private:
    SSEComplex roots;
    SSEComplex poles;
    SSEComplex root_corr;
    SSEComplex pole_corr;
    SIMD_M128 pole_corr_angle;

    SSEComplex Aplus;

    const float Ts;
    SSEComplex gCoef;
};

struct OutputFilterBank
{
  public:
    OutputFilterBank(float sampleTime) : Ts(sampleTime)
    {
        float gcoefs_real alignas(16)[4];
        float gcoefs_imag alignas(16)[4];
        float pole_real alignas(16)[4];
        float pole_imag alignas(16)[4];
        for (size_t i = 0; i < FilterSpec::N_filt; ++i)
        {
            auto gVal = FilterSpec::oFiltRoot[i] / FilterSpec::oFiltPole[i];
            gcoefs_real[i] = gVal.real();
            gcoefs_imag[i] = gVal.imag();

            pole_real[i] = FilterSpec::oFiltPole[i].real();
            pole_imag[i] = FilterSpec::oFiltPole[i].imag();
        }
        gCoef = SSEComplex(gcoefs_real, gcoefs_imag);
        poles = SSEComplex(pole_real, pole_imag);
    }

    inline float calcH0() const { return -1.0f * vSum(gCoef.real()); }

    inline void set_freq(float freq)
    {
        constexpr float originalCutoff = 9500.0f;
        const float freqFactor = freq / originalCutoff;
        pole_corr = poles.map([&freqFactor, this](const std::complex<float> &f) {
            return std::exp(f * freqFactor * Ts);
        });

        pole_corr_angle =
            pole_corr.map_float([](const std::complex<float> &f) { return std::arg(f); });

        Amult = gCoef * pole_corr;
    }

    inline void set_time(float tn)
    {
        Gcalc = Amult * pole_corr.map(
                            [&tn](const std::complex<float> &f) { return std::pow(f, 1.0f - tn); });
    }

    inline void set_delta(float delta) { Aplus = fast_complex_pow(pole_corr_angle, -delta); }

    inline void calcG() noexcept { Gcalc = Aplus * Gcalc; }

    inline void process(SSEComplex u) { x = pole_corr * x + u; }

    SSEComplex x;
    SSEComplex Gcalc{{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};

  private:
    SSEComplex gCoef;
    SSEComplex poles;
    SSEComplex root_corr;
    SSEComplex pole_corr;
    SIMD_M128 pole_corr_angle;

    SSEComplex Aplus;

    const float Ts;
    SSEComplex Amult;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDFILTERBANK_H
