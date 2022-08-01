#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "catch2/catch2.hpp"

TEST_CASE("Note ID in Poly Mode Basics", "[noteid]")
{
    SECTION("Single On Off, Default Sustain")
    {
        auto surge = Surge::Headless::createSurge(48000);
        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->playNote(0, 60, 127, 0, 1423);

        for (int i = 0; i < 20; ++i)
        {
            surge->process();
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            for (auto v : surge->voices[0])
            {
                REQUIRE(v->host_note_id == 1423);
                REQUIRE(v->originating_host_channel == 0);
                REQUIRE(v->originating_host_key == 60);
            }
        }

        surge->releaseNote(0, 60, 127);
        while (!surge->voices[0].empty())
        {
            surge->process();
        }
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
        REQUIRE(surge->endedHostNoteIds[0] == 1423);
        REQUIRE(surge->endedHostNoteOriginalChannel[0] == 0);
        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);

        surge->process();
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
    }

    SECTION("Dual Distinct On Off, Long Sustain")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scene[0].adsr[0].r.val.f = 0.5;

        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->playNote(0, 60, 127, 0, 1423);

        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->playNote(0, 65, 127, 0, 8702);

        for (int i = 0; i < 20; ++i)
        {
            surge->process();
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            for (auto v : surge->voices[0])
            {
                if (v->originating_host_key == 65)
                    REQUIRE(v->host_note_id == 8702);
                else if (v->originating_host_key == 60)
                    REQUIRE(v->host_note_id == 1423);
                else
                    REQUIRE(false);
            }
        }

        surge->releaseNote(0, 65, 0);
        for (int i = 0; i < 20; ++i)
            surge->process();
        surge->releaseNote(0, 60, 0);

        // staggered voice winddown
        REQUIRE(surge->voices[0].size() == 2);
        while (surge->voices[0].size() == 2)
            surge->process();
        REQUIRE(surge->voices[0].size() == 1);
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
        REQUIRE(surge->endedHostNoteIds[0] == 8702);
        REQUIRE(surge->endedHostNoteOriginalKey[0] == 65);
        while (surge->voices[0].size() == 1)
            surge->process();
        REQUIRE(surge->voices[0].size() == 0);
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
        REQUIRE(surge->endedHostNoteIds[0] == 1423);
        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
    }

    SECTION("Dual Mode, Differeing Sustaing")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scenemode.val.i = sm_dual;
        surge->storage.getPatch().scene[1].adsr[0].r.val.f = 1;

        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->playNote(0, 60, 127, 0, 1497);

        for (int i = 0; i < 20; ++i)
        {
            surge->process();
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            REQUIRE(surge->voices[0].size() == 1);
            REQUIRE(surge->voices[1].size() == 1);
            for (auto v : {surge->voices[0].front(), surge->voices[1].front()})
            {
                REQUIRE(v->host_note_id == 1497);
                REQUIRE(v->originating_host_channel == 0);
                REQUIRE(v->originating_host_key == 60);
            }
        }

        surge->releaseNote(0, 60, 127);
        while (!(surge->voices[0].empty() && surge->voices[1].empty()))
        {
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            surge->process();
        }
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
        REQUIRE(surge->endedHostNoteIds[0] == 1497);
        REQUIRE(surge->endedHostNoteOriginalChannel[0] == 0);
        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);

        surge->process();
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
    }

    SECTION("Same Note On Off, Long Sustain")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scene[0].adsr[0].r.val.f = 0.5;

        int blocksBetween = 400;

        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->playNote(0, 60, 127, 0, 1423);

        for (int i = 0; i < 5; ++i)
            surge->process();
        surge->releaseNote(0, 60, 0);

        for (int i = 0; i < blocksBetween; ++i)
            surge->process();
        surge->playNote(0, 65, 127, 0, 8702);

        for (int i = 0; i < 20; ++i)
        {
            surge->process();
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            REQUIRE(surge->voices[0].size() == 2);
            int32_t nid[2], idx{0};
            for (auto v : surge->voices[0])
            {
                nid[idx] = v->host_note_id;
                idx++;
            }
            REQUIRE(nid[0] == 1423);
            REQUIRE(nid[1] == 8702);
        }

        surge->releaseNote(0, 65, 0);
        for (int i = 0; i < 20; ++i)
            surge->process();
        surge->releaseNote(0, 60, 0);

        // staggered voice winddown
        REQUIRE(surge->voices[0].size() == 2);
        int pc2{0}, pc1{0};
        while (surge->voices[0].size() == 2)
        {
            pc2++;
            surge->process();
        }
        REQUIRE(surge->voices[0].size() == 1);
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
        REQUIRE(surge->endedHostNoteIds[0] == 1423);
        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
        while (surge->voices[0].size() == 1)
        {
            pc1++;
            surge->process();
        }
        REQUIRE(surge->voices[0].size() == 0);
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
        REQUIRE(surge->endedHostNoteIds[0] == 8702);
        REQUIRE(surge->endedHostNoteOriginalKey[0] == 65);

        REQUIRE(pc1 > blocksBetween);
        REQUIRE(pc1 < blocksBetween + 50); // we have a non-click end of voice
    }
}

