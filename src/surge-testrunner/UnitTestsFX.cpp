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

#include "SurgeStorage.h"
#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"
#include "AudioInputEffect.h"

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
