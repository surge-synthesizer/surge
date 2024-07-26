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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_SURGESSTFXADAPTER_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_SURGESSTFXADAPTER_H

#include "Parameter.h"
#include "SurgeStorage.h"
#include "Effect.h"
#include "sst/basic-blocks/params/ParamMetadata.h"
#include "sst/filters/BiquadFilter.h"

/*
 * This file provides the code we need to bridge the running in-memory
 * configured Surge Effects to the context-independent implementation of
 * those effects in sst-effects submodule. Our strategy is broadly
 *
 * 1. Port the DSP and the parameter metadata but
 * 2. Don't port the parts of the API which are too tightly coupled, like the
 *    Parameter.h type enum and
 * 3. Don't port the parts of the API which are gross, like the group_ypos stuff
 *
 * We do this with the template strategy outlined more deeply in sst-effects. This
 * file contains the two classes adapting Surge to that strategy.
 *
 * To port a new effect your steps are basically
 *
 * 1. Copy the code to a header in sst-effects and use the concrete names for
 *    the interface which the regtests require. So now you have
 *    sst::effects::Foo<Config>
 * 2. Strip that code from surge and change FooEffect : Effect into
 *    FooEffect : SurgeSSTFXBase<sst::effects::Foo<surge::sstfx::SurgeFXConfig>
 *
 * and voila.
 */
namespace surge::sstfx
{

/*
 * The FXConfig template
 *
 * The sst-effects fx are all template classes which take a
 * configuration adapter to adapt them to their host. Their host has to provide
 * them three things
 *
 * 1. A "Global Storage" configuration. This is, in surge, SurgeStorage, and
 *    provides things like samplerate, db to linear conversion, note to pitch.
 * 2. An "Effect Storage" configuration. This can answer static questions about
 *    the configuration of the effect, like is a parameter deactivated. This is
 *    FXStorage in surge. And
 * 3. A "ValueStorage *" which is optional, but allows the host to present
 *    different external storage for the values. In surge this is a `pdata *`.
 *
 * Finally we provide a base class which can inject into the inheritance
 * hierarchy and use a CRTP static cast type pattern. In this case it is
 * Effect.
 *
 * Given those, the FX are re-factored to query these objects with questions like
 * `floatValue` or `noteToPitch` using an API which is outlined in
 * sst-effects/include/sst/effects/EffectCore.h. The majority of the documentation
 * is there.
 *
 * So basically this FXConfig provides an implementation of all the template query
 * points you need to implement a refactored effect and work in surge.
 *
 */
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

    static inline float floatValueExtendedAt(const Effect *const e, const ValueStorage *const v,
                                             int idx)
    {
        if (e->fxdata->p[idx].extend_range)
            return e->fxdata->p[idx].get_extended(*(e->pd_float[idx]));
        return *(e->pd_float[idx]);
    }

    static inline int intValueAt(const Effect *const e, const ValueStorage *const v, int idx)
    {
        return *(e->pd_int[idx]);
    }

    static inline float envelopeRateLinear(GlobalStorage *s, float f)
    {
        return s->envelope_rate_linear(f);
    }

    static inline bool temposyncInitialized(GlobalStorage *s) { return s->temposyncratio_inv != 0; }
    static inline float temposyncRatio(GlobalStorage *s, EffectStorage *e, int idx)
    {
        return e->p[idx].temposync ? s->temposyncratio : 1;
    }

    static inline float temposyncRatioInv(GlobalStorage *s, EffectStorage *e, int idx)
    {
        return e->p[idx].temposync ? s->temposyncratio_inv : 1;
    }

