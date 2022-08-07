#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE("Channel Split Routes on Channel", "[midi]")
{
    auto surge = std::shared_ptr<SurgeSynthesizer>(Surge::Headless::createSurge(44100));
    REQUIRE(surge);
    REQUIRE(surge->loadPatchByPath("resources/test-data/patches/ChannelSplit-Sin-2OctaveB.fxp", -1,
                                   "Test"));

    SECTION("Regular (non-MPE)")
    {
        surge->mpeEnabled = false;
        for (auto splitChan = 2; splitChan < 14; splitChan++)
        {
            auto smc = splitChan * 8;
            surge->storage.getPatch().splitpoint.val.i = smc;
            for (auto mc = 0; mc < 16; ++mc)
            {
                auto fr = frequencyForNote(surge, 69, 1, 0, mc);
                auto targetfr = mc <= splitChan ? 440 : 440 * 4;
                REQUIRE(fr == Approx(targetfr).margin(0.1));
            }
        }
    }

    SECTION("MPE Enabled")
    {
        surge->mpeEnabled = true;
        for (auto splitChan = 2; splitChan < 14; splitChan++)
        {
            auto smc = splitChan * 8;
            surge->storage.getPatch().splitpoint.val.i = smc;
            for (auto mc = 0; mc < 16; ++mc)
            {
                auto fr = frequencyForNote(surge, 69, 1, 0, mc);
                auto targetfr = mc <= splitChan ? 440 : 440 * 4;
                REQUIRE(fr == Approx(targetfr).margin(0.1));
            }
        }
    }
}

TEST_CASE("Duplicate Note Channel Management Issue 3084", "[midi]")
{
    SECTION("MPE Notes quickly off")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->mpeEnabled = true;
        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->playNote(1, 60, 127, 0);
        surge->process();
        int ct = 0;
        for (auto v : surge->voices[0])
        {
            REQUIRE(ct == 0);
            REQUIRE(v->state.channel == 1);
            REQUIRE(v->state.key == 60);
            REQUIRE(v->state.gate);
            ct++;
        }

        surge->playNote(2, 60, 127, 0);
        surge->process();

        REQUIRE(surge->voices->size() == 2);
        int ch = 1;
        for (auto v : surge->voices[0])
        {
            REQUIRE(v->state.channel == ch);
            REQUIRE(v->state.key == 60);
            REQUIRE(v->state.gate);
            ch++;
        }

        surge->releaseNote(2, 60, 0);
        surge->process();
        REQUIRE(surge->voices->size() == 2);
        ch = 1;
        for (auto v : surge->voices[0])
        {
            REQUIRE(v->state.channel == ch);
            REQUIRE(v->state.key == 60);
            REQUIRE(v->state.gate == (ch == 1));
            ch++;
        }
    }

    SECTION("MPE Notes absent process")
    {
        // Basically the same test just without a call to process between key modifications
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->mpeEnabled = true;
        for (int i = 0; i < 5; ++i)
            surge->process();

        surge->playNote(1, 60, 127, 0);
        int ct = 0;
        for (auto v : surge->voices[0])
        {
            REQUIRE(ct == 0);
            REQUIRE(v->state.channel == 1);
            REQUIRE(v->state.key == 60);
            REQUIRE(v->state.gate);
            ct++;
        }

        surge->playNote(2, 60, 127, 0);

        REQUIRE(surge->voices->size() == 2);
        int ch = 1;
        for (auto v : surge->voices[0])
        {
            REQUIRE(v->state.channel == ch);
            REQUIRE(v->state.key == 60);
            REQUIRE(v->state.gate);
            ch++;
        }

        surge->releaseNote(2, 60, 0);
        REQUIRE(surge->voices->size() == 2);
        ch = 1;
        for (auto v : surge->voices[0])
        {
            REQUIRE(v->state.channel == ch);
            REQUIRE(v->state.key == 60);
            REQUIRE(v->state.gate == (ch == 1));
            ch++;
        }
    }
}

