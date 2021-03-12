#pragma once

#include <cmath>
#include <complex>
#include <vector>

#include "basic_dsp.h"
#include "FastMath.h"

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
constexpr size_t N_filt = 5;

constexpr std::complex<float> iFiltRoot[] = {{251589.0f, 0.0f},
                                             {-130428.0f, -4165.0f},
                                             {-130428.0f, +4165.0f},
                                             {4634.0f, -22873.0f},
                                             {4634.0f, +22873.0f}};
constexpr std::complex<float> iFiltPole[] = {{-46580.0f, 0.0f},
                                             {-55482.0f, -25082.0f},
                                             {-55482.0f, +25082.0f},
                                             {-26292.0f, -59437.0f},
                                             {-26292.0f, +59437.0f}};

constexpr std::complex<float> oFiltRoot[] = {{5092.0f, 0.0f},
                                             {-11256.0f, -99566.0f},
                                             {-11256.0f, +99566.0f},
                                             {-13802.0f, -24606.0f},
                                             {-13802.0f, +24606.0f}};
constexpr std::complex<float> oFiltPole[] = {{-176261.0f, 0.0f},
                                             {-51468.0f, -21437.0f},
                                             {-51468.0f, +21437.0f},
                                             {-26276.0f, -59699.0f},
                                             {-26276.0f, +59699.0f}};
} // namespace FilterSpec

struct PowLUT
{
    PowLUT(float baseMin, float baseMax, float expMin, float expMax, int nTables, int nPoints)
    {
        tables.resize(nTables);
        for(auto &table : tables)
            table.resize(nPoints, 0.0f);

        baseOffset = baseMin;
        baseScale = (float) nTables / (baseMax - baseMin);
        expOffset = expMin;
        expScale = (float) nTables / (expMax - expMin);

        for(int i = 0; i < nTables; ++i)
        {
            for(int j = 0; j < nPoints; ++j)
            {
                tables[i][j] = std::pow((float) i / baseScale + baseOffset, (float) j / expScale + expOffset);
            }
        }
    }

    inline float operator()(float base, float exp) const noexcept
    {
        auto baseIdx = size_t ((base - baseOffset) * baseScale);
        auto expIdx = size_t ((base - expOffset) * expScale);
        return tables[baseIdx][expIdx];
    }

private:
    std::vector<std::vector<float>> tables;

    float baseOffset, baseScale;
    float expOffset, expScale;
};

static PowLUT powLut { -1.0f, 1.0f, -0.1f, 0.1f, 256, 128 };

inline std::complex<float> fast_complex_exp(float angle)
{
    return std::complex<float>(Surge::DSP::fastcos(angle), Surge::DSP::fastsin(angle));
}

inline std::complex<float> fast_complex_pow(float mag, float angle, float b, const PowLUT& powLut)
{
    auto mag_pow = powLut(mag, b); // std::pow(mag, b); // @TODO: use a fast approximation, maybe a lookup table?
    auto angle_pow = angle * b;
    return fast_complex_exp(angle_pow);
}

struct InputFilter
{
  public:
    InputFilter(std::complex<float> sPlaneRoot, std::complex<float> sPlanePole, float sampleTime,
                std::complex<float> *xPtr)
        : root(sPlaneRoot), pole(sPlanePole), x(xPtr), Ts(sampleTime)
    {
        root_corr = root;
        pole_corr = std::exp(pole * Ts);
        *x = 0.0f;

        // make sure the LUT gets initialised here...
        powLut(0.5f, 0.01f);
    }

    inline std::complex<float> calcGUp() noexcept
    {
        Arecurse *= Aplus;
        return gCoef * Arecurse;
    }

    inline void calcGDown() noexcept { Arecurse *= Aminus; }

    inline void process(float u) { *x = pole_corr * (*x) + u; }

    inline void set_freq(float freq)
    {
        constexpr float originalCutoff = 9400.0f;
        root_corr = root * (freq / originalCutoff);
        pole_corr = std::exp(pole * (freq / originalCutoff) * Ts);

        pole_corr_mag = std::abs(pole_corr);
        pole_corr_angle = std::arg(pole_corr);

        gCoef = Ts * root_corr;
        Aminus = fast_complex_pow(pole_corr_mag, pole_corr_angle, -Ts, powLut);
    }

    // since we use recursive and approximate calculations for the other
    // A values, we need to use full precision here to realign everything
    // so the numerical error doesn't get away from us.
    inline void set_time(float tn) { Arecurse = std::pow(pole_corr, tn); }

    inline void set_delta(float delta)
    {
        Aplus = fast_complex_pow(pole_corr_mag, pole_corr_angle, delta, powLut);
    }

  private:
    const std::complex<float> root;
    const std::complex<float> pole;

    std::complex<float> root_corr;
    std::complex<float> pole_corr;
    float pole_corr_mag;
    float pole_corr_angle;

    std::complex<float> Arecurse{1.0f, 0.0f};
    std::complex<float> Aplus;
    std::complex<float> Aminus;

    std::complex<float> *const x;
    const float Ts;
    std::complex<float> gCoef;

    // PowLUT powLut { -1.0f, 1.0f, -0.1f, 0.1f, 256, 128 };
};

struct OutputFilter
{
  public:
    OutputFilter(std::complex<float> sPlaneRoot, std::complex<float> sPlanePole, float sampleTime,
                 std::complex<float> *xPtr)
        : gCoef(sPlaneRoot / sPlanePole), pole(sPlanePole), x(xPtr), Ts(sampleTime)
    {
        pole_corr = std::exp(pole * Ts);
        *x = 0.0f;
    }

    inline std::complex<float> calcGUp() noexcept
    {
        Arecurse *= Aplus;
        return Amult * Arecurse;
    }

    inline void calcGDown() noexcept { Arecurse *= Aminus; }

    inline void process(std::complex<float> u) { *x = pole_corr * (*x) + u; }

    inline std::complex<float> getGCoef() const noexcept { return gCoef; }

    inline void set_freq(float freq)
    {
        constexpr float originalCutoff = 11000.0f;
        pole_corr = std::exp(pole * (freq / originalCutoff) * Ts);

        pole_corr_mag = std::abs(pole_corr);
        pole_corr_angle = std::arg(pole_corr);

        Aminus = fast_complex_pow(pole_corr_mag, pole_corr_angle, Ts, powLut);
        Amult = gCoef * pole_corr;
    }

    inline void set_time(float tn) { Arecurse = std::pow(pole_corr, 1.0f - tn); }

    inline void set_delta(float delta)
    {
        Aplus = fast_complex_pow(pole_corr_mag, pole_corr_angle, -delta, powLut);
    }

  private:
    const std::complex<float> gCoef;
    const std::complex<float> pole;

    std::complex<float> pole_corr;
    float pole_corr_mag;
    float pole_corr_angle;

    std::complex<float> Arecurse{1.0f, 0.0f};
    std::complex<float> Aplus;
    std::complex<float> Aminus;
    std::complex<float> Amult;

    std::complex<float> *const x;
    const float Ts;

    // PowLUT powLut { -1.0f, 1.0f, -0.1f, 0.1f, 256, 128 };
};
