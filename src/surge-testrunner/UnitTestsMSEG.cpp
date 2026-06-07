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
#include "catch2/catch_amalgamated.hpp"
#include "MSEGModulationHelper.h"

using namespace Catch;

struct msegObservation
{
    msegObservation(int ip, float fp, float va)
    {
        iPhase = ip;
        fPhase = fp;
        phase = ip + fp;
        v = va;
    }
    int iPhase;
    float fPhase;
    float v;
    float phase;
};

std::vector<msegObservation> runMSEG(MSEGStorage *ms, float dPhase, float phaseMax,
                                     float deform = 0, float releaseAfter = -1)
{
    auto res = std::vector<msegObservation>();
    double phase = 0.0;
    int iphase = 0;
    Surge::MSEG::EvaluatorState es;

    while (phase + iphase < phaseMax)
    {
        bool release = false;
        if (releaseAfter >= 0 && phase + iphase >= releaseAfter)
            release = true;
        es.released = release;

        auto r = Surge::MSEG::valueAt(iphase, phase, deform, ms, &es, false);
        res.emplace_back(msegObservation(iphase, phase, r));
        phase += dPhase;
        if (phase > 1)
        {
            phase -= 1;
            iphase += 1;
        }
    }
    return res;
}

void resetCP(MSEGStorage *ms)
{
    Surge::MSEG::rebuildCache(ms);
    for (int i = 0; i < ms->n_activeSegments; ++i)
    {
        Surge::MSEG::resetControlPoint(ms, i);
    }
}

// ---------------------------------------------------------------------------
// Helper: 4-segment MSEG, each segment 0.25 wide, FREE endpoints, LOOP mode.
// Values ramp linearly: seg0 0→0.25, seg1 0.25→0.5, seg2 0.5→0.75, seg3 0.75→1.
// loop_start / loop_end left at -1 (full loop) unless the test overrides them.
// ---------------------------------------------------------------------------
static MSEGStorage makeRamp4()
{
    MSEGStorage ms;
    ms.n_activeSegments = 4;
    ms.loopMode = MSEGStorage::LoopMode::LOOP;
    ms.endpointMode = MSEGStorage::EndpointMode::FREE;

    for (int i = 0; i < 4; ++i)
    {
        ms.segments[i].duration = 0.25;
        ms.segments[i].type = MSEGStorage::segment::LINEAR;
        ms.segments[i].v0 = i * 0.25f;
        ms.segments[i].nv1 = (i + 1) * 0.25f;
    }

    ms.loop_start = -1;
    ms.loop_end = -1;

    resetCP(&ms);
    Surge::MSEG::rebuildCache(&ms);
    return ms;
}

/*
 * These tests test the relationship between configuration of MSEG Storage and the phase evaluator
 */
TEST_CASE("Basic MSEG Evaluation", "[mseg]")
{
    SECTION("Simple Square")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 4;
        ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
        ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 1.0;

        ms.segments[1].duration = MSEGStorage::minimumDuration;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = 0.5 - MSEGStorage::minimumDuration;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = -1.0;

        ms.segments[3].duration = MSEGStorage::minimumDuration;
        ms.segments[3].type = MSEGStorage::segment::LINEAR;
        ms.segments[3].v0 = -1.0;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so lets go ahead and run it at a variety of forward phases
        auto runIt = runMSEG(&ms, 0.0321, 5);
        for (auto c : runIt)
        {
            if (c.fPhase < 0.5 - MSEGStorage::minimumDuration)
                REQUIRE(c.v == 1);
            if (c.fPhase > 0.5 && c.fPhase < 1 - MSEGStorage::minimumDuration)
                REQUIRE(c.v == -1);
        }
    }

    SECTION("Longer Square")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 4;
        ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
        ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 1.0;

        ms.segments[1].duration = MSEGStorage::minimumDuration;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = 1.0 - MSEGStorage::minimumDuration;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = -1.0;

        ms.segments[3].duration = MSEGStorage::minimumDuration;
        ms.segments[3].type = MSEGStorage::segment::LINEAR;
        ms.segments[3].v0 = -1.0;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so lets go ahead and run it at a variety of forward phases
        auto runIt = runMSEG(&ms, 0.0321, 14);
        for (auto c : runIt)
        {
            auto dbphase = (int)(2 * c.phase) % 3;
            INFO("phase is " << c.phase << " " << dbphase);
            if (dbphase == 0)
                REQUIRE(c.v == 1);
            if (dbphase == 1 || dbphase == 2)
                REQUIRE(c.v == -1);
        }
    }
}

