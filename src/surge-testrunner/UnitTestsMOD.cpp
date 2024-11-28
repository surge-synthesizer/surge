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
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"
#include "ModControl.h"

using namespace Surge::Test;

TEST_CASE("ADSR Envelope Behaviour", "[mod]")
{

    std::shared_ptr<SurgeSynthesizer> surge(Surge::Headless::createSurge(44100));
    REQUIRE(surge.get());

    /*
    ** OK so lets set up a pretty simple setup
    */

    auto runAdsr = [surge](float a, float d, float s, float r, int a_s, int d_s, int r_s,
                           bool isAnalog, float releaseAfter, float runUntil, float pushSusAt = -1,
                           float pushSusTo = 0) {
        auto *adsrstorage = &(surge->storage.getPatch().scene[0].adsr[0]);
        std::shared_ptr<ADSRModulationSource> adsr(new ADSRModulationSource());
        adsr->init(&(surge->storage), adsrstorage, surge->storage.getPatch().scenedata[0], nullptr);
        REQUIRE(adsr.get());

        int ids, ide;
        setupStorageRanges(&(adsrstorage->a), &(adsrstorage->mode), ids, ide);
        REQUIRE(ide > ids);
        REQUIRE(ide >= 0);
        REQUIRE(ids >= 0);

        auto svn = [](Parameter *p, float vn) {
            p->set_value_f01(p->value_to_normalized(limit_range(vn, p->val_min.f, p->val_max.f)));
        };

        auto svni = [](Parameter *p, int vn) { p->val.i = vn; };

        auto inverseEnvtime = [](float desiredTime) {
            // 2^x = desired time
            auto x = log(desiredTime) / log(2.0);
            return x;
        };

        svn(&(adsrstorage->a), inverseEnvtime(a));
        svn(&(adsrstorage->d), inverseEnvtime(d));
        svn(&(adsrstorage->s), s);
        svn(&(adsrstorage->r), inverseEnvtime(r));

        svni(&(adsrstorage->a_s), a_s);
        svni(&(adsrstorage->d_s), d_s);
        svni(&(adsrstorage->r_s), r_s);

        adsrstorage->mode.val.b = isAnalog;

        copyScenedataSubset(&(surge->storage), 0, ids, ide);
        adsr->attack();

        bool released = false;
        bool pushSus = false;
        int i = 0;
        std::vector<std::pair<float, float>> res;
        res.push_back(std::make_pair(0.f, 0.f));

        while (true)
        {
            auto t = 1.0 * (i + 1) * BLOCK_SIZE * surge->storage.dsamplerate_inv;
            i++;
            if (t > runUntil || runUntil < 0)
                break;

            if (t > releaseAfter && !released)
            {
                adsr->release();
                released = true;
            }

            if (pushSusAt > 0 && !pushSus && t > pushSusAt)
            {
                pushSus = true;
                svn(&(adsrstorage->s), pushSusTo);
                copyScenedataSubset(&(surge->storage), 0, ids, ide);
            }

            adsr->process_block();
            res.push_back(std::make_pair((float)t, adsr->get_output(0)));
            if (false && i > 270 && i < 290)
                std::cout << i << " " << t << " " << adsr->get_output(0) << " "
                          << adsr->getEnvState() << std::endl;
        }
        return res;
    };

    auto detectTurnarounds = [](std::vector<std::pair<float, float>> data) {
        auto pv = -1000.0;
        int dir = 1;
        std::vector<std::pair<float, int>> turns;
        turns.push_back(std::make_pair(0, 1));
        for (auto &p : data)
        {
            auto t = p.first;
            auto v = p.second;
            if (pv >= 0)
            {
                int ldir = 0;
                if (v > 0.999999f)
                    ldir = dir; // sometimes we get a double '1'
                if (turns.size() > 1 && fabs(v - pv) < 5e-6 && fabs(v) < 1e-5)
                    ldir = 0; // bouncing off of 0 is annoying
                else if (fabs(v - pv) < 1e-7)
                    ldir = 0;
                else if (v > pv)
                    ldir = 1;
                else
                    ldir = -1;

                if (v != 1)
                {
                    if (ldir != dir)
                    {
                        turns.push_back(std::make_pair(t, ldir));
                    }
                    dir = ldir;
                }
            }
            pv = v;
        }
        return turns;
    };

    // With 0 sustain I should decay in decay time
    auto runCompare = [&](float a, float d, float s, float r, int a_s, int d_s, int r_s,
                          bool isAnalog) {
        float sustime = 0.1;
        float endtime = 0.1;
        float totaltime = a + d + sustime + r + endtime;

        auto simple = runAdsr(a, d, s, r, a_s, d_s, r_s, isAnalog, a + d + sustime, totaltime);
        auto sturns = detectTurnarounds(simple);
        INFO("ADSR: " << a << " " << d << " " << s << " " << r << " switches: " << a_s << " " << d_s
                      << " " << r_s);
        if (s == 0)
        {
            if (sturns.size() != 3)
            {
                for (const auto &s : simple)
                    std::cout << s.first << " " << s.second << std::endl;
                for (const auto &s : sturns)
                    std::cout << s.first << " " << s.second << std::endl;
            }
            REQUIRE(sturns.size() == 3);
            REQUIRE(sturns[0].first == 0);
            REQUIRE(sturns[0].second == 1);
            REQUIRE(sturns[1].first == Approx(a).margin(0.01));
            REQUIRE(sturns[1].second == -1);
            REQUIRE(sturns[2].first == Approx(a + d).margin(0.01));
            REQUIRE(sturns[2].second == 0);
        }
        else
        {
            if (sturns.size() != 5)
            {
                for (const auto &s : simple)
                    std::cout << s.first << " " << s.second << std::endl;
                std::cout << "TURNS" << std::endl;
                for (const auto &s : sturns)
                    std::cout << s.first << " " << s.second << std::endl;
            }
            REQUIRE(sturns.size() == 5);
            REQUIRE(sturns[0].first == 0);
            REQUIRE(sturns[0].second == 1);
            REQUIRE(sturns[1].first == Approx(a).margin(0.01));
            REQUIRE(sturns[1].second == -1);
            if (d_s == 0)
            {
                // this equality only holds in the linear case; in the polynomial case you get
                // faster reach to non-zero sustain
                REQUIRE(sturns[2].first == Approx(a + d * (1.0 - s)).margin(0.01));
            }
            else if (a + d * (1.0 - s) > 0.1 && d > 0.05)
            {
                REQUIRE(sturns[2].first < a + d * (1.0 - s) + 0.01);
            }
            REQUIRE(sturns[2].second == 0);
            REQUIRE(sturns[3].first == Approx(a + d + sustime).margin(0.01));
            REQUIRE(sturns[3].second == -1);
            if (r_s == 0 || (s > 0.1 && r > 0.05)) // if we are in the non-linear releases at low
                                                   // sustain we get there early
            {
                REQUIRE(sturns[4].first ==
                        Approx(a + d + sustime + r).margin((r_s == 0 ? 0.01 : (r * 0.1))));
                REQUIRE(sturns[4].second == 0);
            }
        }
    };

    SECTION("Test Digital Envelope")
    {
        for (int as = 0; as < 3; ++as)
            for (int ds = 0; ds < 3; ++ds)
                for (int rs = 0; rs < 3; ++rs)
                {
                    runCompare(0.2, 0.3, 0.0, 0.1, as, ds, rs, false);
                    runCompare(0.2, 0.3, 0.5, 0.1, as, ds, rs, false);

                    for (int rc = 0; rc < 10; ++rc)
                    {
                        auto a = rand() * 1.0 / (float)RAND_MAX;
                        auto d = rand() * 1.0 / (float)RAND_MAX;
                        auto s = 0.8 * rand() * 1.0 / (float)RAND_MAX +
                                 0.1; // we have tested the s=0 case above
                        auto r = rand() * 1.0 / (float)RAND_MAX;
                        runCompare(a, d, s, r, as, ds, rs, false);
                    }
                }
    }

    SECTION("Digital Envelope With Quadratic Slope Hits Zero")
    {
        auto res = runAdsr(0.1, 0.1, 0.0, 0.1, 0, 1, 0, false, 0.4, 0.5);
        for (const auto &p : res)
        {
            if (p.first > 0.22)
            {
                REQUIRE(p.second == 0.f);
            }
        }
    }

    SECTION("Test Analog Envelope")
    {
        // OK so we can't check the same thing here since the turns aren't as tight in analog mode
        // Also the analog ADSR sustains at half the given sustain.
        auto testAnalog = [&](float a, float d, float s, float r) {
            INFO("ANALOG " << a << " " << d << " " << s << " " << r);
            auto holdFor = a + d + d + 0.5;

            auto ae = runAdsr(a, d, s, r, 0, 0, 0, true, holdFor, holdFor + 4 * r);
            auto aturns = detectTurnarounds(ae);

            float maxt = 0, maxv = 0;
            float zerot = 0;
            float valAtRelEnd = -1;
            std::vector<float> heldPeriod;
            for (const auto &obs : ae)
            {
                // std::cout << obs.first << " " << obs.second << std::endl;

                if (obs.first > a + d + d * 0.95 && obs.first < holdFor &&
                    s > 0.05) // that 0.1 lets the delay ring off
                {
                    REQUIRE(obs.second == Approx(s * s).margin(1e-3));
                    heldPeriod.push_back(obs.second);
                }
                if (obs.first > a + d && obs.second < 5e-5 && zerot == 0)
                    zerot = obs.first;
                if (obs.first > holdFor + r && valAtRelEnd < 0)
                    valAtRelEnd = obs.second;

                if (obs.second > maxv)
                {
                    maxv = obs.second;
                    maxt = obs.first;
                }
            }

            // In the held period are we mostly constant
            if (heldPeriod.size() > 10)
            {
                float sum = 0;
                for (auto p : heldPeriod)
                    sum += p;
                float mean = sum / heldPeriod.size();
                float var = 0;
                for (auto p : heldPeriod)
                    var += (p - mean) * (p - mean);
                var /= (heldPeriod.size() - 1);
                float stddev = sqrt(var);
                REQUIRE(stddev < d * 5e-3);
            }
            REQUIRE(maxt < a);
            REQUIRE(maxv > 0.99);
            if (s > 0.05)
            {
                REQUIRE(zerot > holdFor + r * 0.9);
                REQUIRE(valAtRelEnd < s * 0.025);
            }
        };

        testAnalog(0.1, 0.2, 0.5, 0.1);
        testAnalog(0.1, 0.2, 0.0, 0.1);
        for (int rc = 0; rc < 50; ++rc)
        {
            auto a = rand() * 1.0 / (float)RAND_MAX + 0.03;
            auto d = rand() * 1.0 / (float)RAND_MAX + 0.03;
            auto s =
                0.7 * rand() * 1.0 / (float)RAND_MAX + 0.2; // we have tested the s=0 case above
            auto r = rand() * 1.0 / (float)RAND_MAX + 0.03;
            testAnalog(a, d, s, r);
        }
    }

    // This is just a rudiemntary little test of this in digital mode
    SECTION("Test Digital Envelope Sustain Push")
    {
        auto testSusPush = [&](float s1, float s2) {
            auto digPush = runAdsr(0.05, 0.05, s1, 0.1, 0, 0, 0, false, 0.5, s2, 0.25, s2);
            int obs = 0;
            for (const auto &s : digPush)
            {
                if (s.first > 0.2 && obs == 0)
                {
                    REQUIRE(s.second == Approx(s1).margin(1e-5));
                    obs++;
                }
                if (s.first > 0.3 && obs == 1)
                {
                    REQUIRE(s.second == Approx(s2).margin(1e-5));
                    obs++;
                }
            }
        };

        for (auto i = 0; i < 10; ++i)
        {
            auto s1 = 0.95f * rand() / (float)RAND_MAX + 0.02;
            auto s2 = 0.95f * rand() / (float)RAND_MAX + 0.02;
            testSusPush(s1, s2);
        }
    }

    /*
    ** This section recreates the somewhat painful SSE code in readable stuff
    */
    auto analogClone = [surge](float a_sec, float d_sec, float s, float r_sec, float releaseAfter,
                               float runUntil, float pushSusAt = -1, float pushSusTo = 0) {
        float a = limit_range((float)(log(a_sec) / log(2.0)), -8.f, 5.f);
        float d = limit_range((float)(log(d_sec) / log(2.0)), -8.f, 5.f);
        float r = limit_range((float)(log(r_sec) / log(2.0)), -8.f, 5.f);

        int i = 0;
        bool released = false;
        std::vector<std::pair<float, float>> res;
        res.push_back(std::make_pair(0.f, 0.f));

        float v_c1 = 0.f;
        float v_c1_delayed = 0.f;
        bool discharge = false;
        const float v_cc = 1.5f;

        while (true)
        {
            float t = 1.0 * (i + 1) * BLOCK_SIZE * surge->storage.dsamplerate_inv;
            i++;
            if (t > runUntil || runUntil < 0)
                break;

            if (t > releaseAfter && !released)
            {
                released = true;
            }

            if (pushSusAt > 0 && t > pushSusAt)
                s = pushSusTo;

            auto gate = !released;
            float v_gate = gate ? v_cc : 0.f;

            // discharge = SIMD_MM(and_ps)(SIMD_MM(or_ps)(SIMD_MM(cmpgt_ss)(v_c1_delayed, one),
            // discharge), v_gate);
            discharge = ((v_c1_delayed > 1) || discharge) && gate;
            v_c1_delayed = v_c1;

            float S = s * s;

            float v_attack = discharge ? 0 : v_gate;
            // OK so this line:
            // auto v_decay = SIMD_MM(or_ps)(SIMD_MM(andnot_ps)(discharge, v_cc_vec),
            // SIMD_MM(and_ps)(discharge, S)); The semantic intent is discharge ? S : v_cc but in
            // the ADSR discharge has a value of v_gate which is 1.5 (v_cc) not 1.0. That bitwise
            // and with 1.5 acts as a binary filter (the mantissa) and a rounding operation (from
            // the .5) so I need to duplicate it exactly here.
            /*
            auto sM = SIMD_MM(load_ss)(&S);
            auto dM = SIMD_MM(load_ss)(&v_cc);
            auto vdv = SIMD_MM(and_ps)(dM, sM);
            float v_decay = discharge ? vdv[0] : v_cc;
            */
            // Alternately I can correct the SSE code for discharge
            float v_decay = discharge ? S : v_cc;
            float v_release = v_gate;

            float diff_v_a = std::max(0.f, v_attack - v_c1);
            float diff_v_d = (discharge && gate) ? v_decay - v_c1 : std::min(0.f, v_decay - v_c1);
            // float diff_v_d = std::min( 0.f, v_decay   - v_c1 );
            float diff_v_r = std::min(0.f, v_release - v_c1);

            const float shortest = 6.f;
            const float longest = -2.f;
            const float coeff_offset = 2.f - log(surge->storage.samplerate / BLOCK_SIZE) / log(2.f);

            float coef_A = pow(2.f, std::min(0.f, coeff_offset - a));
            float coef_D = pow(2.f, std::min(0.f, coeff_offset - d));
            float coef_R = pow(2.f, std::min(0.f, coeff_offset - r));

            v_c1 = v_c1 + diff_v_a * coef_A;
            v_c1 = v_c1 + diff_v_d * coef_D;
            v_c1 = v_c1 + diff_v_r * coef_R;

            // adsr->process_block();
            float output = v_c1;
            if (!gate && !discharge && v_c1 < 1e-6)
                output = 0;

            res.push_back(std::make_pair((float)t, output));
        }
        return res;
    };

    SECTION("Port The Analog Envelope To Non-SIMD")
    {
        auto compareSrgRepl = [&](float a, float d, float s, float r) {
            auto t = a + d + 0.5 + r * 3;
            auto surgeA = runAdsr(a, d, s, r, 0, 0, 0, true, a + d + 0.5, t);
            auto replA = analogClone(a, d, s, r, a + d + 0.5, t);

            REQUIRE(surgeA.size() == Approx(replA.size()).margin(3));
            auto sz = std::min(surgeA.size(), replA.size());
            for (auto i = 0; i < sz; ++i)
            {
                if (replA[i].second > 1e-5) // CI pipelines bounce around zero badly
                    REQUIRE(replA[i].second == Approx(surgeA[i].second).margin(1e-2));
            }
        };

        compareSrgRepl(0.1, 0.2, 0.5, 0.1);
        compareSrgRepl(0.1, 0.2, 0.0, 0.1);

        for (int rc = 0; rc < 100; ++rc)
        {
            float a = rand() * 1.0 / (float)RAND_MAX + 0.03;
            float d = rand() * 1.0 / (float)RAND_MAX + 0.01;
            float s =
                0.7 * rand() * 1.0 / (float)RAND_MAX + 0.1; // we have tested the s=0 case above
            float r = rand() * 1.0 / (float)RAND_MAX +
                      0.1; // smaller versions can get one bad point in the pipeline
            INFO("Testing " << rc << " with ADSR=" << a << " " << d << " " << s << " " << r);
            compareSrgRepl(a, d, s, r);
        }
    }

    SECTION("Test Analog Envelope Sustain Push")
    {
        auto testSusPush = [&](float s1, float s2) {
            auto aSurgePush = runAdsr(0.05, 0.05, s1, 0.1, 0, 0, 0, true, 0.5, s2, 0.25, s2);
            auto aDupPush = analogClone(0.05, 0.05, s1, 0.1, 0.5, 0.7, 0.25, s2);

            int obs = 0;
            INFO("Non-SIMD Analog Envelope Passes Sustain Push");
            for (const auto &s : aDupPush)
            {
                if (s.first > 0.2 && obs == 0)
                {
                    REQUIRE(s.second == Approx(s1 * s1).margin(1e-2));
                    obs++;
                }
                if (s.first > 0.4 && obs == 1)
                {
                    REQUIRE(s.second == Approx(s2 * s2).margin(1e-2));
                    obs++;
                }
            }

            obs = 0;
            INFO("SSE Analog Envelope Passes Sustain Push");
            for (const auto &s : aSurgePush)
            {
                if (s.first > 0.2 && obs == 0)
                {
                    REQUIRE(s.second == Approx(s1 * s1).margin(1e-2));
                    obs++;
                }
                if (s.first > 0.4 && obs == 1)
                {
                    REQUIRE(s.second == Approx(s2 * s2).margin(1e-3));
                    obs++;
                }
            }
        };

        testSusPush(0.3, 0.7);
        for (auto i = 0; i < 10; ++i)
        {
            auto s1 = 0.95f * rand() / (float)RAND_MAX + 0.02;
            auto s2 = 0.95f * rand() / (float)RAND_MAX + 0.02;
            testSusPush(s1, s2);
        }
    }
}

