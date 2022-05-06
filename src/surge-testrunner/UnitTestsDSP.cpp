#include <iostream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "FastMath.h"

#include "SSESincDelayLine.h"

#include "samplerate.h"

#include "SSEComplex.h"
#include <complex>

#include "LanczosResampler.h"
#include "sst/plugininfra/cpufeatures.h"

using namespace Surge::Test;

TEST_CASE("Simple Single Oscillator is Constant", "[dsp]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge);
    // surge->storage.getPatch().scene[0].osc[0].type.val.i = ot_sine;

    int len = 4410 * 5;
    // int len = BLOCK_SIZE * 20;
    Surge::Headless::playerEvents_t heldC = Surge::Headless::makeHoldMiddleC(len);
    REQUIRE(heldC.size() == 2);

    float *data = nullptr;
    int nSamples, nChannels;

    Surge::Headless::playAsConfigured(surge, heldC, &data, &nSamples, &nChannels);
    REQUIRE(data);
    REQUIRE(std::abs(nSamples - len) <= BLOCK_SIZE);
    REQUIRE(nChannels == 2);

    float rms = 0;
    for (int i = 0; i < nSamples * nChannels; ++i)
    {
        rms += data[i] * data[i];
    }
    rms /= (float)(nSamples * nChannels);
    rms = sqrt(rms);
    REQUIRE(rms > 0.1);
    REQUIRE(rms < 0.101);

    int zeroCrossings = 0;
    for (int i = 0; i < nSamples * nChannels - 2; i += 2)
    {
        if (data[i] > 0 && data[i + 2] < 0)
            zeroCrossings++;
    }
    // Somewhere in here
    REQUIRE(zeroCrossings > 130);
    REQUIRE(zeroCrossings < 160);

    delete[] data;
}
TEST_CASE("Unison Absolute and Relative", "[osc]")
{
    auto surge = Surge::Headless::createSurge(44100, true);
    REQUIRE(surge);

    auto assertRelative = [surge](const char *pn) {
        REQUIRE(surge->loadPatchByPath(pn, -1, "Test"));
        auto f60_0 = frequencyForNote(surge, 60, 5, 0);
        auto f60_1 = frequencyForNote(surge, 60, 5, 1);

        auto f60_avg = 0.5 * (f60_0 + f60_1);

        auto f72_0 = frequencyForNote(surge, 72, 5, 0);
        auto f72_1 = frequencyForNote(surge, 72, 5, 1);
        auto f72_avg = 0.5 * (f72_0 + f72_1);

        // In relative mode, the average frequencies should double, as should the individual
        // outliers
        REQUIRE(f72_avg / f60_avg == Approx(2).margin(0.01));
        REQUIRE(f72_0 / f60_0 == Approx(2).margin(0.01));
        REQUIRE(f72_1 / f60_1 == Approx(2).margin(0.01));

        // test extended mode
        surge->storage.getPatch().scene[0].osc[0].p[5].set_extend_range(true);
        auto f60_0e = frequencyForNote(surge, 60, 5, 0);
        auto f60_1e = frequencyForNote(surge, 60, 5, 1);
        REQUIRE(f60_0e / f60_avg == Approx(pow(f60_0 / f60_avg, 12.f)).margin(0.05));
        REQUIRE(f60_1e / f60_avg == Approx(pow(f60_1 / f60_avg, 12.f)).margin(0.05));
    };

    auto assertAbsolute = [surge](const char *pn, bool print = false) {
        REQUIRE(surge->loadPatchByPath(pn, -1, "Test"));
        auto f60_0 = frequencyForNote(surge, 60, 5, 0);
        auto f60_1 = frequencyForNote(surge, 60, 5, 1);

        auto f60_avg = 0.5 * (f60_0 + f60_1);

        auto f72_0 = frequencyForNote(surge, 72, 5, 0);
        auto f72_1 = frequencyForNote(surge, 72, 5, 1);
        auto f72_avg = 0.5 * (f72_0 + f72_1);

        // In absolute mode, the average frequencies should double, but the channels should have
        // constant difference
        REQUIRE(f72_avg / f60_avg == Approx(2).margin(0.01));
        REQUIRE((f72_0 - f72_1) / (f60_0 - f60_1) == Approx(1).margin(0.01));
        if (print)
        {
            std::cout << "F60 " << f60_avg << " " << f60_0 << " " << f60_1 << " " << f60_0 - f60_1
                      << std::endl;
            std::cout << "F72 " << f72_avg << " " << f72_0 << " " << f72_1 << " " << f60_0 - f60_1
                      << std::endl;
        }
    };

    SECTION("Wavetable Oscillator")
    {
        assertRelative("resources/test-data/patches/Wavetable-Sin-Uni2-Relative.fxp");
        assertAbsolute("resources/test-data/patches/Wavetable-Sin-Uni2-Absolute.fxp");
    }

    SECTION("Window Oscillator")
    {
        assertRelative("resources/test-data/patches/Window-Sin-Uni2-Relative.fxp");
        assertAbsolute("resources/test-data/patches/Window-Sin-Uni2-Absolute.fxp");
    }

    SECTION("Classic Oscillator")
    {
        assertRelative("resources/test-data/patches/Classic-Uni2-Relative.fxp");
        assertAbsolute("resources/test-data/patches/Classic-Uni2-Absolute.fxp");
    }

    SECTION("SH Oscillator")
    {
        assertRelative("resources/test-data/patches/SH-Uni2-Relative.fxp");
        assertAbsolute("resources/test-data/patches/SH-Uni2-Absolute.fxp");
    }
}

