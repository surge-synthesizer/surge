// SPDX-License-Identifier: GPL-3.0-or-later
#include "Engine.h"
#include "SurgeSynthesizer.h"
#include "dsp/SurgeVoice.h"
#include "dsp/modulators/FormulaModulationHelper.h"
#include "dsp/effects/ConvolutionEffect.h"
#include "dsp/effects/airwindows/AirWindowsEffect.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

namespace
{
thread_local std::string lastError;
std::string jsonString(const char *s)
{
    std::string result = "\"";
    for (const unsigned char c : std::string(s))
    {
        if (c == '"' || c == '\\')
        {
            result += '\\';
            result += c;
        }
        else if (c < 32)
        {
            char b[7];
            snprintf(b, sizeof(b), "\\u%04x", c);
            result += b;
        }
        else
            result += c;
    }
    return result + '"';
}
template <typename F> int checked(F fn)
{
    lastError.clear();
    try
    {
        fn();
        return 1;
    }
    catch (const std::exception &e)
    {
        lastError = e.what();
    }
    catch (...)
    {
        lastError = "Unknown engine error";
    }
    return 0;
}
} // namespace
struct SurgeWebEngine : SurgeSynthesizer::PluginLayer
{
    std::unique_ptr<SurgeSynthesizer> synth;
    std::string metadata;
    float input[2][BLOCK_SIZE]{};
    float output[2][BLOCK_SIZE]{};
    alignas(16) float effectBlock[2][BLOCK_SIZE]{};
    int cursor{0};
    double sampleRate;
    void resolveAirwindowsSelection()
    {
        // This offline API owns its engine exclusively; unlike the original UI,
        // it has no periodic control pump to finish mailbox requests.
#if SURGE_WEB
        const auto failures = synth->browserEffectPreparationFailures.load(std::memory_order_acquire);
        // At most three transitions: reclaim a superseded request, prepare the
        // current request, then adopt and reclaim it. Never spin on a worker.
        for (int pass = 0; pass < 3; ++pass)
        {
            synth->processBrowserAirwindowsSelections();
            synth->prepareBrowserEffects();
        }
        if (synth->browserEffectPreparationFailures.load(std::memory_order_acquire) != failures)
            throw std::runtime_error("Unable to prepare the Airwindows selection");
        for (const auto &wanted : synth->browserAirwindowsWanted)
            if (wanted.load(std::memory_order_acquire) >= 0)
                throw std::runtime_error("Airwindows selection is still pending");
#else
        for (int slot = 0; slot < n_fx_slots; ++slot)
            if (auto *aw = dynamic_cast<AirWindowsEffect *>(synth->fx[slot].get()))
            {
                auto &selector = synth->storage.getPatch().fx[slot].p[0];
                if (aw->lastSelected != selector.val.i)
                    aw->setupSubFX(selector.val.i, selector.user_data == nullptr);
            }
#endif
    }
    void surgeParameterUpdated(const SurgeSynthesizer::ID &, float) override {}
    void surgeMacroUpdated(long, float) override {}
    SurgeWebEngine(double sr, const char *path) : sampleRate(sr)
    {
        synth = std::make_unique<SurgeSynthesizer>(this, path);
        synth->setSamplerate(sr);
        synth->time_data.tempo = 120;
        synth->time_data.ppqPos = 0;
        synth->processAudioThreadOpsWhenAudioEngineUnavailable(true);
    }
};
extern "C"
{
    EXPORT SurgeWebEngine *surge_create(double sr, const char *path)
    {
        SurgeWebEngine *result = nullptr;
        checked([&] {
            if (!std::isfinite(sr) || sr < 12000 || sr > 384000 || !path)
                throw std::runtime_error("Invalid sample rate or data path");
            result = new SurgeWebEngine(sr, path);
        });
        return result;
    }
    EXPORT void surge_destroy(SurgeWebEngine *e) { delete e; }
    EXPORT double surge_formula_compilation_count(SurgeWebEngine *e)
    {
        return e ? static_cast<double>(e->synth->storage.formulaGlobalData->audioFunctions.compilationAttempts.load(std::memory_order_relaxed)) : -1.;
    }
    EXPORT int surge_set_impulse(SurgeWebEngine *e, int slot, double rate,
                                 const float *left, const float *right, int frames)
    {
        return checked([&] {
            if (!e || slot < 0 || slot >= n_fx_slots || !left || frames < 1 ||
                frames > (1 << 22) || !std::isfinite(rate) || rate < 8000 || rate > 384000)
                throw std::runtime_error("Invalid impulse response");
            for (int i = 0; i < frames; ++i)
                if (!std::isfinite(left[i]) || (right && !std::isfinite(right[i])))
                    throw std::runtime_error("Non-finite impulse sample");
            auto &synth = *e->synth;
            auto candidate = synth.storage.getPatch().fx[slot];
            if (candidate.type.val.i != fxt_convolution)
            {
                candidate.type.val.i = fxt_convolution;
                auto effect = std::unique_ptr<Effect>(spawn_effect(fxt_convolution, &synth.storage, &candidate, nullptr));
                effect->init_ctrltypes();
                effect->init_default_values();
            }
            candidate.user_data.clear();
            candidate.user_data.emplace("irname", ArbitraryBlockStorage::from_string("Imported impulse"));
            candidate.user_data.emplace("samplerate", ArbitraryBlockStorage::from_float(rate));
            candidate.user_data.emplace("left", ArbitraryBlockStorage::from_floats(std::span<const float>(left, frames)));
            if (right)
                candidate.user_data.emplace("right", ArbitraryBlockStorage::from_floats(std::span<const float>(right, frames)));
            synth.fxsync[slot] = std::move(candidate);
            synth.fx_reload[slot] = true;
            synth.load_fx_needed = true;
            synth.processAudioThreadOpsWhenAudioEngineUnavailable(true);
        });
    }
    EXPORT const char *surge_error() { return lastError.c_str(); }
    EXPORT int surge_load_patch(SurgeWebEngine *e, const char *path)
    {
        return checked([&] {
            if (!e || !path)
                throw std::runtime_error("Missing engine or patch path");
            if (!e->synth->loadPatchByPath(path, -1, "", false))
                throw std::runtime_error("Unable to load patch");
            e->synth->processAudioThreadOpsWhenAudioEngineUnavailable(true);
            e->cursor = 0;
            memset(e->output, 0, sizeof(e->output));
        });
    }
    EXPORT int surge_save_patch(SurgeWebEngine *e, const char *path)
    {
        return checked([&] {
            if (!e || !path)
                throw std::runtime_error("Missing engine or patch path");
            e->synth->savePatchToPath(fs::path(path), false);
            if (!fs::exists(path))
                throw std::runtime_error("Unable to save patch");
        });
    }
    EXPORT int surge_parameter_count(SurgeWebEngine *e)
    {
        return e ? static_cast<int>(e->synth->storage.getPatch().param_ptr.size()) : 0;
    }
    EXPORT const char *surge_parameter_info(SurgeWebEngine *e, int id)
    {
        if (!e || id < 0 || id >= surge_parameter_count(e))
            return nullptr;
        if (!checked([&] {
                auto *p = e->synth->storage.getPatch().param_ptr[id];
                char name[256]{}, display[256]{};
                auto sid = e->synth->idForParameter(p);
                e->synth->getParameterName(sid, name);
                e->synth->getParameterDisplay(sid, display);
                std::ostringstream s;
                s << "{\"id\":" << id << ",\"name\":" << jsonString(name)
                  << ",\"display\":" << jsonString(display)
                  << ",\"value\":" << e->synth->getParameter01(sid) << "}";
                e->metadata = s.str();
            }))
            return nullptr;
        return e->metadata.c_str();
    }
    EXPORT int surge_set_parameter(SurgeWebEngine *e, int id, float value)
    {
        return checked([&] {
            if (!e || id < 0 || id >= surge_parameter_count(e) || !std::isfinite(value) ||
                value < 0 || value > 1)
                throw std::runtime_error("Invalid parameter edit");
            auto *p = e->synth->storage.getPatch().param_ptr[id];
            e->synth->setParameter01(e->synth->idForParameter(p), value);
            if (p->ctrltype == ct_airwindows_fx) e->resolveAirwindowsSelection();
        });
    }
    EXPORT int surge_seed_voice_mseg(SurgeWebEngine *e, int scene, int lfo, uint32_t seed)
    {
        return checked([&] {
            if (!e || scene < 0 || scene >= n_scenes || lfo < 0 || lfo >= n_lfos_voice)
                throw std::runtime_error("Invalid voice MSEG seed destination");
            auto &synth = *e->synth;
            if (synth.voices[scene].empty() ||
                synth.storage.getPatch().scene[scene].lfo[lfo].shape.val.i != lt_mseg)
                throw std::runtime_error("No active voice MSEG at the seed destination");
            for (auto *voice : synth.voices[scene])
            {
                auto *source = dynamic_cast<LFOModulationSource *>(voice->modsources[ms_lfo1 + lfo]);
                if (!source) throw std::runtime_error("Missing voice LFO evaluator");
                source->msegstate.seed(seed);
            }
        });
    }
    EXPORT int surge_seed_storage_rng(SurgeWebEngine *e, uint32_t seed)
    {
        return checked([&] {
            if (!e) throw std::runtime_error("Missing engine for RNG seed");
            e->synth->storage.rngGen.g.seed(seed);
        });
    }
    EXPORT int surge_set_oscillator_type(SurgeWebEngine *e, int scene, int oscillator, int type)
    {
        return checked([&] {
            if (!e || scene < 0 || scene >= n_scenes || oscillator < 0 || oscillator >= n_oscs ||
                type < 0 || type >= n_osc_types)
                throw std::runtime_error("Invalid oscillator scene, slot or type");
            auto &synth = *e->synth;
            auto &parameter = synth.storage.getPatch().scene[scene].osc[oscillator].type;
            synth.setParameter01(synth.idForParameter(&parameter), parameter.value_to_normalized(type));
            synth.processAudioThreadOpsWhenAudioEngineUnavailable(true);
        });
    }
    EXPORT int surge_set_effect_type(SurgeWebEngine *e, int slot, int type)
    {
        return checked([&] {
            if (!e || slot < 0 || slot >= n_fx_slots || type < 0 || type >= n_fx_types)
                throw std::runtime_error("Invalid effect slot or type");
            auto &synth = *e->synth;
            auto &parameter = synth.storage.getPatch().fx[slot].type;
            synth.setParameter01(synth.idForParameter(&parameter), parameter.value_to_normalized(type));
            synth.processAudioThreadOpsWhenAudioEngineUnavailable(true);
        });
    }
    EXPORT int surge_set_effect_parameter(SurgeWebEngine *e, int slot, int parameter, float normalized)
    {
        return checked([&] {
            if (!e || slot < 0 || slot >= n_fx_slots || parameter < 0 || parameter >= n_fx_params ||
                !std::isfinite(normalized) || normalized < 0 || normalized > 1)
                throw std::runtime_error("Invalid effect parameter edit");
            auto &synth = *e->synth;
            // This offline API owns the engine exclusively. Resolve a pending
            // sub-effect selection before editing its parameters, otherwise the
            // first audio block would replace those edits with the new defaults.
            if (parameter > 0)
                if (auto *aw = dynamic_cast<AirWindowsEffect *>(synth.fx[slot].get()))
                {
                    auto &selector = synth.storage.getPatch().fx[slot].p[0];
                    if (aw->lastSelected != selector.val.i)
                        aw->setupSubFX(selector.val.i, selector.user_data == nullptr);
                }
            auto &p = synth.storage.getPatch().fx[slot].p[parameter];
            if (p.ctrltype == ct_none) throw std::runtime_error("Effect parameter is inactive");
            synth.setParameter01(synth.idForParameter(&p), normalized);
            if (p.ctrltype == ct_airwindows_fx) e->resolveAirwindowsSelection();
        });
    }
    EXPORT int surge_midi(SurgeWebEngine *e, int status, int a, int b)
    {
        if (!e || status < 0x80 || status > 0xef || a < 0 || a > 127 || b < 0 || b > 127)
            return 0;
        auto &s = *e->synth;
        int ch = status & 15;
        switch (status & 0xf0)
        {
        case 0x80:
            s.releaseNote(ch, a, b);
            break;
        case 0x90:
            if (b)
                s.playNote(ch, a, b, 0);
            else
                s.releaseNote(ch, a, 0);
            break;
        case 0xa0:
            s.polyAftertouch(ch, a, b);
            break;
        case 0xb0:
            s.channelController(ch, a, b);
            break;
        case 0xd0:
            s.channelAftertouch(ch, a);
            break;
        case 0xe0:
            s.pitchBend(ch, (a | (b << 7)) - 8192);
            break;
        default:
            return 0;
        }
        return 1;
    }
    EXPORT int surge_set_transport(SurgeWebEngine *e, double bpm, double ppq)
    {
        if (!e || !std::isfinite(bpm) || bpm <= 0 || !std::isfinite(ppq))
            return 0;
        e->synth->time_data.tempo = bpm;
        e->synth->time_data.ppqPos = ppq;
        return 1;
    }
    EXPORT int surge_render_effect_block(SurgeWebEngine *e, int slot, const float *il,
                                         const float *ir, float *l, float *r)
    {
        static_assert(BLOCK_SIZE == 32, "Update the isolated-effect ABI for a changed block size");
        return checked([&] {
            if (!e || slot < 0 || slot >= n_fx_slots || !il || !l || !r)
                throw std::runtime_error("Invalid isolated effect render arguments");
            if (!ir) ir = il;
            for (int i = 0; i < BLOCK_SIZE; ++i)
                if (!std::isfinite(il[i]) || !std::isfinite(ir[i]))
                    throw std::runtime_error("Non-finite isolated effect input");
            auto &synth = *e->synth;
            synth.storage.getPatch().copy_globaldata(synth.storage.getPatch().globaldata);
            synth.storage.songpos = synth.time_data.ppqPos;
            synth.storage.temposyncratio = synth.time_data.tempo / 120.f;
            synth.storage.temposyncratio_inv = 1.f / synth.storage.temposyncratio;
            memcpy(e->effectBlock[0], il, sizeof(e->effectBlock[0]));
            memcpy(e->effectBlock[1], ir, sizeof(e->effectBlock[1]));
            if (synth.fx[slot])
                synth.fx[slot]->process_ringout(e->effectBlock[0], e->effectBlock[1], true);
            memcpy(l, e->effectBlock[0], sizeof(e->effectBlock[0]));
            memcpy(r, e->effectBlock[1], sizeof(e->effectBlock[1]));
            synth.time_data.ppqPos += BLOCK_SIZE / e->sampleRate * synth.time_data.tempo / 60.;
        });
    }
    EXPORT int surge_render(SurgeWebEngine *e, const float *il, const float *ir, float *l, float *r,
                            int frames)
    {
        if (!e || !l || !r || frames < 0)
            return 0;
        // One block of latency permits any Web Audio quantum, including nonmultiples of 32.
        for (int i = 0; i < frames; ++i)
        {
            const int k = e->cursor;
            e->input[0][k] = il ? il[i] : 0;
            e->input[1][k] = ir ? ir[i] : (il ? il[i] : 0);
            l[i] = e->output[0][k];
            r[i] = e->output[1][k];
            if (++e->cursor == BLOCK_SIZE)
            {
                auto &s = *e->synth;
                s.process_input = il != nullptr;
                memcpy(s.input, e->input, sizeof(e->input));
                s.process();
                memcpy(e->output, s.output, sizeof(e->output));
                s.time_data.ppqPos += BLOCK_SIZE / e->sampleRate * s.time_data.tempo / 60.;
                e->cursor = 0;
            }
        }
        return 1;
    }
}
