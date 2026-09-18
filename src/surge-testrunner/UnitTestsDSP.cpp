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

#include <iostream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"

#include "SSEComplex.h"
#include <complex>
#include "sst/basic-blocks/mechanics/simd-ops.h"

#include "sst/plugininfra/cpufeatures.h"
#include "dsp/effects/chowdsp/shared/chowdsp_DelayLine.h"

using namespace Surge::Test;

TEST_CASE("Simple Single Oscillator is Constant", "[osc]")
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

    SECTION("S&H Noise Oscillator")
    {
        assertRelative("resources/test-data/patches/SH-Uni2-Relative.fxp");
        assertAbsolute("resources/test-data/patches/SH-Uni2-Absolute.fxp");
    }
}

TEST_CASE("Unison at Different Sample Rates", "[osc]")
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

        char txt[TXT_SIZE];
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
            char txt[TXT_SIZE];
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

    static constexpr std::initializer_list<int> srs{44100, 48000};

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
            INFO("S&H Noise Oscillator test at " << sr);
            auto surge = Surge::Headless::createSurge(sr, true);

            assertRelative(surge, "resources/test-data/patches/SH-Uni2-Relative.fxp");
            assertAbsolute(surge, "resources/test-data/patches/SH-Uni2-Absolute.fxp");
            // randomAbsolute(surge, "resources/test-data/patches/SH-Uni2-Absolute.fxp");
        }
    }
}

TEST_CASE("All Patches Have Bounded Output", "[dsp]")
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