TEST_CASE("Sustain Pedal in Poly Mode", "[noteid]")
{
    SECTION("Distinct Notes")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scene[0].adsr[0].r.val.f = -3;

        surge->process();

        surge->channelController(0, 64, 127);
        surge->process();
        int nidbase = 84923;
        for (int i = 0; i < 5; ++i)
        {
            surge->playNote(0, 60 + i * 2, 127, 0, nidbase + i);
            for (int q = 0; q < 4; ++q)
                surge->process();
        }

        REQUIRE(surge->voices[0].size() == 5);

        for (int i = 0; i < 5; ++i)
        {
            surge->releaseNote(0, 60 + i * 2, 0);
            for (int q = 0; q < 4; ++q)
                surge->process();
        }

        REQUIRE(surge->voices[0].size() == 5);

        for (int i = 0; i < 500; ++i)
        {
            surge->process();
            REQUIRE(surge->voices[0].size() == 5);
        }

        // Now release the sustain pedal
        surge->channelController(0, 64, 0);

        // And in theory we should all run out in about 20 blocks
        while (surge->voices[0].size() == 5)
        {
            surge->process();
        }
        REQUIRE(surge->voices[0].size() == 0);
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 5);
        for (int i = 0; i < 5; ++i)
        {
            REQUIRE(surge->endedHostNoteOriginalKey[i] == 60 + i * 2);
            REQUIRE(surge->endedHostNoteOriginalChannel[i] == 0);
            REQUIRE(surge->endedHostNoteIds[i] == nidbase + i);
        }
    }
    SECTION("Same Note")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scene[0].adsr[0].r.val.f = -3;

        surge->process();

        surge->channelController(0, 64, 127);
        surge->process();
        int nidbase = 71231;
        // press release here rather than press press press release release release
        for (int i = 0; i < 5; ++i)
        {
            surge->playNote(0, 60 + i * 2, 127, 0, nidbase + i);
            for (int q = 0; q < 4; ++q)
                surge->process();
            surge->releaseNote(0, 60 + i * 2, 0);
            for (int q = 0; q < 4; ++q)
                surge->process();
        }

        REQUIRE(surge->voices[0].size() == 5);

        REQUIRE(surge->voices[0].size() == 5);

        for (int i = 0; i < 500; ++i)
        {
            surge->process();
            REQUIRE(surge->voices[0].size() == 5);
        }

        // Now release the sustain pedal
        surge->channelController(0, 64, 0);

        // And in theory we should all run out in about 20 blocks
        while (surge->voices[0].size() == 5)
        {
            surge->process();
        }
        REQUIRE(surge->voices[0].size() == 0);
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 5);
        for (int i = 0; i < 5; ++i)
        {
            REQUIRE(surge->endedHostNoteOriginalKey[i] == 60 + i * 2);
            REQUIRE(surge->endedHostNoteOriginalChannel[i] == 0);
            REQUIRE(surge->endedHostNoteIds[i] == nidbase + i);
        }
    }
}

