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

#ifndef SURGE_SRC_COMMON_MODULATIONSOURCE_H
#define SURGE_SRC_COMMON_MODULATIONSOURCE_H

#include <random>
#include <cassert>

#include "basic_dsp.h"

enum modsrctype
{
    mst_undefined = 0,
    mst_controller,
    mst_adsr,
    mst_lfo,
    mst_stepseq,
};

enum modsources
{
    ms_original = 0,
    ms_velocity,
    ms_keytrack,
    ms_polyaftertouch,
    ms_aftertouch,
    ms_pitchbend,
    ms_modwheel,
    ms_ctrl1,
    ms_ctrl2,
    ms_ctrl3,
    ms_ctrl4,
    ms_ctrl5,
    ms_ctrl6,
    ms_ctrl7,
    ms_ctrl8,
    ms_ampeg,
    ms_filtereg,
    ms_lfo1,
    ms_lfo2,
    ms_lfo3,
    ms_lfo4,
    ms_lfo5,
    ms_lfo6,
    ms_slfo1,
    ms_slfo2,
    ms_slfo3,
    ms_slfo4,
    ms_slfo5,
    ms_slfo6,
    ms_timbre,
    ms_releasevelocity,
    ms_random_bipolar,
    ms_random_unipolar,
    ms_alternate_bipolar,
    ms_alternate_unipolar,
    ms_breath,
    ms_expression,
    ms_sustain,
    ms_lowest_key,
    ms_highest_key,
    ms_latest_key,
    n_modsources,
};

const int modsource_display_order[n_modsources] = {
    ms_original,
    ms_velocity,
    ms_releasevelocity,
    ms_keytrack,
    ms_lowest_key,
    ms_highest_key,
    ms_latest_key,
    ms_aftertouch,
    ms_polyaftertouch,
    ms_pitchbend,
    ms_modwheel,
    ms_breath,
    ms_expression,
    ms_sustain,
    ms_timbre,
    ms_alternate_bipolar,
    ms_alternate_unipolar,
    ms_random_bipolar,
    ms_random_unipolar,
    ms_filtereg,
    ms_ampeg,
    ms_lfo1,
    ms_lfo2,
    ms_lfo3,
    ms_lfo4,
    ms_lfo5,
    ms_lfo6,
    ms_slfo1,
    ms_slfo2,
    ms_slfo3,
    ms_slfo4,
    ms_slfo5,
    ms_slfo6,
    ms_ctrl1,
    ms_ctrl2,
    ms_ctrl3,
    ms_ctrl4,
    ms_ctrl5,
    ms_ctrl6,
    ms_ctrl7,
    ms_ctrl8,
};

const int n_customcontrollers = 8;

const char modsource_names_button[n_modsources][32] = {
    "Off",         "Velocity",
    "Keytrack",    "Poly AT",
    "Channel AT", // note there is an override of this in SGE modulatorName for MPE
    "Pitch Bend",  "Modwheel",
    "Macro 1",     "Macro 2",
    "Macro 3",     "Macro 4",
    "Macro 5",     "Macro 6",
    "Macro 7",     "Macro 8",
    "Amp EG",      "Filter EG",
    "LFO 1",       "LFO 2",
    "LFO 3",       "LFO 4",
    "LFO 5",       "LFO 6",
    "S-LFO 1",     "S-LFO 2",
    "S-LFO 3",     "S-LFO 4",
    "S-LFO 5",     "S-LFO 6",
    "Timbre",      "Release Velocity",
    "Random",      "Rand Uni",
    "Alternate",   "Alternate Uni",
    "Breath",      "Expression",
    "Sustain",
    "Lowest Key", // "key/voice is now an index
    "Highest Key", "Latest Key",
};

const char modsource_names[n_modsources][32] = {
    "Off",
    "Velocity",
    "Keytrack",
    "Polyphonic Aftertouch",
    "Channel Aftertouch",
    "Pitch Bend",
    "Modwheel",
    "Macro 1",
    "Macro 2",
    "Macro 3",
    "Macro 4",
    "Macro 5",
    "Macro 6",
    "Macro 7",
    "Macro 8",
    "Amp EG",
    "Filter EG",
    "Voice LFO 1",
    "Voice LFO 2",
    "Voice LFO 3",
    "Voice LFO 4",
    "Voice LFO 5",
    "Voice LFO 6",
    "Scene LFO 1",
    "Scene LFO 2",
    "Scene LFO 3",
    "Scene LFO 4",
    "Scene LFO 5",
    "Scene LFO 6",
    "Timbre",
    "Release Velocity",
    "Random Bipolar",
    "Random Unipolar",
    "Alternate Bipolar",
    "Alternate Unipolar",
    "Breath",
    "Expression",
    "Sustain Pedal",
    "Lowest Key", // "key/voice is now an index"
    "Highest Key",
    "Latest Key",
};