TEST_CASE("Every Oscillator Plays", "[dsp]")
{
    for (int i = 0; i < n_osc_types; ++i)
    {
        DYNAMIC_SECTION("Oscillator type " << osc_type_names[i])
        {
            auto surge = Surge::Headless::createSurge(44100, true);
            REQUIRE(surge->storage.wt_list.size() > 0);

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
    SECTION("Can Make std::complex on m128")
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
        SIMD_MM(store1_ps)(sumSSE, sst::basic_blocks::mechanics::sum_ps_to_ss(qtr.real()));
        REQUIRE(sum == Approx(sumSSE[0]).margin(1e-5));

        float angles alignas(16)[4];
        angles[0] = 0;
        angles[1] = M_PI / 2;
        angles[2] = 3 * M_PI / 4;
        angles[3] = M_PI;
        auto asse = SIMD_MM(load_ps)(angles);
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

        // At this extrema we are a touch less accurate
        auto c3 = c.atIndex(3);
        REQUIRE(c3.real() == Approx(-1).margin(1e-4));
        REQUIRE(c3.imag() == Approx(0).margin(1e-4));

        auto powV = q.map([](const std::complex<float> &f) { return std::pow(f, 2.1f); });
        for (int i = 0; i < 4; ++i)
            REQUIRE(powV.atIndex(i) == pow(q.atIndex(i), 2.1f));
    }
}

// When we return to #1514 this is a good starting point
#if 0
TEST_CASE( "NaN Patch From Issue #1514", "[dsp]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   REQUIRE( surge->loadPatchByPath( "resources/test-data/patches/VinceyCrash1514.fxp", -1, "Test" ) );

   for( int d=0; d<10; d++ )
   {
      auto events = Surge::Headless::makeHoldNoteFor( 60 + 24, 4410, 100, 0 );
      for( auto &e : events )
         e.atSample += 1000;

      surge->stopSound();
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

TEST_CASE("Basic DSP", "[dsp]")
{
    SECTION("limit_range()")
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

TEST_CASE("Wavehaper Lookup Table", "[dsp]")
{
    SECTION("ASYM SSE2")
    {
        // need to do this to init tables only
        auto surge = Surge::Headless::createSurge(44100);

        auto wst = sst::waveshapers::GetQuadWaveshaper(sst::waveshapers::WaveshaperType::wst_asym);
        auto shafted_tanh = [](double x) { return (exp(x) - exp(-x * 1.2)) / (exp(x) + exp(-x)); };
        sst::waveshapers::QuadWaveshaperState qss{};
        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
            qss.R[i] = SIMD_MM(setzero_ps)();

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
            auto d = SIMD_MM(set1_ps)(1.0);
            float out alignas(16)[4], inv alignas(16)[4];
            auto in = SIMD_MM(set_ps)(x, x + 0.01, x + 0.03, x + 0.05);
            SIMD_MM(store_ps)(inv, in);

            auto r = wst(&qss, in, d);
            SIMD_MM(store_ps)(out, r);

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

TEST_CASE("Don't Fear The Reaper", "[dsp]")
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

TEST_CASE("Reverb1 White Noise Blast", "[dsp]")
{
    auto surge = Surge::Headless::createSurge(44100, true);
    int idxT = 1, idxF = -1;
    const auto &pl = surge->storage.patch_list;
    for (int i = 0; i < pl.size(); ++i)
    {
        if (pl[i].name == "Tolk")
        {
            idxT = i;
        }
        if (pl[i].name == "Monster Feedback")
        {
            idxF = i;
        }
    }
    REQUIRE(idxF >= 0);
    REQUIRE(idxT >= 0);

    int swapEvery = 127;
    int t = idxF;
    int n = idxT;
    int since = 0;
    int swaps = 0;
    for (int r = 0; r < swapEvery * 101; r++)
    {
        if (r % swapEvery == 0)
        {
            std::swap(t, n);
            surge->loadPatch(t);
            since = 0;
            swaps++;
        }
        surge->process();
        INFO("Running " << r << " " << since << " " << swaps << " " << pl[t].name);
        for (int s = 0; s < BLOCK_SIZE; ++s)
        {
            REQUIRE(std::fabs(surge->output[0][s]) < 1e-6);
            REQUIRE(std::fabs(surge->output[1][s]) < 1e-6);
        }
        since++;
    }
}

TEST_CASE("Oscillator Onset", "[dsp]") // See issue 7570
{
    /*
    ** An oscillator should start making sound immediately, whether or not retrigger is on.
    **
    ** The BLIT oscillators used to seed oscstate with a positive random value when
    ** retrigger was off, which leaves the buffers empty and the voice silent until the
    ** first convolute fires. That is a delay of up to a full cycle rather than a random
    ** start phase: at MIDI 60 it reached 169 oversampled samples for Classic and 337 for
    ** Wavetable, and it scales with the period, so a bass note could lose several
    ** milliseconds off the front of its attack.
    */
    constexpr int maxOnset{16};
    constexpr int nTrials{50};
    constexpr int nBlocks{32}; // enough to cover the old worst case even at MIDI 24

    for (const auto &rt : {true, false})
    {
        for (const auto &ot :
             {ot_classic, ot_wavetable, ot_window, ot_sine, ot_twist, ot_shnoise, ot_FM2, ot_FM3})
        {
            for (const auto &note : {24, 60, 96})
            {
                auto surge =
                    Surge::Headless::createSurge(44100, ot == ot_wavetable || ot == ot_window);
                auto storage = &surge->storage;

                auto oscstorage = &(storage->getPatch().scene[0].osc[0]);

                unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

                oscstorage->retrigger.val.b = rt;

                auto o = spawn_osc(ot, storage, oscstorage, storage->getPatch().scenedata[0],
                                   storage->getPatch().scenedataOrig[0], oscbuffer);
                o->init_ctrltypes();
                o->init_default_values();
                o->init_extra_config();

                int worstOnset{0};

                for (int trial = 0; trial < nTrials; ++trial)
                {
                    o->init(note);

                    int onset{nBlocks * BLOCK_SIZE_OS}, n{0};
                    bool found{false};

                    for (int j = 0; j < nBlocks && !found; ++j)
                    {
                        o->process_block(note, 0, true, false, 0);

                        for (int i = 0; i < BLOCK_SIZE_OS; ++i, ++n)
                        {
                            if (std::fabs(o->output[i]) > 1e-6)
                            {
                                onset = n;
                                found = true;
                                break;
                            }
                        }
                    }

                    worstOnset = std::max(worstOnset, onset);
                }

                o->~Oscillator();

                INFO("Oscillator " << osc_type_names[ot] << " at note " << note
                                   << " with retrigger " << (rt ? "on" : "off")
                                   << " has a worst case onset of " << worstOnset
                                   << " oversampled samples");
                REQUIRE(worstOnset <= maxOnset);
            }
        }
    }
}

TEST_CASE("Chow DSP Delay", "[dsp]")
{
    chowdsp::DelayLine<float> floatDelay(1024);
    floatDelay.prepare({48000, 16, 2});
    floatDelay.setDelay(128);
    for (int i = 0; i < 1024; ++i)
    {
        auto dL = floatDelay.popSample(0);
        auto dR = floatDelay.popSample(1);
        floatDelay.pushSample(0, i / 1024.);
        floatDelay.pushSample(1, 1 - i / 1024.);
        if (i >= 128)
        {
            INFO("I = " << i);
            auto ni = i - 128;
            REQUIRE(dL == ni / 1024.);
            REQUIRE(dR == 1 - ni / 1024.);
        }
        else
        {
            INFO("i = " << i);
            REQUIRE(dL == 0.f);
            REQUIRE(dR == 0.f);
        }
    }
}
TEST_CASE("Oscillator Onset Across Parameters", "[dsp]") // See issue 7570
{
    /*
    ** The onset bound is a property of the seeding, so it has to hold across the parameter
    ** space rather than only at the defaults. The Classic segment durations depend on both
    ** pulse widths and its levels on Shape and Sub Mix, so an error in the replay can hide
    ** at 0.5 and show up at the edges.
    */
    constexpr int maxOnset{16};
    constexpr int nTrials{8};
    constexpr int nBlocks{32};

    for (float shape : {-1.f, 0.f, 1.f})
    {
        for (float w1 : {0.01f, 0.5f, 0.99f})
        {
            for (float w2 : {0.01f, 0.5f, 0.99f})
            {
                for (float sub : {0.f, 1.f})
                {
                    for (int uni : {1, 7})
                    {
                        auto surge = Surge::Headless::createSurge(44100);
                        auto storage = &surge->storage;
                        auto oscstorage = &(storage->getPatch().scene[0].osc[0]);

                        unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

                        oscstorage->retrigger.val.b = false;

                        auto o = spawn_osc(ot_classic, storage, oscstorage,
                                           storage->getPatch().scenedata[0],
                                           storage->getPatch().scenedataOrig[0], oscbuffer);
                        o->init_ctrltypes();
                        o->init_default_values();
                        o->init_extra_config();

                        // the oscillator reads localcopy, which is scenedata here, so the
                        // parameters have to be pushed across rather than only set on oscdata
                        auto setf = [&](int idx, float v) {
                            oscstorage->p[idx].val.f = v;
                            storage->getPatch()
                                .scenedata[0][oscstorage->p[idx].param_id_in_scene]
                                .f = v;
                        };

                        setf(0, shape); // co_shape
                        setf(1, w1);    // co_width1
                        setf(2, w2);    // co_width2
                        setf(3, sub);   // co_mainsubmix
                        oscstorage->p[6].val.i = uni;

                        int worstOnset{0};

                        for (int trial = 0; trial < nTrials; ++trial)
                        {
                            o->init(60);

                            int onset{nBlocks * BLOCK_SIZE_OS}, n{0};
                            bool found{false};

                            for (int j = 0; j < nBlocks && !found; ++j)
                            {
                                o->process_block(60, 0, true, false, 0);

                                for (int i = 0; i < BLOCK_SIZE_OS; ++i, ++n)
                                {
                                    if (std::fabs(o->output[i]) > 1e-6)
                                    {
                                        onset = n;
                                        found = true;
                                        break;
                                    }
                                }
                            }

                            worstOnset = std::max(worstOnset, onset);
                        }

                        o->~Oscillator();

                        INFO("Classic with shape " << shape << " width1 " << w1 << " width2 " << w2
                                                   << " sub " << sub << " unison " << uni
                                                   << " has a worst case onset of " << worstOnset
                                                   << " oversampled samples");
                        REQUIRE(worstOnset <= maxOnset);
                    }
                }
            }
        }
    }
}

TEST_CASE("Retrigger Off Is A Phase Shift", "[dsp]") // See issue 7570
{
    /*
    ** Turning retrigger off should change where in the cycle a voice starts and nothing
    ** else, so the retrigger-off output has to be the retrigger-on output shifted in time.
    **
    ** This is the invariant the mid-cycle seeding exists to satisfy, and the one that
    ** breaks if ::init and ::convolute stop agreeing about the cycle: a voice seeded with
    ** the wrong period still makes sound and still starts promptly, but it is no longer the
    ** same waveform.
    */
    constexpr int nBlocks{64};
    constexpr int settle{1024}; // oversampled samples, past the integrator transient
    constexpr int window{512};

    /*
    ** Classic walks a four segment cycle lasting 2 * t, so the shift can be up to two
    ** periods rather than one. At MIDI 60 a period is about 337 oversampled samples, so
    ** this covers two full cycles.
    */
    constexpr int maxLag{1400};
    constexpr int nTrials{8};

    static_assert(settle + window + maxLag < nBlocks * BLOCK_SIZE_OS, "render is too short");

    auto render = [](SurgeSynthesizer *surge, int type, bool retrigger, std::vector<float> &out) {
        auto storage = &surge->storage;
        auto oscstorage = &(storage->getPatch().scene[0].osc[0]);

        unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

        oscstorage->retrigger.val.b = retrigger;

        auto o = spawn_osc(type, storage, oscstorage, storage->getPatch().scenedata[0],
                           storage->getPatch().scenedataOrig[0], oscbuffer);
        o->init_ctrltypes();
        o->init_default_values();
        o->init_extra_config();

        /*
        ** init_default_values only fills oscdata. The oscillator reads its parameters from
        ** localcopy, which is scenedata here, so the defaults have to be pushed across or
        ** it runs on zeros, and a zero Width 2 degenerates two of the four segments to
        ** nothing.
        */
        for (int q = 0; q < n_osc_params; ++q)
        {
            storage->getPatch().scenedata[0][oscstorage->p[q].param_id_in_scene].f =
                oscstorage->p[q].val.f;
        }

        o->init(60, false, false); // no drift, so the two runs differ only in start phase

        out.clear();

        for (int j = 0; j < nBlocks; ++j)
        {
            o->process_block(60, 0, false, false, 0);

            for (int i = 0; i < BLOCK_SIZE_OS; ++i)
            {
                out.push_back(o->output[i]);
            }
        }

        o->~Oscillator();
    };

    // normalized cross correlation of a window of a against the same window of b at lag
    auto correlate = [](const std::vector<float> &a, const std::vector<float> &b, int lag) {
        double ma{0}, mb{0};

        for (int i = 0; i < window; ++i)
        {
            ma += a[settle + i];
            mb += b[settle + lag + i];
        }

        ma /= window;
        mb /= window;

        double num{0}, da{0}, db{0};

        for (int i = 0; i < window; ++i)
        {
            double x = a[settle + i] - ma;
            double y = b[settle + lag + i] - mb;

            num += x * y;
            da += x * x;
            db += y * y;
        }

        return (da > 0 && db > 0) ? num / std::sqrt(da * db) : 0.0;
    };

    for (const auto &ot : {ot_classic, ot_wavetable})
    {
        auto surge = Surge::Headless::createSurge(44100, ot == ot_wavetable);

        std::vector<float> on, off;

        render(surge.get(), ot, true, on);

        double worst{1.0};

        for (int trial = 0; trial < nTrials; ++trial)
        {
            render(surge.get(), ot, false, off);

            double best{-1.0};

            for (int lag = 0; lag < maxLag; ++lag)
            {
                best = std::max(best, correlate(on, off, lag));
            }

            worst = std::min(worst, best);
        }

        INFO("Oscillator " << osc_type_names[ot]
                           << " retrigger off correlates with retrigger on at " << worst);
        REQUIRE(worst > 0.98);
    }
}

/*
** The three tests around this one cover different failure modes of the mid-cycle seeding,
** and none of them subsumes another. That is worth stating, because it is not obvious and
** it cost a few rounds of mutation testing to establish:
**
**   - Retrigger Off Is A Phase Shift compares steady state, so it catches a voice that
**     settles into the wrong waveform. It cannot catch anything about the onset: Classic
**     resets its level absolutely at state 0 and recomputes rate on every convolute, so
**     seeding errors wash out within a cycle or two, and a correlation that searches over
**     lag is blind to a phase offset by construction.
**   - Onset Step Stays Within The Waveform looks at the level of sample 0, so it catches a
**     wrongly seeded level. It is blind to an error in WHEN the next impulse fires.
**   - First Block Runs At The Steady Rate looks at how fast the first block is running, so
**     it catches a wrongly seeded period, which is the bug that reached review.
**
** One gap is known and not covered: seeding the voice into the wrong segment of the cycle
** is not reliably caught by any of the three.
*/

TEST_CASE("Onset Step Stays Within The Waveform", "[dsp]") // See issue 7570
{
    /*
    ** Starting mid-cycle means starting at an arbitrary level, so there is a step at note
    ** on. It should stay within what the waveform does anyway rather than exceeding it.
    **
    ** The bound is 1.4 times the oscillator's own largest step. Measured at 1.11 with sync
    ** off and 1.14 with sync at 60. Seeding the integrator with a level 50% too large takes
    ** it to 1.67, so the bound sits in the gap rather than near either end.
    **
    ** This is a default parameter bound. At extreme Width and Shape settings the ratio
    ** legitimately reaches about 3.7, since the waveform is far more asymmetric there.
    */
    constexpr float maxRatio{1.4f};
    constexpr int nTrials{200};
    constexpr int warmup{8};
    constexpr int steadyBlocks{40};

    for (const auto &note : {36, 60})
    {
        for (const auto &syncv : {0.f, 60.f})
        {
            auto surge = Surge::Headless::createSurge(44100);
            auto storage = &surge->storage;
            auto oscstorage = &(storage->getPatch().scene[0].osc[0]);

            unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

            oscstorage->retrigger.val.b = false;

            auto o = spawn_osc(ot_classic, storage, oscstorage, storage->getPatch().scenedata[0],
                               storage->getPatch().scenedataOrig[0], oscbuffer);
            o->init_ctrltypes();
            o->init_default_values();
            o->init_extra_config();

            /*
            ** init_default_values only fills oscdata. The oscillator reads its parameters
            ** from localcopy, which is scenedata here, so the defaults have to be pushed
            ** across or it runs on zeros and the pulse widths degenerate.
            */
            for (int q = 0; q < n_osc_params; ++q)
            {
                storage->getPatch().scenedata[0][oscstorage->p[q].param_id_in_scene].f =
                    oscstorage->p[q].val.f;
            }

            oscstorage->p[4].val.f = syncv; // co_sync
            storage->getPatch().scenedata[0][oscstorage->p[4].param_id_in_scene].f = syncv;

            float worst{0.f}, worstJump{0.f}, worstStep{0.f};

            for (int trial = 0; trial < nTrials; ++trial)
            {
                o->init(note, false, false);

                o->process_block(note, 0, false, false, 0);

                // the buffers start empty, so the step at sample 0 is measured from silence
                float jump = std::fabs(o->output[0]);
                float prev = o->output[BLOCK_SIZE_OS - 1];

                for (int j = 0; j < warmup; ++j)
                {
                    o->process_block(note, 0, false, false, 0);
                    prev = o->output[BLOCK_SIZE_OS - 1];
                }

                float step{0.f};

                for (int j = 0; j < steadyBlocks; ++j)
                {
                    o->process_block(note, 0, false, false, 0);

                    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
                    {
                        step = std::max(step, std::fabs(o->output[i] - prev));
                        prev = o->output[i];
                    }
                }

                float ratio = jump / std::max(step, 1e-6f);

                if (ratio > worst)
                {
                    worst = ratio;
                    worstJump = jump;
                    worstStep = step;
                }
            }

            o->~Oscillator();

            INFO("Classic at note " << note << " with sync " << syncv << " opens with a step of "
                                    << worstJump << " against a largest running step of "
                                    << worstStep << ", a ratio of " << worst);
            REQUIRE(worst < maxRatio);
        }
    }
}

TEST_CASE("First Block Runs At The Steady Rate", "[dsp]") // See issue 7570
{
    /*
    ** A voice seeded mid-cycle has to be seeded from the period it will actually run at.
    ** Computing that period from detune alone, while convolute computes it from detune plus
    ** sync, leaves a synced voice running its first block at the wrong rate before it
    ** settles. That is the bug this guards, and it reached review unnoticed because the
    ** voice still starts promptly and still settles correctly.
    **
    ** The proxy for rate is the mean absolute first difference of a block. A voice emitting
    ** half as many impulses roughly halves it. It needs no amplitude threshold and, unlike
    ** counting edges, cannot merge two impulses that land a couple of samples apart, which
    ** they do at this pitch.
    **
    ** Assert on the mean across trials rather than on any single trial. With sync active a
    ** voice is reset often, so depending on its random start phase an individual trial can
    ** recover before the block ends: seeding the period without sync still produced single
    ** trials up to 0.997 even though its mean was 0.365, against a baseline mean of 0.937
    ** that stayed inside 0.90 to 1.03. The bug is statistical and the test has to be too.
    */
    constexpr double minMeanRatio{0.8};
    constexpr int note{72};
    constexpr float syncv{60.f};
    constexpr int nTrials{100};
    constexpr int warmup{8};
    constexpr int steadyBlocks{40};

    auto meanAbsDiff = [](const float *b, float prev) {
        double s{0};

        for (int i = 0; i < BLOCK_SIZE_OS; ++i)
        {
            s += std::fabs(b[i] - prev);
            prev = b[i];
        }

        return s / BLOCK_SIZE_OS;
    };

    auto surge = Surge::Headless::createSurge(44100);
    auto storage = &surge->storage;
    auto oscstorage = &(storage->getPatch().scene[0].osc[0]);

    unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

    oscstorage->retrigger.val.b = false;

    auto o = spawn_osc(ot_classic, storage, oscstorage, storage->getPatch().scenedata[0],
                       storage->getPatch().scenedataOrig[0], oscbuffer);
    o->init_ctrltypes();
    o->init_default_values();
    o->init_extra_config();

    for (int q = 0; q < n_osc_params; ++q)
    {
        storage->getPatch().scenedata[0][oscstorage->p[q].param_id_in_scene].f =
            oscstorage->p[q].val.f;
    }

    oscstorage->p[4].val.f = syncv; // co_sync
    storage->getPatch().scenedata[0][oscstorage->p[4].param_id_in_scene].f = syncv;

    double sumRatio{0};

    for (int trial = 0; trial < nTrials; ++trial)
    {
        o->init(note, false, false);

        o->process_block(note, 0, false, false, 0);

        // the first block starts from silence, so the previous sample is zero
        double first = meanAbsDiff(o->output, 0.f);
        float prev = o->output[BLOCK_SIZE_OS - 1];

        for (int j = 0; j < warmup; ++j)
        {
            o->process_block(note, 0, false, false, 0);
            prev = o->output[BLOCK_SIZE_OS - 1];
        }

        double steady{0};

        for (int j = 0; j < steadyBlocks; ++j)
        {
            o->process_block(note, 0, false, false, 0);
            steady += meanAbsDiff(o->output, prev);
            prev = o->output[BLOCK_SIZE_OS - 1];
        }

        steady /= steadyBlocks;

        sumRatio += first / std::max(steady, 1e-9);
    }

    o->~Oscillator();

    double meanRatio = sumRatio / nTrials;

    INFO("Classic at note " << note << " with sync " << syncv << " runs its first block at "
                            << meanRatio << " of the steady state rate, averaged over " << nTrials
                            << " note ons");
    REQUIRE(meanRatio > minMeanRatio);
}