TEST_CASE("Non-MPE Pitch Bend", "[mod]")
{
    SECTION("Simple Bend Distances")
    {
        auto surge = surgeOnSine();
        surge->mpeEnabled = false;
        surge->storage.getPatch().scene[0].pbrange_up.val.i = 2;
        surge->storage.getPatch().scene[0].pbrange_dn.val.i = 2;

        auto f60 = frequencyForNote(surge, 60);
        auto f62 = frequencyForNote(surge, 62);
        auto f58 = frequencyForNote(surge, 58);

        surge->pitchBend(0, 8192);
        auto f60bendUp = frequencyForNote(surge, 60);

        surge->pitchBend(0, -8192);
        auto f60bendDn = frequencyForNote(surge, 60);

        REQUIRE(f62 == Approx(f60bendUp).margin(0.3));
        REQUIRE(f58 == Approx(f60bendDn).margin(0.3));
    }

    SECTION("Asymmetric Bend Distances")
    {
        auto surge = surgeOnSine();
        surge->mpeEnabled = false;
        for (int tests = 0; tests < 20; ++tests)
        {
            int bUp = rand() % 24 + 1;
            int bDn = rand() % 24 + 1;

            surge->storage.getPatch().scene[0].pbrange_up.val.i = bUp;
            surge->storage.getPatch().scene[0].pbrange_dn.val.i = bDn;
            auto fUpD = frequencyForNote(surge, 60 + bUp);
            auto fDnD = frequencyForNote(surge, 60 - bDn);

            // Bend pitch and let it get there
            surge->pitchBend(0, 8192);
            for (int i = 0; i < 100; ++i)
                surge->process();

            auto fUpB = frequencyForNote(surge, 60);

            // Bend pitch and let it get there
            surge->pitchBend(0, -8192);
            for (int i = 0; i < 100; ++i)
                surge->process();
            auto fDnB = frequencyForNote(surge, 60);

            REQUIRE(fUpD ==
                    Approx(fUpB).margin(3 * bUp)); // It can take a while for the midi lag to
                                                   // normalize and my pitch detector is so so
            REQUIRE(fDnD == Approx(fDnB).margin(3 * bDn));

            surge->pitchBend(0, 0);
            for (int i = 0; i < 100; ++i)
                surge->process();
        }
    }
}