TEST_CASE("Unlocked Endpoints", "[mseg]")
{
    SECTION("Simple Square Default Locks")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 3;
        ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
        ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 1.0;

        ms.segments[1].duration = MSEGStorage::minimumDuration;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = 0.5;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = -1.0;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so lets go ahead and run it at a variety of forward phases
        auto runIt = runMSEG(&ms, 0.0321, 5);
        for (auto c : runIt)
        {
            if (c.fPhase < 0.5 - MSEGStorage::minimumDuration)
                REQUIRE(c.v == 1);
            if (c.fPhase > 0.5 && c.fPhase < 1 - MSEGStorage::minimumDuration)
                REQUIRE(c.v >= -1);
        }
    }

    SECTION("Free Endpoint Is a Square")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 3;
        ms.endpointMode = MSEGStorage::EndpointMode::FREE;
        ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 1.0;

        ms.segments[1].duration = MSEGStorage::minimumDuration;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = 0.5;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = -1.0;
        ms.segments[2].nv1 = -1.0; // The free mode will preserve this

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so lets go ahead and run it at a variety of forward phases
        auto runIt = runMSEG(&ms, 0.0321, 5);
        for (auto c : runIt)
        {
            if (c.fPhase < 0.5 - MSEGStorage::minimumDuration)
                REQUIRE(c.v == 1);
            if (c.fPhase > 0.5 && c.fPhase < 1 - MSEGStorage::minimumDuration)
                REQUIRE(c.v == -1);
        }
    }
}

TEST_CASE("Deform Per Segment", "[mseg]")
{
    SECTION("Triangle With Deform")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 2;
        ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
        ms.segments[0].duration = 0.5;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = -1.0;

        ms.segments[1].duration = 0.5;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so lets go ahead and run it at a variety of forward phases
        auto runIt = runMSEG(&ms, 0.0321, 3);
        for (auto c : runIt)
        {
            INFO("At " << c.fPhase << " " << c.phase << " " << c.v);
            if (c.fPhase < 0.5)
                REQUIRE(c.v == Approx(2 * c.fPhase / 0.5 - 1));
            if (c.fPhase > 0.5)
                REQUIRE(c.v == Approx(1 - 2 * (c.fPhase - 0.5) / 0.5));
        }

        auto runDef = runMSEG(&ms, 0.0321, 3, 0.9);
        for (auto c : runDef)
        {
            if (c.fPhase < 0.5 && c.v != -1)
                REQUIRE(c.v < 2 * c.fPhase / 0.5 - 1);
            if (c.fPhase > 0.5 && c.v != 1)
                REQUIRE(c.v > 1 - 2 * (c.fPhase - 0.5) / 0.5);
        }

        ms.segments[0].useDeform = false;
        auto runDefPartial = runMSEG(&ms, 0.0321, 3, 0.9);
        for (auto c : runDefPartial)
        {
            if (c.fPhase < 0.5 && c.v != -1)
                REQUIRE(c.v == Approx(2 * c.fPhase / 0.5 - 1));
            if (c.fPhase > 0.5 && c.v != 1)
                REQUIRE(c.v > 1 - 2 * (c.fPhase - 0.5) / 0.5);
        }
    }
}

