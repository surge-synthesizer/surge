#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "FastMath.h"

using namespace Surge::Test;

TEST_CASE("Airwindows Loud", "[fx]")
{
    SECTION("Make Loud")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        for (int i = 0; i < 100; ++i)
            surge->process();

        auto *pt = &(surge->storage.getPatch().fx[0].type);
        auto awv = 1.f * fxt_airwindows / (pt->val_max.i - pt->val_min.i);

        auto did = surge->idForParameter(pt);
        surge->setParameter01(did, awv, false);

        auto *pawt = &(surge->storage.getPatch().fx[0].p[0]);

        for (int i = 0; i < 500; ++i)
        {
            pawt->val.i = 34;
            surge->process();

            float soo = 0.f;

            INFO("Swapping aiwrindow attempt " << i);
            for (int s = 0; s < 100; ++s)
            {
                surge->process();
                for (int p = 0; p < BLOCK_SIZE; ++p)
                {
                    soo += surge->output[0][p] + surge->output[1][p];
                    REQUIRE(fabs(surge->output[0][p]) < 1e-5);
                    REQUIRE(fabs(surge->output[1][p]) < 1e-5);
                }
            }

            REQUIRE(fabs(soo) < 1e-5);

            // Toggle to something which isn't 'loud'
            pawt->val.i = rand() % 30 + 2;
            for (int s = 0; s < 100; ++s)
            {
                surge->process();
            }
        }
    }
}

TEST_CASE("FX Move with Modulation", "[fx]")
{
    auto setFX = [](auto surge, auto slot, auto type) {
        auto *pt = &(surge->storage.getPatch().fx[slot].type);
        auto awv = 1.f * fxt_chorus4 / (pt->val_max.i - pt->val_min.i);

        auto did = surge->idForParameter(pt);
        surge->setParameter01(did, awv, false);

        for (int i = 0; i < 10; ++i)
            surge->process();
    };
    auto step = [](auto surge) {
        for (int i = 0; i < 10; ++i)
            surge->process();
    };

    auto confirmDestinations = [](auto surge, const std::vector<std::pair<int, int>> &fxp) {
        std::map<int, int> destinations;
        for (const auto &mg : surge->storage.getPatch().modulation_global)
        {
            if (destinations.find(mg.destination_id) == destinations.end())
                destinations[mg.destination_id] = 0;
            destinations[mg.destination_id]++;
        }

        auto rd = [surge, &destinations](auto f, auto p) {
            auto id = surge->storage.getPatch().fx[f].p[p].id;
            if (destinations.find(id) == destinations.end())
                destinations[id] = 0;
            destinations[id]--;
        };

        for (auto p : fxp)
        {
            rd(p.first, p.second);
        }

        for (auto p : destinations)
        {
            INFO("Confirming destination " << p.first);
            REQUIRE(p.second == 0);
        }
        return true;
    };

    SECTION("Setup a Modulated Combulator and Move It")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        setFX(surge, 0, fxt_combulator);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);
        step(surge);

        surge->reorderFx(0, 1, SurgeSynthesizer::MOVE);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);

        confirmDestinations(surge, {{1, 2}});
    }

    SECTION("Setup a Modulated Combulator With Two Mods and another Move It")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        setFX(surge, 0, fxt_combulator);
        setFX(surge, 4, fxt_chorus4);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        surge->setModDepth01(surge->storage.getPatch().fx[0].p[3].id, ms_slfo2, 0, 0, 0.2);
        surge->setModDepth01(surge->storage.getPatch().fx[4].p[3].id, ms_slfo1, 0, 0, 0.3);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 3);
        step(surge);

        surge->reorderFx(0, 1, SurgeSynthesizer::MOVE);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 3);

        REQUIRE(confirmDestinations(surge, {{1, 2}, {1, 3}, {4, 3}}));
    }

    SECTION("Setup a Modulated Combulator and Copy It")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        setFX(surge, 0, fxt_combulator);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);
        step(surge);

        surge->reorderFx(0, 1, SurgeSynthesizer::COPY);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);

        REQUIRE(confirmDestinations(surge, {{0, 2}, {1, 2}}));
    }

    SECTION("Move over a modulated FX")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        setFX(surge, 0, fxt_combulator);
        setFX(surge, 1, fxt_chorus4);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        surge->setModDepth01(surge->storage.getPatch().fx[1].p[3].id, ms_slfo2, 0, 0, 0.1);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);
        confirmDestinations(surge, {{0, 2}, {1, 3}});

        surge->reorderFx(0, 1, SurgeSynthesizer::MOVE);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);
        confirmDestinations(surge, {{1, 2}});
    }

    SECTION("Copy over a modulated FX")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        setFX(surge, 0, fxt_combulator);
        setFX(surge, 1, fxt_chorus4);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        surge->setModDepth01(surge->storage.getPatch().fx[1].p[3].id, ms_slfo2, 0, 0, 0.1);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);
        confirmDestinations(surge, {{0, 2}, {1, 3}});

        surge->reorderFx(0, 1, SurgeSynthesizer::COPY);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);
        confirmDestinations(surge, {{0, 2}, {1, 2}});
    }

    SECTION("Swap Two modulated FX")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        setFX(surge, 0, fxt_combulator);
        setFX(surge, 1, fxt_chorus4);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        surge->setModDepth01(surge->storage.getPatch().fx[1].p[3].id, ms_slfo2, 0, 0, 0.1);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);
        confirmDestinations(surge, {{0, 2}, {1, 3}});

        surge->reorderFx(0, 1, SurgeSynthesizer::SWAP);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);
        confirmDestinations(surge, {{0, 3}, {1, 2}});
    }
}