TEST_CASE("Extended Pitch Bend", "[mod]")
{
    SECTION("Simple Bend Distances Up")
    {
        auto surge = surgeOnSine();
        surge->mpeEnabled = false;
        surge->storage.getPatch().scene[0].pbrange_up.set_extend_range(true);
        surge->storage.getPatch().scene[0].pbrange_up.val.i = 250;
        surge->storage.getPatch().scene[0].pbrange_dn.val.i = 2;

        auto f60 = frequencyForNote(surge, 60);
        auto f58 = frequencyForNote(surge, 58);

        surge->pitchBend(0, 8192);
        auto f60bendUp = frequencyForNote(surge, 60);

        surge->pitchBend(0, -8192);
        auto f60bendDn = frequencyForNote(surge, 60);

        REQUIRE(f60bendUp == Approx(f60 * pow(2, (2.5f / 12.f))).margin(0.3));
        REQUIRE(f58 == Approx(f60bendDn).margin(0.3));
    }

    SECTION("Simple Bend Distances Down")
    {
        auto surge = surgeOnSine();
        surge->mpeEnabled = false;
        surge->storage.getPatch().scene[0].pbrange_up.val.i = 2;
        surge->storage.getPatch().scene[0].pbrange_dn.set_extend_range(true);
        surge->storage.getPatch().scene[0].pbrange_dn.val.i = 250;

        auto f60 = frequencyForNote(surge, 60);
        auto f62 = frequencyForNote(surge, 62);

        surge->pitchBend(0, 8192);
        auto f60bendUp = frequencyForNote(surge, 60);

        surge->pitchBend(0, -8192);
        auto f60bendDn = frequencyForNote(surge, 60);

        REQUIRE(f62 == Approx(f60bendUp).margin(0.3));
        REQUIRE(f60bendDn == Approx(f60 * pow(2, (-2.5f / 12.f))).margin(0.3));
    }

    SECTION("Pitch By Ratio")
    {
        auto surge = surgeOnSine();
        std::string errMsg;
        surge->mpeEnabled = false;
        surge->storage.getPatch().scene[0].pbrange_up.set_extend_range(true);
        surge->storage.getPatch().scene[0].pbrange_up.set_value_from_string("9/8", errMsg);

        surge->storage.getPatch().scene[0].pbrange_dn.set_extend_range(true);
        surge->storage.getPatch().scene[0].pbrange_dn.set_value_from_string("3/2", errMsg);

        auto f60 = frequencyForNote(surge, 60);

        surge->pitchBend(0, 8192);
        auto f60bendUp = frequencyForNote(surge, 60);

        surge->pitchBend(0, -8192);
        auto f60bendDn = frequencyForNote(surge, 60);

        REQUIRE(f60bendUp == Approx(f60 * pow(2, (2.04f / 12.f))).margin(0.3));
        REQUIRE(f60bendDn == Approx(f60 * pow(2, (-7.02f / 12.f))).margin(0.3));
    }
}

