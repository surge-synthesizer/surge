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
    for (const auto &rt : {true, false})
    {
        for (const auto &ot :
             {ot_classic, ot_wavetable, ot_window, ot_sine, ot_twist, ot_shnoise, ot_FM2, ot_FM3})
        {
            auto surge = Surge::Headless::createSurge(44100, ot == ot_wavetable || ot == ot_window);
            auto storage = &surge->storage;

            auto oscstorage = &(storage->getPatch().scene[0].osc[0]);

            unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

            oscstorage->retrigger.val.b = rt;

            auto o = spawn_osc(ot, storage, oscstorage, storage->getPatch().scenedata[0],
                               storage->getPatch().scenedataOrig[0], oscbuffer);
            o->init_ctrltypes();
            o->init_default_values();
            o->init_extra_config();

            o->init(60);

            int itUntilBigger{0};
            bool continueChecking{true};

            for (int j = 0; j < 10 && continueChecking; ++j)
            {
                o->process_block(60, 0, true, false, 0);
                for (int i = 0; i < BLOCK_SIZE_OS && continueChecking; ++i)
                {
                    itUntilBigger++;
                    if (std::fabs(o->output[i]) > 1e-6)
                    {
                        continueChecking = false;
                        break;
                    }
                }
            }
            o->~Oscillator();
            /*std::cout << "Onset Delay for " << osc_type_names[ot] << " with retrig=" << rt << " is
               "
                      << (continueChecking ? "Unknown" : std::to_string(itUntilBigger))
                      << std::endl;*/
        }
    }
}
