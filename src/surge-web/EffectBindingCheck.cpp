// SPDX-License-Identifier: GPL-3.0-or-later
#include "Effect.h"
#include "dsp/effects/Reverb2Effect.h"
#include "dsp/effects/PreparedEffect.h"
#include "dsp/effects/airwindows/AirWindowsEffect.h"
#include "AllocationProbe.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>

static void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    try
    {
        AllocationProbe::begin();
        auto *probe = ::operator new(128);
        ::operator delete(probe);
        AllocationProbe::end();
        require(AllocationProbe::allocations == 1 && AllocationProbe::deallocations == 1,
                "Allocation probe did not observe its control");
        auto storage = std::make_unique<SurgeStorage>(argv[1]);
        const auto airwindowsCount = AirWinBaseClass::pluginRegistry().size();
        int checked = 0;
        for (float rate : {44100.f, 48000.f})
        {
            storage->setSamplerate(rate);
            for (int type = 1; type < n_fx_types; ++type)
            for (int variant = 0; variant < (type == fxt_airwindows ? airwindowsCount : 1); ++variant)
            {
                auto source = std::make_unique<FxStorage>(storage->getPatch().fx[0]);
                auto sourceValues = std::make_unique<std::array<pdata, n_global_params>>();
                auto targetValues = std::make_unique<std::array<pdata, n_global_params>>();
                source->type.val.i = type;
                if (type == fxt_convolution)
                {
                    std::vector<float> impulse(1024);impulse[0] = .5f;impulse[63] = .2f;
                    source->user_data["left"] = ArbitraryBlockStorage::from_floats(impulse);
                    source->user_data["samplerate"] = ArbitraryBlockStorage::from_float(rate);
                    source->user_data["irname"] = ArbitraryBlockStorage::from_string("Binding fixture");
                }
                std::unique_ptr<Effect> effect(spawn_effect(type, storage.get(), source.get(), sourceValues->data()));
                require(bool(effect), "An effect family did not construct");
                effect->init_ctrltypes();effect->init_default_values();
                if (type == fxt_airwindows) source->p[0].val.i = variant;
                {
                    PreparedEffect prepared(*storage, *source);
                    require(prepared.matches(*source, rate), "Fresh construction did not match its request");
                    require(!prepared.matches(*source, rate + 1), "Construction accepted a changed sample rate");
                    const auto previousValue = source->p[0].val;
                    if (source->p[0].valtype == vt_float) source->p[0].val.f += .125f;
                    else if (source->p[0].valtype == vt_bool) source->p[0].val.b = !source->p[0].val.b;
                    else source->p[0].val.i++;
                    require(!prepared.matches(*source, rate), "Construction accepted changed parameter values");
                    source->p[0].val = previousValue;
                    source->p[0].temposync = !source->p[0].temposync;
                    require(!prepared.matches(*source, rate), "Construction accepted changed parameter flags");
                    source->p[0].temposync = !source->p[0].temposync;
                    source->p[0].midictrl++;
                    require(!prepared.matches(*source, rate), "Construction accepted changed MIDI mapping");
                    source->p[0].midictrl--;
                    source->user_data["ownership-check"] = std::make_shared<std::vector<uint8_t>>(1);
                    require(!prepared.matches(*source, rate), "Construction accepted changed asset ownership");
                    source->user_data.erase("ownership-check");
                    require(prepared.matches(*source, rate), "Restored request did not match");
                    prepared.consumed = true;
                    require(!prepared.matches(*source, rate), "Consumed construction was reused");
                }
                for (const auto &parameter : source->p) (*sourceValues)[parameter.id] = parameter.val;
                // Exercise the actual private preparation used by the browser,
                // including initialization before attachment to live storage.
                auto prepared = std::make_unique<PreparedEffect>(*storage, *source);
                prepared->initialize();
                require(prepared->matches(*source, rate), "Initialization changed the request identity");
                require(prepared->initialized, "Effect processing state was not prepared");
                effect = std::move(prepared->effect);
                auto *aw = dynamic_cast<AirWindowsEffect *>(effect.get());
                auto *preparedSubEffect = aw ? aw->airwin.get() : nullptr;
                if (aw)
                    require(preparedSubEffect && aw->lastSelected == variant,
                            "Airwindows sub-effect was not prepared before attachment");

                auto &target = storage->getPatch().fx[1];
                target.type.val.i = type;
                target.user_data = source->user_data;
                for (int i = 0; i < n_fx_params; ++i)
                {
                    const auto id = target.p[i].id;
                    prepared->parameters.p[i].id = id;
                    // Force heap-backed outgoing metadata, including an engaged
                    // optional, so a copy/reset cannot pass through SSO alone.
                    target.p[i].oscName = std::string(512, 'o');
                    target.p[i].basicBlocksParamMetaData.emplace();
                    target.p[i].basicBlocksParamMetaData->name = std::string(512, 'm');
                }
                AllocationProbe::begin();
                prepared->swapParameterMetadata(target);
                const bool allocationFree = AllocationProbe::end();
                require(allocationFree, "Prepared metadata transfer touched the C++ heap");
                for (int i = 0; i < n_fx_params; ++i)
                {
                    require(prepared->parameters.p[i].oscName == std::string(512, 'o') &&
                                prepared->parameters.p[i].basicBlocksParamMetaData->name == std::string(512, 'm'),
                            "Outgoing metadata was not retained for control-thread reclamation");
                    const auto id = target.p[i].id;
                    (*targetValues)[id] = target.p[i].val;
                }
                effect->rebindParameterStorage(&target, targetValues->data());
                std::unique_ptr<FxStorage> referenceParameters;
                std::unique_ptr<std::array<pdata, n_global_params>> referenceValues;
                std::unique_ptr<Effect> reference;
                if (auto *convolution = dynamic_cast<ConvolutionEffect *>(effect.get()))
                {
                    require(!convolution->initialized, "Preparation built an IR synchronously");
                    auto kernel = ConvolutionKernel::prepare(target.by_key("left").as<float>(), {},
                        rate, rate * target.p[ConvolutionEffect::convolution_size].val.f,
                        target.p[ConvolutionEffect::convolution_start].val.f,
                        target.p[ConvolutionEffect::convolution_reverse].val.f,
                        target.p[ConvolutionEffect::convolution_reverse].deactivated);
                    require(bool(kernel), "IR preparation failed");
                    convolution->adoptPreparedKernel(kernel);
                    require(!kernel && convolution->initialized, "Prepared IR was not transferred");
                    referenceParameters = std::make_unique<FxStorage>(target);
                    referenceValues = std::make_unique<std::array<pdata, n_global_params>>(*targetValues);
                    reference.reset(spawn_effect(type, storage.get(), referenceParameters.get(), referenceValues->data()));
                    reference->init_ctrltypes();
                    reference->init();
                }
                for (int i = 0; i < n_fx_params; ++i)
                {
                    require(effect->pd_float[i] == &(*targetValues)[target.p[i].id].f &&
                                effect->pd_int[i] == &(*targetValues)[target.p[i].id].i,
                            "Effect retained a private value pointer");
                }
                if (auto *sst = dynamic_cast<Reverb2Effect *>(effect.get()))
                    require(sst->fxStorage == &target && sst->valueStorage == targetValues->data(),
                            "SST adapter retained private storage");

                // Poison the preparation buffers before freeing them: a retained
                // value pointer should not silently pass because old values match.
                for (auto &value : *sourceValues) value.f = std::numeric_limits<float>::quiet_NaN();
                source.reset();sourceValues.reset();
                for (auto &value : prepared->values) value.f = std::numeric_limits<float>::quiet_NaN();
                prepared.reset();
                alignas(16) std::array<float, BLOCK_SIZE> left{}, right{};
                alignas(16) std::array<float, BLOCK_SIZE> referenceLeft{}, referenceRight{};
                for (int block = 0; block < 128; ++block)
                {
                    for (int i = 0; i < BLOCK_SIZE; ++i)
                    {
                        left[i] = .05f * std::sin((block * BLOCK_SIZE + i) * .1f);
                        right[i] = .03f * std::cos((block * BLOCK_SIZE + i) * .07f);
                    }
                    if (reference)
                    {
                        referenceLeft = left;referenceRight = right;
                        reference->process(referenceLeft.data(), referenceRight.data());
                    }
                    effect->process(left.data(), right.data());
                    if (aw)
                        require(aw->airwin.get() == preparedSubEffect && aw->lastSelected == variant,
                                "Audio processing replaced the prepared Airwindows sub-effect");
                    for (int i = 0; i < BLOCK_SIZE; ++i)
                    {
                        require(std::isfinite(left[i]) && std::isfinite(right[i]),
                                "Rebound effect produced non-finite audio");
                        if (reference)
                            require(left[i] == referenceLeft[i] && right[i] == referenceRight[i],
                                    "Split convolution preparation changed reference audio");
                    }
                }
                ++checked;
            }
        }
        auto offRequest = storage->getPatch().fx[0];
        offRequest.type.val.i = fxt_off;
        PreparedEffect off(*storage, offRequest);
        off.initialize();
        require(!off.effect && off.initialized, "Off metadata was not prepared");
        for (const auto &p : off.parameters.p)
            require(p.ctrltype == ct_none && p.val.i == 0, "Off retained an active parameter");
        AllocationProbe::begin();
        off.swapParameterMetadata(storage->getPatch().fx[1]);
        const bool offAllocationFree = AllocationProbe::end();
        require(offAllocationFree, "Off metadata adoption touched the C++ heap");
        std::printf("Prepared metadata transfers: no C++ allocations or deallocations, including Off\n");
        std::printf("Effect binding: %d preparations attach live parameter storage and render finite audio; %zu Airwindows variants retain their prepared sub-effect at both rates\n", checked, airwindowsCount);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "%s\n", e.what());return 1;
    }
}