    static inline bool isDeactivated(EffectStorage *e, int idx) { return e->p[idx].deactivated; }
    static inline bool isExtended(EffectStorage *e, int idx) { return e->p[idx].extend_range; }
    static inline int deformType(EffectStorage *e, int idx) { return e->p[idx].deform_type; }

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

/*
 * The SurgeSSTFXBase is a template class which adapts the surge Effect virtyal
 * methods to the sst-effects T<Config> concrete methods.
 */
template <typename T> struct SurgeSSTFXBase : T
{
    SurgeSSTFXBase(SurgeStorage *storage, FxStorage *fxdata, pdata *pd) : T(storage, fxdata, pd) {}

    void init() override
    {
        // the values are not copied to the modulation array in all cases at init.
        // Make sure they are here.
        for (int j = 0; j < n_fx_params; j++)
        {
            *T::pd_float[j] = T::fxdata->p[j].val.f;
        }
        T::initialize();
    }

    void process(float *dataL, float *dataR) override { T::processBlock(dataL, dataR); }

    void suspend() override { T::suspendProcessing(); }

    int get_ringout_decay() override { return T::getRingoutDecay(); }

    void sampleRateReset() override { T::onSampleRateChanged(); }

    const char *get_effectname() override { return T::effectName; }

    void init_default_values() override
    {
        for (int i = 0; i < T::numParams; ++i)
        {
            auto pmd = T::paramAt(i);
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::FLOAT)
            {
                this->fxdata->p[i].val.f = pmd.defaultVal;
            }
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::INT)
            {
                this->fxdata->p[i].val.i = (int)std::round(pmd.defaultVal);
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
            bool started{false};
            auto pmd = T::paramAt(i);

            if (this->fxdata->p[i].ctrltype == ct_none &&
                pmd.type == sst::basic_blocks::params::ParamMetaData::NONE)
                continue;

            if (pmd.name.empty())
            {
                std::cout << "\n\n----- " << i << " [pmdname-missing] "
                          << this->fxdata->p[i].get_name() << std::endl;
                started = true;
            }
            this->fxdata->p[i].set_name(pmd.name.c_str());
            this->fxdata->p[i].basicBlocksParamMetaData = pmd;
            auto check = [&, i](auto a, auto b, auto msg) {
                if (a != b)
                {
                    if (!started)
                    {
                        std::cout << "\n\n----- " << i << " `" << pmd.name << "` `"
                                  << this->fxdata->p[i].get_name() << "'" << std::endl;
                        started = true;
                    }
                    // If you are sitting here it is because your ct_blah for p[i] is not
                    // consistent with your sst-effects ParamMetaData. Go fix one of the other
                    // and this message will vanish
                    std::string fxn = fx_type_names[this->fxdata->type.val.i];
                    std::cout << "Metadata Mismatch (fx=" << fxn << " attr=" << msg << "): param["
                              << i << "]='" << pmd.name << "'; param metadata value=" << a
                              << " surge value=" << b << " " << __FILE__ << ":" << __LINE__
                              << std::endl;
                }
            };
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::FLOAT)
            {
                check(pmd.minVal, this->fxdata->p[i].val_min.f, "Minimum Values");
                check(pmd.maxVal, this->fxdata->p[i].val_max.f, "Maximum Values");
                // check(pmd.defaultVal, this->fxdata->p[i].val_default.f, "Default Values");
                this->fxdata->p[i].val_default.f = pmd.defaultVal;
            }
            if (pmd.type == sst::basic_blocks::params::ParamMetaData::Type::INT)
            {
                check((int)pmd.minVal, this->fxdata->p[i].val_min.i, "Minimum Values");
                check((int)pmd.maxVal, this->fxdata->p[i].val_max.i, "Maximum Values");
            }
            check(pmd.canTemposync, this->fxdata->p[i].can_temposync(), "Can Temposync");
            check(pmd.canDeform, this->fxdata->p[i].has_deformoptions(), "Can Deform");
            check(pmd.canAbsolute, this->fxdata->p[i].can_be_absolute(), "Can Be Absolute");
            check(pmd.canExtend, this->fxdata->p[i].can_extend_range(), "Can Extend");
            check(pmd.canDeactivate, this->fxdata->p[i].can_deactivate(), "Can Deactivate");
            check(pmd.supportsStringConversion, true, "Supports string value");
        }
    }
};

} // namespace surge::sstfx

#endif // SURGE_SURGESSTFXADAPTER_H