TEST_CASE("Unison at Sample Rates", "[osc]")
{
    auto assertRelative = [](const std::shared_ptr<SurgeSynthesizer> &surge, const char *pn) {
        REQUIRE(surge->loadPatchByPath(pn, -1, "Test"));
        auto f60_0 = frequencyForNote(surge, 60, 5, 0);
        auto f60_1 = frequencyForNote(surge, 60, 5, 1);
        auto f60_avg = 0.5 * (f60_0 + f60_1);
        REQUIRE(f60_avg == Approx(261.6).margin(1));

        auto f72_0 = frequencyForNote(surge, 72, 5, 0);
        auto f72_1 = frequencyForNote(surge, 72, 5, 1);
        auto f72_avg = 0.5 * (f72_0 + f72_1);
        REQUIRE(f72_avg == Approx(2 * 261.6).margin(1));

        // In relative mode, the average frequencies should double, as should the individual
        // outliers
        REQUIRE(f72_avg / f60_avg == Approx(2).margin(0.01));
        REQUIRE(f72_0 / f60_0 == Approx(2).margin(0.01));
        REQUIRE(f72_1 / f60_1 == Approx(2).margin(0.01));
    };

    auto assertAbsolute = [](const std::shared_ptr<SurgeSynthesizer> &surge, const char *pn,
                             bool print = false) {
        REQUIRE(surge->loadPatchByPath(pn, -1, "Test"));
        auto f60_0 = frequencyForNote(surge, 60, 5, 0);
        auto f60_1 = frequencyForNote(surge, 60, 5, 1);

        auto f60_avg = 0.5 * (f60_0 + f60_1);
        REQUIRE(f60_avg == Approx(261.6).margin(2));

        auto f72_0 = frequencyForNote(surge, 72, 5, 0);
        auto f72_1 = frequencyForNote(surge, 72, 5, 1);
        auto f72_avg = 0.5 * (f72_0 + f72_1);
        REQUIRE(f72_avg == Approx(2 * 261.6).margin(2));

        // In absolute mode, the average frequencies should double, but the channels should have
        // constant difference
        REQUIRE(f72_avg / f60_avg == Approx(2).margin(0.01));
        REQUIRE((f72_0 - f72_1) / (f60_0 - f60_1) == Approx(1).margin(0.01));

        // While this test is correct, the differences depend on samplerate in 1.6.5. That should
        // not be the case here.
        auto ap = &(surge->storage.getPatch().scene[0].osc[0].p[n_osc_params - 2]);

        char txt[256];
        ap->get_display(txt);
        float spreadWhichMatchesDisplay = ap->val.f * 16.f;
        INFO("Comparing absolute with " << txt << " " << spreadWhichMatchesDisplay);
        REQUIRE(spreadWhichMatchesDisplay == Approx(f60_0 - f60_1).margin(0.05));

        if (print)
        {
            std::cout << "F60 " << f60_avg << " " << f60_0 << " " << f60_1 << " " << f60_0 - f60_1
                      << std::endl;
            std::cout << "F72 " << f72_avg << " " << f72_0 << " " << f72_1 << " " << f60_0 - f60_1
                      << std::endl;
        }
    };

    auto randomAbsolute = [](const std::shared_ptr<SurgeSynthesizer> &surge, const char *pn,
                             bool print = false) {
        for (int i = 0; i < 10; ++i)
        {
            REQUIRE(surge->loadPatchByPath(pn, -1, "Test"));
            int note = rand() % 70 + 22;
            float abss = rand() * 1.f / (float)RAND_MAX * 0.8 + 0.15;
            auto ap = &(surge->storage.getPatch().scene[0].osc[0].p[n_osc_params - 2]);
            ap->set_value_f01(abss);
            char txt[256];
            ap->get_display(txt);

            INFO("Test[" << i << "] note=" << note << " at absolute spread " << abss << " = "
                         << txt);
            for (int j = 0; j < 200; ++j)
                surge->process();

            auto fn_0 = frequencyForNote(surge, note, 5, 0);
            auto fn_1 = frequencyForNote(surge, note, 5, 1);
            REQUIRE(16.f * abss == Approx(fn_0 - fn_1).margin(0.3));
        }
    };

    std::vector<int> srs = {{44100, 48000}};

    SECTION("Wavetable Oscillator")
    {
        for (auto sr : srs)
        {
            INFO("Wavetable test at " << sr);
            auto surge = Surge::Headless::createSurge(sr, true);

            assertRelative(surge, "resources/test-data/patches/Wavetable-Sin-Uni2-Relative.fxp");
            assertAbsolute(surge, "resources/test-data/patches/Wavetable-Sin-Uni2-Absolute.fxp");
            // HF noise in the wavetable makes my detector unreliable at zero crossings in this case
            // It passes 99% of the time but leave this test out for now.
            // randomAbsolute(surge, "resources/test-data/patches/Wavetable-Sin-Uni2-Absolute.fxp");
        }
    }

    SECTION("Test Each Oscillator")
    {
        for (auto sr : srs)
        {
            INFO("Window Oscillator test at " << sr);
            auto surge = Surge::Headless::createSurge(sr, true);

            assertRelative(surge, "resources/test-data/patches/Window-Sin-Uni2-Relative.fxp");
            assertAbsolute(surge, "resources/test-data/patches/Window-Sin-Uni2-Absolute.fxp");
            randomAbsolute(surge, "resources/test-data/patches/Window-Sin-Uni2-Absolute.fxp");
        }

        for (auto sr : srs)
        {
            INFO("Classic Oscillator test at " << sr);
            auto surge = Surge::Headless::createSurge(sr, true);

            assertRelative(surge, "resources/test-data/patches/Classic-Uni2-Relative.fxp");
            assertAbsolute(surge, "resources/test-data/patches/Classic-Uni2-Absolute.fxp");
            randomAbsolute(surge, "resources/test-data/patches/Classic-Uni2-Absolute.fxp");
        }

        for (auto sr : srs)
        {
            INFO("SH Oscillator test at " << sr);
            auto surge = Surge::Headless::createSurge(sr, true);

            assertRelative(surge, "resources/test-data/patches/SH-Uni2-Relative.fxp");
            assertAbsolute(surge, "resources/test-data/patches/SH-Uni2-Absolute.fxp");
            // randomAbsolute(surge, "resources/test-data/patches/SH-Uni2-Absolute.fxp");
        }
    }
}