TEST_CASE("Overlapping Notes", "[noteid]")
{
    SECTION("5 notes overlap; no id on release")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->process();

        // OK so lets send 5 note ons that overlap without note offs
        int nidbase = 7223;
        // press release here rather than press press press release release release
        for (int i = 0; i < 5; ++i)
        {
            surge->playNote(0, 60, 127, 0, nidbase + i);
            for (int q = 0; q < 4; ++q)
                surge->process();
        }

        REQUIRE(surge->voices[0].size() == 5);
        int idx = 0;
        for (auto v : surge->voices[0])
        {
            REQUIRE(v->originating_host_key == 60);
            REQUIRE(v->originating_host_channel == 0);
            REQUIRE(v->host_note_id == nidbase + idx);
            idx++;
        }

        // OK so we can stack them on. How do we turn them off. Right now the releaseNote
        // releases all notes on that key if you don't provide a noteid so here you look
        // for all the voices to go to release mode with a single releaseNote
        surge->releaseNote(0, 60, 0);
        while (surge->voices[0].size() == 5)
            surge->process();
        REQUIRE(surge->voices[0].empty());
        REQUIRE(surge->hostNoteEndedDuringBlockCount == 5);
        for (int i = 0; i < 5; ++i)
        {
            REQUIRE(surge->endedHostNoteOriginalKey[i] == 60);
            REQUIRE(surge->endedHostNoteOriginalChannel[i] == 0);
            REQUIRE(surge->endedHostNoteIds[i] == nidbase + i);
        }

        // And the other 4 release notes don't do any harm
        for (int i = 0; i < 5 - 1; ++i)
        {
            surge->releaseNote(0, 60, 0);
            for (int q = 0; q < 50; ++q)
            {
                REQUIRE(surge->voices[0].empty());
                surge->process();
            }
        }
    }

    SECTION("5 notes overlap; release with id")
    {
        auto surge = Surge::Headless::createSurge(48000);
        surge->process();

        // OK so lets send 5 note ons that overlap without note offs
        int nidbase = 7223;
        // press release here rather than press press press release release release
        for (int i = 0; i < 5; ++i)
        {
            surge->playNote(0, 60, 127, 0, nidbase + i);
            for (int q = 0; q < 4; ++q)
                surge->process();
        }

        REQUIRE(surge->voices[0].size() == 5);
        int idx = 0;
        for (auto v : surge->voices[0])
        {
            REQUIRE(v->originating_host_key == 60);
            REQUIRE(v->originating_host_channel == 0);
            REQUIRE(v->host_note_id == nidbase + idx);
            idx++;
        }

        // If release note comes with an ID we can stack the releases also, as
        // we show here.
        for (int i = 0; i < 5; ++i)
        {
            INFO("Releasing note " << i);
            surge->releaseNote(0, 60, 0, nidbase + i);
            while (surge->voices[0].size() == 5 - i)
            {
                surge->process();
            }
            REQUIRE(surge->voices[0].size() == 4 - i);
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
            REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
            REQUIRE(surge->endedHostNoteOriginalChannel[0] == 0);
            REQUIRE(surge->endedHostNoteIds[0] == nidbase + i);

            for (int q = 0; q < 100; ++q)
                surge->process();
        }
    }
}