TEST_CASE("Sustain Pedal and Mono", "[midi]") // #1459
{
    auto playingNoteCount = [](std::shared_ptr<SurgeSynthesizer> surge) {
        int ct = 0;
        for (auto v : surge->voices[0])
        {
            if (v->state.gate)
                ct++;
        }
        return ct;
    };
    auto solePlayingNote = [](std::shared_ptr<SurgeSynthesizer> surge) {
        int ct = 0;
        int res = -1;
        for (auto v : surge->voices[0])
        {
            if (v->state.gate)
            {
                ct++;
                res = v->state.key;
            }
        }
        REQUIRE(ct == 1);
        return res;
    };
    SECTION("No Pedal Mono")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
        auto step = [surge]() {
            for (int i = 0; i < 25; ++i)
                surge->process();
        };
        step();

        // Press 4 chromatic keys down
        for (int n = 60; n < 64; ++n)
        {
            surge->playNote(0, n, 120, 0);
            step();
            REQUIRE(solePlayingNote(surge) == n);

            for (int i = 0; i < 128; ++i)
            {
                int xp = (i >= 60 && i <= n) ? 120 : 0;
                REQUIRE(surge->channelState[0].keyState[i].keystate == xp);
            }
        }

        // And remove them in reverse order
        for (int n = 63; n >= 60; --n)
        {
            surge->releaseNote(0, n, 120);
            step();

            if (n == 60)
                REQUIRE(playingNoteCount(surge) == 0);
            else
                REQUIRE(solePlayingNote(surge) == n - 1);

            for (int i = 0; i < 128; ++i)
            {
                int xp = (i >= 60 && i < n) ? 120 : 0;
                REQUIRE(surge->channelState[0].keyState[i].keystate == xp);
            }
        }
    }

    SECTION("Release to Highest by Default")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
        surge->storage.getPatch().scene[0].monoVoicePriorityMode =
            NOTE_ON_LATEST_RETRIGGER_HIGHEST; // Legacy mode
        auto step = [surge]() {
            for (int i = 0; i < 25; ++i)
                surge->process();
        };
        step();

        // In order play 60 66 64 65 62
        surge->playNote(0, 60, 120, 0);
        step();
        surge->playNote(0, 66, 120, 0);
        step();
        surge->playNote(0, 64, 120, 0);
        step();
        surge->playNote(0, 65, 120, 0);
        step();
        surge->playNote(0, 62, 120, 0);
        step();

        // So note 62 should be playing
        REQUIRE(solePlayingNote(surge) == 62);

        // Now release 64 and that should be unch
        surge->releaseNote(0, 64, 0);
        step();
        REQUIRE(solePlayingNote(surge) == 62);

        // And release 62 and 66 should play because we have highest note semantics
        surge->releaseNote(0, 62, 0);
        step();
        REQUIRE(solePlayingNote(surge) == 66);
    }

    SECTION("Pedal Holds Last Played")
    {
        auto step = [](auto surge) {
            for (int i = 0; i < 25; ++i)
                surge->process();
        };
        auto testInSurge = [](auto kernel) {
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);
            surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
            kernel(surge);
        };

        int susp = 64;
        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Note On and Off");
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Pedal Before Note On and Off");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - use pedal play and release
            INFO("Single Pedal After Note On");
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Note Release with note held. This is the one Evil wants changed.");
            surge->playNote(0, 48, 127, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60); // We want a mode where this is 48
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 48);
            surge->releaseNote(0, 48, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Under-Note Release with note held. This is the one Evil wants changed.");
            surge->playNote(0, 48, 127, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60); // We want a mode where this is 48
            surge->releaseNote(0, 48, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60); // and in that mode this would stay 48
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Under-Note Release with pedan on.");
            surge->playNote(0, 48, 127, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 48, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });
    }

    SECTION("Pedal Mode Release")
    {
        auto step = [](auto surge) {
            for (int i = 0; i < 25; ++i)
                surge->process();
        };
        auto testInSurge = [](auto kernel) {
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);
            surge->storage.monoPedalMode = RELEASE_IF_OTHERS_HELD;
            surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
            kernel(surge);
        };

        int susp = 64;
        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Note On and Off");
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Pedal Before Note On and Off");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - use pedal play and release
            INFO("Single Pedal After Note On");
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Note Release with note held. This is the one Evil wants changed.");
            surge->playNote(0, 48, 127, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 48); // This is the difference in modes
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 48);
            surge->releaseNote(0, 48, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Under-Note Release with note held. This is the one Evil wants changed.");
            surge->playNote(0, 48, 127, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 48); // We want a mode where this is 48
            surge->releaseNote(0, 48, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 48); // and in that mode this would stay 48
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Under-Note Release with pedan on.");
            surge->playNote(0, 48, 127, 0);
            step(surge);
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 48, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(solePlayingNote(surge) == 60);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingNoteCount(surge) == 0);
        });
    }
}

