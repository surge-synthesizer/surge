#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "FastMath.h"
#include "AudioInputEffect.h"

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
        Surge::Test::setFX(surge, 0, fxt_combulator);

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

    SECTION("Setup a Modulated Combulator and Copy It")
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

    SECTION("Move over a modulated FX")
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

    SECTION("Copy over a modulated FX")
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

    SECTION("Swap Two modulated FX")
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

TEST_CASE("High SampleRate Nimbus", "[fx]")
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
                auto awv = 1.f * fxt_nimbus / (pt->val_max.i - pt->val_min.i);

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

TEST_CASE("ScenesOutputData",  "[fx]") {
    SECTION("Providing data")
    {
        ScenesOutputData scenesOutputData{};
        REQUIRE(!scenesOutputData.thereAreClients(0));
        REQUIRE(!scenesOutputData.thereAreClients(1));

        {
            auto clientDataLeft = scenesOutputData.getSceneData(0,0);
            auto clientDataRight = scenesOutputData.getSceneData(0,1);
            REQUIRE(clientDataLeft);
            REQUIRE(clientDataRight);
            REQUIRE(scenesOutputData.thereAreClients(0));
            REQUIRE(!scenesOutputData.thereAreClients(1));

            float data[32]{1.0f};
            scenesOutputData.provideSceneData(0,0, data);
            scenesOutputData.provideSceneData(0,1, data);
            REQUIRE(scenesOutputData.getSceneData(0,0)[0] == 1.0f);
            REQUIRE(scenesOutputData.getSceneData(0,1)[0] == 1.0f);

            scenesOutputData.provideSceneData(1,0, data);
            scenesOutputData.provideSceneData(1,1, data);
            REQUIRE(scenesOutputData.getSceneData(1,0)[0] == 0.0f);
            REQUIRE(scenesOutputData.getSceneData(1,1)[0] == 0.0f);
        }
        REQUIRE(!scenesOutputData.thereAreClients(0));
        REQUIRE(!scenesOutputData.thereAreClients(1));
    }
}

void testExpectedValues(std::shared_ptr<SurgeSynthesizer>surge, int slot,
                        float *leftInput, float *rightInput,
                        float *expectedLeftInput, float *expectedRightInput)
{
    surge->fx[slot]->process(leftInput, rightInput);
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE(leftInput[i] == Approx(expectedLeftInput[i]).margin(0.001));
        REQUIRE(rightInput[i] == Approx(expectedRightInput[i]).margin(0.001));
    }
}
struct InParamsGroup {
    int slot;

    std::string testGroup;
    AudioInputEffect::in_params inputChannel;
    AudioInputEffect::in_params inputLevel;
    AudioInputEffect::in_params inputPan;
    float leftEffectInput  alignas(16)[BLOCK_SIZE];
    float rightEffectInput  alignas(16)[BLOCK_SIZE];

    float sceneALeftInput  alignas(16)[BLOCK_SIZE];
    float sceneARightInput  alignas(16)[BLOCK_SIZE];

    float sceneBLeftInput  alignas(16)[BLOCK_SIZE];
    float sceneBRightInput  alignas(16)[BLOCK_SIZE];

    float audioLeftInput  alignas(16)[BLOCK_SIZE];
    float audioRightInput  alignas(16)[BLOCK_SIZE];

    float expectedLeftInput[BLOCK_SIZE];
    float expectedRightInput[BLOCK_SIZE];

    void fillWithData(SurgeStorage* surgeStorage) {
        surgeStorage->scenesOutputData.provideSceneData(0, 0, sceneALeftInput);
        surgeStorage->scenesOutputData.provideSceneData(0, 1, sceneARightInput);
        surgeStorage->scenesOutputData.provideSceneData(1, 0, sceneBLeftInput);
        surgeStorage->scenesOutputData.provideSceneData(1, 1, sceneBRightInput);
        copy_block(audioLeftInput, surgeStorage->audio_in_nonOS[0], BLOCK_SIZE_QUAD);
        copy_block(audioRightInput, surgeStorage->audio_in_nonOS[1], BLOCK_SIZE_QUAD);
    }
};