TEST_CASE("Pitch Bend And Tuning", "[mod][tun]")
{
    SECTION("Multi Scale Bend Distances")
    {
        auto surge = surgeOnSine();
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        surge->mpeEnabled = false;

        // clang-format off
        static constexpr std::initializer_list<const char *> testScales
                    {"resources/test-data/scl/12-intune.scl",
                     "resources/test-data/scl/zeus22.scl",
                     "resources/test-data/scl/6-exact.scl"};
        // clang-format on

        for (auto sclf : testScales)
        {
            INFO("Retuning pitch bend to " << sclf);
            auto s = Tunings::readSCLFile(sclf);
            surge->storage.retuneToScale(s);
            for (int tests = 0; tests < 30; ++tests)
            {
                int bUp = rand() % 24 + 1;
                int bDn = rand() % 24 + 1;

                surge->storage.getPatch().scene[0].pbrange_up.val.i = bUp;
                surge->storage.getPatch().scene[0].pbrange_dn.val.i = bDn;
                auto fUpD = frequencyForNote(surge, 60 + bUp);
                auto fDnD = frequencyForNote(surge, 60 - bDn);

                // Bend pitch and let it get there
                surge->pitchBend(0, 8192);
                for (int i = 0; i < 500; ++i)
                    surge->process();

                auto fUpB = frequencyForNote(surge, 60);

                // Bend pitch and let it get there
                surge->pitchBend(0, -8192);
                for (int i = 0; i < 500; ++i)
                    surge->process();
                auto fDnB = frequencyForNote(surge, 60);
                auto dup = surge->storage.note_to_pitch(60 + bUp + 2) -
                           surge->storage.note_to_pitch(60 + bUp);
                dup = dup * 8.17;

                auto ddn = surge->storage.note_to_pitch(60 - bDn) -
                           surge->storage.note_to_pitch(60 - bDn - 2);
                ddn = ddn * 8.17;

                INFO("Pitch Bend " << bUp << " " << bDn << " " << fUpD << " " << fUpB << " " << dup
                                   << " " << ddn);
                REQUIRE(fUpD ==
                        Approx(fUpB).margin(dup)); // It can take a while for the midi lag to
                                                   // normalize and my pitch detector is so so
                REQUIRE(fDnD == Approx(fDnB).margin(ddn));

                surge->pitchBend(0, 0);
                for (int i = 0; i < 100; ++i)
                    surge->process();
            }
        }
    }
}