TEST_CASE("Mono Voice Priority Modes", "[midi]")
{
    struct TestCase
    {
        TestCase(std::string lab, int pm, bool mp, MonoVoicePriorityMode m)
            : name(lab), polymode(pm), mpe(mp), mode(m)
        {
        }
        std::vector<std::pair<int, bool>> onoffs;
        int polymode = pm_mono;
        bool mpe = false;
        std::string name = "";
        MonoVoicePriorityMode mode = NOTE_ON_LATEST_RETRIGGER_HIGHEST;
        void on(int n) { onoffs.push_back(std::make_pair(n, true)); }
        void off(int n) { onoffs.push_back(std::make_pair(n, false)); }
    };

    auto step = [](auto surge) {
        for (int i = 0; i < 25; ++i)
            surge->process();
    };
    auto testInSurge = [](TestCase &c) {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->storage.getPatch().scene[0].monoVoicePriorityMode = c.mode;
        surge->mpeEnabled = c.mpe;

        std::map<int, int> channelAssignments;
        int nxtch = 1;

        surge->storage.getPatch().scene[0].polymode.val.i = c.polymode;
        for (auto n : c.onoffs)
        {
            int channel = 0;
            // Implement a simple channel increase strategy
            if (c.mpe)
            {
                if (n.second)
                {
                    channelAssignments[n.first] = nxtch++;
                }
                channel = channelAssignments[n.first];
                if (nxtch > 16)
                    nxtch = 1;
                if (!n.second)
                {
                    channelAssignments[n.first] = -1;
                }
            }
            if (n.second)
            {
                surge->playNote(channel, n.first, 127, 0);
            }
            else
            {
                surge->releaseNote(channel, n.first, 0);
            }
        }
        return surge;
    };

    auto playingNoteCount = [](std::shared_ptr<SurgeSynthesizer> surge) {
        int ct = 0;
        for (auto v : surge->voices[0])
        {
            if (v->state.gate)
                ct++;
        }
        return ct;
    };

    auto solePlayingNote = [](std::shared_ptr<SurgeSynthesizer> surge) {
        int ct = 0;
        int res = -1;
        for (auto v : surge->voices[0])
        {
            if (v->state.gate)
            {
                ct++;
                res = v->state.key;
            }
        }
        REQUIRE(ct == 1);
        return res;
    };

    // With no second argument, confirms no note; with a second argument confirms that note
    auto confirmResult = [&](TestCase &c, int which = -1) {
        INFO("Running " << c.name);
        auto s = testInSurge(c);
        REQUIRE(playingNoteCount(s) == (which >= 0 ? 1 : 0));
        if (which >= 0)
            REQUIRE(solePlayingNote(s) == which);
    };

    auto modes = {pm_mono, pm_mono_st, pm_mono_fp, pm_mono_st_fp};
    auto mpe = {false, true};
    for (auto mp : mpe)
    {
        for (auto m : modes)
        {
            DYNAMIC_SECTION("Legacy in PlayMode " << m << " " << (mp ? "mpe" : "non-mpe"))
            {
                {
                    TestCase c("No Notes", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    auto s = testInSurge(c);
                    confirmResult(c);
                }

                {
                    TestCase c("One Note", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(60);
                    confirmResult(c, 60);
                }

                {
                    TestCase c("On Off", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(60);
                    c.off(60);
                    confirmResult(c);
                }

                {
                    TestCase c("Three Notes", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(60);
                    c.on(61);
                    c.on(59);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Three On and an Off", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(60);
                    c.on(61);
                    c.on(59);
                    c.off(59);
                    confirmResult(c, 61);
                }

                {
                    TestCase c("Second Three On", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(59);
                    confirmResult(c, 61);
                }

                {
                    TestCase c("Third Three On", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(60);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("All Backs Off", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(61);
                    c.off(60);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Toggle 59", m, mp, NOTE_ON_LATEST_RETRIGGER_HIGHEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(59);
                    c.on(59);
                    confirmResult(c, 59);
                }
            }

            DYNAMIC_SECTION("Latest in PlayMode " << m << " " << (mp ? "mpe" : "non-mpe"))
            {
                {
                    TestCase c("No Notes", m, mp, ALWAYS_LATEST);
                    confirmResult(c);
                }

                {
                    TestCase c("One One", m, mp, ALWAYS_LATEST);
                    c.on(60);
                    confirmResult(c, 60);
                }

                {
                    TestCase c("On Off", m, mp, ALWAYS_LATEST);
                    c.on(60);
                    c.off(60);
                    confirmResult(c);
                }

                {
                    TestCase c("Three Ons", m, mp, ALWAYS_LATEST);
                    c.on(60);
                    c.on(61);
                    c.on(59);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Three On and an Off AW", m, mp, ALWAYS_LATEST);
                    c.on(60);
                    c.on(61);
                    c.on(59);
                    c.off(59);
                    confirmResult(c, 61);
                }

                {
                    TestCase c("Three On and an Off Two", m, mp, ALWAYS_LATEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(59);
                    confirmResult(c, 60); // First difference
                }

                {
                    TestCase c("Three On and Back Off", m, mp, ALWAYS_LATEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(60);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Three On and Under Off", m, mp, ALWAYS_LATEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(61);
                    c.off(60);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Three On and Trill", m, mp, ALWAYS_LATEST);
                    c.on(61);
                    c.on(60);
                    c.on(59);
                    c.off(59);
                    c.on(59);
                    confirmResult(c, 59);
                }
            }

            DYNAMIC_SECTION("Highest in PlayMode " << m << " " << (mp ? "mpe" : "non-mpe"))
            {
                {
                    TestCase c("No Notes", m, mp, ALWAYS_HIGHEST);
                    confirmResult(c);
                }

                {
                    TestCase c("One One", m, mp, ALWAYS_HIGHEST);
                    c.on(60);
                    confirmResult(c, 60);
                }

                {
                    TestCase c("On Off", m, mp, ALWAYS_HIGHEST);
                    c.on(60);
                    c.off(60);
                    confirmResult(c);
                }

                {
                    TestCase c("Three Ons", m, mp, ALWAYS_HIGHEST);
                    c.on(60);
                    c.on(61);
                    c.on(59);
                    confirmResult(c, 61);
                }

                {
                    TestCase c("Release to Highest", m, mp, ALWAYS_HIGHEST);
                    c.on(60);
                    c.on(61);
                    c.on(62);
                    c.off(62);
                    confirmResult(c, 61);
                }

                {
                    TestCase c("Release to Highest Two", m, mp, ALWAYS_HIGHEST);
                    c.on(61);
                    c.on(60);
                    c.on(62);
                    c.off(62);
                    confirmResult(c, 61);
                }

                {
                    TestCase c("Release Underneath Me", m, mp, ALWAYS_HIGHEST);
                    c.on(61);
                    c.on(60);
                    c.on(62);
                    c.off(61);
                    c.off(62);
                    confirmResult(c, 60);
                }
            }
            DYNAMIC_SECTION("Lowest in PlayMode " << m << " " << (mp ? "mpe" : "non-mpe"))
            {
                {
                    TestCase c("No Notes", m, mp, ALWAYS_LOWEST);
                    confirmResult(c);
                }

                {
                    TestCase c("One One", m, mp, ALWAYS_LOWEST);
                    c.on(60);
                    confirmResult(c, 60);
                }

                {
                    TestCase c("On Off", m, mp, ALWAYS_LOWEST);
                    c.on(60);
                    c.off(60);
                    confirmResult(c);
                }

                {
                    TestCase c("Three Ons", m, mp, ALWAYS_LOWEST);
                    c.on(60);
                    c.on(59);
                    c.on(61);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Release to Lowest", m, mp, ALWAYS_LOWEST);
                    c.on(60);
                    c.on(59);
                    c.on(58);
                    c.off(58);
                    confirmResult(c, 59);
                }

                {
                    TestCase c("Release to Lowest Two", m, mp, ALWAYS_LOWEST);
                    c.on(60);
                    c.on(61);
                    c.on(59);
                    c.off(59);
                    confirmResult(c, 60);
                }

                {
                    TestCase c("Release Above Me", m, mp, ALWAYS_HIGHEST);
                    c.on(61);
                    c.on(62);
                    c.on(60);
                    c.off(62);
                    c.off(61);
                    confirmResult(c, 60);
                }
            }
        }
    }
}

TEST_CASE("MPE Mono Recoup in Split Mode", "[midi]")
{

    auto playingNoteCount = [](std::shared_ptr<SurgeSynthesizer> surge, int sc) {
        int ct = 0;
        for (auto v : surge->voices[sc])
        {
            if (v->state.gate)
                ct++;
        }
        return ct;
    };

    auto soloPlayingNote = [](std::shared_ptr<SurgeSynthesizer> surge, int sc) {
        int ct = 0;
        for (auto v : surge->voices[sc])
        {
            if (v->state.gate)
                return v->state.key;
        }
        return -1;
    };

    SECTION("Simple Two Note Split")
    {
        /*
         * Set up a surge in channel split mode and send MPE data
         */
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.getPatch().scenemode.val.i = sm_chsplit;
        surge->storage.getPatch().splitpoint.val.i = 64;
        surge->mpeEnabled = true;
        REQUIRE(surge);

        /*
         * OK so does note on note off give us voices in the right scene
         */
        surge->playNote(1, 50, 127, 0);
        surge->playNote(10, 70, 127, 0);
        surge->process();
        int ct = 0;
        for (auto v : surge->voices[0])
        {
            REQUIRE(ct++ == 0);
            REQUIRE(v->state.key == 50);
            REQUIRE(v->state.scene_id == 0);
        }
        REQUIRE(ct == 1);
        ct = 0;
        for (auto v : surge->voices[1])
        {
            REQUIRE(ct++ == 0);
            REQUIRE(v->state.key == 70);
            REQUIRE(v->state.scene_id == 1);
        }
        surge->releaseNote(1, 50, 0);
        surge->process();
        REQUIRE(playingNoteCount(surge, 0) == 0);
        REQUIRE(playingNoteCount(surge, 1) == 1);

        surge->releaseNote(10, 70, 0);
        surge->process();
        REQUIRE(playingNoteCount(surge, 0) == 0);
        REQUIRE(playingNoteCount(surge, 1) == 0);
    }

    auto modes = {pm_mono, pm_mono_st, pm_mono_fp, pm_mono_st_fp};
    for (auto m : modes)
    {
        DYNAMIC_SECTION("Single Note ChSplit Mono mode=" << m)
        {
            /*
             * Set up a surge in channel split mode and send MPE data
             */
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.getPatch().scenemode.val.i = sm_chsplit;
            surge->storage.getPatch().splitpoint.val.i = 64;
            surge->storage.getPatch().scene[0].monoVoicePriorityMode = ALWAYS_HIGHEST;
            surge->storage.getPatch().scene[0].polymode.val.i = m;

            surge->storage.getPatch().scene[1].monoVoicePriorityMode = ALWAYS_HIGHEST;
            surge->storage.getPatch().scene[1].polymode.val.i = m;

            surge->mpeEnabled = true;
            REQUIRE(surge);

            /*
             * OK so does note on note off give us voices in the right scene
             */
            surge->playNote(1, 50, 127, 0);
            surge->playNote(10, 70, 127, 0);
            surge->process();
            int ct = 0;
            for (auto v : surge->voices[0])
            {
                REQUIRE(ct++ == 0);
                REQUIRE(v->state.key == 50);
                REQUIRE(v->state.scene_id == 0);
            }
            REQUIRE(ct == 1);
            ct = 0;
            for (auto v : surge->voices[1])
            {
                REQUIRE(ct++ == 0);
                REQUIRE(v->state.key == 70);
                REQUIRE(v->state.scene_id == 1);
            }
            surge->releaseNote(1, 50, 0);
            surge->process();
            REQUIRE(playingNoteCount(surge, 0) == 0);
            REQUIRE(playingNoteCount(surge, 1) == 1);

            surge->releaseNote(10, 70, 0);
            surge->process();
            REQUIRE(playingNoteCount(surge, 0) == 0);
            REQUIRE(playingNoteCount(surge, 1) == 0);
        }

        DYNAMIC_SECTION("Two Note ChSplit Mono mode=" << m)
        {
            /*
             * Set up a surge in channel split mode and send MPE data
             */
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.getPatch().scenemode.val.i = sm_chsplit;
            surge->storage.getPatch().splitpoint.val.i = 9 * 8; // split at channel 9
            surge->storage.getPatch().scene[0].monoVoicePriorityMode = ALWAYS_HIGHEST;
            surge->storage.getPatch().scene[0].polymode.val.i = m;

            surge->storage.getPatch().scene[1].monoVoicePriorityMode = ALWAYS_HIGHEST;
            surge->storage.getPatch().scene[1].polymode.val.i = m;

            surge->mpeEnabled = true;
            REQUIRE(surge);

            /*
             * OK so does note on note off give us voices in the right scene
             */
            surge->playNote(1, 50, 127, 0);
            surge->playNote(2, 55, 127, 0);
            surge->playNote(10, 70, 127, 0);
            for (int i = 0; i < 100; ++i)
                surge->process();

            REQUIRE(playingNoteCount(surge, 0) == 1);
            REQUIRE(playingNoteCount(surge, 1) == 1);
            REQUIRE(soloPlayingNote(surge, 0) == 55);
            REQUIRE(soloPlayingNote(surge, 1) == 70);

            surge->releaseNote(2, 55, 0);
            surge->process();
            REQUIRE(playingNoteCount(surge, 0) == 1);
            REQUIRE(playingNoteCount(surge, 1) == 1);
            REQUIRE(soloPlayingNote(surge, 0) == 50);
            REQUIRE(soloPlayingNote(surge, 1) == 70);

            surge->releaseNote(1, 50, 0);
            surge->process();
            REQUIRE(playingNoteCount(surge, 0) == 0);
            REQUIRE(playingNoteCount(surge, 1) == 1);
            REQUIRE(soloPlayingNote(surge, 1) == 70);

            surge->releaseNote(10, 70, 0);
            surge->process();
            REQUIRE(playingNoteCount(surge, 0) == 0);
            REQUIRE(playingNoteCount(surge, 1) == 0);
        }

        DYNAMIC_SECTION("Split Works @ Highest mode=" << m)
        {
            int splitChannel = 8;
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.getPatch().scenemode.val.i = sm_chsplit;
            surge->storage.getPatch().splitpoint.val.i = splitChannel * 8; // split at channel 9
            surge->storage.getPatch().scene[0].monoVoicePriorityMode = ALWAYS_HIGHEST;
            surge->storage.getPatch().scene[0].polymode.val.i = m;

            surge->storage.getPatch().scene[1].monoVoicePriorityMode = ALWAYS_HIGHEST;
            surge->storage.getPatch().scene[1].polymode.val.i = m;

            surge->mpeEnabled = true;
            REQUIRE(surge);
            for (int i = 1; i < 16; ++i)
            {
                INFO("Playing note " << 50 + i << " on channel " << i);
                surge->playNote(i, 50 + i, 127, 0);
                for (int q = 0; q < 20; ++q)
                    surge->process();

                if (i <= splitChannel)
                {
                    REQUIRE(playingNoteCount(surge, 0) == 1);
                    REQUIRE(soloPlayingNote(surge, 0) == 50 + i);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                }
                else
                {
                    REQUIRE(playingNoteCount(surge, 0) == 1);
                    REQUIRE(soloPlayingNote(surge, 0) == 50 + splitChannel);
                    REQUIRE(playingNoteCount(surge, 1) == 1);
                    REQUIRE(soloPlayingNote(surge, 1) == 50 + i);
                }
            }

            for (int i = 15; i > 0; --i)
            {
                INFO("Releasing note " << 50 + i << " on channel " << i);

                surge->releaseNote(i, 50 + i, 0);
                for (int q = 0; q < 20; ++q)
                    surge->process();

                if (i <= splitChannel)
                {
                    if (i == 1)
                    {
                        REQUIRE(playingNoteCount(surge, 0) == 0);
                    }
                    else
                    {
                        REQUIRE(playingNoteCount(surge, 0) == 1);
                        REQUIRE(soloPlayingNote(surge, 0) == 50 + i - 1);
                    }
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                }
                else
                {
                    REQUIRE(playingNoteCount(surge, 0) == 1);
                    REQUIRE(soloPlayingNote(surge, 0) == 50 + splitChannel);
                    if (i == splitChannel + 1)
                    {
                        // We released last note
                        REQUIRE(playingNoteCount(surge, 1) == 0);
                    }
                    else
                    {
                        REQUIRE(playingNoteCount(surge, 1) == 1);
                        REQUIRE(soloPlayingNote(surge, 1) == 50 + i - 1);
                    }
                }
            }
        }
    }

    SECTION("Ch Split Occurs Coherently in Poly")
    {
        int splitChannel = 8;
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.getPatch().scenemode.val.i = sm_chsplit;
        surge->storage.getPatch().splitpoint.val.i = splitChannel * 8; // split at channel 9
        surge->storage.getPatch().scene[0].polymode.val.i = pm_poly;

        surge->storage.getPatch().scene[1].polymode.val.i = pm_poly;

        surge->mpeEnabled = true;
        REQUIRE(surge);
        for (int i = 1; i < 16; ++i)
        {
            INFO("Plaing note " << 50 + i << " on channel " << i);
            surge->playNote(i, 50 + i, 127, 0);
            if (i <= splitChannel)
            {
                REQUIRE(playingNoteCount(surge, 0) == i);
                REQUIRE(playingNoteCount(surge, 1) == 0);
            }
            else
            {
                REQUIRE(playingNoteCount(surge, 0) == splitChannel);
                REQUIRE(playingNoteCount(surge, 1) == i - splitChannel);
            }
        }

        for (int i = 15; i > 0; --i)
        {
            INFO("Releasing note " << 50 + i << " on channel " << i);
            surge->releaseNote(i, 50 + i, 0);
            if (i <= splitChannel)
            {
                REQUIRE(playingNoteCount(surge, 0) == i - 1);
                REQUIRE(playingNoteCount(surge, 1) == 0);
            }
            else
            {
                REQUIRE(playingNoteCount(surge, 0) == splitChannel);
                REQUIRE(playingNoteCount(surge, 1) == i - splitChannel - 1);
            }
        }
    }
}

TEST_CASE("Priority Modes with Keysplit", "[midi]")
{
    // These tests could be more complete
    auto playingNoteCount = [](std::shared_ptr<SurgeSynthesizer> surge, int sc) {
        int ct = 0;
        for (auto v : surge->voices[sc])
        {
            if (v->state.gate)
                ct++;
        }
        return ct;
    };

    auto solePlayingNote = [](std::shared_ptr<SurgeSynthesizer> surge, int sc) {
        int ct = 0;
        int res = -1;
        for (auto v : surge->voices[sc])
        {
            if (v->state.gate)
            {
                ct++;
                res = v->state.key;
            }
        }
        REQUIRE(ct == 1);
        return res;
    };

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

    auto modes = {pm_mono, pm_mono_st, pm_mono_fp, pm_mono_st_fp};
    auto mpe = {false, true};
    for (auto mp : mpe)
    {
        for (auto m : modes)
        {
            auto setupSurge = [m, mp](auto vm) {
                auto surge = Surge::Headless::createSurge(44100);
                surge->storage.getPatch().scenemode.val.i = sm_split;
                surge->storage.getPatch().splitpoint.val.i = 60; // split at channel 9
                surge->storage.getPatch().scene[0].polymode.val.i = m;
                surge->storage.getPatch().scene[0].monoVoicePriorityMode = vm;
                surge->storage.getPatch().scene[1].polymode.val.i = m;
                surge->storage.getPatch().scene[1].monoVoicePriorityMode = vm;
                surge->mpeEnabled = mp;
                return surge;
            };
            DYNAMIC_SECTION("KeySplit Highest mode=" << m << " mpe=" << mp)
            {
                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 70}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }

                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 70, -40}, mp);
                    REQUIRE(playingNoteCount(surge, 0) == 0);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 70, -70}, mp);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                }
                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 41}, mp);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                    REQUIRE(solePlayingNote(surge, 0) == 41);
                }
                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 70}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 70, 41}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 41);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_HIGHEST);
                    playSequence(surge, {40, 70, -40}, mp);
                    REQUIRE(playingNoteCount(surge, 0) == 0);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
            }

            DYNAMIC_SECTION("KeySplit Lowest mode=" << m << " mpe=" << mp)
            {
                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 70}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }

                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 70, -40}, mp);
                    REQUIRE(playingNoteCount(surge, 0) == 0);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 70, -70}, mp);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                }
                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 41}, mp);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                }
                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 70}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 70, 41}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_LOWEST);
                    playSequence(surge, {40, 70, -40}, mp);
                    REQUIRE(playingNoteCount(surge, 0) == 0);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
            }

            DYNAMIC_SECTION("KeySplit Latesr mode=" << m << " mpe=" << mp)
            {
                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 70}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }

                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 70, -40}, mp);
                    REQUIRE(playingNoteCount(surge, 0) == 0);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 70, -70}, mp);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                }
                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 41}, mp);
                    REQUIRE(playingNoteCount(surge, 1) == 0);
                    REQUIRE(solePlayingNote(surge, 0) == 41);
                }
                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 70}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 40);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 70, 41}, mp);
                    REQUIRE(solePlayingNote(surge, 0) == 41);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
                {
                    auto surge = setupSurge(ALWAYS_LATEST);
                    playSequence(surge, {40, 70, -40}, mp);
                    REQUIRE(playingNoteCount(surge, 0) == 0);
                    REQUIRE(solePlayingNote(surge, 1) == 70);
                }
            }
        }
    }
}