TEST_CASE("AudioInputEffect: channels panning",  "[fx]")
{
    std::map<int, std::vector<int>> slots {
        {AudioInputEffect::a_insert_slot,
                {fxslot_ains1, fxslot_ains2, fxslot_ains3, fxslot_ains4}},
        {AudioInputEffect::b_insert_slot,
                {fxslot_bins1, fxslot_bins2, fxslot_bins3, fxslot_bins4}},
        {AudioInputEffect::send_slot,
                {fxslot_send1, fxslot_send2, fxslot_send3, fxslot_send4}},
        {AudioInputEffect::global_slot,
                {fxslot_global1, fxslot_global2, fxslot_global3, fxslot_global4}},
    };
    std::vector<InParamsGroup> inParamsGroups {
        InParamsGroup
        {
            AudioInputEffect::a_insert_slot,
            "Effect input",
            AudioInputEffect::in_effect_input_channel,
            AudioInputEffect::in_effect_input_level,
            AudioInputEffect::in_effect_input_pan,
                {0.4f,0.2f,0.4f,0.2f,},
                {0.2f, 0.4f, 0.2f, 0.4f,},
                {}, {}, {}, {},{},{}
        },
        InParamsGroup
        {
            AudioInputEffect::a_insert_slot,
            "Slot a-insert, Scene B input",
            AudioInputEffect::in_scene_input_channel,
                AudioInputEffect::in_scene_input_level,
                AudioInputEffect::in_scene_input_pan,
            {},{}, {}, {},
            {0.4f,0.2f,0.4f,0.2f,},
            {0.2f, 0.4f, 0.2f, 0.4f,},{},{}
        },
        InParamsGroup
        {
            AudioInputEffect::b_insert_slot,
            "On insert-B, Scene A input",
            AudioInputEffect::in_scene_input_channel,
            AudioInputEffect::in_scene_input_level,
            AudioInputEffect::in_scene_input_pan,
            {},{},
            {0.4f,0.2f,0.4f,0.2f,},
            {0.2f, 0.4f, 0.2f, 0.4f,},
            {}, {}, {}, {}
        },
        InParamsGroup
        {
            AudioInputEffect::a_insert_slot,
            "Audio input",
            AudioInputEffect::in_audio_input_channel,
            AudioInputEffect::in_audio_input_level,
            AudioInputEffect::in_audio_input_pan,
            {},{},
            {}, {}, {}, {},
            {0.4f,0.2f,0.4f,0.2f,},
            {0.2f, 0.4f, 0.2f, 0.4f,},
        }
    };
    for(InParamsGroup& inParamsGroup: inParamsGroups)
    {
        for (int slot: slots[inParamsGroup.slot])
        {

            SECTION(inParamsGroup.testGroup + ", slot " + std::to_string(slot))
            {

                auto surge = Surge::Headless::createSurge(44100);
                REQUIRE(surge);



                Surge::Test::setFX(surge, slot, fxt_input_blender);
                SurgeStorage *surgeStorage = &surge->storage;
                FxStorage *fxStorage = &surgeStorage->getPatch().fx[slot];
                REQUIRE(fxStorage->type.val.i == fxt_input_blender);
                fxStorage->p[AudioInputEffect::in_output_width].val.f = 1.0f;
                fxStorage->p[AudioInputEffect::in_output_mix].val.f = 1.0f;

                inParamsGroup.fillWithData(surgeStorage);

                SECTION(inParamsGroup.testGroup +
                        " with default params the result should be unchanged")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.4f, 0.2f, 0.4f, 0.2f};
                    float expectedRightInput[BLOCK_SIZE]{0.2f, 0.4f, 0.2f, 0.4f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 0.0f;
                    testExpectedValues(surge, slot, inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput, expectedLeftInput,
                                       expectedRightInput);
                }

                SECTION(inParamsGroup.testGroup + " accepts only left channel")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.4f, 0.2f, 0.4f, 0.2f};
                    float expectedRightInput[BLOCK_SIZE]{0.0f, 0.0f, 0.0f, 0.0f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = -1.0f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 0.0f;
                    testExpectedValues(surge, slot,
                                       inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput,
                                       expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup + " accepts 50% of left channel and 100% right")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.3f, 0.15f, 0.3f, 0.15f};
                    float expectedRightInput[BLOCK_SIZE]{0.2f, 0.4f, 0.2f, 0.4f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = 0.25f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 0.0f;
                    testExpectedValues(surge, slot,
                                       inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput,
                                       expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup + " accepts 100% of left channel and 50% right")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.4f, 0.2f, 0.4f, 0.2f};
                    float expectedRightInput[BLOCK_SIZE]{0.1f, 0.2f, 0.1f, 0.2f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = -0.50f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 0.0f;
                    testExpectedValues(surge, slot,
                                       inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput,
                                       expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup +
                        " accepts 100% of left channel and 50% right with 50% input level")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.2f, 0.1f, 0.2f, 0.1f};
                    float expectedRightInput[BLOCK_SIZE]{0.05f, 0.1f, 0.05f, 0.1f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = -0.50f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = -5.995f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 0.0f;
                    testExpectedValues(surge, slot, inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput, expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup + "'s channels move to the left")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.6f, 0.6f, 0.6f, 0.6f};
                    float expectedRightInput[BLOCK_SIZE]{0.0f, 0.0f, 0.0f, 0.0f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = -1.0f;
                    testExpectedValues(surge, slot, inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput, expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup + "'s channels move to the right")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.0f, 0.0f, 0.0f, 0.0f};
                    float expectedRightInput[BLOCK_SIZE]{0.6f, 0.6f, 0.6f, 0.6f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 1.0f;
                    testExpectedValues(surge, slot, inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput, expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup + "'s channels move to the right by 50%")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.2f, 0.1f, 0.2f, 0.1f};
                    float expectedRightInput[BLOCK_SIZE]{0.4f, 0.5f, 0.4f, 0.5f};
                    fxStorage->p[inParamsGroup.inputChannel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 0.5f;
                    testExpectedValues(surge, slot, inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput, expectedLeftInput,
                                       expectedRightInput);
                }
                SECTION(inParamsGroup.testGroup +
                        "'s left channels move to the right, the right channel "
                        "is deleted")
                {
                    float expectedLeftInput[BLOCK_SIZE]{0.0f, 0.0f, 0.0f, 0.0f};
                    float expectedRightInput[BLOCK_SIZE]{
                        0.4f,
                        0.2f,
                        0.4f,
                        0.2f,
                    };
                    fxStorage->p[inParamsGroup.inputChannel].val.f = -1.0f;
                    fxStorage->p[inParamsGroup.inputLevel].val.f = 0.0f;
                    fxStorage->p[inParamsGroup.inputPan].val.f = 1.0f;
                    testExpectedValues(surge, slot, inParamsGroup.leftEffectInput,
                                       inParamsGroup.rightEffectInput, expectedLeftInput,
                                       expectedRightInput);
                }
            }
        }
    }
}