TEST_CASE("MPE Pitch Bend", "[mod]")
{
    SECTION("Channel 0 Bend Should Be a Global Bend")
    { // note that this test actually checks if channel 0 bends behave like non-MPE bends
        auto surge = surgeOnSine();
        surge->mpeEnabled = true;
        surge->storage.mpePitchBendRange = 48;

        surge->storage.getPatch().scene[0].pbrange_up.val.i = 2;
        surge->storage.getPatch().scene[0].pbrange_dn.val.i = 2;

        auto f60 = frequencyForNote(surge, 60);
        auto f62 = frequencyForNote(surge, 62);
        auto f58 = frequencyForNote(surge, 58);

        surge->pitchBend(0, 8192);
        auto f60bendUp = frequencyForNote(surge, 60);

        surge->pitchBend(0, -8192);
        auto f60bendDn = frequencyForNote(surge, 60);

        REQUIRE(f62 == Approx(f60bendUp).margin(0.3));
        REQUIRE(f58 == Approx(f60bendDn).margin(0.3));
    }

    SECTION("Channel n Bend Should Be a Correct Note Bend")
    {
        auto surge = surgeOnSine();
        surge->mpeEnabled = true;
        auto pbr = 48;
        auto sbs = 8192 * 1.f / pbr;

        surge->storage.mpePitchBendRange = pbr;

        surge->storage.getPatch().scene[0].pbrange_up.val.i = 2;
        surge->storage.getPatch().scene[0].pbrange_dn.val.i = 2;

        // Play on channel 1 which is now an MPE bend channel and send bends on that chan
        auto f60 = frequencyForNote(surge, 60, 2, 0, 1);
        auto f62 = frequencyForNote(surge, 62, 2, 0, 1);
        auto f58 = frequencyForNote(surge, 58, 2, 0, 1);

        // for MPE bend after the note starts to be the most accurate simulation
        auto noteFreqWithBend = [surge](int n, int b) {
            Surge::Headless::playerEvents_t events;

            Surge::Headless::Event on, off, bend;
            on.type = Surge::Headless::Event::NOTE_ON;
            on.channel = 1;
            on.data1 = n;
            on.data2 = 100;
            on.atSample = 100;

            off.type = Surge::Headless::Event::NOTE_OFF;
            off.channel = 1;
            off.data1 = n;
            off.data2 = 100;
            off.atSample = 44100 * 2 + 100;

            bend.type = Surge::Headless::Event::LAMBDA_EVENT;
            bend.surgeLambda = [b](std::shared_ptr<SurgeSynthesizer> s) { s->pitchBend(1, b); };
            bend.atSample = 400;

            events.push_back(on);
            events.push_back(bend);
            events.push_back(off);

            return frequencyForEvents(surge, events, 0, 4000, 44100 * 2 - 8000);
        };

        auto f60bendUp = noteFreqWithBend(60, 2 * sbs);
        auto f60bendDn = noteFreqWithBend(60, -2 * sbs);

        REQUIRE(f62 == Approx(f60bendUp).margin(0.3));
        REQUIRE(f58 == Approx(f60bendDn).margin(0.3));
    }
}

TEST_CASE("LFO Tempo Sync Drift in Latch Mode", "[mod]")
{
    SECTION("Latch Drift")
    {
        auto surge = Surge::Headless::createSurge(44100);

        int64_t bpm = 120;
        surge->time_data.tempo = bpm;

        REQUIRE(surge);

        auto lfo = std::make_unique<LFOModulationSource>();
        auto ss = std::make_unique<StepSequencerStorage>();
        auto lfostorage = &(surge->storage.getPatch().scene[0].lfo[0]);
        lfostorage->rate.temposync = true;
        SurgeSynthesizer::ID rid;
        surge->fromSynthSideId(lfostorage->rate.id, rid);
        surge->setParameter01(rid, 0.455068, false, false);
        lfostorage->shape.val.i = lt_square;

        surge->storage.getPatch().copy_scenedata(surge->storage.getPatch().scenedata[0],
                                                 surge->storage.getPatch().scenedataOrig[1], 0);

        lfo->assign(&(surge->storage), lfostorage, surge->storage.getPatch().scenedata[0], nullptr,
                    ss.get(), nullptr, nullptr);
        lfo->attack();
        lfo->process_block();

        float p = -1000;
        for (int64_t i = 0; i < 10000000; ++i)
        {
            if (lfo->get_output(0) > p)
            {
                double time = i * surge->storage.dsamplerate_inv * BLOCK_SIZE;
                double beats = time * bpm / 60;
                int bt2 = round(beats * 2);
                double drift = fabs(beats * 2 - bt2);
                // std::cout << time / 60.0 <<  " " << drift << std::endl;
            }
            p = lfo->get_output(0);
            lfo->process_block();
        }
    }
}