TEST_CASE("Oneshot vs Loop", "[mseg]")
{
    SECTION("Triangle Loop")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 2;
        ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
        ms.segments[0].duration = 0.5;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = -1.0;

        ms.segments[1].duration = 0.5;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so lets go ahead and run it at a variety of forward phases
        auto runIt = runMSEG(&ms, 0.0321, 3);
        for (auto c : runIt)
        {
            if (c.fPhase < 0.5)
                REQUIRE(c.v == Approx(2 * c.fPhase / 0.5 - 1));
            if (c.fPhase > 0.5)
                REQUIRE(c.v == Approx(1 - 2 * (c.fPhase - 0.5) / 0.5));
        }

        ms.loopMode = MSEGStorage::LoopMode::ONESHOT;
        auto runOne = runMSEG(&ms, 0.0321, 3);
        for (auto c : runOne)
        {
            if (c.phase < 1)
            {
                INFO("At " << c.fPhase << " Value is " << c.v << " " << c.phase);
                if (c.fPhase < 0.5)
                    REQUIRE(c.v == Approx(2 * c.fPhase / 0.5 - 1));
                if (c.fPhase > 0.5)
                    REQUIRE(c.v == Approx(1 - 2 * (c.fPhase - 0.5) / 0.5));
            }
            else
            {
                REQUIRE(c.v == -1);
            }
        }
    }
}

TEST_CASE("Loop Test Cases", "[mseg]")
{
    SECTION("Non-Gated Loop Over a Ramp Pair")
    {
        // This is an attack up from -1 over 0.5, then a ramp in 0,1and we loop over the end
        MSEGStorage ms;
        ms.n_activeSegments = 5;
        ms.endpointMode = MSEGStorage::EndpointMode::FREE;
        ms.segments[0].duration = 0.5;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = -1.0;

        ms.segments[1].duration = 0.25;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = MSEGStorage::minimumDuration;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = 0.0;

        ms.segments[3].duration = 0.25;
        ms.segments[3].type = MSEGStorage::segment::LINEAR;
        ms.segments[3].v0 = 1.0;

        ms.segments[4].duration = MSEGStorage::minimumDuration;
        ms.segments[4].type = MSEGStorage::segment::LINEAR;
        ms.segments[4].v0 = 0.0;
        ms.segments[4].nv1 = 1.0;

        ms.loop_start = 1;
        ms.loop_end = 4;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        auto runOne = runMSEG(&ms, 0.0321, 3);
        for (auto c : runOne)
        {
            // make this more robust obviously but for now this will fail
            if (c.phase > 0.5)
            {
                float rampPhase = (c.phase - 0.5) * 4;
                rampPhase = rampPhase - (int)rampPhase;
                float rampValue = 1.0 - rampPhase;
                INFO("At " << c.phase << " " << rampPhase << " comparing with ramp");
                REQUIRE(c.v == Approx(rampValue));
            }
        }
    }

    SECTION("Gated Loop Over a Ramp")
    {
        // This is an attack up from -1 over 0.5, then a ramp in 0,1and we loop over the end
        MSEGStorage ms;
        ms.n_activeSegments = 5;
        ms.loopMode = MSEGStorage::LoopMode::GATED_LOOP;

        ms.endpointMode = MSEGStorage::EndpointMode::FREE;
        ms.segments[0].duration = 0.5;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = -1.0;

        ms.segments[1].duration = 0.25;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = MSEGStorage::minimumDuration;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = 0.0;

        ms.segments[3].duration = 0.25;
        ms.segments[3].type = MSEGStorage::segment::LINEAR;
        ms.segments[3].v0 = 1.0;

        ms.segments[4].duration = 0.25;
        ms.segments[4].type = MSEGStorage::segment::LINEAR;
        ms.segments[4].v0 = -1.0;
        ms.segments[4].nv1 = 0.0;

        ms.loop_start = 1;
        ms.loop_end = 2;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        auto runOne =
            runMSEG(&ms, 0.025, 3, 0,
                    1); /* pick a round dPhase so we hit releaseAfter exactly in this test */
        for (auto c : runOne)
        {
            // make this more robust obviously but for now this will fail
            if (c.phase > 0.5 && c.phase < 1.0)
            {
                float rampPhase = (c.phase - 0.5) * 4;
                rampPhase = rampPhase - (int)rampPhase;
                if (c.phase == 0.75)
                    rampPhase = 1; // Special case which side we pick
                float rampValue = 1.0 - rampPhase;
                INFO("At " << c.phase << " " << rampPhase << " comparing with ramp");
                REQUIRE(c.v == Approx(rampValue));
            }
            if (c.phase > 1.0)
            {
                if (c.phase > 1.50)
                {
                    // Past the end
                    REQUIRE(c.v == 0);
                }
                else if (c.phase > 1.25)
                {
                    // release segment 2 (segment 4) from -1 to 0 in 0.25
                    auto rampPhase = (c.phase - 1.25) * 4;
                    auto rampValue = -1.0 + rampPhase;
                    INFO("At " << c.phase << " and rampPhase " << rampPhase);
                    REQUIRE(c.v == Approx(rampValue).margin(1e-4));
                }
                else
                {
                    // release segment 1 but the ramp segment will be started at 0
                    // so we should be a ramp from 0 to -1, not from 1 to -1
                    // This is a bit fragile though since we run every .025 the release
                    // will not be when we've exactly hit 0 but rather when we are at
                    // 0.09999 still
                    auto rampPhase = (c.phase - 1.0) * 4;
                    auto rampValue = -rampPhase * 1.0999999 + 0.0999999;
                    INFO("In first envelope At " << c.phase << " and rampPhase " << rampPhase);
                    REQUIRE(c.v == Approx(rampValue).margin(1e-4));
                }
            }
        }
    }
}