TEST_CASE("Voice Stealing", "[midi]")
{
    for (auto pc : {4, 16, 32, 50, 61, 64})
    {
        DYNAMIC_SECTION("PolyCount " << pc)
        {
            auto surge = surgeOnSine();
            surge->storage.getPatch().polylimit.val.i = pc;
            for (int i = 0; i < 10; ++i)
                surge->process();

            for (int v = 1; v < 127; ++v)
            {
                surge->playNote(0, v, 120, 0);
                for (int i = 0; i < 20; ++i)
                    surge->process();
                REQUIRE(surge->voices[0].size() <= pc);
                REQUIRE(surge->voices[0].back()->state.key == v);
                if (v > pc)
                {
                    REQUIRE(surge->voices[0].front()->state.key == v - pc + 1);
                    REQUIRE(surge->voices[0].size() == pc);
                }
            }
        }
    }
}

TEST_CASE("Single Key Pedal Voice Count", "[midi]") // #1459
{
    auto playingVoiceCount = [](std::shared_ptr<SurgeSynthesizer> surge) {
        int ct = 0;
        for (auto v : surge->voices[0])
        {
            if (v->state.gate)
                ct++;
        }
        return ct;
    };

    SECTION("In C, in Various Forms")
    {
        auto step = [](auto surge) {
            for (int i = 0; i < 25; ++i)
                surge->process();
        };
        auto testInSurge = [](auto kernel) {
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);
            surge->storage.getPatch().scene[0].polymode.val.i = pm_poly;
            kernel(surge);
        };

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Pedal On Off");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingVoiceCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Pedal On Off Pedal Down");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(playingVoiceCount(surge) == 1);
        });

        testInSurge([&](auto surge) {
            // Case one - no pedal play and release
            INFO("Single Pedal Double Strike Pedal Down");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            REQUIRE(playingVoiceCount(surge) == 2);
        });

        testInSurge([&](auto surge) {
            INFO("Single Pedal Double Strike Pedal Up");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingVoiceCount(surge) == 0);
        });

        testInSurge([&](auto surge) {
            INFO("Single Pedal Single Release Pedal Up");
            surge->channelController(0, 64, 127);
            step(surge);
            surge->playNote(0, 60, 120, 0);
            step(surge);
            surge->releaseNote(0, 60, 0);
            step(surge);
            surge->playNote(0, 60, 120, 0); // critically don't release this
            step(surge);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingVoiceCount(surge) == 1);
        });

        testInSurge([&](auto surge) {
            INFO("Single Pedal Single Release Pedal Up");
            surge->channelController(0, 64, 127);
            step(surge);
            for (int i = 0; i < 5; ++i)
            {
                surge->playNote(0, 60, 120, 0);
                step(surge);
                surge->releaseNote(0, 60, 0);
                step(surge);
            }
            surge->playNote(0, 60, 120, 0); // critically don't release this
            step(surge);
            surge->channelController(0, 64, 0);
            step(surge);
            REQUIRE(playingVoiceCount(surge) == 1);
        });
    }
}