// these names are used for streaming (TODO XT2) and OSC interface - don't change them!
const char modsource_names_tag[n_modsources][32] = {
    "off",     "vel",     "keytrk",  "pat",        "at",          "pb",         "mw",
    "macro_1", "macro_2", "macro_3", "macro_4",    "macro_5",     "macro_6",    "macro_7",
    "macro_8", "aeg",     "feg",     "vlfo_1",     "vlfo_2",      "vlfo_3",     "vlfo_4",
    "vlfo_5",  "vlfo_6",  "slfo_1",  "slfo_2",     "slfo_3",      "slfo_4",     "slfo_5",
    "slfo_6",  "timbre",  "rel_vel", "rand_bi",    "rand_uni",    "alt_bi",     "alt_uni",
    "breath",  "expr",    "sus",     "lowest_key", "highest_key", "latest_key",
};

inline bool isScenelevel(modsources ms)
{
    return (((ms <= ms_ctrl8) || ((ms >= ms_slfo1) && (ms <= ms_slfo6))) &&
            ((ms != ms_velocity) && (ms != ms_keytrack) && (ms != ms_polyaftertouch) &&
             (ms != ms_releasevelocity))) ||
           ((ms >= ms_breath) && (ms <= ms_latest_key));
}

inline bool canModulateMonophonicTarget(modsources ms)
{
    return isScenelevel(ms) || ms == ms_aftertouch;
}

inline bool isCustomController(modsources ms) { return (ms >= ms_ctrl1) && (ms <= ms_ctrl8); }

inline bool isEnvelope(modsources ms) { return (ms == ms_ampeg) || (ms == ms_filtereg); }

inline bool isLFO(modsources ms) { return (ms >= ms_lfo1) && (ms <= ms_slfo6); }

inline bool canModulateModulators(modsources ms) { return (ms != ms_ampeg) && (ms != ms_filtereg); }

inline bool isVoiceModulator(modsources ms) { return !((ms >= ms_slfo1) && (ms <= ms_slfo6)); }

inline bool canModulateVoiceModulators(modsources ms)
{
    return (ms <= ms_ctrl8) || ms == ms_timbre;
}

class SurgeStorage;

namespace ModulatorName
{
std::string modulatorName(const SurgeStorage *s, int ms, bool forButton, int current_scene,
                          int forScene = -1);
std::string modulatorIndexExtension(const SurgeStorage *s, int scene, int ms, int index,
                                    bool shortV = false);
std::string modulatorNameWithIndex(const SurgeStorage *s, int scene, int ms, int index,
                                   bool forButton, bool useScene, bool baseNameOnly = false);
bool supportsIndexedModulator(int scene, modsources modsource);

} // namespace ModulatorName

struct ModulationRouting
{
    int source_id;
    int destination_id;
    float depth;
    bool muted = false;
    int source_index{0};
    /*
     * In the case of routing LFO -> Global, the target parameter doesnt have
     * a scene so we need to disambiguate the source scene. This is only used
     * for global routings. See #2285
     */
    int source_scene{-1};
};

class ModulationSource
{
  public:
    ModulationSource()
    {
        for (int i = 0; i < vecsize; ++i)
        {
            voutput[i] = 0.f;
        }
    }

    virtual ~ModulationSource() {}
    virtual const char *get_title() { return 0; }
    virtual int get_type() { return mst_undefined; }
    virtual void process_block() {}
    virtual void attack(){};
    virtual void release(){};
    virtual void reset(){};

    // override these if you support indices
    virtual void set_active_outputs(int ao) { active_outputs = ao; }
    virtual int get_active_outputs() { return active_outputs; }

    virtual float get_output(int which) const
    {
        if (which == 0)
        {
            return output;
        }
        else if (which < vecsize)
        {
            return voutput[which];
        }
        else
        {
            return 0.f;
        }
    }

    virtual float get_output01(int which) const
    {
        if (which == 0)
        {
            return output;
        }
        else if (which < vecsize)
        {
            return voutput[which];
        }
        else
        {
            return 0.f;
        }
    }

    virtual void set_output(int which, float f)
    {
        if (which == 0)
        {
            output = f;
        }
        else if (which < vecsize)
        {
            voutput[which] = f;
        }
    }