TEST_CASE("Zero Size Loops", "[mseg]")
{
    SECTION("Zero Size Non Gated")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 3;
        ms.loopMode = MSEGStorage::LoopMode::LOOP;
        ms.endpointMode = MSEGStorage::EndpointMode::FREE;

        // A simple ADSR where the S is made by a size 0 loop after the decay
        ms.segments[0].duration = 0.25;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 0;

        ms.segments[1].duration = 0.25;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = 0.25;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = 0.75;
        ms.segments[2].nv1 = 0.0;

        ms.loop_start = 2;
        ms.loop_end = 1;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so this is loop so it should just push the 0.75 out the back after 0.5
        auto runOne =
            runMSEG(&ms, 0.025, 2, 0,
                    1); /* pick a round dPhase so we hit releaseAfter exactly in this test */
        for (auto c : runOne)
        {
            if (c.phase > 0.5)
                REQUIRE(c.v == 0.75);
        }
    }

    SECTION("Zero Size Gated")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 3;
        ms.loopMode = MSEGStorage::LoopMode::GATED_LOOP;
        ms.endpointMode = MSEGStorage::EndpointMode::FREE;

        // A simple ADSR where the S is made by a size 0 loop after the decay
        ms.segments[0].duration = 0.25;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 0;

        ms.segments[1].duration = 0.25;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 1.0;

        ms.segments[2].duration = 0.25;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = 0.75;
        ms.segments[2].nv1 = 0.0;

        ms.loop_start = 2;
        ms.loop_end = 1;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // OK so this is loop so it should just push the 0.75 out the back after 0.5
        auto runOne =
            runMSEG(&ms, 0.025, 2, 0,
                    1); /* pick a round dPhase so we hit releaseAfter exactly in this test */
        for (auto c : runOne)
        {
            if (c.phase > 0.5 && c.phase <= 1.0)
                REQUIRE(c.v == 0.75);
            if (c.phase > 1.0 && c.phase <= 1.25)
            {
                auto rp = (c.phase - 1.0) * 4;
                REQUIRE(c.v == Approx(0.75 * (1.0 - rp)).margin(1e-4));
            }
            if (c.phase > 1.25)
                REQUIRE(c.v == 0);
        }
    }
}