TEST_CASE("All Patches have Bounded Output", "[dsp]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    Surge::Headless::playerEvents_t scale =
        Surge::Headless::make120BPMCMajorQuarterNoteScale(0, 44100);

    auto callBack = [](const Patch &p, const PatchCategory &pc, const float *data, int nSamples,
                       int nChannels) -> void {
        bool writeWav = false; // toggle this to true to write each sample to a wav file
        REQUIRE(nSamples * nChannels > 0);

        if (nSamples * nChannels > 0)
        {
            const auto minmaxres = std::minmax_element(data, data + nSamples * nChannels);
            auto mind = minmaxres.first;
            auto maxd = minmaxres.second;

            float rms = 0, L1 = 0;
            for (int i = 0; i < nSamples * nChannels; ++i)
            {
                rms += data[i] * data[i];
                L1 += fabs(data[i]);
            }
            L1 = L1 / (nChannels * nSamples);
            rms = sqrt(rms / nChannels / nSamples);

            REQUIRE(L1 < 1);
            REQUIRE(rms < 1);
            REQUIRE(*maxd < 6);
            REQUIRE(*maxd >= 0);
            REQUIRE(*mind > -6);
            REQUIRE(*mind <= 0);

            /*
            std::cout << "cat/patch = " <<  pc.name << " / " << std::left << std::setw(30) <<
            p.name; std::cout << "  range = [" << std::setw(10)
                      << std::fixed << *mind << ", " << std::setw(10) << std::fixed << *maxd << "]"
                      << " L1=" << L1
                      << " rms=" << rms
                      << " samp=" << nSamples << " chan=" << nChannels << std::endl;
            */
        }
    };

    // Surge::Headless::playOnNRandomPatches(surge, scale, 100, callBack);
}

TEST_CASE("lipol_ps class", "[dsp]")
{
    lipol_ps mypol;
    float prevtarget = -1.0;
    mypol.set_target(prevtarget);
    mypol.instantize();

    constexpr size_t nfloat = 64;
    constexpr size_t nfloat_quad = 16;
    float storeTarget alignas(16)[nfloat];
    mypol.store_block(storeTarget, nfloat_quad);

    for (auto i = 0; i < nfloat; ++i)
        REQUIRE(storeTarget[i] == prevtarget); // should be constant in the first instance

    for (int i = 0; i < 10; ++i)
    {
        float target = (i) * (i) / 100.0;
        mypol.set_target(target);

        mypol.store_block(storeTarget, nfloat_quad);

        REQUIRE(storeTarget[nfloat - 1] == Approx(target));

        float dy = storeTarget[1] - storeTarget[0];
        for (auto j = 1; j < nfloat; ++j)
        {
            REQUIRE(storeTarget[j] - storeTarget[j - 1] == Approx(dy).epsilon(1e-3));
        }

        REQUIRE(prevtarget + dy == Approx(storeTarget[0]));

        prevtarget = target;
    }
}

