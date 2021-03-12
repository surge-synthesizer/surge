#pragma once

#include "../shared/wdf.h"
#include <memory>

class BBDNonlin
{
  public:
    BBDNonlin() = default;

    void reset(double sampleRate)
    {
        using namespace chowdsp::WDF;
        constexpr double alpha = 0.4;

        Cpk = std::make_unique<Capacitor>(0.33e-12, sampleRate, alpha);
        P1 = std::make_unique<WDFParallel>(Cpk.get(), &Is);

        Cgp = std::make_unique<Capacitor>(1.7e-12, sampleRate, alpha);
        S1 = std::make_unique<WDFSeries>(Cgp.get(), P1.get());

        Cgk = std::make_unique<Capacitor>(1.6e-12, sampleRate, alpha);
        P2 = std::make_unique<WDFParallel>(Cgk.get(), S1.get());

        I1 = std::make_unique<PolarityInverter>(&Vin);
        P3 = std::make_unique<WDFParallel>(I1.get(), P2.get());

        S2 = std::make_unique<WDFSeries>(&Rgk, P3.get());
        D1.connectToNode(S2.get());

        Vp = 0.0;
        lut(0.0);
    }

    inline double getCurrent(double _Vg, double _Vp) const noexcept
    {
        return lut(0.1 * _Vg - 0.01 * _Vp);
    }

    void setDrive(float newDrive) { drive = newDrive; }

    inline float processSample(float Vg)
    {
        Vin.setVoltage(Vg);
        Is.setCurrent(getCurrent(Vg, Vp));

        D1.incident(S2->reflected());
        S2->incident(D1.reflected());
        Vp = Cpk->voltage();

        return (float)Vp * drive + (1.0f - drive) * Vg;
    }

  private:
    struct NonlinLUT
    {
        NonlinLUT(double min, double max, int nPoints)
        {
            table.resize(nPoints, 0.0f);

            offset = min;
            scale = (double)nPoints / (max - min);

            for (int i = 0; i < nPoints; ++i)
            {
                auto x = (double)i / scale + offset;
                table[i] = 2.0e-9 * pwrs(std::abs(x), 0.33);
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

        inline double operator()(double x) const noexcept
        {
            auto idx = size_t((x - offset) * scale);
            return sgn(x) * table[idx];
        }

      private:
        std::vector<double> table;
        double offset, scale;
    };

    chowdsp::WDF::ResistiveCurrentSource Is;
    chowdsp::WDF::ResistiveVoltageSource Vin;
    chowdsp::WDF::Resistor Rgk{2.7e3};
    chowdsp::WDF::Diode D1{1.0e-10, 0.02585};

    std::unique_ptr<chowdsp::WDF::Capacitor> Cpk;
    std::unique_ptr<chowdsp::WDF::Capacitor> Cgp;
    std::unique_ptr<chowdsp::WDF::Capacitor> Cgk;

    std::unique_ptr<chowdsp::WDF::WDFParallel> P1;
    std::unique_ptr<chowdsp::WDF::WDFSeries> S1;
    std::unique_ptr<chowdsp::WDF::WDFParallel> P2;
    std::unique_ptr<chowdsp::WDF::WDFParallel> P3;
    std::unique_ptr<chowdsp::WDF::WDFSeries> S2;
    std::unique_ptr<chowdsp::WDF::PolarityInverter> I1;

    NonlinLUT lut{-5.0, 5.0, 8192}; // @TODO: can we make this static?

    double Vp = 0.0;
    float drive = 1.0f;
};