TEST_CASE("Loop Start At Zero", "[mseg]")
{
    SECTION("Full loop with loop_start=0 loops the whole MSEG")
    {
        // loop_start=0, loop_end=3 is the same as both being -1 (default full
        // loop), but setting them explicitly exercises the non-default path.
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 0;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // Run for 3 cycles. The ramp is v = fracPhase so we can check midpoints.
        auto run = runMSEG(&ms, 0.025, 3);
        for (auto c : run)
        {
            float frac = c.phase - (int)c.phase;
            // Check a few mid-segment points where the linear ramp is unambiguous.
            if (frac > 0.05f && frac < 0.20f)
            {
                INFO("phase=" << c.phase << " frac=" << frac << " v=" << c.v);
                REQUIRE(c.v == Approx(frac).margin(0.03f));
            }
            if (frac > 0.55f && frac < 0.70f)
            {
                INFO("phase=" << c.phase << " frac=" << frac << " v=" << c.v);
                REQUIRE(c.v == Approx(frac).margin(0.03f));
            }
        }
    }

    SECTION("Single-segment loop at index 0 never plays later segments")
    {
        MSEGStorage ms;
        ms.n_activeSegments = 3;
        ms.loopMode = MSEGStorage::LoopMode::LOOP;
        ms.endpointMode = MSEGStorage::EndpointMode::FREE;

        // seg0: ramp 0→1. rebuildCache sets nv1 = seg1.v0, so seg1.v0 must be 1
        // to get the intended 0→1 ramp; we set it explicitly here.
        ms.segments[0].duration = 0.25;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = 0.0f;

        // seg1, seg2: v0=1 so that rebuildCache writes seg0.nv1=1 (correct ramp).
        // These segments should never be reached while the loop holds.
        for (int i = 1; i < 3; ++i)
        {
            ms.segments[i].duration = 0.25;
            ms.segments[i].type = MSEGStorage::segment::LINEAR;
            ms.segments[i].v0 = 1.0f;
            ms.segments[i].nv1 = 1.0f;
        }

        ms.loop_start = 0;
        ms.loop_end = 0;

        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        // seg0 ramps 0→1, so all values must stay in [0, 1].
        auto run = runMSEG(&ms, 0.025, 2);
        for (auto c : run)
        {
            INFO("phase=" << c.phase << " v=" << c.v);
            REQUIRE(c.v >= -0.01f);
            REQUIRE(c.v <= 1.01f);
        }
    }
}

