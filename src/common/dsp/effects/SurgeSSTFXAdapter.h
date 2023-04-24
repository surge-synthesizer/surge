//
// Created by Paul Walker on 4/22/23.
//

#ifndef SURGE_SURGESSTFXADAPTER_H
#define SURGE_SURGESSTFXADAPTER_H

#include "Parameter.h"
#include "SurgeStorage.h"
#include "Effect.h"
#include "sst/basic-blocks/params/ParamMetadata.h"
#include "sst/filters/BiquadFilter.h"

namespace surge::sstfx
{
struct SurgeFXConfig
{
    static constexpr uint16_t blockSize{BLOCK_SIZE};
    using BaseClass = Effect;
    using GlobalStorage = SurgeStorage;
    using EffectStorage = FxStorage;
    using ValueStorage = pdata;
    using BiquadAdapter = sst::filters::Biquad::DefaultTuningAndDBAdapter<GlobalStorage>;

    static inline float floatValueAt(const Effect *const e, const ValueStorage *const v, int idx)
    {
        return *(e->pd_float[idx]);
    }

    static inline int intValueAt(const Effect *const e, const ValueStorage *const v, int idx)
    {
        return *(e->pd_float[idx]);
    }

    static inline float envelopeRateLinear(GlobalStorage *s, float f)
    {
        return s->envelope_rate_linear(f);
    }

    static inline float temposyncRatio(GlobalStorage *s, EffectStorage *e, int idx)
    {
        return e->p[idx].temposync ? s->temposyncratio : 1;
    }

    static inline bool isDeactivated(EffectStorage *e, int idx) { return e->p[idx].deactivated; }

    static inline float rand01(GlobalStorage *s) { return s->rand_01(); }

    static inline double sampleRate(GlobalStorage *s) { return s->samplerate; }

    static inline float noteToPitch(GlobalStorage *s, float p) { return s->note_to_pitch(p); }
    static inline float noteToPitchIgnoringTuning(GlobalStorage *s, float p)
    {
        return s->note_to_pitch_ignoring_tuning(p);
    }

    static inline float noteToPitchInv(GlobalStorage *s, float p)
    {
        return s->note_to_pitch_inv(p);
    }

    static inline float dbToLinear(GlobalStorage *s, float f) { return s->db_to_linear(f); }
};

template <typename T> struct SurgeSSTFXBase : T
{
    SurgeSSTFXBase(SurgeStorage *storage, FxStorage *fxdata, pdata *pd) : T(storage, fxdata, pd) {}

    void init() override { T::initialize(); }

    void process(float *dataL, float *dataR) override { T::processBlock(dataL, dataR); }

    void suspend() override { T::suspendProcessing(); }

    int get_ringout_decay() override { return T::getRingoutDecay(); }

    const char *get_effectname() override { return T::effectName; }

    void init_default_values() override
    {
        for (int i = 0; i < T::numParams; ++i)
        {
            auto pmd = T::paramAt(i);
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::FLOAT)
            {
                this->fxdata->p[i].val_default.f = pmd.defaultVal;
            }
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::INT)
            {
                this->fxdata->p[i].val_default.i = (int)std::round(pmd.defaultVal);
            }
        }
    }

    void configureControlsFromFXMetadata()
    {
        // TODO
        // 1: Expand this and
        // 2: make it throw a logic_error for really miscofingured cases
        for (int i = 0; i < T::numParams; ++i)
        {
            auto pmd = T::paramAt(i);
            this->fxdata->p[i].set_name(pmd.name.c_str());
            auto check = [&](auto a, auto b, auto msg) {
                if (a != b)
                    std::cout << "Unable to match " << pmd.name << " " << a << " " << b << " "
                              << msg << std::endl;
            };
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::FLOAT)
            {
                check(pmd.minVal, this->fxdata->p[i].val_min.f, "Minimum Values");
                check(pmd.maxVal, this->fxdata->p[i].val_max.f, "Maximum Values");
            }
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::INT)
            {
                check((int)pmd.minVal, this->fxdata->p[i].val_min.i, "Minimum Values");
                check((int)pmd.maxVal, this->fxdata->p[i].val_max.i, "Maximum Values");
            }
        }
    }
};

} // namespace surge::sstfx

#endif // SURGE_SURGESSTFXADAPTER_H
