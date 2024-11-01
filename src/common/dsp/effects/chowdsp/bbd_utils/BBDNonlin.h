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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDNONLIN_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDNONLIN_H

// #include "../shared/wdf.h"
#include "../shared/wdf_sse.h"
#include "../shared/omega.h"
#include <memory>

// 2.7% w/ spikes up to 3.2%
// After SSE: 1.8% +/- 0.2%

/**
 * Faster version of chowdsp::WDF_SSE::Diode.
 * Basically making use of the fact that we know some other values
 * in the WDF tree will not change on the fly.
 */
class FastDiode : public chowdsp::WDF_SSE::WDFNode
{
  public:
    /** Creates a new WDF diode, with the given diode specifications.
     * @param Is: reverse saturation current
     * @param Vt: thermal voltage
     */
    FastDiode(float Is, float Vt)
        : chowdsp::WDF_SSE::WDFNode("Diode"), Is(vLoad1(Is)), Vt(vLoad1(Vt)),
          oneOverVt(vLoad1(1.0 / Vt))
    {
    }

    virtual ~FastDiode() {}

    inline void precomputeValues()
    {
        nextR_Is = vMul(next->R, Is);

        logVal = vMul(nextR_Is, oneOverVt);
        // array must be aligned to 16-byte boundary for SSE load
        float f alignas(16)[4];
        SIMD_MM(store_ps)(f, logVal);
        f[0] = std::log(f[0]);
        f[1] = std::log(f[1]);
        f[2] = std::log(f[2]);
        f[3] = std::log(f[3]);
        logVal = SIMD_MM(load_ps)(f);
    }

    inline void calcImpedance() override {}

    /** Accepts an incident wave into a WDF diode. */
    inline void incident(SIMD_M128 x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF diode. */
    inline SIMD_M128 reflected() noexcept override
    {
        // See eqn (10) from reference paper
        b = vAdd(logVal, vMul(vAdd(a, nextR_Is), oneOverVt));
        // array must be aligned to 16-byte boundary for SSE load
        float f alignas(16)[4];
        SIMD_MM(store_ps)(f, b);
        f[0] = chowdsp::Omega::omega2(f[0]);
        f[1] = chowdsp::Omega::omega2(f[1]);
        f[2] = chowdsp::Omega::omega2(f[2]);
        f[3] = chowdsp::Omega::omega2(f[3]);
        b = SIMD_MM(load_ps)(f);

        b = vAdd(a, vSub(vMul(vLoad1(2.0f), nextR_Is), vMul(vLoad1(2.0f), vMul(Vt, b))));
        return b;
    }

  private:
    const SIMD_M128 Is;        // reverse saturation current
    const SIMD_M128 Vt;        // thermal voltage
    const SIMD_M128 oneOverVt; // thermal voltage

    SIMD_M128 logVal;
    SIMD_M128 nextR_Is;
};

struct NonlinLUT
{
    NonlinLUT(float min, float max, int nPoints)
    {
        table.resize(nPoints, 0.0f);

        offset = min;
        scale = (float)nPoints / (max - min);

        for (int i = 0; i < nPoints; ++i)
        {
            auto x = (float)i / scale + offset;
            table[i] = 2.0e-9 * pwrs(std::abs(x), 0.33f);
        }
    }

    template <typename T> inline int sgn(T val) const noexcept
    {
        return (T(0) < val) - (val < T(0));
    }

    template <typename T> inline T pwrs(T x, T y) const noexcept
    {
        return std::pow(std::abs(x), y);
    }

    inline float operator()(float x) const noexcept
    {
        auto idx = size_t((x - offset) * scale);
        return sgn(x) * table[idx];
    }

  private:
    std::vector<float> table;
    float offset, scale;
};

static NonlinLUT bbdNonlinLUT{-5.0, 5.0, 1 << 16};

class BBDNonlin
{
  public:
    BBDNonlin() = default;

    void reset(float sampleRate)
    {
        using namespace chowdsp::WDF_SSE;
        constexpr float alpha = 0.4;

        S2.port1 = std::make_unique<Resistor>(2.7e3f); // Rgk

        auto &P3 = S2.port2;
        auto &I1 = P3->port1;
        I1->port1 = std::make_unique<ResistiveVoltageSource>(); // Vin
        Vin = I1->port1.get();

        auto &P2 = P3->port2;
        P2->port1 = std::make_unique<Capacitor>(1.6e-12f, sampleRate, alpha); // Cgk

        auto &S1 = P2->port2;
        S1->port1 = std::make_unique<Capacitor>(1.7e-12f, sampleRate, alpha); // Cgp

        auto &P1 = S1->port2;
        P1->port1 = std::make_unique<Capacitor>(0.33e-12f, sampleRate, alpha); // Cpk
        Cpk = P1->port1.get();

        P1->port2 = std::make_unique<ResistiveCurrentSource>(); // Is
        Is = P1->port2.get();

        P1->initialise();
        S1->initialise();
        P2->initialise();
        I1->initialise();
        P3->initialise();
        S2.initialise();

        D1.connectToNode(&S2);
        D1.precomputeValues();

        Vp = vZero;
        bbdNonlinLUT(0.0);
    }

    inline SIMD_M128 getCurrent(SIMD_M128 _Vg, SIMD_M128 _Vp) const noexcept
    {
        auto input = vSub(vMul(vLoad1(0.1f), _Vg), vMul(vLoad1(0.001f), _Vp));

        // array must be aligned to 16-byte boundary for SSE load
        float f alignas(16)[4];
        SIMD_MM(store_ps)(f, input);
        f[0] = bbdNonlinLUT(f[0]);
        f[1] = bbdNonlinLUT(f[1]);
        f[2] = bbdNonlinLUT(f[2]);
        f[3] = bbdNonlinLUT(f[3]);
        return SIMD_MM(load_ps)(f);
    }

    void setDrive(float newDrive) { drive = newDrive; }

    inline SIMD_M128 processSample(SIMD_M128 Vg) noexcept
    {
        Vin->setVoltage(Vg);
        Is->setCurrent(getCurrent(Vg, Vp));

        D1.incident(S2.reflected());
        S2.incident(D1.reflected());
        Vp = Cpk->voltage();

        auto vd = vLoad1(drive);
        auto vdm = vLoad1(1.0f - drive);
        auto a1 = vMul(Vp, vd);
        auto a2 = vMul(Vg, vdm);
        auto res = vAdd(a1, a2);
        return res;
    }

  private:
    FastDiode D1{1.0e-10, 0.02585};

    chowdsp::WDF_SSE::ResistiveVoltageSource *Vin;
    chowdsp::WDF_SSE::ResistiveCurrentSource *Is;
    chowdsp::WDF_SSE::Capacitor *Cpk;

    chowdsp::WDF_SSE::WDFSeriesT<
        chowdsp::WDF_SSE::Resistor,
        chowdsp::WDF_SSE::WDFParallelT<
            chowdsp::WDF_SSE::PolarityInverterT<chowdsp::WDF_SSE::ResistiveVoltageSource>,
            chowdsp::WDF_SSE::WDFParallelT<
                chowdsp::WDF_SSE::Capacitor,
                chowdsp::WDF_SSE::WDFSeriesT<
                    chowdsp::WDF_SSE::Capacitor,
                    chowdsp::WDF_SSE::WDFParallelT<chowdsp::WDF_SSE::Capacitor,
                                                   chowdsp::WDF_SSE::ResistiveCurrentSource>>>>>
        S2;

    SIMD_M128 Vp = vZero;
    float drive = 1.0f;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDNONLIN_H