TEST_CASE("Manipulation Routines", "[mseg]")
{
    // --- insert ---

    SECTION("Insert before loop shifts both loop indices up")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 2;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // insertBefore(t=0.125) → insertAtIndex(0).
        // loop_start 2 >= 0 → 3; loop_end 3 >= -1 → 4.
        Surge::MSEG::insertBefore(&ms, 0.125f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 5);
        REQUIRE(ms.loop_start == 3);
        REQUIRE(ms.loop_end == 4);
    }

    SECTION("Insert after loop end does not shift loop indices")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 0;
        ms.loop_end = 1;
        Surge::MSEG::rebuildCache(&ms);

        // insertAfter(t=0.875) → insertAtIndex(4) = append.
        // loop_start 0, loop_end 1 are both before the insertion point.
        Surge::MSEG::insertAfter(&ms, 0.875f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 5);
        REQUIRE(ms.loop_start == 0);
        REQUIRE(ms.loop_end == 1);
    }

    SECTION("Insert inside loop region shifts loop end but not loop start")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 1;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // insertBefore(t=0.5) → insertAtIndex(2).
        // loop_start 1 < 2: unchanged.
        // loop_end 3 >= 1 (insertIndex - 1): → 4.
        Surge::MSEG::insertBefore(&ms, 0.5f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 5);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 4);
    }

    // --- delete ---

    SECTION("Delete before loop shifts both loop indices down")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 2;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // Delete idx 0 (before loop).
        // loop_start 2 > 0 → 1; loop_end 3 >= 0 → 2.
        Surge::MSEG::deleteSegment(&ms, 0);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 2);
    }

    SECTION("Delete after loop end does not change loop indices")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 0;
        ms.loop_end = 1;
        Surge::MSEG::rebuildCache(&ms);

        // Delete idx 3 (after loop).
        // loop_start 0, loop_end 1 are both < 3.
        Surge::MSEG::deleteSegment(&ms, 3);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 0);
        REQUIRE(ms.loop_end == 1);
    }

    SECTION("Delete loop start segment: loop_start unchanged, loop_end shifts")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 1;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // Delete idx 1 (== loop_start).
        // loop_start: condition is > idx (not >=), so 1 is NOT > 1: unchanged.
        // loop_end 3 >= 1 → 2.
        Surge::MSEG::deleteSegment(&ms, 1);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 2);
    }

    // --- split ---

    SECTION("Split before loop shifts both loop indices up")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 2;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // splitSegment(t=0.125) → internally insertAtIndex(1).
        // loop_start 2 >= 1 → 3; loop_end 3 >= 0 → 4.
        Surge::MSEG::splitSegment(&ms, 0.125f, 0.1f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 5);
        REQUIRE(ms.loop_start == 3);
        REQUIRE(ms.loop_end == 4);
    }

    SECTION("Split inside loop shifts loop end but not loop start")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 1;
        ms.loop_end = 2;
        Surge::MSEG::rebuildCache(&ms);

        // splitSegment(t=0.375) → insertAtIndex(2).
        // loop_start 1 < 2: unchanged. loop_end 2 >= 1 → 3.
        Surge::MSEG::splitSegment(&ms, 0.375f, 0.3f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 5);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 3);
    }

    // --- unsplit ---

    SECTION("Unsplit before loop shifts both loop indices down")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 2;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // unsplitSegment(t=0.25) merges seg0+seg1, deletes idx 1.
        // loop_start 2 > 1 → 1; loop_end 3 >= 1 → 2.
        Surge::MSEG::unsplitSegment(&ms, 0.25f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 2);
    }

    SECTION("Unsplit inside loop shifts loop end but not loop start")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 1;
        ms.loop_end = 3;
        Surge::MSEG::rebuildCache(&ms);

        // unsplitSegment(t=0.625) merges seg2+seg3, deletes idx 3.
        // loop_start 1, not > 3: unchanged. loop_end 3 >= 3 → 2.
        Surge::MSEG::unsplitSegment(&ms, 0.625f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 2);
    }

    SECTION("Unsplit after loop end does not change loop indices")
    {
        MSEGStorage ms = makeRamp4();
        ms.loop_start = 0;
        ms.loop_end = 1;
        Surge::MSEG::rebuildCache(&ms);

        // unsplitSegment(t=0.75) merges seg2+seg3, deletes idx 3.
        // loop_start 0, loop_end 1: both < 3, unchanged.
        Surge::MSEG::unsplitSegment(&ms, 0.75f);
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 0);
        REQUIRE(ms.loop_end == 1);
    }
}