TEST_CASE("Check FastMath Functions", "[dsp]")
{
    SECTION("Clamp to -PI,PI")
    {
        for (float f = -2132.7; f < 37424.3; f += 0.741)
        {
            float q = Surge::DSP::clampToPiRange(f);
            INFO("Clamping " << f << " to " << q);
            REQUIRE(q > -M_PI);
            REQUIRE(q < M_PI);
        }
    }

    SECTION("fastSin and fastCos in range -PI, PI")
    {
        float sds = 0, md = 0;
        int nsamp = 100000;
        for (int i = 0; i < nsamp; ++i)
        {
            float p = 2.f * M_PI * rand() / RAND_MAX - M_PI;
            float cp = std::cos(p);
            float sp = std::sin(p);
            float fcp = Surge::DSP::fastcos(p);
            float fsp = Surge::DSP::fastsin(p);

            float cd = fabs(cp - fcp);
            float sd = fabs(sp - fsp);
            if (cd > md)
                md = cd;
            if (sd > md)
                md = sd;
            sds += cd * cd + sd * sd;
        }
        sds = sqrt(sds) / nsamp;
        REQUIRE(md < 1e-4);
        REQUIRE(sds < 1e-6);
    }

    SECTION("fastSin and fastCos in range -10*PI, 10*PI with clampRange")
    {
        float sds = 0, md = 0;
        int nsamp = 100000;
        for (int i = 0; i < nsamp; ++i)
        {
            float p = 2.f * M_PI * rand() / RAND_MAX - M_PI;
            p *= 10;
            p = Surge::DSP::clampToPiRange(p);
            float cp = std::cos(p);
            float sp = std::sin(p);
            float fcp = Surge::DSP::fastcos(p);
            float fsp = Surge::DSP::fastsin(p);

            float cd = fabs(cp - fcp);
            float sd = fabs(sp - fsp);
            if (cd > md)
                md = cd;
            if (sd > md)
                md = sd;
            sds += cd * cd + sd * sd;
        }
        sds = sqrt(sds) / nsamp;
        REQUIRE(md < 1e-4);
        REQUIRE(sds < 1e-6);
    }

    SECTION("fastSin and fastCos in range -100*PI, 100*PI with clampRange")
    {
        float sds = 0, md = 0;
        int nsamp = 100000;
        for (int i = 0; i < nsamp; ++i)
        {
            float p = 2.f * M_PI * rand() / RAND_MAX - M_PI;
            p *= 100;
            p = Surge::DSP::clampToPiRange(p);
            float cp = std::cos(p);
            float sp = std::sin(p);
            float fcp = Surge::DSP::fastcos(p);
            float fsp = Surge::DSP::fastsin(p);

            float cd = fabs(cp - fcp);
            float sd = fabs(sp - fsp);
            if (cd > md)
                md = cd;
            if (sd > md)
                md = sd;
            sds += cd * cd + sd * sd;
        }
        sds = sqrt(sds) / nsamp;
        REQUIRE(md < 1e-4);
        REQUIRE(sds < 1e-6);
    }

    SECTION("fastTanh and fastTanhSSE")
    {
        for (float x = -4.9; x < 4.9; x += 0.02)
        {
            INFO("Testing unclamped at " << x);
            auto q = _mm_set_ps1(x);
            auto r = Surge::DSP::fasttanhSSE(q);
            auto rn = tanh(x);
            auto rd = Surge::DSP::fasttanh(x);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).epsilon(1e-4));
            REQUIRE(rd == Approx(rn).epsilon(1e-4));
        }

        for (float x = -10; x < 10; x += 0.02)
        {
            INFO("Testing clamped at " << x);
            auto q = _mm_set_ps1(x);
            auto r = Surge::DSP::fasttanhSSEclamped(q);
            auto cn = tanh(x);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(cn).epsilon(5e-4));
        }
    }

    SECTION("fastTan")
    {
        // need to bump start point slightly, fasttan is only valid just after -PI/2
        for (float x = -M_PI / 2.0 + 0.001; x < M_PI / 2.0; x += 0.02)
        {
            INFO("Testing fasttan at " << x);
            auto rn = tanf(x);
            auto rd = Surge::DSP::fasttan(x);
            REQUIRE(rd == Approx(rn).epsilon(1e-4));
        }
    }

    SECTION("fastexp and fastexpSSE")
    {
        for (float x = -3.9; x < 2.9; x += 0.02)
        {
            INFO("Testing fastexp at " << x);
            auto q = _mm_set_ps1(x);
            auto r = Surge::DSP::fastexpSSE(q);
            auto rn = exp(x);
            auto rd = Surge::DSP::fastexp(x);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = r;

            if (x < 0)
            {
                REQUIRE(U.a[0] == Approx(rn).margin(1e-3));
                REQUIRE(rd == Approx(rn).margin(1e-3));
            }
            else
            {
                REQUIRE(U.a[0] == Approx(rn).epsilon(1e-3));
                REQUIRE(rd == Approx(rn).epsilon(1e-3));
            }
        }
    }

    SECTION("fastSine and fastSinSSE")
    {
        for (float x = -3.14; x < 3.14; x += 0.02)
        {
            INFO("Testing unclamped at " << x);
            auto q = _mm_set_ps1(x);
            auto r = Surge::DSP::fastsinSSE(q);
            auto rn = sin(x);
            auto rd = Surge::DSP::fastsin(x);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).margin(1e-4));
            REQUIRE(rd == Approx(rn).margin(1e-4));
            REQUIRE(U.a[0] == Approx(rd).margin(1e-6));
        }
    }

    SECTION("fastCos and fastCosSSE")
    {
        for (float x = -3.14; x < 3.14; x += 0.02)
        {
            INFO("Testing unclamped at " << x);
            auto q = _mm_set_ps1(x);
            auto r = Surge::DSP::fastcosSSE(q);
            auto rn = cos(x);
            auto rd = Surge::DSP::fastcos(x);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).margin(1e-4));
            REQUIRE(rd == Approx(rn).margin(1e-4));
            REQUIRE(U.a[0] == Approx(rd).margin(1e-6));
        }
    }

    SECTION("Clamp to -PI,PI SSE")
    {
        for (float f = -800.7; f < 816.4; f += 0.245)
        {
            auto fs = _mm_set_ps1(f);

            auto q = Surge::DSP::clampToPiRangeSSE(fs);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = q;
            for (int s = 0; s < 4; ++s)
            {
                REQUIRE(U.a[s] == Approx(Surge::DSP::clampToPiRange(f)).margin(1e-4));
            }
        }
    }
}