TEST_CASE("MIDI Controller Smoothing", "[mod]")
{
    SECTION("Legacy Mode")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        ControllerModulationSource a(Modulator::SmoothingMode::LEGACY);
        a.set_samplerate(surge->storage.samplerate, surge->storage.samplerate_inv);
        a.init(0.5f);
        REQUIRE(a.get_output(0) == 0.5f);
        float t = 0.6;
        a.set_target(t);
        float priorO = a.get_output(0);
        float dO = 100000;
        for (int i = 0; i < 100; ++i)
        {
            a.process_block();
            REQUIRE(t - a.get_output(0) < t - priorO);
            REQUIRE(a.get_output(0) - priorO < dO);
            dO = a.get_output(0) - priorO;
            priorO = a.get_output(0);
        }
    }

    SECTION("Fast Exponential Mode Gets There")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        ControllerModulationSource a(Modulator::SmoothingMode::FAST_EXP);
        a.set_samplerate(surge->storage.samplerate, surge->storage.samplerate_inv);
        a.init(0.5f);
        REQUIRE(a.get_output(0) == 0.5f);
        float t = 0.6;
        a.set_target(t);
        float priorO = a.get_output(0);
        float dO = 100000;
        for (int i = 0; i < 200; ++i)
        {
            a.process_block();
            REQUIRE(t - a.get_output(0) <= t - priorO);
            REQUIRE(((a.get_output(0) == t) || (a.get_output(0) - priorO < dO)));
            dO = a.get_output(0) - priorO;
            priorO = a.get_output(0);
        }
        REQUIRE(a.get_output(0) == t);
    }

    SECTION("Slow Exponential Mode Gets There Eventually")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        ControllerModulationSource a(Modulator::SmoothingMode::SLOW_EXP);
        a.set_samplerate(surge->storage.samplerate, surge->storage.samplerate_inv);
        a.init(0.5f);
        REQUIRE(a.get_output(0) == 0.5f);
        float t = 0.6;
        a.set_target(t);
        int idx = 0;
        while (a.get_output(0) != t && idx < 10000)
        {
            a.process_block();
            idx++;
        }
        REQUIRE(a.get_output(0) == t);
        REQUIRE(idx < 1000);
    }

    SECTION("Go As a Line")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        ControllerModulationSource a(Modulator::SmoothingMode::FAST_LINE);
        a.set_samplerate(surge->storage.samplerate, surge->storage.samplerate_inv);
        a.init(0.5f);
        for (int i = 0; i < 10; ++i)
        {
            float r = rand() / ((float)RAND_MAX);
            a.set_target(r);
            for (int j = 0; j < 60; ++j)
            {
                a.process_block();
            }
            REQUIRE(a.get_output(0) == r);
        }
    }

    SECTION("Direct Is Direct")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        ControllerModulationSource a(Modulator::SmoothingMode::DIRECT);
        a.set_samplerate(surge->storage.samplerate, surge->storage.samplerate_inv);
        a.init(0.5f);
        for (int i = 0; i < 100; ++i)
        {
            float r = rand() / ((float)RAND_MAX);
            a.set_target(r);
            a.process_block();
            REQUIRE(a.get_output(0) == r);
        }
    }
}

TEST_CASE("Keytrack Morph", "[mod]")
{
    INFO("See issue #3046");
    SECTION("Run High Key")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->loadPatchByPath("resources/test-data/patches/Keytrack-Morph-3046.fxp", -1, "Test");
        for (int i = 0; i < 100; ++i)
            surge->process();

        surge->playNote(0, 100, 127, 0);
        for (int i = 0; i < 10; ++i)
        {
            surge->process();
            /*
             * FIXME: Make this an assertive test. What we are really checking is, is l_shape 3.33
             * inside the oscillator? But there's no easy way to assert that so just leave the test
             * here as a debugging harness around issue
             * https://github.com/surge-synthesizer/surge/issues/3046
             */
        }
    }
}

TEST_CASE("Keytrack In Play Modes", "[mod]")
{
    /*
     * See issue 2892. In mono mode keytrack needs to follow held keys not just soloe playing voice
     */
    auto playSequence = [](std::shared_ptr<SurgeSynthesizer> surge, std::vector<int> notes,
                           bool mpe) {
        std::unordered_map<int, int> noteToChan;
        int cmpe = 1;
        for (auto n : notes)
        {
            int chan = 0;
            if (mpe)
            {
                if (n < 0)
                {
                    chan = noteToChan[-n];
                }
                else
                {
                    cmpe++;
                    if (cmpe > 15)
                        cmpe = 1;
                    noteToChan[n] = cmpe;
                    chan = cmpe;
                }
            }
            if (n > 0)
            {
                surge->playNote(chan, n, 127, 0);
            }
            else
            {
                surge->releaseNote(chan, -n, 0);
            }
            for (int i = 0; i < 10; ++i)
                surge->process();
        }
    };

    auto modes = {pm_poly, pm_mono, pm_mono_st, pm_mono_fp, pm_mono_st_fp};
    auto mpe = {false, true};
    for (auto mp : mpe)
    {
        for (auto m : modes)
        {
            auto cs = [m, mp]() {
                auto surge = Surge::Headless::createSurge(44100);
                surge->mpeEnabled = mp;
                surge->storage.getPatch().scene[0].polymode.val.i = m;
                surge->storage.getPatch().scene[0].keytrack_root.val.i = 60;
                return surge;
            };
            auto checkModes = [](std::shared_ptr<SurgeSynthesizer> surge, float low, float high,
                                 float latest) {
                REQUIRE(surge->storage.getPatch().scene[0].modsources[ms_lowest_key]->get_output(
                            0) == low);
                REQUIRE(surge->storage.getPatch().scene[0].modsources[ms_highest_key]->get_output(
                            0) == high);
                REQUIRE(surge->storage.getPatch().scene[0].modsources[ms_latest_key]->get_output(
                            0) == latest);
                return true;
            };
            DYNAMIC_SECTION("Keytrack Test Mode: " << m << " MPE: " << mp)
            {
                {
                    auto surge = cs();
                    REQUIRE(checkModes(surge, 0, 0, 0));
                }
                {
                    auto surge = cs();
                    playSequence(surge, {48}, mp);
                    checkModes(surge, -1, -1, -1);
                }
                {
                    auto surge = cs();
                    playSequence(surge, {48, -48}, mp);
                    checkModes(surge, -1, -1, -1); // This is the change in #3600 - keys are sticky
                }
                auto surge = cs();
                {
                    auto surge = cs();
                    playSequence(surge, {48, 84, 36, 72, -36}, mp);
                    checkModes(surge, -1, 2, 1);
                }

                {
                    /*
                     * This is the one which fails in mono mode with the naive just-voices
                     * implementation
                     */
                    auto surge = cs();
                    playSequence(surge, {48, 84, 72}, mp);
                    checkModes(surge, -1, 2, 1);
                }
            }
        }
    }
}

