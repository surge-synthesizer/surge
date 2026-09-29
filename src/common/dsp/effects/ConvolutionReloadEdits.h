// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SurgeStorage.h"
#include <array>
#include <bit>

// Captured by the IR selector under fxSpawnMutex; merged by the serialized
// engine owner under the same mutex. Weak owners identify the exact selection
// without retaining large sample buffers or releasing them on the audio thread.
class ConvolutionReloadEdits
{
  public:
    void capture(const FxStorage &live, const FxStorage &candidate)
    {
        active_ = true;
        type_ = live.type.val.i;
        left_ = data(candidate, "left");
        right_ = data(candidate, "right");
        rate_ = data(candidate, "samplerate");
        for (int i = 0; i < n_fx_params; ++i) before_[i] = State(live.p[i]);
    }

    // Returns false when the selection was superseded. Never copy metadata from
    // the old effect over an unrelated preset or a different impulse response.
    bool merge(const FxStorage &live, FxStorage &candidate)
    {
        if (!active_) return false;
        if (live.type.val.i != type_ || candidate.type.val.i != type_ ||
            !owns(left_, data(candidate, "left")) ||
            !owns(right_, data(candidate, "right")) ||
            !owns(rate_, data(candidate, "samplerate")))
        {
            finish();
            return false;
        }
        for (int i = 0; i < n_fx_params; ++i) before_[i].merge(live.p[i], candidate.p[i]);
        return true;
    }

    // Keep weak control blocks until the next UI capture or object teardown;
    // ending a transaction itself must not deallocate on the audio thread.
    void finish() { active_ = false; }

  private:
    using Bytes = std::vector<std::uint8_t>;
    static const std::shared_ptr<Bytes> &data(const FxStorage &fx, const char *key)
    {
        static const std::shared_ptr<Bytes> empty;
        auto found = fx.user_data.find(key);
        return found == fx.user_data.end() ? empty : found->second;
    }
    static bool owns(const std::weak_ptr<Bytes> &a, const std::shared_ptr<Bytes> &b)
    {
        return !a.owner_before(b) && !b.owner_before(a);
    }
    struct State
    {
        pdata value{};
        bool temposync{}, absolute{}, deactivated{}, extend_range{};
        bool porta_constrate{}, porta_gliss{}, porta_retrigger{};
        int porta_curve{}, deform_type{}, midictrl{}, midichan{};
        State() = default;
        explicit State(const Parameter &p)
            : value(p.val), temposync(p.temposync), absolute(p.absolute),
              deactivated(p.deactivated), extend_range(p.extend_range),
              porta_constrate(p.porta_constrate), porta_gliss(p.porta_gliss),
              porta_retrigger(p.porta_retrigger), porta_curve(p.porta_curve),
              deform_type(p.deform_type), midictrl(p.midictrl), midichan(p.midichan) {}
        void merge(const Parameter &live, Parameter &target)
        {
            const bool valueChanged = live.valtype == vt_float
                ? std::bit_cast<std::uint32_t>(live.val.f) != std::bit_cast<std::uint32_t>(value.f)
                : live.valtype == vt_bool ? live.val.b != value.b : live.val.i != value.i;
            if (valueChanged) target.val = live.val;
            // Merge fields independently: changing a value must not overwrite
            // a candidate's unrelated range/deactivation/tempo settings.
#define MERGE_FIELD(field) if (live.field != field) target.field = live.field;
            MERGE_FIELD(temposync)
            MERGE_FIELD(absolute)
            MERGE_FIELD(deactivated)
            MERGE_FIELD(extend_range)
            MERGE_FIELD(porta_constrate)
            MERGE_FIELD(porta_gliss)
            MERGE_FIELD(porta_retrigger)
            MERGE_FIELD(porta_curve)
            MERGE_FIELD(deform_type)
            MERGE_FIELD(midictrl)
            MERGE_FIELD(midichan)
#undef MERGE_FIELD
            *this = State(live);
        }
    };
    bool active_{};
    int type_{};
    std::weak_ptr<Bytes> left_, right_, rate_;
    std::array<State, n_fx_params> before_;
};
