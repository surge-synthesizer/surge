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
#include <cmath>
#include <limits>

#include "HeadlessUtils.h"
#include "Player.h"

#include "SurgeStorage.h"
#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"
#include "AudioInputEffect.h"
#include "DelayEffect.h"
#include "DistortionEffect.h"

using namespace Surge::Test;

TEST_CASE("Every FX Is Created And Processes", "[fx]")
{
    for (int i = fxt_off + 1; i < n_fx_types; ++i)
    {
        DYNAMIC_SECTION("FX Testing " << i << " " << fx_type_names[i])
        {
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);

            for (int i = 0; i < 100; ++i)
                surge->process();
            auto *pt = &(surge->storage.getPatch().fx[0].type);
            auto awv = 1.f * i / (pt->val_max.i - pt->val_min.i);

            auto did = surge->idForParameter(pt);
            surge->setParameter01(did, awv, false);

            surge->playNote(0, 60, 100, 0, -1);
            for (int s = 0; s < 100; ++s)
            {
                surge->process();
            }

            surge->releaseNote(0, 60, 100);
            for (int s = 0; s < 20; ++s)
            {
                surge->process();
            }
        }
    }
}

TEST_CASE("Airwindows Loud", "[fx]")
{
    SECTION("Make Loud")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        for (int i = 0; i < 100; ++i)
            surge->process();

        auto *pt = &(surge->storage.getPatch().fx[0].type);
        auto awv = 1.f * float(fxt_airwindows) / (pt->val_max.i - pt->val_min.i);

        auto did = surge->idForParameter(pt);
        surge->setParameter01(did, awv, false);

        auto *pawt = &(surge->storage.getPatch().fx[0].p[0]);

        for (int i = 0; i < 500; ++i)
        {
            pawt->val.i = 34;
            surge->process();

            float soo = 0.f;

            INFO("Swapping Airwindows attempt " << i);
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

TEST_CASE("Move FX With Assigned Modulation", "[fx]")
{
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

    SECTION("Setup Combulator With Modulation And Move It")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        Surge::Test::setFX(surge, 0, fxt_combulator);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);
        step(surge);

        surge->reorderFx(0, 1, SurgeSynthesizer::MOVE);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);

        confirmDestinations(surge, {{1, 2}});
    }

    SECTION("Setup Combulator With Modulation And Another FX Then Move It")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        Surge::Test::setFX(surge, 0, fxt_combulator);
        Surge::Test::setFX(surge, 4, fxt_chorus4);

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

    SECTION("Setup Combulator With Modulation And Copy It")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        Surge::Test::setFX(surge, 0, fxt_combulator);

        surge->setModDepth01(surge->storage.getPatch().fx[0].p[2].id, ms_slfo1, 0, 0, 0.1);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 1);
        step(surge);

        surge->reorderFx(0, 1, SurgeSynthesizer::COPY);
        step(surge);
        REQUIRE(surge->storage.getPatch().modulation_global.size() == 2);

        REQUIRE(confirmDestinations(surge, {{0, 2}, {1, 2}}));
    }

    SECTION("Move Over a Modulated FX")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        Surge::Test::setFX(surge, 0, fxt_combulator);
        Surge::Test::setFX(surge, 1, fxt_chorus4);

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

    SECTION("Copy Over a Modulated FX")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        Surge::Test::setFX(surge, 0, fxt_combulator);
        Surge::Test::setFX(surge, 1, fxt_chorus4);

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

    SECTION("Swap Two Modulated FX")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        step(surge);
        Surge::Test::setFX(surge, 0, fxt_combulator);
        Surge::Test::setFX(surge, 1, fxt_chorus4);

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

TEST_CASE("Reverb 2 at High Sample Rate", "[fx]")
{
    SECTION("Make Reverb 2")
    {
        auto surge = Surge::Headless::createSurge(48000 * 32);
        REQUIRE(surge);

        for (int i = 0; i < 10; ++i)
            surge->process();

        auto *pt = &(surge->storage.getPatch().fx[0].type);
        auto awv = 1.f * float(fxt_reverb2) / (pt->val_max.i - pt->val_min.i);

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

TEST_CASE("Waveshaper Pops", "[fx]")
{
    SECTION("Print The Pop")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        for (int i = 0; i < 10; ++i)
            surge->process();

        auto *pt = &(surge->storage.getPatch().fx[0].type);
        auto awv = 1.f * float(fxt_waveshaper) / (pt->val_max.i - pt->val_min.i);

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
#if 0
            // Print the last 32 samples
            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                std::cout << "DIGI   " << b << " " << std::setw(4) << s << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[0][s] << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[1][s] << std::endl;
            }
#endif
        }
        shaper->val.i = (int)sst::waveshapers::WaveshaperType::wst_cheby2;
        for (int b = 0; b < 3; ++b)
        {
            surge->process();
#if 0
            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                std::cout << "CHEBY2 " << b << " " << std::setw(4) << s << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[0][s] << " " << std::setw(16)
                          << std::setprecision(7) << surge->output[1][s] << std::endl;
            }
#endif
        }
    }
}