TEST_CASE("High/Low/Latest Note Modulators In All Modes", "[mod]")
{
    // See issue #3597
    auto setup = [](MonoVoicePriorityMode priomode, play_mode polymode) {
        auto surge = Surge::Headless::createSurge(44100);

        // Set synth to mono and low note priority
        surge->storage.getPatch().scene[0].polymode.val.i = polymode;
        surge->storage.getPatch().scene[0].monoVoicePriorityMode = priomode;

        // Assign highest note keytrack to any parameter. Lets do this with highest latest and
        // lowest
        surge->setModDepth01(surge->storage.getPatch().scene[0].osc[0].pitch.id, ms_highest_key, 0,
                             0, 0.2);
        surge->setModDepth01(surge->storage.getPatch().scene[0].osc[0].p[0].id, ms_latest_key, 0, 0,
                             0.2);
        surge->setModDepth01(surge->storage.getPatch().scene[0].osc[0].p[1].id, ms_lowest_key, 0, 0,
                             0.2);
        return surge;
    };

    auto rcmp = [](std::shared_ptr<SurgeSynthesizer> surge, int ch, int key, int vel, int low,
                   int high, int latest) {
        if (vel == 0)
            surge->releaseNote(ch, key, vel);
        else
            surge->playNote(ch, key, vel, 0);
        for (int i = 0; i < 20; ++i)
            surge->process();
        for (auto v : surge->voices[0])
            if (v->state.gate)
            {
                REQUIRE(v->modsources[ms_highest_key]->get_output(0) * 12 + 60 == high);
                REQUIRE(v->modsources[ms_latest_key]->get_output(0) * 12 + 60 == latest);
                REQUIRE(v->modsources[ms_lowest_key]->get_output(0) * 12 + 60 == low);
            }
    };

    std::map<MonoVoicePriorityMode, std::string> lab;
    lab[NOTE_ON_LATEST_RETRIGGER_HIGHEST] = "legacy";
    lab[ALWAYS_HIGHEST] = "always highest";
    lab[ALWAYS_LATEST] = "always latest";
    lab[ALWAYS_LOWEST] = "always lowest";

    for (auto mpemode : {true, false})
        for (auto polymode : {pm_mono_st, pm_poly, pm_mono, pm_mono_fp, pm_mono_st, pm_mono_st_fp})
            for (auto priomode :
                 {NOTE_ON_LATEST_RETRIGGER_HIGHEST, ALWAYS_LOWEST, ALWAYS_HIGHEST, ALWAYS_LATEST})
            {
                DYNAMIC_SECTION("Play Up Test " << lab[priomode] << " MPE: " << mpemode
                                                << " Play Mode: " << polymode)
                {
                    // From the issue: Steps to reproduce the behavior:
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    // press and hold three increasing notes
                    int ch = 0;

                    if (mpemode)
                        ch = 1;
                    rcmp(surge, ch, 60, 127, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 66, 127, 60, 66, 66);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 72, 127, 60, 72, 72);
                }
                DYNAMIC_SECTION("Play Down Test " << lab[priomode] << " MPE: " << mpemode
                                                  << " Play Mode: " << polymode)
                {
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    // press and hold three decreasing notes
                    int ch = 0;

                    if (mpemode)
                        ch = 1;
                    rcmp(surge, ch, 60, 127, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 54, 127, 54, 60, 54);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 48, 127, 48, 60, 48);
                }

                DYNAMIC_SECTION("Play V Test " << lab[priomode] << " MPE: " << mpemode
                                               << " Play Mode: " << polymode)
                {
                    // From the issue: Steps to reproduce the behavior:
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    // press and hold three intersecting notes
                    int ch = 0;

                    if (mpemode)
                        ch = 1;
                    rcmp(surge, ch, 60, 127, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 72, 127, 60, 72, 72);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 66, 127, 60, 72, 66);
                }

                DYNAMIC_SECTION("Releases One " << lab[priomode] << " MPE: " << mpemode
                                                << " Play Mode: " << polymode)
                {
                    // From the issue: Steps to reproduce the behavior:
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    // press and hold three intersecting notes
                    int ch = 0;

                    if (mpemode)
                        ch = 1;
                    rcmp(surge, ch, 60, 127, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 72, 127, 60, 72, 72);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 66, 127, 60, 72, 66);

                    if (mpemode)
                        ch--;
                    rcmp(surge, ch, 72, 0, 60, 66, 66);
                }

                DYNAMIC_SECTION("Releases One " << lab[priomode] << " MPE: " << mpemode
                                                << " Play Mode: " << polymode)
                {
                    // From the issue: Steps to reproduce the behavior:
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    // press and hold three intersecting notes
                    int ch = 0;

                    if (mpemode)
                        ch = 1;
                    rcmp(surge, ch, 60, 127, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 72, 127, 60, 72, 72);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 66, 127, 60, 72, 66);

                    // and unwind them
                    ch = 0;

                    if (mpemode)
                        ch = 1;
                    rcmp(surge, ch, 60, 0, 66, 72, 66);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 72, 0, 66, 66, 66);

                    if (mpemode)
                        ch++;
                    rcmp(surge, ch, 66, 0, 60, 60, 60); // 60 maps to 0
                }
            }
}