TEST_CASE("Sinc Delay Line", "[dsp]")
{
    // This requires SurgeStorate to initialize its tables. Easiest way
    // to do that is to just make a surge
    auto surge = Surge::Headless::createSurge(44100);
    SECTION("Test Constants")
    {
        float val = 1.324;
        SSESincDelayLine<4096> dl4096(surge->storage.sinctable);

        for (int i = 0; i < 10000; ++i)
        {
            dl4096.write(val);
        }
        for (int i = 0; i < 20000; ++i)
        {
            INFO("Iteration " << i);
            float a = dl4096.read(174.3);
            float b = dl4096.read(1732.4);
            float c = dl4096.read(3987.2);
            float d = dl4096.read(256.0);

            REQUIRE(a == Approx(val).margin(1e-3));
            REQUIRE(b == Approx(val).margin(1e-3));
            REQUIRE(c == Approx(val).margin(1e-3));
            REQUIRE(d == Approx(val).margin(1e-3));

            dl4096.write(val);
        }
    }

    SECTION("Test Ramp")
    {
        float val = 0;
        float dRamp = 0.01;
        SSESincDelayLine<4096> dl4096(surge->storage.sinctable);

        for (int i = 0; i < 10000; ++i)
        {
            dl4096.write(val);
            val += dRamp;
        }
        for (int i = 0; i < 20000; ++i)
        {
            INFO("Iteration " << i);
            float a = dl4096.read(174.3);
            float b = dl4096.read(1732.4);
            float c = dl4096.read(3987.2);
            float d = dl4096.read(256.0);

            auto cval = val - dRamp;

            REQUIRE(a == Approx(cval - 174.3 * dRamp).epsilon(1e-3));
            REQUIRE(b == Approx(cval - 1732.4 * dRamp).epsilon(1e-3));
            REQUIRE(c == Approx(cval - 3987.2 * dRamp).epsilon(1e-3));
            REQUIRE(d == Approx(cval - 256.0 * dRamp).epsilon(1e-3));

            float aL = dl4096.readLinear(174.3);
            float bL = dl4096.readLinear(1732.4);
            float cL = dl4096.readLinear(3987.2);
            float dL = dl4096.readLinear(256.0);

            REQUIRE(aL == Approx(cval - 174.3 * dRamp).epsilon(1e-3));
            REQUIRE(bL == Approx(cval - 1732.4 * dRamp).epsilon(1e-3));
            REQUIRE(cL == Approx(cval - 3987.2 * dRamp).epsilon(1e-3));
            REQUIRE(dL == Approx(cval - 256.0 * dRamp).epsilon(1e-3));

            dl4096.write(val);
            val += dRamp;
        }
    }

#if 0
// This prints output I used for debugging
    SECTION( "Generate Output" )
    {
        float dRamp = 0.01;
        SSESincDelayLine<4096> dl4096;

        float v = 0;
        int nsamp = 500;
        for( int i=0; i<nsamp; ++i )
        {
            dl4096.write(v);
            v += dRamp;
        }

        for( int i=100; i<300; ++i )
        {
            auto bi = dl4096.buffer[i];
            auto off = nsamp - i;
            auto bsv = dl4096.read(off - 1);
            auto bsl = dl4096.readLinear(off);

            auto bsvh = dl4096.read(off - 1 - 0.5);
            auto bslh = dl4096.readLinear(off - 0.0 );
            std::cout << off << ", " << bi << ", " << bsv
                      << ", " << bsl << ", " << bi -bsv << ", " << bi-bsl
                      << ", " << bslh << ", " << bi - bslh << std::endl;

            for( int q=0; q<111; ++q )
            {
                std::cout << " " << q << " "
                          << ( dl4096.read(off - q * 0.1) - bi ) / dRamp
                          << " " << ( dl4096.readLinear(off - q * 0.1) - bi ) / dRamp
                          << std::endl;
            }
        }
    }
#endif
}

