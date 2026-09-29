// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurgeStorage.h"
#include "dsp/effects/airwindows/AirWindowsEffect.h"
#include "AllocationProbe.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

static void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

// Development reference for direct selector edits. Preset loading deliberately
// uses streamed values, whereas the selector installs the new processor defaults.
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    try
    {
        AllocationProbe::begin();
        auto *control = ::operator new(128);
        ::operator delete(control);
        AllocationProbe::end();
        require(AllocationProbe::allocations == 1 && AllocationProbe::deallocations == 1,
                "Allocation counter failed its positive control");
        auto storage = std::make_unique<SurgeStorage>(argv[1]);
        const auto &registry = AirWinBaseClass::pluginRegistry();
        unsigned selections = 0, allocatingSelections = 0, allocatingSuspends = 0;
        for (float rate : {44100.f, 48000.f})
        {
            storage->setSamplerate(rate);
            auto parameters = std::make_unique<FxStorage>(storage->getPatch().fx[0]);
            auto values = std::make_unique<std::array<pdata, n_global_params>>();
            parameters->type.val.i = fxt_airwindows;
            auto effect = std::make_unique<AirWindowsEffect>(storage.get(), parameters.get(), values->data());
            effect->init_ctrltypes();
            effect->init_default_values();
            effect->init();
            effect->updateAfterReload();
            // Start from a different selector so every iteration changes it.
            effect->setupSubFX(registry.size() - 1, false);
            parameters->p[0].val.i = registry.size() - 1;
            for (int variant = 0; variant < registry.size(); ++variant)
            {
                for (int i = 1; i < n_fx_params; ++i) parameters->p[i].val.f = .123456f;
                parameters->p[0].val.i = variant;
                for (const auto &p : parameters->p) (*values)[p.id] = p.val;
                alignas(16) std::array<float, BLOCK_SIZE> left{}, right{};
                AllocationProbe::begin();
                effect->process(left.data(), right.data());
                AllocationProbe::end();
                const auto selectorAllocations = AllocationProbe::allocations;
                allocatingSelections += selectorAllocations != 0;
                require(effect->airwin && effect->lastSelected == variant,
                        "Direct selector did not install the requested processor");
                const auto &registration = AirWindowsEffect::fxreg[variant];
                auto defaults = registration.create(registration.id, rate, 2);
                for (int i = 0; i < defaults->paramCount; ++i)
                    require(parameters->p[i + 1].val.f == defaults->getParameter(i),
                            "Direct selection retained the old values instead of processor defaults");
                for (int i = defaults->paramCount + 1; i < n_fx_params; ++i)
                    require(parameters->p[i].ctrltype == ct_none,
                            "Direct selection retained active metadata beyond its parameter count");
                for (const auto &p : parameters->p) (*values)[p.id] = p.val;
                auto *selected = effect->airwin.get();
                AllocationProbe::begin();
                for (int block = 0; block < 8; ++block)
                    effect->process(left.data(), right.data());
                const bool stableNoHeap = AllocationProbe::end();
                require(stableNoHeap && effect->airwin.get() == selected,
                        "Stable Airwindows processing touched the C++ heap or replaced its processor");
                for (int i = 0; i < BLOCK_SIZE; ++i)
                    require(std::isfinite(left[i]) && std::isfinite(right[i]), "Non-finite output");
                AllocationProbe::begin();
                effect->suspend();
                AllocationProbe::end();
                const auto suspendAllocations = AllocationProbe::allocations;
                require(suspendAllocations == 0 && AllocationProbe::deallocations == 0 &&
                            effect->airwin.get() == selected,
                        "Suspension replaced a prepared processor or touched the C++ heap");
                if (dynamic_cast<AirWindowsNoOp *>(effect->airwin.get()))
                {
                    left.fill(.25f); right.fill(-.5f);
                    effect->process(left.data(), right.data());
                    for (int i = 0; i < BLOCK_SIZE; ++i)
                        require(left[i] == 0 && right[i] == 0,
                                "Retired processor no longer produces silence after suspension");
                    parameters->p[0].set_user_data(nullptr);
                    effect->suspend();
                    require(parameters->p[0].user_data && effect->lastSelected == variant,
                            "Missing retired metadata was not repaired");
                    parameters->p[0].val.i = 0;
                    effect->suspend();
                    require(effect->lastSelected == 0 &&
                                !dynamic_cast<AirWindowsNoOp *>(effect->airwin.get()),
                            "A mismatched retired selector was incorrectly retained");
                }
                allocatingSuspends += suspendAllocations != 0;
                if (suspendAllocations)
                    std::printf("Suspend constructs: %.0f Hz variant %d, %d parameters, %u allocations\n",
                                rate, variant, defaults->paramCount, suspendAllocations);
                ++selections;
            }
        }
        std::printf("Airwindows lifecycle: %u direct selections preserve native defaults; stable processing has no observed C++ heap operations\n", selections);
        std::printf("Migration gaps: %u allocating direct selections, %u allocating suspensions\n",
                    allocatingSelections, allocatingSuspends);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