    virtual bool per_voice() { return false; }
    virtual bool is_bipolar() { return false; }
    virtual void set_bipolar(bool b) {}

    inline void set_samplerate(float sr, float sri)
    {
        samplerate = sr;
        samplerate_inv = sri;
    }

  protected:
    float samplerate{0}, samplerate_inv{0};
    int active_outputs{0};
    static constexpr int vecsize = 16;
    float output, voutput[vecsize];
};

namespace Modulator
{
enum SmoothingMode
{
    LEGACY = -1, // This is (1) the exponential backoff and (2) not streamed.
    SLOW_EXP,    // Legacy with a sigma clamp
    FAST_EXP,    // Faster Legacy with a sigma clamp
    FAST_LINE,   // Linearly move
    DIRECT       // Apply the value directly
};
}

template <int NDX = 1> class ControllerModulationSourceVector : public ModulationSource
{
  public:
    // smoothing and shaping behaviors
    Modulator::SmoothingMode smoothingMode = Modulator::SmoothingMode::LEGACY;

    ControllerModulationSourceVector()
    {
        for (int i = 0; i < NDX; ++i)
        {
            target[i] = 0.f;
            value[i] = 0.f;
            changed[i] = true;
        }
        smoothingMode = Modulator::SmoothingMode::LEGACY;
        bipolar = false;
    }

    ControllerModulationSourceVector(Modulator::SmoothingMode mode)
        : ControllerModulationSourceVector()
    {
        smoothingMode = mode;
    }

    virtual ~ControllerModulationSourceVector() {}

    void set_target(float f)
    {
        assert(NDX == 1);
        set_target(0, f);
    }

    void set_target(int idx, float f)
    {
        target[idx] = f;
        startingpoint[idx] = value[idx];
        changed[idx] = true;
    }

    void init(float f)
    {
        assert(NDX == 1);
        init(0, f);
    }

    void init(int idx, float f)
    {
        target[idx] = f;
        value[idx] = f;
        startingpoint[idx] = f;
        changed[idx] = true;
    }

    int get_active_outputs() override
    {
        return NDX; // bipolar can't support lognormal obvs
    }

    void set_target01(float f, bool updatechanged = true)
    {
        assert(NDX == 1);
        set_target01(0, f, updatechanged);
    }

    void set_target01(int idx, float f, bool updatechanged = true)
    {
        if (bipolar)
        {
            target[idx] = 2.f * f - 1.f;
        }
        else
        {
            target[idx] = f;
        }

        startingpoint[idx] = value[idx];

        if (updatechanged)
        {
            changed[idx] = true;
        }
    }

    virtual float get_output(int which) const override { return value[which]; }

    virtual float get_output01(int i) const override
    {
        if (bipolar)
        {
            return 0.5f + 0.5f * value[i];
        }

        return value[i];
    }

    virtual float get_target01(int idx)
    {
        if (bipolar)
        {
            return 0.5f + 0.5f * target[idx];
        }

        return target[idx];
    }

    virtual bool has_changed(int idx, bool reset)
    {
        if (changed[idx])
        {
            if (reset)
            {
                changed[idx] = false;
            }

            return true;
        }

        return false;
    }

    virtual void reset() override
    {
        for (int idx = 0; idx < NDX; ++idx)
        {
            target[idx] = 0.f;
            value[idx] = 0.f;
            bipolar = false;
        }
    }

    inline void processSmoothing(Modulator::SmoothingMode mode, float sigma)
    {
        assert(samplerate > 1000);

        for (int idx = 0; idx < NDX; ++idx)
        {
            if (mode == Modulator::SmoothingMode::LEGACY ||
                mode == Modulator::SmoothingMode::SLOW_EXP ||
                mode == Modulator::SmoothingMode::FAST_EXP)
            {
                float b = fabs(target[idx] - value[idx]);

                if (b < sigma && mode != Modulator::SmoothingMode::LEGACY)
                {
                    value[idx] = target[idx];
                }
                else
                {
                    // Don't allow us to push outside of [target,value]
                    // so clamp the interpolator to 0,1
                    float a =
                        std::clamp((mode == Modulator::SmoothingMode::FAST_EXP ? 0.99f : 0.9f) *
                                       44100 * samplerate_inv * b,
                                   0.f, 1.f);

                    value[idx] = (1 - a) * value[idx] + a * target[idx];
                }

                return;
            };

            if (mode == Modulator::SmoothingMode::FAST_LINE)
            {
                // Apply a constant change until we get there
                // Rate is set so we cover the entire [0, 1] range in 50 blocks at 44.1k
                float sampf = samplerate / 44100;
                float da = (target[idx] - startingpoint[idx]) / (50 * sampf);
                float b = target[idx] - value[idx];

                if (fabs(b) < fabs(da))
                {
                    value[idx] = target[idx];
                }
                else
                {
                    value[idx] += da;
                }
            }

            if (mode == Modulator::SmoothingMode::DIRECT)
            {
                value[idx] = target[idx];
            }

            // Just in case #6835 sneaks back
            assert(!std::isnan(value[idx]) && !std::isinf(value[idx]));
        }
    }

    virtual void process_block() override
    {
        assert(samplerate > 1000);

        processSmoothing(smoothingMode,
                         smoothingMode == Modulator::SmoothingMode::FAST_EXP ? 0.005f : 0.0025f);
    }

    virtual bool process_block_until_close(float sigma)
    {
        assert(samplerate > 1000);

        if (smoothingMode == Modulator::SmoothingMode::LEGACY)
        {
            processSmoothing(Modulator::SmoothingMode::SLOW_EXP, sigma);
        }
        else
        {
            processSmoothing(smoothingMode, sigma);
        }

        auto res = (value[0] != target[0]);

        for (int i = 1; i < NDX; ++i)
        {
            res &= (value[i] != target[i]);
        }

        return res;
    }

    virtual bool is_bipolar() override { return bipolar; }
    virtual void set_bipolar(bool b) override { bipolar = b; }

    float target[NDX], startingpoint[NDX], value[NDX];
    int id; // can be used to assign the controller to a parameter id
    bool bipolar;
    bool changed[NDX];
};