TEST_CASE("libsamplerate basics", "[dsp]")
{
    for (auto tsr : {44100, 48000}) // { 44100, 48000, 88200, 96000, 192000 })
    {
        for (auto ssr : {44100, 48000, 88200})
        {
            DYNAMIC_SECTION("libsamplerate from " << ssr << " to " << tsr)
            {
                int error;
                auto state = src_new(SRC_SINC_FASTEST, 1, &error);
                REQUIRE(state);
                REQUIRE(error == 0);

                static constexpr int buffer_size = 1024 * 100;
                static constexpr int output_block = 64;

                float input_data[buffer_size];
                float copied_output[buffer_size];

                int cwp = 0, irp = 0;
                float output_data[output_block];

                float dPhase = 440.0 / ssr * 2.0 * M_PI;
                float phase = 0;
                for (int i = 0; i < buffer_size; ++i)
                {
                    input_data[i] = std::sin(phase);
                    phase += dPhase;
                    if (phase >= 2.0 * M_PI)
                        phase -= 2.0 * M_PI;
                }

                SRC_DATA sdata;
                sdata.end_of_input = 0;
                while (irp + output_block < buffer_size && cwp + output_block < buffer_size)
                {
                    sdata.data_in = &(input_data[irp]);
                    sdata.data_out = output_data;
                    sdata.input_frames = 64;
                    sdata.output_frames = 64;
                    sdata.src_ratio = 1.0 * tsr / ssr;

                    auto res = src_process(state, &sdata);
                    memcpy((void *)(copied_output + cwp), (void *)output_data,
                           sdata.output_frames_gen * sizeof(float));
                    irp += sdata.input_frames_used;
                    cwp += sdata.output_frames_gen;
                    REQUIRE(res == 0);
                    REQUIRE(sdata.input_frames_used + sdata.output_frames_gen > 0);
                }

                state = src_delete(state);
                REQUIRE(!state);

                // At this point the output block should be a 440hz sine wave at the target rate
                dPhase = 440.0 / tsr * 2.0 * M_PI;
                phase = 0;
                for (int i = 0; i < cwp; ++i)
                {
                    auto cw = std::sin(phase);
                    REQUIRE(copied_output[i] == Approx(cw).margin(1e-2));
                    phase += dPhase;
                    if (phase >= 2.0 * M_PI)
                        phase -= 2.0 * M_PI;
                }
            }
        }
    }
}

TEST_CASE("Every Oscillator Plays", "[dsp]")
{
    for (int i = 0; i < n_osc_types; ++i)
    {
        DYNAMIC_SECTION("Oscillator type " << osc_type_names[i])
        {
            auto surge = Surge::Headless::createSurge(44100, true);

            for (int q = 0; q < BLOCK_SIZE; q++)
            {
                surge->input[0][q] = 0.f;
                surge->input[1][q] = 0.f;
            }

            surge->storage.getPatch().scene[0].osc[0].queue_type = i;

            for (int q = 0; q < 10; ++q)
                surge->process();

            int idx = 0;
            bool got = false;
            for (auto q : surge->storage.wt_list)
            {
                if (q.name == "Sine Power HQ")
                {
                    got = true;
                    surge->storage.getPatch().scene[0].osc[0].wt.queue_id = idx;
                }
                idx++;
            }
            REQUIRE(got);
            for (int q = 0; q < 10; ++q)
                surge->process();

            REQUIRE(std::string(surge->storage.getPatch().scene[0].osc[0].wavetable_display_name) ==
                    "Sine Power HQ");

            float sumAbsOut = 0;
            surge->playNote(0, 60, 127, 0);
            for (int q = 0; q < 100; ++q)
            {
                surge->process();
                for (int s = 0; s < BLOCK_SIZE; ++s)
                    sumAbsOut += fabs(surge->output[0][s]);
            }
            if (i == ot_audioinput)
                REQUIRE(sumAbsOut < 1e-4);
            else
                REQUIRE(sumAbsOut > 1);
        }
    }
}

TEST_CASE("Untuned is 2^x", "[dsp]")
{
    auto surge = Surge::Headless::createSurge(44100);
    for (int i = 0; i < 6000; ++i)
    {
        float n = i * 1.0 / 6000.0 * 200;
        INFO("Note is " << n);
        float p = surge->storage.note_to_pitch(n);
        float pinv = surge->storage.note_to_pitch_inv(n);
        REQUIRE(fabs(p - (float)pow(2.0, n / 12.0)) < p * 1e-5);
        REQUIRE(fabs(pinv - (float)pow(2.0, -n / 12.0)) < p * 1e-5);
    }
}

