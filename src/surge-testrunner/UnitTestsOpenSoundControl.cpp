/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

using namespace Surge::Test;

TEST_CASE("OSC parsing", "[OpenSoundControl]")
{
    SECTION("Simple Sine Case")
    {
        auto s = surgeOnSine();

        auto proc = [&s]() {
            for (int i = 0; i < 5; ++i)
            {
                s->process();
            }
        };

        auto voicecount = [&s]() -> int {
            int res{0};
            for (auto sc = 0; sc < n_scenes; ++sc)
            {
                for (const auto &v : s->voices[sc])
                {
                    if (v->state.gate)
                        res++;
                }
            }
            return res;
        };

        proc();

        s->playNote(0, 60, 127, 0, 173);
        proc();
        REQUIRE(voicecount() == 1);

        s->playNote(0, 64, 127, 0, 177);
        proc();
        REQUIRE(voicecount() == 2);

        s->releaseNoteByHostNoteID(173, 0);
        proc();
        REQUIRE(voicecount() == 1);

        s->releaseNoteByHostNoteID(177, 0);
        proc();
        REQUIRE(voicecount() == 0);
    }

    SECTION("Playmode Dual Sine Case")
    {
        auto s = surgeOnSine();
        s->storage.getPatch().scenemode.val.i = sm_dual;

        auto proc = [&s]() { // Why is this called 5 times?
            for (int i = 0; i < 5; ++i)
            {
                s->process();
            }
        };

        auto voicecount = [&s]() -> int {
            int res{0};
            for (auto sc = 0; sc < n_scenes; ++sc)
            {
                for (const auto &v : s->voices[sc])
                {
                    if (v->state.gate)
                        res++;
                }
            }
            return res;
        };

        proc();
    }
}