TEST_CASE("High/Low/Latest Keytrack With Key Splits", "[mod]")
{
    auto setup = [](MonoVoicePriorityMode priomode, play_mode polymode) {
        auto surge = Surge::Headless::createSurge(44100);

        // Set synth to mono and low note priority
        surge->storage.getPatch().scene[0].polymode.val.i = polymode;
        surge->storage.getPatch().scene[0].monoVoicePriorityMode = priomode;

        // Assign highest note keytrack to any parameter. Lets do this with highest latest and
        // lowest
        surge->setModDepth01(surge->storage.getPatch().scene[0].osc[0].pitch.id, ms_highest_key, 0,
                             0, 0.2);
        surge->setModDepth01(surge->storage.getPatch().scene[0].osc[0].p[0].id, ms_latest_key, 0, 0,
                             0.2);
        surge->setModDepth01(surge->storage.getPatch().scene[0].osc[0].p[1].id, ms_lowest_key, 0, 0,
                             0.2);

        surge->setModDepth01(surge->storage.getPatch().scene[1].osc[0].pitch.id, ms_highest_key, 0,
                             0, 0.2);
        surge->setModDepth01(surge->storage.getPatch().scene[1].osc[0].p[0].id, ms_latest_key, 0, 0,
                             0.2);
        surge->setModDepth01(surge->storage.getPatch().scene[1].osc[0].p[1].id, ms_lowest_key, 0, 0,
                             0.2);

        return surge;
    };

    auto play = [](std::shared_ptr<SurgeSynthesizer> surge, int ch, int key, int vel) {
        if (vel == 0)
            surge->releaseNote(ch, key, vel);
        else
            surge->playNote(ch, key, vel, 0);
        for (int i = 0; i < 50; ++i)
            surge->process();
    };
    auto compval = [](std::shared_ptr<SurgeSynthesizer> surge, int sc, int low, int high,
                      int latest) {
        for (auto v : surge->voices[sc])
            if (v->state.gate)
            {
                REQUIRE(v->modsources[ms_highest_key]->get_output(0) * 12 + 60 == high);
                REQUIRE(v->modsources[ms_latest_key]->get_output(0) * 12 + 60 == latest);
                REQUIRE(v->modsources[ms_lowest_key]->get_output(0) * 12 + 60 == low);
            }
    };
    for (auto mpemode : {true, false})
        for (auto polymode : {pm_mono_st, pm_poly, pm_mono, pm_mono_fp, pm_mono_st, pm_mono_st_fp})
            for (auto priomode :
                 {NOTE_ON_LATEST_RETRIGGER_HIGHEST, ALWAYS_LOWEST, ALWAYS_HIGHEST, ALWAYS_LATEST})
            {
                DYNAMIC_SECTION("DUAL " << priomode << " MPE: " << mpemode
                                        << " Play Mode: " << polymode)

                {
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    surge->storage.getPatch().scenemode.val.i = sm_dual;

                    int ch = 0;
                    if (mpemode)
                        ch = 1;

                    play(surge, ch, 60, 127);
                    compval(surge, 0, 60, 60, 60);
                    compval(surge, 1, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    play(surge, ch, 70, 127);
                    compval(surge, 0, 60, 70, 70);
                    compval(surge, 1, 60, 70, 70);

                    if (mpemode)
                        ch++;
                    play(surge, ch, 65, 127);
                    compval(surge, 0, 60, 70, 65);
                    compval(surge, 1, 60, 70, 65);
                }

                DYNAMIC_SECTION("Key Split " << priomode << " MPE: " << mpemode
                                             << " Play Mode: " << polymode)

                {
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    surge->storage.getPatch().scenemode.val.i = sm_split;
                    surge->storage.getPatch().splitpoint.val.i = 70;

                    int ch = 0;
                    if (mpemode)
                        ch = 1;

                    play(surge, ch, 50, 127);
                    compval(surge, 0, 50, 50, 50);
                    compval(surge, 1, 60, 60, 60);

                    if (mpemode)
                        ch++;
                    play(surge, ch, 90, 127);
                    compval(surge, 0, 50, 50, 50);
                    compval(surge, 1, 90, 90, 90);

                    if (mpemode)
                        ch++;
                    play(surge, ch, 69, 127);
                    compval(surge, 0, 50, 69, 69);
                    compval(surge, 1, 90, 90, 90);

                    if (mpemode)
                        ch++;
                    play(surge, ch, 70, 127);
                    compval(surge, 0, 50, 69, 69);
                    compval(surge, 1, 70, 90, 70);
                }

                DYNAMIC_SECTION("Channel Split " << priomode << " MPE: " << mpemode
                                                 << " Play Mode: " << polymode)

                {
                    auto surge = setup(priomode, polymode);
                    surge->mpeEnabled = mpemode;

                    surge->storage.getPatch().scenemode.val.i = sm_chsplit;
                    surge->storage.getPatch().splitpoint.val.i = 64;

                    int cha = 0;
                    int chb = 10;
                    if (mpemode)
                        cha = 1;

                    play(surge, cha, 50, 127);
                    compval(surge, 0, 50, 50, 50);
                    compval(surge, 1, 60, 60, 60);

                    play(surge, chb, 90, 127);
                    compval(surge, 0, 50, 50, 50);
                    compval(surge, 1, 90, 90, 90);

                    play(surge, cha + (mpemode ? 1 : 0), 69, 127);
                    compval(surge, 0, 50, 69, 69);
                    compval(surge, 1, 90, 90, 90);

                    play(surge, chb + (mpemode ? 1 : 0), 70, 127);
                    compval(surge, 0, 50, 69, 69);
                    compval(surge, 1, 70, 90, 70);
                }
            }
}

TEST_CASE("Mod LFO Is Well Behaved", "[mod]")
{
    for (int m = Surge::ModControl::mod_sine; m <= Surge::ModControl::mod_square; ++m)
    {
        DYNAMIC_SECTION("Mod Control Bounded For Type " << m)
        {
            for (int tries = 0; tries < 500; ++tries)
            {
                float rate = (float)rand() / (float)RAND_MAX * 10;
                float phase_offset = 0.0999 * (tries % 10);
                float depth = 1.0;

                INFO("200 Samples at " << rate << " with po " << phase_offset);
                Surge::ModControl mc(48000, 1.0 / 48000);
                for (int s = 0; s < 200; ++s)
                {
                    mc.pre_process(m, rate, depth, phase_offset);
                    REQUIRE(mc.value() <= 1.0);
                    REQUIRE(mc.value() >= -1.0);
                }
            }
        }
    }
}