TEST_CASE("High Samplerate Reverb2", "[fx]")
{
    SECTION("Make Reverb2")
    {
        auto surge = Surge::Headless::createSurge(48000 * 32);
        REQUIRE(surge);

        for (int i = 0; i < 10; ++i)
            surge->process();

        auto *pt = &(surge->storage.getPatch().fx[0].type);
        auto awv = 1.f * fxt_reverb2 / (pt->val_max.i - pt->val_min.i);

        auto did = surge->idForParameter(pt);
        surge->setParameter01(did, awv, false);

        for (int i = 0; i < 10; ++i)
        {
            surge->process();
        }

        surge->playNote(0, 60, 127, 0);
        for (int i = 0; i < 100; ++i)
        {
            surge->process();
        }
        surge->releaseNote(0, 60, 0);
        for (int i = 0; i < 1000; ++i)
        {
            surge->process();
        }
    }
}

TEST_CASE("Waveshaper Pop", "[fx]")
{
    SECTION("Print the Pop")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        for (int i = 0; i < 10; ++i)
            surge->process();

        auto *pt = &(surge->storage.getPatch().fx[0].type);
        auto awv = 1.f * fxt_waveshaper / (pt->val_max.i - pt->val_min.i);

        auto did = surge->idForParameter(pt);
        surge->setParameter01(did, awv, false);

        auto *shaper = &(surge->storage.getPatch().fx[0].p[2]);

        for (int i = 0; i < 10; ++i)
            surge->process();

        shaper->val.i = (int)sst::waveshapers::WaveshaperType::wst_digital;
        for (int i = 0; i < 8; ++i)
            surge->process();

        for (int b = 0; b < 2; ++b)
        {
            surge->process();
            // Print the last 32 samples
            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                std::cout << "DIGI   " << b << " " << std::setw(4) << s << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[0][s] << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[1][s] << std::endl;
            }
        }
        shaper->val.i = (int)sst::waveshapers::WaveshaperType::wst_cheby2;
        for (int b = 0; b < 3; ++b)
        {
            surge->process();

            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                std::cout << "CHEBY2 " << b << " " << std::setw(4) << s << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[0][s] << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[1][s] << std::endl;
            }
        }
    }
}