TEST_CASE("Poly AT on Multiple Channels", "[midi]")
{
    for (int ch = 0; ch < 16; ++ch)
    {
        DYNAMIC_SECTION("Channel " << ch)
        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.smoothingMode = Modulator::SmoothingMode::DIRECT;
            REQUIRE(surge);

            auto pid = surge->storage.getPatch().scene[0].osc[0].pitch.id;
            surge->setModDepth01(pid, ms_polyaftertouch, 0, 0, 0.1);
            surge->process();
            surge->playNote(ch, 60, 120, 0);
            surge->process();
            surge->polyAftertouch(ch, 60, 64);
            surge->process();
            for (auto v : surge->voices[0])
            {
                REQUIRE(v->modsources[ms_polyaftertouch]->get_output01(0) == 64.f / 127.f);
            }
        }
    }
}

TEST_CASE("MIDI Learn", "[midi]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge);

    for (int i = 0; i < 5; ++i)
        surge->process();

    auto &pp = surge->storage.getPatch().scene[0].osc[0].pitch;
    auto pv = pp.val.f;
    REQUIRE(pv == 0);

    surge->learn_param_from_cc = pp.id;
    surge->process();
    surge->channelController(0, 41, 64);
    surge->process();
    REQUIRE(surge->learn_param_from_cc == -1);

    surge->channelController(0, 41, 127);
    for (int i = 0; i < 300; ++i)
        surge->process();
    auto pu = pp.val.f;
    REQUIRE(pu == Approx(7).margin(.1));

    surge->channelController(0, 41, 0);
    for (int i = 0; i < 600; ++i)
        surge->process();
    auto pd = pp.val.f;
    REQUIRE(pd == Approx(-7).margin(.1));
}