TEST_CASE("Nimbus at High Sample Rate", "[fx]")
{
    for (auto base : {44100, 48000})
    {
        for (int m = 1; m <= 8; m *= 2)
        {
            DYNAMIC_SECTION("High Sample Rate " + std::to_string(base) + " * " + std::to_string(m))
            {
                auto surge = Surge::Headless::createSurge(base * m);
                REQUIRE(surge);

                for (int i = 0; i < 100; ++i)
                    surge->process();

                auto *pt = &(surge->storage.getPatch().fx[0].type);
                auto awv = 1.f * float(fxt_nimbus) / (pt->val_max.i - pt->val_min.i);

                auto did = surge->idForParameter(pt);
                surge->setParameter01(did, awv, false);
                surge->process();

                auto sp = [&](auto id, auto val) {
                    auto *pawt = &(surge->storage.getPatch().fx[0].p[id]);
                    auto did = surge->idForParameter(pawt);
                    surge->setParameter01(did, val, false);
                };
                sp(2, 0.5f);  // position
                sp(5, 0.75f); // density
                sp(11, 1.f);  // mix

                surge->playNote(0, 60, 127, 0);
                auto maxAmp = -10.f;
                for (int i = 0; i < 5000 * m; ++i)
                {
                    surge->process();

                    for (int s = 0; s < BLOCK_SIZE; ++s)
                        maxAmp = std::max(maxAmp, surge->output[0][s]);
                }
                REQUIRE(maxAmp > 0.1);
                surge->releaseNote(0, 60, 0);
                for (int i = 0; i < 500; ++i)
                {
                    surge->process();
                }
            }
        }
    }
}

TEST_CASE("Scenes Output Data", "[fx]")
{
    SECTION("Providing data")
    {
        Surge::Storage::ScenesOutputData scenesOutputData{};
        REQUIRE(!scenesOutputData.thereAreClients(0));
        REQUIRE(!scenesOutputData.thereAreClients(1));

        {
            auto clientDataLeft = scenesOutputData.getSceneData(0, 0);
            auto clientDataRight = scenesOutputData.getSceneData(0, 1);
            REQUIRE(clientDataLeft);
            REQUIRE(clientDataRight);
            REQUIRE(scenesOutputData.thereAreClients(0));
            REQUIRE(!scenesOutputData.thereAreClients(1));

            float data[32]{1.0f};
            scenesOutputData.provideSceneData(0, 0, data);
            scenesOutputData.provideSceneData(0, 1, data);
            REQUIRE(scenesOutputData.getSceneData(0, 0).get()[0] == 1.0f);
            REQUIRE(scenesOutputData.getSceneData(0, 1).get()[0] == 1.0f);

            scenesOutputData.provideSceneData(1, 0, data);
            scenesOutputData.provideSceneData(1, 1, data);
            REQUIRE(scenesOutputData.getSceneData(1, 0).get()[0] == 0.0f);
            REQUIRE(scenesOutputData.getSceneData(1, 1).get()[0] == 0.0f);
        }
        REQUIRE(!scenesOutputData.thereAreClients(0));
        REQUIRE(!scenesOutputData.thereAreClients(1));
    }
}

void testExpectedValues(std::shared_ptr<SurgeSynthesizer> surge, int slot, float *leftInput,
                        float *rightInput, float *expectedLeftOutput, float *expectedRightOutput)
{
    surge->fx[slot]->process(leftInput, rightInput);
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE(leftInput[i] == Approx(expectedLeftOutput[i]).margin(0.001));
        REQUIRE(rightInput[i] == Approx(expectedRightOutput[i]).margin(0.001));
    }
}

struct ControlParam
{
    int param;
    float value;
};
struct ExpectedOutput
{
    float expectedLeftOutput[BLOCK_SIZE];
    float expectedRightOutput[BLOCK_SIZE];
};

struct SubTestCase
{
    std::string name;
    std::vector<ControlParam> controlParams;
    ExpectedOutput expectedOutput;
};

struct InParamsGroup
{
    int slot;

    std::string testGroup;
    float leftEffectInput alignas(16)[BLOCK_SIZE];
    float rightEffectInput alignas(16)[BLOCK_SIZE];

    float sceneALeftInput alignas(16)[BLOCK_SIZE];
    float sceneARightInput alignas(16)[BLOCK_SIZE];

    float sceneBLeftInput alignas(16)[BLOCK_SIZE];
    float sceneBRightInput alignas(16)[BLOCK_SIZE];

    float audioLeftInput alignas(16)[BLOCK_SIZE];
    float audioRightInput alignas(16)[BLOCK_SIZE];

    std::vector<SubTestCase> expectedOutput;

    void fillWithData(SurgeStorage *surgeStorage)
    {
        surgeStorage->scenesOutputData.provideSceneData(0, 0, sceneALeftInput);
        surgeStorage->scenesOutputData.provideSceneData(0, 1, sceneARightInput);
        surgeStorage->scenesOutputData.provideSceneData(1, 0, sceneBLeftInput);
        surgeStorage->scenesOutputData.provideSceneData(1, 1, sceneBRightInput);
        memcpy(surgeStorage->audio_in_nonOS[0], audioLeftInput, BLOCK_SIZE * sizeof(float));
        memcpy(surgeStorage->audio_in_nonOS[1], audioRightInput, BLOCK_SIZE * sizeof(float));
    }
};