TEST_CASE("SSE std::complex", "[dsp]")
{
    SECTION("Can make a complex on m128")
    {
        auto a = SSEComplex();
        // The atIndex operation is expensive. Stay vectorized as long as possible.
        REQUIRE(a.atIndex(0) == std::complex<float>{0, 0});
        auto b = a + a;

        for (int i = 0; i < 4; ++i)
            REQUIRE(a.atIndex(i) == b.atIndex(i));

        auto q = SSEComplex({0.f, 1.f, 2.f, 3.f}, {1.f, 0.f, -1.f, -2.f});
        auto r = SSEComplex({12.f, 1.2f, 2.4f, 3.7f}, {1.2f, 0.4f, -1.2f, -2.7f});
        REQUIRE(q.atIndex(0) == std::complex<float>(0.f, 1.f));
        REQUIRE(q.atIndex(1) == std::complex<float>(1.f, 0.f));
        REQUIRE(q.atIndex(2) == std::complex<float>(2.f, -1.f));
        REQUIRE(q.atIndex(3) == std::complex<float>(3.f, -2.f));

        auto qpr = q + r;
        for (int i = 0; i < 4; ++i)
            REQUIRE(qpr.atIndex(i) == q.atIndex(i) + r.atIndex(i));

        auto qtr = q * r;
        for (int i = 0; i < 4; ++i)
            REQUIRE(qtr.atIndex(i) == q.atIndex(i) * r.atIndex(i));

        float sum = 0.f;
        for (int i = 0; i < 4; ++i)
            sum += qtr.atIndex(i).real();

        float sumSSE alignas(16)[4];
        _mm_store1_ps(sumSSE, sum_ps_to_ss(qtr.real()));
        REQUIRE(sum == Approx(sumSSE[0]).margin(1e-5));

        float angles alignas(16)[4];
        angles[0] = 0;
        angles[1] = M_PI / 2;
        angles[2] = 3 * M_PI / 4;
        angles[3] = M_PI;
        auto asse = _mm_load_ps(angles);
        auto c = SSEComplex::fastExp(asse);

        auto c0 = c.atIndex(0);
        REQUIRE(c0.real() == Approx(1).margin(1e-5));
        REQUIRE(c0.imag() == Approx(0).margin(1e-5));

        auto c1 = c.atIndex(1);
        REQUIRE(c1.real() == Approx(0).margin(1e-5));
        REQUIRE(c1.imag() == Approx(1).margin(1e-5));

        auto c2 = c.atIndex(2);
        REQUIRE(c2.real() == Approx(-sqrt(2.0) / 2).margin(1e-5));
        REQUIRE(c2.imag() == Approx(sqrt(2.0) / 2).margin(1e-5));

        // At this extrema we are a touch less acrrucate
        auto c3 = c.atIndex(3);
        REQUIRE(c3.real() == Approx(-1).margin(1e-4));
        REQUIRE(c3.imag() == Approx(0).margin(1e-4));

        auto powV = q.map([](const std::complex<float> &f) { return std::pow(f, 2.1f); });
        for (int i = 0; i < 4; ++i)
            REQUIRE(powV.atIndex(i) == pow(q.atIndex(i), 2.1f));
    }
}

#if 0
TEST_CASE("LanczosResampler", "[dsp]")
{
    SECTION("Can Interpolate Sine")
    {
        LanczosResampler lr(48000, 88100);

        std::cout << lr.inputsRequiredToGenerateOutputs(64) << std::endl;
        // plot 'lancos_raw.csv' using 1:2 with lines, 'lancos_samp.csv' using 1:2 with lines
        std::ofstream raw("lancos_raw.csv"), samp("lancos_samp.csv");
        int points = 2000;
        double dp = 1.0 / 370;
        float phase = 0;
        for (auto i = 0; i < points; ++i)
        {
            auto obsS = std::sin(i * dp * 2.0 * M_PI);
            auto obsR = phase * 2 - 1;
            phase += dp;
            if (phase > 1)
                phase -= 1;
            auto obs = i > 800 ? obsS : obsR;
            lr.push(obs);

            raw << i << ", " << obs << "\n";
        }
        std::cout << lr.inputsRequiredToGenerateOutputs(64) << std::endl;

        float outBlock[64];
        int q, gen;
        while ((gen = lr.populateNext(outBlock, 64)) > 0)
        {
            for (int i = 0; i < gen; ++i)
            {
                samp << q * 48000.0 / 88100.0 << ", " << outBlock[i] << std::endl;
                ++q;
            }
        }
    }
}
#endif

// When we return to #1514 this is a good starting point
#if 0
TEST_CASE( "NaN Patch from Issue 1514", "[dsp]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   REQUIRE( surge->loadPatchByPath( "resources/test-data/patches/VinceyCrash1514.fxp", -1, "Test" ) );

   for( int d=0; d<10; d++ )
   {
      auto events = Surge::Headless::makeHoldNoteFor( 60 + 24, 4410, 100, 0 );
      for( auto &e : events )
         e.atSample += 1000;

      surge->allNotesOff();
      for( int i=0; i<100; ++i )
         surge->process();
      
      float *res;
      int nS, nC;
      playAsConfigured(surge, events, &res, &nS, &nC );
      surge->storage.getPatch().scene[0].lfo[0].decay.set_value_f01( .325 + d/1000.f );
      char txt[512];
      surge->storage.getPatch().scene[0].lfo[0].decay.get_display( txt, false, 0.f );
      const auto minmaxres = std::minmax_element(res, res + nS * nC);
      auto mind = minmaxres.first;
      auto maxd = minmaxres.second;

      // std::cout << "minMax at " << d << " / " << txt << " is " << *mind << " / " << *maxd << std::endl;
      std::string title = "delay = " + std::string( txt );
      std::string fname = "/tmp/nanPatch_" + std::to_string( d ) + ".png";
      makePlotPNGFromData( fname, title, res, nS, nC );

      delete[] res;
   }
   
}
#endif