TEST_CASE("Poly Chords Blow Through Limit", "[midi]")
{
    INFO("See Issue 6221");
    SECTION("Stagger Note On")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->storage.getPatch().polylimit.val.i = 3;
        for (int i = 0; i < 10; ++i)
            surge->process();

        for (int i = 0; i < 5; ++i)
        {
            surge->playNote(0, 60 + 2 * i, 100, 0, 123 + i);
            surge->process();
        }
        for (int i = 0; i < 100; ++i)
            surge->process();

        REQUIRE(surge->voices[0].size() == 3);
    }
    SECTION("Simultaneous Note On")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        surge->storage.getPatch().polylimit.val.i = 3;
        for (int i = 0; i < 10; ++i)
            surge->process();

        for (int i = 0; i < 5; ++i)
            surge->playNote(0, 60 + 2 * i, 100, 0, 123 + i);

        for (int i = 0; i < 100; ++i)
            surge->process();

        REQUIRE(surge->voices[0].size() == 3);
    }
}

TEST_CASE("Mono Modes Across Channels", "[midi]")
{
    for (auto env :
         {MonoVoiceEnvelopeMode::RESTART_FROM_ZERO, MonoVoiceEnvelopeMode::RESTART_FROM_LATEST})
    {
        for (auto mode : {pm_mono, pm_mono_st, pm_mono_fp, pm_mono_st_fp})
        {
            for (auto secondChannel : {1, 5})
            {
                DYNAMIC_SECTION("On on 1, Off on " << secondChannel << " mode="
                                                   << play_mode_names[mode] << " regrigger=" << env)
                {
                    auto surge = Surge::Headless::createSurge(44100);
                    REQUIRE(surge);
                    surge->storage.getPatch().scene[0].polymode.val.i = mode;
                    surge->storage.getPatch().scene[0].monoVoiceEnvelopeMode = env;

                    for (int i = 0; i < 10; ++i)
                        surge->process();

                    surge->playNote(secondChannel - 1, 60, 127, 0);
                    for (int i = 0; i < 50; ++i)
                        surge->process();
                    REQUIRE(surge->voices[0].size() == 1);

                    surge->playNote(0, 65, 127, 0);
                    for (int i = 0; i < 50; ++i)
                        surge->process();
                    REQUIRE(surge->voices[0].size() == 1);

                    surge->releaseNote(secondChannel - 1, 60, 0);
                    for (int i = 0; i < 50; ++i)
                        surge->process();
                    REQUIRE(surge->voices[0].size() == 1);

                    surge->releaseNote(0, 65, 0);
                    for (int i = 0; i < 50; ++i)
                        surge->process();

                    // Enbling this means you have addressed #6287
                    // REQUIRE(surge->voices[0].size() == 0);
                }
            }
        }
    }
}