TEST_CASE("AudioInputEffect: mixing inputs",  "[fx]")
{

    std::map<int, std::vector<int>> slots {
        {AudioInputEffect::a_insert_slot,
         {fxslot_ains1, fxslot_ains2, fxslot_ains3, fxslot_ains4}},
        {AudioInputEffect::b_insert_slot,
         {fxslot_bins1, fxslot_bins2, fxslot_bins3, fxslot_bins4}},
        {AudioInputEffect::send_slot,
         {fxslot_send1, fxslot_send2, fxslot_send3, fxslot_send4}},
        {AudioInputEffect::global_slot,
         {fxslot_global1, fxslot_global2, fxslot_global3, fxslot_global4}},
    };

    std::vector<InParamsGroup> inParamsGroups
    {
        {
            AudioInputEffect::a_insert_slot,
            "A Insert",
            AudioInputEffect::in_audio_input_channel, //ignore it
            AudioInputEffect::in_audio_input_level,     //ignore it
            AudioInputEffect::in_audio_input_pan,        //ignore it
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.2f, 0.2f, 0.2f, 0.2f},
            {0.2f, 0.2f, 0.2f, 0.2f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.3f,0.3f,0.3f,0.3f}, ////we expect the sum of effect input, b-input and audio input
            {0.3f,0.3f,0.3f,0.3f}
        },
        {
            AudioInputEffect::b_insert_slot,
            "B Insert",
            AudioInputEffect::in_audio_input_channel, //ignore it
            AudioInputEffect::in_audio_input_level,     //ignore it
            AudioInputEffect::in_audio_input_pan,        //ignore it
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.2f, 0.2f, 0.2f, 0.2f},
            {0.2f, 0.2f, 0.2f, 0.2f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.4f,0.4f,0.4f,0.4f}, //we expect the sum of effect input, a-input and audio input
            {0.4f,0.4f,0.4f,0.4f}
        },
        {
             AudioInputEffect::send_slot,
             "Send",
             AudioInputEffect::in_audio_input_channel, //ignore it
             AudioInputEffect::in_audio_input_level,     //ignore it
             AudioInputEffect::in_audio_input_pan,        //ignore it
             {0.1f, 0.1f, 0.1f, 0.1f},
             {0.1f, 0.1f, 0.1f, 0.1f},
             {0.2f, 0.2f, 0.2f, 0.2f},
             {0.2f, 0.2f, 0.2f, 0.2f},
             {0.1f, 0.1f, 0.1f, 0.1f},
             {0.1f, 0.1f, 0.1f, 0.1f},
             {0.1f, 0.1f, 0.1f, 0.1f},
             {0.1f, 0.1f, 0.1f, 0.1f},
             {0.2f, 0.2f, 0.2f, 0.2f}, // we only expect the sum of effect input and audio input
             {0.2f, 0.2f, 0.2f, 0.2f}
        },
        {
            AudioInputEffect::global_slot,
            "Global",
            AudioInputEffect::in_audio_input_channel, //ignore it
            AudioInputEffect::in_audio_input_level,     //ignore it
            AudioInputEffect::in_audio_input_pan,        //ignore it
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.2f, 0.2f, 0.2f, 0.2f},
            {0.2f, 0.2f, 0.2f, 0.2f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.1f, 0.1f, 0.1f, 0.1f},
            {0.2f, 0.2f, 0.2f, 0.2f}, // we only expect the sum of effect input and audio input
            {0.2f, 0.2f, 0.2f, 0.2f}
        }
    };
    //TODO: test combination of effects in the slots
    for(InParamsGroup& inParamsGroup: inParamsGroups)
    {
        SECTION(inParamsGroup.testGroup)
        {
            for (int slot : slots[inParamsGroup.slot])
            {

                auto surge = Surge::Headless::createSurge(44100);
                REQUIRE(surge);

                Surge::Test::setFX(surge, slot, fxt_input_blender);
                SurgeStorage *surgeStorage = &surge->storage;
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
                REQUIRE(fxStorage->type.val.i == fxt_input_blender);

                SECTION(inParamsGroup.testGroup + ", slot " + std::to_string(slot))
                {
                    inParamsGroup.fillWithData(surgeStorage);

                    testExpectedValues(
                        surge, slot,
                        inParamsGroup.leftEffectInput, inParamsGroup.rightEffectInput,
                        inParamsGroup.expectedLeftInput, inParamsGroup.expectedRightInput
                    );
                };
            }
        }
    }
}