TEST_CASE("Mono Modes", "[noteid]")
{
    for (auto mode : {pm_mono, pm_mono_st, pm_mono_fp, pm_mono_st_fp})
    {
        /*
         * There's no way to retrigger DAW envelopes on hammer off, so lets
         * have all our mono modes treat the note id as if it is legato.
         * That means we expect
         *
         * -> ON k=60 id=1
         * [ voice stack size 1 with k=60, id=1 ]
         * -> ON k=61 id=2
         * [ voice stack size 1 with k=61, id=1 ]
         * <- VOICE END K=61 ID=2
         * -> OFF k=61 id=2
         * [ voice stack size 1 with k=60, id=1 on it
         * -> OFF k=60
         * <- VOICE END K=60 ID=1
         * [ voice stack empty]
         */

        DYNAMIC_SECTION("One note hammer on off in mode " << play_mode_names[mode])
        {
            auto surge = Surge::Headless::createSurge(48000);
            REQUIRE(surge);
            surge->storage.getPatch().scene[0].polymode.val.i = mode;
            for (int i = 0; i < 10; ++i)
                surge->process();

            int nidbase = 14000;
            surge->playNote(0, 60, 127, 0, nidbase);
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        REQUIRE(v->host_note_id == nidbase); // it is re-used
                        REQUIRE(v->originating_host_key == 60);
                    }
                }
                REQUIRE(ct == 1);
            }

            surge->playNote(0, 61, 127, 0, nidbase + 1);

            bool sawOne = false;
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        if (mode == pm_mono || mode == pm_mono_fp)
                        {
                            REQUIRE(v->host_note_id == nidbase + 1); // it is re-triggered
                            REQUIRE(v->originating_host_key == 61);
                        }
                        else
                        {
                            REQUIRE(v->host_note_id == nidbase); // it is re-used
                            REQUIRE(v->originating_host_key == 60);
                        }
                    }
                }
                REQUIRE(ct == 1);

                if (surge->hostNoteEndedDuringBlockCount > 0)
                {
                    sawOne = true;
                    REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);

                    if (mode == pm_mono || mode == pm_mono_fp)
                    {
                        // Retrigger so original ends
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
                    }
                    else
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase + 1);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 61);
                    }
                }
            }
            REQUIRE(sawOne);

            surge->releaseNote(0, 61, 127);

            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        if (mode == pm_mono || mode == pm_mono_fp)
                        {
                            REQUIRE(v->host_note_id == nidbase + 1); // it is re-triggered
                            REQUIRE(v->originating_host_key == 61);
                        }
                        else
                        {
                            REQUIRE(v->host_note_id == nidbase); // it is re-used
                            REQUIRE(v->originating_host_key == 60);
                        }
                    }
                }
                REQUIRE(ct == 1);
                REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            }

            sawOne = false;
            surge->releaseNote(0, 60, 127);
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    REQUIRE(!v->state.gate);
                }
                if (surge->hostNoteEndedDuringBlockCount > 0)
                {
                    sawOne = true;
                    REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
                    // We kill the recycled
                    if (mode == pm_mono || mode == pm_mono_fp)
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase + 1);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 61);
                    }
                    else
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
                    }
                }
            }
            REQUIRE(sawOne);
        }

        DYNAMIC_SECTION("Three Note hammer on off in mode " << play_mode_names[mode])
        {
            auto surge = Surge::Headless::createSurge(48000);
            REQUIRE(surge);
            surge->storage.getPatch().scene[0].polymode.val.i = mode;
            for (int i = 0; i < 10; ++i)
                surge->process();

            int nidbase = 14000;
            surge->playNote(0, 60, 127, 0, nidbase);
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        REQUIRE(v->host_note_id == nidbase); // it is re-used
                        REQUIRE(v->originating_host_key == 60);
                    }
                }
                REQUIRE(ct == 1);
            }

            surge->playNote(0, 61, 127, 0, nidbase + 1);

            bool sawOne = false;
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        if (mode == pm_mono || mode == pm_mono_fp)
                        {
                            REQUIRE(v->host_note_id == nidbase + 1); // it is re-used
                            REQUIRE(v->originating_host_key == 61);
                        }
                        else
                        {
                            REQUIRE(v->host_note_id == nidbase); // it is re-used
                            REQUIRE(v->originating_host_key == 60);
                        }
                    }
                }
                REQUIRE(ct == 1);

                if (surge->hostNoteEndedDuringBlockCount > 0)
                {
                    sawOne = true;
                    REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
                    if (mode == pm_mono || mode == pm_mono_fp)
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
                    }
                    else
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase + 1);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 61);
                    }
                }
            }
            REQUIRE(sawOne);

            surge->playNote(0, 63, 127, 0, nidbase + 2);

            sawOne = false;
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        if (mode == pm_mono || mode == pm_mono_fp)
                        {
                            REQUIRE(v->host_note_id == nidbase + 2); // it is re-triggered
                            REQUIRE(v->originating_host_key == 63);
                        }
                        else
                        {
                            REQUIRE(v->host_note_id == nidbase); // it is re-used
                            REQUIRE(v->originating_host_key == 60);
                        }
                    }
                }
                REQUIRE(ct == 1);

                if (surge->hostNoteEndedDuringBlockCount > 0)
                {
                    sawOne = true;
                    REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
                    if (mode == pm_mono || mode == pm_mono_fp)
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase + 1);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 61);
                    }
                    else
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase + 2);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 63);
                    }
                }
            }
            REQUIRE(sawOne);

            surge->releaseNote(0, 60, 127);

            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        if (mode == pm_mono || mode == pm_mono_fp)
                        {
                            REQUIRE(v->host_note_id == nidbase + 2); // it is re-used
                            REQUIRE(v->originating_host_key == 63);
                        }
                        else
                        {
                            REQUIRE(v->host_note_id == nidbase); // it is re-used
                            REQUIRE(v->originating_host_key == 60);
                        }
                    }
                }
                REQUIRE(ct == 1);
                REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            }

            sawOne = false;
            surge->releaseNote(0, 63, 127);
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    if (v->state.gate)
                    {
                        ct++;
                        if (mode == pm_mono || mode == pm_mono_fp)
                        {
                            REQUIRE(v->host_note_id == nidbase + 2); // it is re-used
                            REQUIRE(v->originating_host_key == 63);
                        }
                        else
                        {
                            REQUIRE(v->host_note_id == nidbase); // it is re-used
                            REQUIRE(v->originating_host_key == 60);
                        }
                    }
                }
                REQUIRE(ct == 1);
                REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
            }

            sawOne = false;
            surge->releaseNote(0, 61, 127);
            for (int i = 0; i < 50; ++i)
            {
                surge->process();
                int ct = 0;
                for (auto v : surge->voices[0])
                {
                    REQUIRE(!v->state.gate);
                }
                if (surge->hostNoteEndedDuringBlockCount > 0)
                {
                    sawOne = true;
                    REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
                    // We kill the recycled
                    if (mode == pm_mono || mode == pm_mono_fp)
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase + 2);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 63);
                    }
                    else
                    {
                        REQUIRE(surge->endedHostNoteIds[0] == nidbase);
                        REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);
                    }
                }
            }
            REQUIRE(sawOne);
        }
    }
}

