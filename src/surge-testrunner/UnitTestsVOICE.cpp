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

using namespace Surge::Test;

TEST_CASE("Release by Note ID", "[voice]")
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
        REQUIRE(voicecount() == 2);

        s->playNote(0, 64, 127, 0, 177);
        proc();
        REQUIRE(voicecount() == 4);

        s->releaseNoteByHostNoteID(173, 0);
        proc();
        REQUIRE(voicecount() == 2);

        s->releaseNoteByHostNoteID(177, 0);
        proc();
        REQUIRE(voicecount() == 0);
    }

    for (auto pm : {pm_poly, pm_mono, pm_mono_fp, pm_mono_st, pm_mono_st_fp})
    {
        DYNAMIC_SECTION("PlayMode Sine Case " << pm << " " << play_mode_names[pm])
        {
            bool recycleNoteId = (pm == pm_mono_st || pm == pm_mono_st_fp);
            auto s = surgeOnSine();
            s->storage.getPatch().scene[0].polymode.val.i = pm;
            s->process();

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

            std::cout << "A" << std::endl;
            for (auto v : s->voices[0])
            {
                std::cout << v->host_note_id << " " << v->state.key << " "
                          << v->originating_host_key << std::endl;
            }

            s->playNote(0, 64, 127, 0, 177);
            proc();
            REQUIRE(voicecount() == (pm == pm_poly ? 2 : 1));
            std::cout << "B" << std::endl;
            for (auto v : s->voices[0])
            {
                std::cout << v->host_note_id << " " << v->state.key << " "
                          << v->originating_host_key << std::endl;
            }

            std::cout << "FIRST" << std::endl;
            s->releaseNoteByHostNoteID(173, 0);
            proc();
            REQUIRE(voicecount() == (recycleNoteId ? 0 : 1));

            std::cout << "SECOND" << std::endl;
            s->releaseNoteByHostNoteID(177, 0);
            proc();
            REQUIRE(voicecount() == 0);
        }
    }
}

TEST_CASE("Play and release Frequency and Note ID", "[voice]")
{
    using cmd = std::tuple<bool, float, int, int, int>;
    using tcase = std::tuple<std::string, int, bool, std::vector<cmd>>;

    // clang-format off
    // This is a list of (mode) (byKeyOrFreq) and actions
    // an action is play-or-release then key, channel, nid, exp
    // exp is the expected voice count after the action.
    auto data = std::vector<tcase>{
        {"Simple Poly", pm_poly, true,
         {
             {true, 60, 0, 184, 1},
             {false, -1, -1, 184, 0}
         }
        },
        {"Two Poly", pm_poly, true,
         {
             {true, 60, 0, 184, 1},
             {true, 65, 0, 187, 2},
             {false, -1, -1, 187, 1},
             {false, -1, -1, 184, 0}
         }
        },
        {"Two Poly Order Swapped", pm_poly, true,
         {
             {true, 60, 0, 184, 1},
             {true, 65, 0, 187, 2},
             {false, -1, -1, 184, 1},
             {false, -1, -1, 187, 0}
         }
        },
        // This are the cases that led to this test.
        // in mono_st you recycle voice ids
        {"Mono ST", pm_mono_st, true,
         {
             {true, 60, 0, 2421, 1},
             {true, 65, 0, 9324, 1}, // but 9324 is gone and we are mono
             {false, -1, -1, 9324, 1}, // so killing it doesnt matter
             {false, -1, -1, 2421, 0}
         }
        },
        {"Mono ST Order Swapped", pm_mono_st, true,
         {
             {true, 60, 0, 2421, 1},
             {true, 65, 0, 9324, 1}, // but 9324 is gone and we are mono
             {false, -1, -1, 2421, 0}, // so killing the lead voice silences us
             {false, -1, -1, 9324, 0} // and this is a no-op
         }
        },
        {"Mono FP", pm_mono_fp, false,
         {
             {true, 440, 0, 23, 1},
             {true, 550, 0, 24, 1},
             {false, -1, -1, 23, 1},
             {false, -1, -1, 24, 0}
         }
        },


        {"Mono FP Swap Order", pm_mono_fp, false,
         {
             {true, 440, 0, 23, 1},
             {true, 550, 0, 24, 1},
             {false, -1, -1, 24, 1},
             {false, -1, -1, 23, 0}
         }
        },
    };
    // clang-format on

    for (auto &d : data)
    {
        DYNAMIC_SECTION("Test Case: " << std::get<0>(d))
        {
            auto mode = std::get<1>(d);
            auto byNote = std::get<2>(d);

            auto s = surgeOnSine();
            s->storage.getPatch().scene[0].polymode.val.i = mode;

            // Process enough blocks to allow things to develop
            auto proc = [&s]() {
                for (int i = 0; i < 17; ++i)
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

            auto cmd = std::get<3>(d);
            for (auto &c : cmd)
            {
                auto [play, keyOrFreq, channel, nid, expected] = c;
                INFO("CMD " << play << " " << keyOrFreq << " " << nid);
                if (play)
                {
                    if (byNote)
                    {
                        s->playNote(channel, keyOrFreq, 90, 0, nid);
                    }
                    else
                    {
                        s->playNoteByFrequency(keyOrFreq, 90, nid);
                    }
                }
                else
                {
                    s->releaseNoteByHostNoteID(nid, 0);
                }
                proc();
                if (voicecount() != expected)
                {
                    for (auto sc = 0; sc < n_scenes; ++sc)
                    {
                        for (const auto &v : s->voices[sc])
                        {
                            std::cout << v << " " << v->state.gate << " " << v->state.key << " "
                                      << v->host_note_id << std::endl;
                        }
                    }
                }
                REQUIRE(voicecount() == expected);
            }
        }
    }
}