TEST_CASE("Audio Input Effect", "[fx]")
{

    std::map<int, std::vector<int>> slots{
        {AudioInputEffect::a_insert_slot, {fxslot_ains1, fxslot_ains2, fxslot_ains3, fxslot_ains4}},
        {AudioInputEffect::b_insert_slot, {fxslot_bins1, fxslot_bins2, fxslot_bins3, fxslot_bins4}},
        {AudioInputEffect::send_slot, {fxslot_send1, fxslot_send2, fxslot_send3, fxslot_send4}},
        {AudioInputEffect::global_slot,
         {fxslot_global1, fxslot_global2, fxslot_global3, fxslot_global4}},
    };

    std::vector<InParamsGroup> inParamsGroups{
        {
            AudioInputEffect::a_insert_slot,
            "A Insert",
            {0.1f, 0.1f, 0.1f, 0.1f},     // leftEffectInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // rightEffectInput (half of leftEffectInput)
            {0.2f, 0.2f, 0.2f, 0.2f},     // sceneALeftInput
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneARightInput (half of sceneALeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneBLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // sceneBRightInput (half of sceneBLeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // audioLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // audioRightInput (half of audioLeftInput)
            {
                {"Stereo Output",
                 {{AudioInputEffect::in_output_mix, 1.0f},
                  {AudioInputEffect::in_output_width, 1.0f}},
                 {
                     // ExpectedOutput
                     {0.3f, 0.3f, 0.3f,
                      0.3f}, // expectedLeftOutput (sum of
                             // leftEffectInput, sceneBLeftInput, and audioLeftInput)
                     {0.15f, 0.15f, 0.15f, 0.15f}, // expectedRightOutput (sum of
                                                   // rightEffectInput,
                                                   // sceneBRightInput, and audioRightInput)
                 }},
                {"Switching Left And Right",
                 {{AudioInputEffect::in_output_mix, 1.0f},
                  {AudioInputEffect::in_output_width, -1.0f}},
                 {
                     // ExpectedOutput
                     {0.15f, 0.15f, 0.15f, 0.15f}, // expectedRightOutput (sum of
                                                   // rightEffectInput,
                                                   // sceneBRightInput, and audioRightInput)
                     {0.3f, 0.3f, 0.3f,
                      0.3f}, // expectedLeftOutput (sum of
                             // leftEffectInput, sceneBLeftInput, and audioLeftInput)

                 }},
                {"Mono Output",
                 {
                     {AudioInputEffect::in_output_mix, 1.0f},
                     {AudioInputEffect::in_output_width, 0.0f} // mono
                 },
                 {
                     // ExpectedOutput
                     {0.225f, 0.225f, 0.225f,
                      0.225f}, // expectedLeftOutput (sum of
                               // leftEffectInput, sceneBLeftInput, and audioLeftInput)
                     {0.225f, 0.225f, 0.225f, 0.225f}, // expectedRightOutput (sum of
                                                       // rightEffectInput,
                                                       // sceneBRightInput, and audioRightInput)
                 }},
                {"Only Dry Signal, Width = 1",
                 {
                     {AudioInputEffect::in_output_mix, 0.0f},
                     {AudioInputEffect::in_output_width, 1.0f} // stereo
                 },
                 {
                     // ExpectedOutput stays attached
                     {0.1f, 0.1f, 0.1f, 0.1f}, //
                     {0.05f, 0.05f, 0.05f, 0.05f},
                 }

                },
                {"Only Dry Signal, Width = 0",
                 {
                     {AudioInputEffect::in_output_mix, 0.0f},
                     {AudioInputEffect::in_output_width, 0.0f} // stereo
                 },
                 {
                     // ExpectedOutput stays attached
                     {0.1f, 0.1f, 0.1f, 0.1f}, //
                     {0.05f, 0.05f, 0.05f, 0.05f},
                 }

                },
            },

        },
        {
            AudioInputEffect::b_insert_slot,
            "B Insert",
            {0.1f, 0.1f, 0.1f, 0.1f},     // leftEffectInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // rightEffectInput (half of leftEffectInput)
            {0.2f, 0.2f, 0.2f, 0.2f},     // sceneALeftInput
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneARightInput (half of sceneALeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneBLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // sceneBRightInput (half of sceneBLeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // audioLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // audioRightInput (half of audioLeftInput)
            {{"Stereo Output",
              {{AudioInputEffect::in_output_mix, 1.0f}, {AudioInputEffect::in_output_width, 1.0f}},
              {
                  {0.4f, 0.4f, 0.4f, 0.4f}, // expectedLeftOutput (sum of leftEffectInput,
                                            //  sceneALeftInput, and audioLeftInput)
                  {0.2f, 0.2f, 0.2f, 0.2f}, // expectedRightOutput (sum of rightEffectInput,
                                            // sceneARightInput, and audioRightInput)
              }}},
        },

        {
            AudioInputEffect::send_slot,
            "Send",
            {0.1f, 0.1f, 0.1f, 0.1f},     // leftEffectInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // rightEffectInput (half of leftEffectInput)
            {0.2f, 0.2f, 0.2f, 0.2f},     // sceneALeftInput
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneARightInput (half of sceneALeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneBLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // sceneBRightInput (half of sceneBLeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // audioLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // audioRightInput (half of audioLeftInput)
            {{"Stereo Output",
              {{AudioInputEffect::in_output_mix, 1.0f}, {AudioInputEffect::in_output_width, 1.0f}},
              {
                  // ExpectedOutput
                  {0.2f, 0.2f, 0.2f,
                   0.2f}, // expectedLeftOutput (sum of leftEffectInput and audioLeftInput)
                  {0.1f, 0.1f, 0.1f, 0.1f}, // expectedRightOutput (sum of rightEffectInput and
                                            // audioRightInput)
              }}},
        },
        {
            AudioInputEffect::global_slot,
            "Global",
            {0.1f, 0.1f, 0.1f, 0.1f},     // leftEffectInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // rightEffectInput (half of leftEffectInput)
            {0.2f, 0.2f, 0.2f, 0.2f},     // sceneALeftInput
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneARightInput (half of sceneALeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // sceneBLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // sceneBRightInput (half of sceneBLeftInput)
            {0.1f, 0.1f, 0.1f, 0.1f},     // audioLeftInput
            {0.05f, 0.05f, 0.05f, 0.05f}, // audioRightInput (half of audioLeftInput)
            {{"Stereo Output",
              {{AudioInputEffect::in_output_mix, 1.0f}, {AudioInputEffect::in_output_width, 1.0f}},
              {
                  // ExpectedOutput
                  {0.2f, 0.2f, 0.2f,
                   0.2f}, // expectedLeftOutput (sum of leftEffectInput and audioLeftInput)
                  {0.1f, 0.1f, 0.1f, 0.1f}, // expectedRightOutput (sum of rightEffectInput and
                                            // audioRightInput)
              }}},
        },

    };
    std::vector<InParamsGroup> panningTestCases = {
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, Default Params",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBLeftInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Result Should Be Unchanged",
              {
                  {AudioInputEffect::in_scene_input_channel, 0.0f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 0.0f},
              },
              {
                  // ExpectedOutput
                  {0.4f, 0.2f, 0.4f,
                   0.2f}, // expectedLeftOutput (sum of leftEffectInput and audioLeftInput)
                  {0.2f, 0.4f, 0.2f,
                   0.4f}, // expectedRightOutput (sum of rightEffectInput and audioRightInput)
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_channel = -1",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBLeftInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Only Left Channel Should Be Accepted",
              {
                  {AudioInputEffect::in_scene_input_channel, -1.0f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 0.0f},
              },
              {
                  // ExpectedOutput
                  {0.4f, 0.2f, 0.4f, 0.2f}, // expectedLeftOutput
                  {0.0f, 0.0f, 0.0f, 0.0f}, // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_channel = 0.25",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBLeftInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Accepts 50% Left Channel, 100% Right Channel",
              {
                  {AudioInputEffect::in_scene_input_channel, 0.25f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 0.0f},
              },
              {
                  // ExpectedOutput
                  {0.3f, 0.15f, 0.3f, 0.15f}, // expectedLeftOutput
                  {0.2f, 0.4f, 0.2f, 0.4f},   // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_channel = -0.50",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBLeftInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Accepts 100% Left Channel, 50% Right Channel",
              {
                  {AudioInputEffect::in_scene_input_channel, -0.50f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 0.0f},
              },
              {
                  // ExpectedOutput
                  {0.4f, 0.2f, 0.4f, 0.2f}, // expectedLeftOutput
                  {0.1f, 0.2f, 0.1f, 0.2f}, // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_channel = -0.50, Input "
            "Level = -5.995",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBLeftInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Accepts 100% Left Channel, 50% Right Channel, 50% Input Level",
              {
                  {AudioInputEffect::in_scene_input_channel, -0.50f},
                  {AudioInputEffect::in_scene_input_level, -5.995f},
                  {AudioInputEffect::in_scene_input_pan, 0.0f},
              },
              {
                  // ExpectedOutput
                  {0.2f, 0.1f, 0.2f, 0.1f},   // expectedLeftOutput
                  {0.05f, 0.1f, 0.05f, 0.1f}, // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_pan = -1.0",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBRightInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Channels Should Move to Left",
              {
                  {AudioInputEffect::in_scene_input_channel, 0.0f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, -1.0f},
              },
              {
                  // ExpectedOutput
                  {0.6f, 0.6f, 0.6f, 0.6f}, // expectedLeftOutput
                  {0.0f, 0.0f, 0.0f, 0.0f}, // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_pan = 1.0",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBRightInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Channels Should Move to Right",
              {
                  {AudioInputEffect::in_scene_input_channel, 0.0f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 1.0f},
              },
              {
                  // ExpectedOutput
                  {0.0f, 0.0f, 0.0f, 0.0f}, // expectedLeftOutput
                  {0.6f, 0.6f, 0.6f, 0.6f}, // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_pan = 0.5",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARight Input
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBRightInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Channels Should Move to 50% Right",
              {
                  {AudioInputEffect::in_scene_input_channel, 0.0f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 0.5f},
              },
              {
                  // ExpectedOutput
                  {0.2f, 0.1f, 0.2f, 0.1f}, // expectedLeftOutput
                  {0.4f, 0.5f, 0.4f, 0.5f}, // expectedRightOutput
              }}},
        },
        {
            AudioInputEffect::a_insert_slot,
            "Apply Pan to Audio Input, in_scene_input_channel = -1, "
            "in_scene_input_pan = 1.0",
            {0.0f}, // leftEffectInput
            {0.0f}, // rightEffectInput
            {},
            {}, // sceneALeftInput and sceneARightInput
            {
                0.4f,
                0.2f,
                0.4f,
                0.2f,
            }, // sceneBLeftInput
            {
                0.2f,
                0.4f,
                0.2f,
                0.4f,
            }, // sceneBRightInput
            {},
            {}, // audioLeftInput and audioRightInput
            {{"Left Channels Should Move to Right, Right Channel Should Be Deleted",
              {
                  {AudioInputEffect::in_scene_input_channel, -1.0f},
                  {AudioInputEffect::in_scene_input_level, 0.0f},
                  {AudioInputEffect::in_scene_input_pan, 1.0f},
              },
              {
                  // ExpectedOutput
                  {0.0f, 0.0f, 0.0f, 0.0f}, // expectedLeftOutput
                  {0.4f, 0.2f, 0.4f, 0.2f}, // expectedRightOutput
              }}},
        },

    };
    for (InParamsGroup &panningTestCase : panningTestCases)
    {
        /// ==================== Test with audio input from scene B ====================
        std::string testGroup = panningTestCase.testGroup;
        panningTestCase.testGroup = testGroup + " (audio input from scene B)";
        inParamsGroups.push_back(panningTestCase);

        /// ==================== Test with audio input from scene A ====================
        panningTestCase.slot = AudioInputEffect::b_insert_slot;
        memcpy(panningTestCase.sceneALeftInput, panningTestCase.sceneBLeftInput,
               BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.sceneARightInput, panningTestCase.sceneBRightInput,
               BLOCK_SIZE * sizeof(float));
        panningTestCase.testGroup = testGroup + " (audio input from scene A)";
        inParamsGroups.push_back(panningTestCase);

        /// ==================== Test with audio input from a mic ====================
        panningTestCase.testGroup = testGroup + " (audio input is from a mic)";
        panningTestCase.slot = AudioInputEffect::a_insert_slot;
        memcpy(panningTestCase.audioLeftInput, panningTestCase.sceneBLeftInput,
               BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.audioRightInput, panningTestCase.sceneBRightInput,
               BLOCK_SIZE * sizeof(float));
        panningTestCase.expectedOutput[0].controlParams[0].param =
            AudioInputEffect::in_audio_input_channel;
        panningTestCase.expectedOutput[0].controlParams[1].param =
            AudioInputEffect::in_audio_input_level;
        panningTestCase.expectedOutput[0].controlParams[2].param =
            AudioInputEffect::in_audio_input_pan;
        float zeros[BLOCK_SIZE]{0.0f};
        memcpy(panningTestCase.sceneALeftInput, zeros, BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.sceneARightInput, zeros, BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.sceneBLeftInput, zeros, BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.sceneBRightInput, zeros, BLOCK_SIZE * sizeof(float));
        inParamsGroups.push_back(panningTestCase);

        /// ==================== Test with audio effect input  ====================
        panningTestCase.testGroup = testGroup + " (audio input is from an audio effect)";
        panningTestCase.slot = AudioInputEffect::a_insert_slot;
        memcpy(panningTestCase.leftEffectInput, panningTestCase.audioLeftInput,
               BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.rightEffectInput, panningTestCase.audioRightInput,
               BLOCK_SIZE * sizeof(float));
        panningTestCase.expectedOutput[0].controlParams[0].param =
            AudioInputEffect::in_effect_input_channel;
        panningTestCase.expectedOutput[0].controlParams[1].param =
            AudioInputEffect::in_effect_input_level;
        panningTestCase.expectedOutput[0].controlParams[2].param =
            AudioInputEffect::in_effect_input_pan;
        memcpy(panningTestCase.audioLeftInput, zeros, BLOCK_SIZE * sizeof(float));
        memcpy(panningTestCase.audioRightInput, zeros, BLOCK_SIZE * sizeof(float));

        inParamsGroups.push_back(panningTestCase);
    }

    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge);
    SurgeStorage *surgeStorage = &surge->storage;

    for (InParamsGroup &inParamsGroup : inParamsGroups)
    {
        SECTION(inParamsGroup.testGroup)
        {
            for (int slot : slots[inParamsGroup.slot])
            {
                Surge::Test::setFX(surge, slot, fxt_audio_input);
                FxStorage *fxStorage = &surgeStorage->getPatch().fx[slot];
                fxStorage->p[AudioInputEffect::in_audio_input_channel].val.f = 0.0f;
                fxStorage->p[AudioInputEffect::in_audio_input_level].val.f = 0.0f;
                fxStorage->p[AudioInputEffect::in_audio_input_pan].val.f = 0.0f;

                fxStorage->p[AudioInputEffect::in_scene_input_channel].val.f = 0.0f;
                fxStorage->p[AudioInputEffect::in_scene_input_level].val.f = 0.0f;
                fxStorage->p[AudioInputEffect::in_scene_input_pan].val.f = 0.0f;

                fxStorage->p[AudioInputEffect::in_effect_input_channel].val.f = 0.0f;
                fxStorage->p[AudioInputEffect::in_effect_input_level].val.f = 0.0f;
                fxStorage->p[AudioInputEffect::in_effect_input_pan].val.f = 0.0f;

                fxStorage->p[AudioInputEffect::in_output_width].val.f = 1.0f;
                fxStorage->p[AudioInputEffect::in_output_mix].val.f = 1.0f;
                REQUIRE(fxStorage->type.val.i == fxt_audio_input);

                for (SubTestCase &subTestCase : inParamsGroup.expectedOutput)
                {
                    SECTION(inParamsGroup.testGroup + ", slot " + std::to_string(slot) + ", " +
                            subTestCase.name)
                    {
                        ExpectedOutput &expectedOutput = subTestCase.expectedOutput;
                        for (ControlParam &controlParam : subTestCase.controlParams)
                        {
                            fxStorage->p[controlParam.param].val.f = controlParam.value;
                        }
                        inParamsGroup.fillWithData(surgeStorage);

                        // This call simulated loading the modulation stack from surge->process
                        // while still allowing direct fx->process to work
                        surge->storage.getPatch().copy_globaldata(
                            surge->storage.getPatch().globaldata);

                        surge->fx[slot]->init();
                        surge->fx[slot]->process(inParamsGroup.leftEffectInput,
                                                 inParamsGroup.rightEffectInput);

                        for (int i = 0; i < 4; ++i)
                        {
                            REQUIRE(inParamsGroup.leftEffectInput[i] ==
                                    Approx(expectedOutput.expectedLeftOutput[i]).margin(0.001));
                            REQUIRE(expectedOutput.expectedRightOutput[i] ==
                                    Approx(expectedOutput.expectedRightOutput[i]).margin(0.001));
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("Distortion Digital Waveshaper Does Not Diverge", "[fx]")
{
    // Regression test for the distortion effect blowing up when:
    // 1. The DIGITAL waveshaper is selected (it maps near-zero input to non-zero output)
    // 2. Drive is very low (making the drive^-1 scaling factor enormous)
    // 3. Feedback is large and negative
    //
    // The bug: drive.multiply_2_blocks scales the input down by a small drive factor,
    // then for SSE shapers the code scales back up by dInv = 1/drive (huge for low drive).
    // DIGITAL's non-zero output at zero input, multiplied through this large dInv, combined
    // with strong negative feedback causes the feedback state L,R to diverge to ~1e7.
    //
    // Both sections below should pass after the fix. They will FAIL with the current code.

    SECTION("Trance Pluck Patch Does Not Blow Out")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        for (int i = 0; i < 10; ++i)
            surge->process();

        auto patchPath = fs::path{"resources/data/patches_3rdparty"} / "Damon Armani" / "Plucks" /
                         "Trance Pluck.fxp";
        INFO("Patch path: " << path_to_string(patchPath));
        REQUIRE(fs::exists(patchPath));
        surge->loadPatchByPath(path_to_string(patchPath).c_str(), -1, "Damon Armani");

        for (int i = 0; i < 10; ++i)
            surge->process();

        surge->playNote(0, 60, 100, 0);

        float maxOut = 0.f;
        for (int i = 0; i < 500; ++i)
        {
            surge->process();
            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                maxOut = std::max(maxOut, std::fabs(surge->output[0][s]));
                maxOut = std::max(maxOut, std::fabs(surge->output[1][s]));
            }
            std::cout << "block " << i << " maxout = " << maxOut << std::endl;
        }

        // Normal audio output should be well under 10; divergence reaches ~1e7
        INFO("Max output amplitude: " << maxOut);
        REQUIRE(maxOut < 7.9f);
    }

    SECTION("Manual Config: Digital Waveshaper, Low Drive, Negative Feedback")
    {
        // Same conditions replicated programmatically so the test works without
        // the 3rd-party patch file.
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        for (int i = 0; i < 10; ++i)
            surge->process();

        Surge::Test::setFX(surge, 0, fxt_distortion);

        auto &fxp = surge->storage.getPatch().fx[0];

        // DIGITAL is index 4 in FXWaveShapers (wst_soft=0, wst_hard=1, wst_asym=2,
        // wst_sine=3, wst_digital=4)
        auto *modelParam = &fxp.p[DistortionEffect::dist_model];
        surge->setParameter01(surge->idForParameter(modelParam),
                              4.f / (modelParam->val_max.i - modelParam->val_min.i), false);

        // Drive at minimum (-24 dB) so dInv = 1/drive is very large
        auto *driveParam = &fxp.p[DistortionEffect::dist_drive];
        surge->setParameter01(surge->idForParameter(driveParam), 0.0f, false);

        // Large negative feedback: -0.9 maps to 01-value = (-0.9 - (-1)) / (1 - (-1)) = 0.05
        auto *fbParam = &fxp.p[DistortionEffect::dist_feedback];
        surge->setParameter01(surge->idForParameter(fbParam), 0.05f, false);

        for (int i = 0; i < 20; ++i)
            surge->process();

        surge->playNote(0, 60, 100, 0);

        float maxOut = 0.f;
        for (int i = 0; i < 500; ++i)
        {
            surge->process();
            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                maxOut = std::max(maxOut, std::fabs(surge->output[0][s]));
                maxOut = std::max(maxOut, std::fabs(surge->output[1][s]));
                std::cout << "SO " << surge->output[0][s] << " " << surge->output[1][s]
                          << std::endl;
            }
        }

        // Normal output should stay well under 10; divergence reaches ~1e7
        INFO("Max output amplitude: " << maxOut);
        REQUIRE(maxOut < 7.9f);
    }
}

TEST_CASE("Stopping Sound Clears Poisoned FX State", "[fx]") // See issue 8240
{
    // A single non-finite sample that reaches an effect's internal state must not survive
    // stopSound(). Otherwise it recirculates forever, and the output hard clipper turns it
    // into a sustained full scale signal that no volume control can attenuate.
    for (int t = fxt_off + 1; t < n_fx_types; ++t)
    {
        DYNAMIC_SECTION("FX " << t << " " << fx_type_names[t])
        {
            auto surge = Surge::Headless::createSurge(48000);
            REQUIRE(surge);

            Surge::Test::setFX(surge, 0, (fx_type)t);
            REQUIRE(surge->fx[0]);

            float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

            std::fill(L, L + BLOCK_SIZE, std::numeric_limits<float>::quiet_NaN());
            std::fill(R, R + BLOCK_SIZE, std::numeric_limits<float>::quiet_NaN());
            surge->fx[0]->process(L, R);

            surge->stopSound();

            for (int b = 0; b < 100; ++b)
            {
                std::fill(L, L + BLOCK_SIZE, 0.f);
                std::fill(R, R + BLOCK_SIZE, 0.f);
                surge->fx[0]->process(L, R);

                INFO("Block " << b);
                for (int s = 0; s < BLOCK_SIZE; ++s)
                {
                    REQUIRE(std::isfinite(L[s]));
                    REQUIRE(std::isfinite(R[s]));
                }
            }
        }
    }
}

namespace
{
struct DelayLineModeProbe
{
    int lineModeLeft{0};
    int lineModeRight{0};
    bool linkRight{false};
    float startSeconds{0.1f};
    float endSeconds{0.2f};
    float modRate{0.f};
    float modDepth{0.f};
    int channel{0};

    static constexpr int sampleRate{44100};

    std::shared_ptr<SurgeSynthesizer> makeDelay() const
    {
        auto surge = Surge::Headless::createSurge(sampleRate);
        REQUIRE(surge);

        Surge::Test::setFX(surge, 0, fxt_delay);

        auto &patch = surge->storage.getPatch();
        auto *fx = &(patch.fx[0]);

        fx->p[DelayEffect::dly_time_left].val.f = std::log2(startSeconds);
        fx->p[DelayEffect::dly_time_right].val.f = std::log2(startSeconds);
        fx->p[DelayEffect::dly_time_right].deactivated = linkRight;
        fx->p[DelayEffect::dly_time_left].deform_type = lineModeLeft;
        fx->p[DelayEffect::dly_time_right].deform_type = lineModeRight;
        fx->p[DelayEffect::dly_feedback].val.f = 0.f;
        fx->p[DelayEffect::dly_crossfeed].val.f = 0.f;
        fx->p[DelayEffect::dly_mod_rate].val.f = modRate;
        fx->p[DelayEffect::dly_mod_depth].val.f = modDepth;
        fx->p[DelayEffect::dly_input_channel].val.f = 0.f;
        fx->p[DelayEffect::dly_mix].val.f = 1.f;
        fx->p[DelayEffect::dly_width].val.f = 0.f;
        fx->p[DelayEffect::dly_lowcut].deactivated = true;
        fx->p[DelayEffect::dly_highcut].deactivated = true;

        patch.copy_globaldata(patch.globaldata);
        surge->fx[0]->init();

        return surge;
    }

    /*
     * Render an impulse through a 100% wet, feedback free Dual Delay whose delay time
     * changes from startSeconds to endSeconds in the same block the impulse arrives in,
     * and report the sample index at which the delayed impulse comes back out.
     */
    int impulseReturnIndex() const
    {
        auto surge = makeDelay();
        auto &patch = surge->storage.getPatch();
        auto *fx = &(patch.fx[0]);

        float dataL alignas(16)[BLOCK_SIZE], dataR alignas(16)[BLOCK_SIZE];

        auto runBlock = [&](bool impulse) {
            std::fill(dataL, dataL + BLOCK_SIZE, 0.f);
            std::fill(dataR, dataR + BLOCK_SIZE, 0.f);

            if (impulse)
            {
                dataL[0] = 1.f;
                dataR[0] = 1.f;
            }

            surge->fx[0]->process(dataL, dataR);
        };

        for (int b = 0; b < 100; ++b)
        {
            runBlock(false);
        }

        fx->p[DelayEffect::dly_time_left].val.f = std::log2(endSeconds);
        fx->p[DelayEffect::dly_time_right].val.f = std::log2(endSeconds);
        patch.copy_globaldata(patch.globaldata);

        int peakIndex = -1;
        float peakValue = 0.f;

        for (int b = 0, n = 0; b < sampleRate / BLOCK_SIZE; ++b)
        {
            runBlock(b == 0);

            for (int k = 0; k < BLOCK_SIZE; ++k, ++n)
            {
                auto v = std::fabs(channel == 0 ? dataL[k] : dataR[k]);

                if (v > peakValue)
                {
                    peakValue = v;
                    peakIndex = n;
                }
            }
        }

        INFO("Delayed impulse peaked at " << peakValue);
        REQUIRE(peakValue > 0.05f);

        return peakIndex;
    }

    /*
     * Feed a steady sine through the same delay and report the largest sample to sample
     * step in the output, once while the delay time is held and once across the change
     * from startSeconds to endSeconds. A tap that is repositioned without a crossfade
     * splices two unrelated phases of the sine together, which shows up here as a step
     * far larger than the sine's own slope.
     */
    std::pair<float, float> sineStepDiscontinuity() const
    {
        auto surge = makeDelay();
        auto &patch = surge->storage.getPatch();
        auto *fx = &(patch.fx[0]);

        // deliberately not a whole number of cycles per delay time, so that the two taps
        // are out of phase with each other
        constexpr double twoPi{6.283185307179586};
        const double dPhase = twoPi * 437.0 / sampleRate;
        double phase = 0.0;

        float dataL alignas(16)[BLOCK_SIZE], dataR alignas(16)[BLOCK_SIZE];
        float previous = 0.f;
        bool havePrevious = false;

        auto runBlock = [&](float *maxDelta) {
            for (int k = 0; k < BLOCK_SIZE; ++k)
            {
                dataL[k] = (float)std::sin(phase);
                dataR[k] = dataL[k];
                phase += dPhase;
            }

            surge->fx[0]->process(dataL, dataR);

            for (int k = 0; k < BLOCK_SIZE; ++k)
            {
                if (havePrevious && maxDelta)
                {
                    *maxDelta = std::max(*maxDelta, std::fabs(dataL[k] - previous));
                }

                previous = dataL[k];
                havePrevious = true;
            }
        };

        for (int b = 0; b < 400; ++b)
        {
            runBlock(nullptr);
        }

        float held = 0.f;

        for (int b = 0; b < 200; ++b)
        {
            runBlock(&held);
        }

        fx->p[DelayEffect::dly_time_left].val.f = std::log2(endSeconds);
        fx->p[DelayEffect::dly_time_right].val.f = std::log2(endSeconds);
        patch.copy_globaldata(patch.globaldata);

        float changing = 0.f;

        for (int b = 0; b < 200; ++b)
        {
            runBlock(&changing);
        }

        return {held, changing};
    }

    /*
     * A crossfade between two taps holding correlated material cannot be made
     * transparent: the taps can land in antiphase and cancel at the midpoint whatever
     * the fade length. So the thing Clean mode controls is how OFTEN it pays that cost.
     *
     * Sweep the delay time continuously the way host automation would, through a 100%
     * wet, feedback free delay carrying a steady tone, and count the resulting dropouts.
     * Each retime shows up as a dip in the tone's level; a delay that chases the
     * automation block by block produces one every fade length, which is what gets heard
     * as the time being chewed rather than swept.
     */
    int retimeEventsDuringSweep(float toneHz, float fromSeconds, float toSeconds,
                                float sweepSeconds) const
    {
        auto surge = makeDelay();
        auto &patch = surge->storage.getPatch();
        auto *fx = &(patch.fx[0]);

        constexpr double twoPi{6.283185307179586};
        const double dPhase = twoPi * toneHz / sampleRate;
        double phase = 0.0;

        float dataL alignas(16)[BLOCK_SIZE], dataR alignas(16)[BLOCK_SIZE];
        std::vector<float> captured;

        auto runBlock = [&](bool keep) {
            for (int k = 0; k < BLOCK_SIZE; ++k)
            {
                dataL[k] = (float)std::sin(phase);
                dataR[k] = dataL[k];
                phase += dPhase;
            }

            surge->fx[0]->process(dataL, dataR);

            if (keep)
            {
                captured.insert(captured.end(), dataL, dataL + BLOCK_SIZE);
            }
        };

        // fill the line and settle
        for (int b = 0; b < (int)(2.5 * sampleRate / BLOCK_SIZE); ++b)
        {
            runBlock(false);
        }

        auto blocks = (int)(sweepSeconds * sampleRate / BLOCK_SIZE);
        auto from = std::log2(fromSeconds), to = std::log2(toSeconds);

        for (int b = 0; b < blocks; ++b)
        {
            auto v = (float)(from + (to - from) * b / (double)blocks);

            fx->p[DelayEffect::dly_time_left].val.f = v;
            fx->p[DelayEffect::dly_time_right].val.f = v;
            patch.copy_globaldata(patch.globaldata);

            runBlock(true);
        }

        // level per period of the tone, so the tone's own ripple is averaged away
        auto frame = (int)std::round(sampleRate / toneHz);
        std::vector<float> level;

        for (size_t i = 0; i + frame <= captured.size(); i += frame)
        {
            double s = 0;

            for (int k = 0; k < frame; ++k)
            {
                s += (double)captured[i + k] * captured[i + k];
            }

            level.push_back((float)std::sqrt(s / frame));
        }

        REQUIRE(level.size() > 50);

        std::vector<float> sorted(level);
        std::sort(sorted.begin(), sorted.end());
        auto steady = sorted[sorted.size() * 3 / 4];
        auto threshold = steady * 0.7f; // about 3 dB down

        int events = 0;
        bool inDip = false;

        for (auto v : level)
        {
            if (v < threshold && !inDip)
            {
                ++events;
                inDip = true;
            }
            else if (v >= threshold)
            {
                inDip = false;
            }
        }

        return events;
    }
};
} // namespace

TEST_CASE("Dual Delay Line Modes", "[fx]")
{
    SECTION("Tape mode glides to the new delay time")
    {
        DelayLineModeProbe probe;
        probe.lineModeLeft = DelayEffect::dly_line_tape;
        probe.lineModeRight = DelayEffect::dly_line_tape;

        auto idx = probe.impulseReturnIndex();

        INFO("Impulse returned at sample " << idx);
        // The tap is still gliding when the impulse catches up with it, so the impulse
        // comes back out well before the newly requested 0.2s
        REQUIRE(idx > 0.1 * DelayLineModeProbe::sampleRate);
        REQUIRE(idx < 0.2 * DelayLineModeProbe::sampleRate - 1000);
    }

    SECTION("Clean mode applies the new delay time immediately")
    {
        DelayLineModeProbe probe;
        probe.lineModeLeft = DelayEffect::dly_line_clean;
        probe.lineModeRight = DelayEffect::dly_line_clean;

        auto idx = probe.impulseReturnIndex();

        INFO("Impulse returned at sample " << idx);
        REQUIRE(idx == Approx(0.2 * DelayLineModeProbe::sampleRate).margin(64));
    }

    SECTION("Clean mode crossfades to the new delay time rather than splicing")
    {
        DelayLineModeProbe probe;
        probe.lineModeLeft = DelayEffect::dly_line_clean;
        probe.lineModeRight = DelayEffect::dly_line_clean;

        auto [held, changing] = probe.sineStepDiscontinuity();

        INFO("Largest output step was " << held << " while held and " << changing
                                        << " across the change");
        REQUIRE(changing < 3 * held);
    }

    SECTION("Clean mode keeps the mod LFO gliding so Rate and Depth still chorus")
    {
        DelayLineModeProbe unmodulated;
        unmodulated.lineModeLeft = DelayEffect::dly_line_clean;
        unmodulated.lineModeRight = DelayEffect::dly_line_clean;

        auto modulated = unmodulated;
        // a fast, deep vibrato, so the LFO never settles and every block asks the tap to
        // move by a meaningful amount
        modulated.modRate = 5.f;
        modulated.modDepth = 2.f;

        auto flat = unmodulated.sineStepDiscontinuity().first;
        auto chorused = modulated.sineStepDiscontinuity().first;

        INFO("Largest output step was " << flat << " unmodulated and " << chorused << " modulated");
        // pitch modulation raises the sine's own slope a little, but a tap that stepped
        // at block rate instead of gliding would be several times worse
        REQUIRE(chorused < 2 * flat);
    }

    SECTION("Clean mode retimes sparingly while the delay time is swept")
    {
        DelayLineModeProbe probe;
        probe.lineModeLeft = DelayEffect::dly_line_clean;
        probe.lineModeRight = DelayEffect::dly_line_clean;
        probe.startSeconds = 1.f;

        auto events = probe.retimeEventsDuringSweep(250.f, 1.f, 0.05f, 2.f);

        INFO("Dropped out " << events << " times across the sweep");
        // only the retimes whose taps land near antiphase actually dip, so this counts
        // audible dropouts rather than retimes. Spacing them by half a lap of the line
        // measures 10 here, which is also what Replika's Modern mode measures on
        // comparable material; shortening the fade tenfold, so that retimes come ten
        // times as often, measures 50
        REQUIRE(events < 20);
    }

    SECTION("A linked Right channel follows the Left channel's line mode")
    {
        DelayLineModeProbe probe;
        probe.lineModeLeft = DelayEffect::dly_line_clean;
        probe.lineModeRight = DelayEffect::dly_line_tape;
        probe.linkRight = true;
        probe.channel = 1;

        auto idx = probe.impulseReturnIndex();

        INFO("Impulse returned at sample " << idx);
        REQUIRE(idx == Approx(0.2 * DelayLineModeProbe::sampleRate).margin(64));
    }

    SECTION("An unlinked Right channel keeps its own line mode")
    {
        DelayLineModeProbe probe;
        probe.lineModeLeft = DelayEffect::dly_line_clean;
        probe.lineModeRight = DelayEffect::dly_line_tape;
        probe.linkRight = false;
        probe.channel = 1;

        auto idx = probe.impulseReturnIndex();

        INFO("Impulse returned at sample " << idx);
        REQUIRE(idx < 0.2 * DelayLineModeProbe::sampleRate - 1000);
    }
}