TEST_CASE("Note ID 0 is Valid", "[noteid]")
{
    for (auto nid : {0, 1, 77, std::numeric_limits<int32_t>::max() - 1})
    {
        DYNAMIC_SECTION("NoteID " << nid << " is a Valid Note")
        {
            int unid = 0;
            auto surge = Surge::Headless::createSurge(48000);
            for (int i = 0; i < 5; ++i)
                surge->process();
            surge->playNote(0, 60, 127, 0, unid);

            for (int i = 0; i < 20; ++i)
            {
                surge->process();
                REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
                for (auto v : surge->voices[0])
                {
                    REQUIRE(v->host_note_id == unid);
                    REQUIRE(v->originating_host_channel == 0);
                    REQUIRE(v->originating_host_key == 60);
                }
            }

            surge->releaseNote(0, 60, 127);
            while (!surge->voices[0].empty())
            {
                surge->process();
            }
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 1);
            REQUIRE(surge->endedHostNoteIds[0] == unid);
            REQUIRE(surge->endedHostNoteOriginalChannel[0] == 0);
            REQUIRE(surge->endedHostNoteOriginalKey[0] == 60);

            surge->process();
            REQUIRE(surge->hostNoteEndedDuringBlockCount == 0);
        }
    }
}

// TODO
// mono and poly dual mix
// mpe poly
// mpe mono
// mpe sustain pedal
// mono sustain pedal