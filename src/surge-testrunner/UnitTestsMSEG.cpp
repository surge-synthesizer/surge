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

/*
 * Tests to add
 * - loop point 0 (start = end + 1)
 * - loop start at 0
 * - manipulation routines
 *    - insert
 *    - delete
 *    - split
 *    - unsplit
 *  - each of those with loop preservation also
 */