TEST_CASE("BasicDSP", "[dsp]")
{
    SECTION("limit range ")
    {
        REQUIRE(limit_range(0.1, 0.2, 0.5) == 0.2);
        REQUIRE(limit_range(0.2, 0.2, 0.5) == 0.2);
        REQUIRE(limit_range(0.3, 0.2, 0.5) == 0.3);
        REQUIRE(limit_range(0.4, 0.2, 0.5) == 0.4);
        REQUIRE(limit_range(0.5, 0.2, 0.5) == 0.5);
        REQUIRE(limit_range(0.6, 0.2, 0.5) == 0.5);

        REQUIRE(limit_range(0.1f, 0.2f, 0.5f) == 0.2f);
        REQUIRE(limit_range(0.2f, 0.2f, 0.5f) == 0.2f);
        REQUIRE(limit_range(0.3f, 0.2f, 0.5f) == 0.3f);
        REQUIRE(limit_range(0.4f, 0.2f, 0.5f) == 0.4f);
        REQUIRE(limit_range(0.5f, 0.2f, 0.5f) == 0.5f);
        REQUIRE(limit_range(0.6f, 0.2f, 0.5f) == 0.5f);

        REQUIRE(limit_range(1, 2, 5) == 2);
        REQUIRE(limit_range(2, 2, 5) == 2);
        REQUIRE(limit_range(3, 2, 5) == 3);
        REQUIRE(limit_range(4, 2, 5) == 4);
        REQUIRE(limit_range(5, 2, 5) == 5);
        REQUIRE(limit_range(6, 2, 5) == 5);
    }
}

TEST_CASE("Wavehaper LUT", "[dsp]")
{
    SECTION("ASYM_SSE2")
    {
        // need to do this to init tables only
        auto surge = Surge::Headless::createSurge(44100);

        auto wst = sst::waveshapers::GetQuadWaveshaper(sst::waveshapers::WaveshaperType::wst_asym);
        auto shafted_tanh = [](double x) { return (exp(x) - exp(-x * 1.2)) / (exp(x) + exp(-x)); };
        sst::waveshapers::QuadWaveshaperState qss{};
        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
            qss.R[i] = _mm_setzero_ps();

        /*
         * asym:
         * for (int i = 0; i < 1024; i++)
         *  {
         *    double x = ((double)i - 512.0) * mult; // mult = 1/32
         *    waveshapers[wst_asym][i] = (float)shafted_tanh(x + 0.5) - shafted_tanh(0.5);
         *
         * output is i = x * 32 + 512 then interp
         */
        for (float x = -8; x < 8; x += 0.07)
        {
            auto d = _mm_set1_ps(1.0);
            float out alignas(16)[4], inv alignas(16)[4];
            auto in = _mm_set_ps(x, x + 0.01, x + 0.03, x + 0.05);
            _mm_store_ps(inv, in);

            auto r = wst(&qss, in, d);
            _mm_store_ps(out, r);

            // sinus has functional form sin((i-512) * PI/512)
            // or i = 256 x + 512
            for (int q = 0; q < 4; ++q)
            {
                float i = (inv[q] * 32 + 512);
                float v = shafted_tanh(x + 0.5) - shafted_tanh(0.5);
                INFO(inv[q] << " " << i << " " << v);
                REQUIRE(out[q] == Approx(v).margin(0.1));
            }
        }
    }
}

TEST_CASE("Dont Fear The Reaper", "[dsp]")
{
    // Reaper added cool per-plugin oversampling. Will we do OK with that?
    for (auto base : {44100, 48000})
    {
        // A few like string and noise won't pass this test so be explicit about our choices
        for (auto t : {ot_sine, ot_FM2, ot_wavetable, ot_window, ot_alias})
        {
            DYNAMIC_SECTION("Oversample Test " << base << " on " << osc_type_names[t])
            {
                std::vector surges = {Surge::Headless::createSurge(base),
                                      Surge::Headless::createSurge(base * 2)};
                // Surge::Headless::createSurge(base * 4)};

                constexpr static int nsurge = 2;
                REQUIRE(nsurge == surges.size());

                static constexpr int nsamples = 1024;
                float samples[3][nsamples];

                for (int s = 0; s < nsurge; ++s)
                {
                    // std::cout << "SampleRate at " << s << " = " << surges[s]->storage.samplerate
                    // << std::endl;
                    surges[s]->storage.getPatch().scene[0].osc[0].queue_type = t;
                    surges[s]->storage.getPatch().scene[0].osc[0].retrigger.val.b = true;

                    for (int q = 0; q < 10; ++q)
                        surges[s]->process();

                    surges[s]->playNote(0, 60, 127, 0);
                }

                for (int i = 0; i < nsurge; ++i)
                {
                    int mul = 1 << i;
                    int wp = 0;
                    while (wp < nsamples)
                    {
                        surges[i]->process();

                        int q = 0;
                        while (q < BLOCK_SIZE && wp < nsamples)
                        {
                            samples[i][wp] = surges[i]->output[0][q];
                            wp++;
                            q += mul;
                        }
                    }
                }

                for (int i = 0; i < nsamples; i++)
                {
                    // std::cout << base << " " << osc_type_names[t] << " " << i << " "
                    //     << samples[0][i] << " " << samples[1][i] << " " << samples[2][i] <<
                    //     std::endl;

                    // So we don't line up perfectly but if we stay in phase the
                    // per sample values won't matter that much since we have a long time
                    for (int q = 1; q < nsurge; ++q)
                    {
                        INFO("Checking at " << i << " " << q);
                        REQUIRE(fabs(samples[0][i] - samples[q][i]) < 0.05);
                    }
                    // REQUIRE(fabs(samples[0][i] - samples[2][i]) < 0.05);
                }
            }
        }
    }
}