using ControllerModulationSource = ControllerModulationSourceVector<1>;

struct MacroModulationSource : ControllerModulationSource
{
    MacroModulationSource(Modulator::SmoothingMode mode)
        : ControllerModulationSource(mode), modunderlyer(mode)
    {
        modunderlyer.init(0);
    }

    virtual float get_output(int which) const override
    {
        return value[which] + modunderlyer.get_output(which);
    }

    virtual float get_output01(int i) const override
    {
        if (bipolar)
            return 0.5f + 0.5f * (value[i] + modunderlyer.value[i]);
        return value[i] + modunderlyer.value[i];
    }

    void setModulationDepth(float d) { modunderlyer.set_target(d); }

    ControllerModulationSource modunderlyer;
    void process_block() override
    {
        modunderlyer.set_samplerate(samplerate, samplerate_inv);
        modunderlyer.process_block();
        return ControllerModulationSource::process_block();
    }

    bool process_block_until_close(float sigma) override
    {
        modunderlyer.set_samplerate(samplerate, samplerate_inv);
        modunderlyer.process_block_until_close(sigma);
        return ControllerModulationSource::process_block_until_close(sigma);
    }
};

class RandomModulationSource : public ModulationSource
{
  public:
    RandomModulationSource(bool bp) : bipolar(bp)
    {
        std::random_device rd;
        gen = std::minstd_rand(rd());
        if (bp)
        {
            dis = std::uniform_real_distribution<float>(-1.0, 1.0);
            norm = std::normal_distribution<float>(0, 0.33); // stddev
        }
        else
        {
            dis = std::uniform_real_distribution<float>(0.0, 1.0);
            norm = std::normal_distribution<float>(0, 0.33); // stddev
        }
    }

    int get_active_outputs() override
    {
        return 2; // bipolar can't support lognormal obvs
    }

    float get_output(int which) const override { return output[which]; }

    virtual void attack() override
    {
        if (bipolar)
        {
            output[0] = dis(gen);
            output[1] = limitpm1(norm(gen));
        }
        else
        {
            output[0] = dis(gen);
            output[1] = limit01(fabs(norm(gen)));
        }
    }

    float output[2];
    bool bipolar{false};
    std::minstd_rand gen;
    std::uniform_real_distribution<float> dis;
    std::normal_distribution<float> norm;
};

class AlternateModulationSource : public ModulationSource
{
  public:
    AlternateModulationSource(bool bp) : state(false)
    {
        if (bp)
            nv = -1;
        else
            nv = 0;

        pv = 1;
    }

    virtual void attack() override
    {
        if (state)
            output = pv;
        else
            output = nv;
        state = !state;
    }

    bool state;
    float nv, pv;
};

#endif // SURGE_SRC_COMMON_MODULATIONSOURCE_H
