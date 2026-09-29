// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Effect.h"
#include "ConvolutionEffect.h"
#include <algorithm>
#include <array>
#include <bit>
#include <type_traits>

// Construct/destroy on the control thread. The serialized engine owner may
// inspect matches() and move effect out, then rebind it before processing.
// Initialization is a separate control-thread step. Convolution prepares its
// filters here and adopts the independently prepared kernel on attachment.
struct PreparedEffect
{
    FxStorage request, parameters;
    std::array<pdata, n_global_params> values{};
    std::unique_ptr<Effect> effect;
    float sampleRate;
    bool consumed{false};
    bool initialized{false};

    PreparedEffect(SurgeStorage &storage, const FxStorage &candidate)
        : request(candidate), parameters(candidate), sampleRate(storage.samplerate)
    {
        effect.reset(spawn_effect(candidate.type.val.i, &storage, &parameters, values.data()));
        if (effect) effect->init_ctrltypes();
    }
    void initialize()
    {
        if (initialized) return;
        if (!effect)
        {
            // Prepare the Off slot's metadata here too. The audio callback
            // must not clear heap-owning metadata or build parameter labels.
            for (int i = 0; i < n_fx_params; ++i)
            {
                auto &p = parameters.p[i];
                p.set_type(ct_none);
                p.set_name(("Param " + std::to_string(i + 1)).c_str());
                p.val.i = 0;
            }
        }
        else
        {
            // Match loadFx's preset normalization before init reads parameters.
            // All pointers still refer to this private preparation storage.
            for (auto &p : parameters.p)
            {
                p.set_extend_range(p.extend_range);
                if (p.ctrltype != ct_none)
                {
                    if (p.valtype == vt_float)
                        p.val.f = std::clamp(p.val.f, p.val_min.f, p.val_max.f);
                    else if (p.valtype == vt_int)
                        p.val.i = std::clamp(p.val.i, p.val_min.i, p.val_max.i);
                }
                values[p.id] = p.val;
            }
            if (auto *convolution = dynamic_cast<ConvolutionEffect *>(effect.get()))
                convolution->prepareProcessingState();
            else
                effect->init();
            effect->updateAfterReload();
            // Reload hooks may establish sub-effect parameter metadata/values.
            for (const auto &p : parameters.p) values[p.id] = p.val;
        }
        initialized = true;
    }
    void swapParameterMetadata(FxStorage &target) noexcept
    {
        static_assert(std::is_nothrow_move_constructible_v<Parameter>);
        static_assert(std::is_nothrow_move_assignable_v<Parameter>);
        // Keep the old owned metadata here for control-thread reclamation.
        // This preparation object remains alive until after audio adoption.
        for (int i = 0; i < n_fx_params; ++i)
            std::swap(parameters.p[i], target.p[i]);
    }
    bool matches(const FxStorage &candidate, float rate) const
    {
        if (consumed || rate != sampleRate || request.type.val.i != candidate.type.val.i ||
            request.user_data != candidate.user_data) return false;
        for (int i = 0; i < n_fx_params; ++i)
        {
            const auto &a = request.p[i], &b = candidate.p[i];
            if (a.valtype != b.valtype || a.id != b.id) return false;
            const bool sameValue = a.valtype == vt_float
                ? std::bit_cast<uint32_t>(a.val.f) == std::bit_cast<uint32_t>(b.val.f)
                : a.valtype == vt_bool ? a.val.b == b.val.b : a.val.i == b.val.i;
            if (!sameValue) return false;
#define MATCH_FIELD(field) if (a.field != b.field) return false;
            MATCH_FIELD(temposync)
            MATCH_FIELD(absolute)
            MATCH_FIELD(deactivated)
            MATCH_FIELD(extend_range)
            MATCH_FIELD(porta_constrate)
            MATCH_FIELD(porta_gliss)
            MATCH_FIELD(porta_retrigger)
            MATCH_FIELD(porta_curve)
            MATCH_FIELD(deform_type)
            MATCH_FIELD(midictrl)
            MATCH_FIELD(midichan)
#undef MATCH_FIELD
        }
        return true;
    }
};