TEST_CASE("Manipulation With Loop Preservation Playback", "[mseg]")
{
    SECTION("Insert outside loop does not disturb loop playback")
    {
        // 3-segment MSEG: seg0 is a pre-attack, segs 1+2 form a 0→1→0 loop.
        auto build = []() {
            MSEGStorage ms;
            ms.n_activeSegments = 3;
            ms.loopMode = MSEGStorage::LoopMode::LOOP;
            ms.endpointMode = MSEGStorage::EndpointMode::FREE;

            ms.segments[0].duration = 0.5f;
            ms.segments[0].type = MSEGStorage::segment::LINEAR;
            ms.segments[0].v0 = -1.0f;
            ms.segments[0].nv1 = 0.0f;

            ms.segments[1].duration = 0.25f;
            ms.segments[1].type = MSEGStorage::segment::LINEAR;
            ms.segments[1].v0 = 0.0f;
            ms.segments[1].nv1 = 1.0f;

            ms.segments[2].duration = 0.25f;
            ms.segments[2].type = MSEGStorage::segment::LINEAR;
            ms.segments[2].v0 = 1.0f;
            ms.segments[2].nv1 = 0.0f;

            ms.loop_start = 1;
            ms.loop_end = 2;
            resetCP(&ms);
            Surge::MSEG::rebuildCache(&ms);
            return ms;
        };

        // Reference: run as-is and capture the steady-state loop values.
        auto msRef = build();
        auto refRun = runMSEG(&msRef, 0.025f, 4);

        // Modified: insert a segment before the loop, then run.
        auto msMod = build();
        Surge::MSEG::insertBefore(&msMod, 0.1f); // inserts at idx 0, pre-loop
        Surge::MSEG::rebuildCache(&msMod);

        REQUIRE(msMod.loop_start == 2);
        REQUIRE(msMod.loop_end == 3);

        auto modRun = runMSEG(&msMod, 0.025f, 4);

        // The loop region is 0→1→0 in both cases. Once we're past the
        // pre-loop section, values must stay non-negative (loop never dips < 0).
        for (auto c : modRun)
        {
            float loopStart = msMod.segmentStart[msMod.loop_start];
            if (c.phase > loopStart + 0.05f)
            {
                INFO("phase=" << c.phase << " v=" << c.v);
                REQUIRE(c.v >= -0.01f);
                REQUIRE(c.v <= 1.01f);
            }
        }
    }

    SECTION("Delete outside loop does not disturb loop playback")
    {
        // 4-segment MSEG: seg0 pre-loop, segs 1+2 looped (0→1→0), seg3 post-loop.
        MSEGStorage ms;
        ms.n_activeSegments = 4;
        ms.loopMode = MSEGStorage::LoopMode::LOOP;
        ms.endpointMode = MSEGStorage::EndpointMode::FREE;

        ms.segments[0].duration = 0.25f;
        ms.segments[0].type = MSEGStorage::segment::LINEAR;
        ms.segments[0].v0 = -1.0f;
        ms.segments[0].nv1 = 0.0f;

        ms.segments[1].duration = 0.25f;
        ms.segments[1].type = MSEGStorage::segment::LINEAR;
        ms.segments[1].v0 = 0.0f;
        ms.segments[1].nv1 = 1.0f;

        ms.segments[2].duration = 0.25f;
        ms.segments[2].type = MSEGStorage::segment::LINEAR;
        ms.segments[2].v0 = 1.0f;
        ms.segments[2].nv1 = 0.0f;

        ms.segments[3].duration = 0.25f;
        ms.segments[3].type = MSEGStorage::segment::LINEAR;
        ms.segments[3].v0 = 0.0f;
        ms.segments[3].nv1 = 0.0f;

        ms.loop_start = 1;
        ms.loop_end = 2;
        resetCP(&ms);
        Surge::MSEG::rebuildCache(&ms);

        Surge::MSEG::deleteSegment(&ms, 3); // remove post-loop segment
        Surge::MSEG::rebuildCache(&ms);

        REQUIRE(ms.n_activeSegments == 3);
        REQUIRE(ms.loop_start == 1);
        REQUIRE(ms.loop_end == 2);

        // Loop region (segs 1+2) is 0→1→0, always non-negative.
        auto run = runMSEG(&ms, 0.025f, 3);
        for (auto c : run)
        {
            if (c.phase > ms.segmentStart[ms.loop_start] + 0.05f)
            {
                INFO("phase=" << c.phase << " v=" << c.v);
                REQUIRE(c.v >= -0.01f);
                REQUIRE(c.v <= 1.01f);
            }
        }
    }
}
