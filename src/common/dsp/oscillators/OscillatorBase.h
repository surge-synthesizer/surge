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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_OSCILLATORBASE_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_OSCILLATORBASE_H

#include "SurgeStorage.h"
#include "OscillatorCommonFunctions.h"
#include "sst/basic-blocks/dsp/Lag.h"

class alignas(16) Oscillator
{
  public:
    // The data blocks processed by the SIMD instructions (e.g. SSE2), which must
    // always be before any other variables in the class, in order to be properly
    // aligned to 16 bytes.
    float output alignas(16)[BLOCK_SIZE_OS];
    float outputR alignas(16)[BLOCK_SIZE_OS];

    template <typename T, bool first = true> using lag = sst::basic_blocks::dsp::SurgeLag<T, first>;

    Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual ~Oscillator();
    virtual void init(float pitch, bool is_display = false, bool nonzero_init_drift = true){};
    virtual void init_ctrltypes(int scene, int oscnum) { init_ctrltypes(); };
    virtual void init_ctrltypes(){};
    virtual void init_default_values(){};
    virtual void init_extra_config() { oscdata->extraConfig.nData = 0; }
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f)
    {
    }

    virtual void processSamplesForDisplay(float *samples, int size, bool real){};

    virtual void assign_fm(float *master_osc) { this->master_osc = master_osc; }
    virtual bool allow_display() { return true; }
    inline double pitch_to_omega(float x)
    {
        return (2.0 * M_PI * Tunings::MIDI_0_FREQ * storage->note_to_pitch(x) *
                storage->dsamplerate_os_inv);
    }
    inline double pitch_to_dphase(float x)
    {
        return (double)(Tunings::MIDI_0_FREQ * storage->note_to_pitch(x) *
                        storage->dsamplerate_os_inv);
    }

    inline double pitch_to_dphase_with_absolute_offset(float x, float off)
    {
        return (double)(std::max(1.0, Tunings::MIDI_0_FREQ * storage->note_to_pitch(x) + off) *
                        storage->dsamplerate_os_inv);
    }

    virtual void setGate(bool g) { gate = g; }

    virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
    {
        // No-op here.
    }

  protected:
    SurgeStorage *storage;
    OscillatorStorage *oscdata;
    pdata *localcopy;
    float *__restrict master_osc;
    float drift;
    int ticker;
    bool gate = true;
};

class AbstractBlitOscillator : public Oscillator
{
  public:
    AbstractBlitOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);

  protected:
    float oscbuffer alignas(16)[OB_LENGTH + FIRipol_N];
    float oscbufferR alignas(16)[OB_LENGTH + FIRipol_N];
    float dcbuffer alignas(16)[OB_LENGTH + FIRipol_N];
    SIMD_M128 osc_out, osc_out2, osc_outR, osc_out2R;
    void prepare_unison(int voices);
    float integrator_hpf;
    float pitchmult, pitchmult_inv;
    int bufpos;
    int n_unison;
    float out_attenuation, out_attenuation_inv, detune_bias, detune_offset;
    float oscstate[MAX_UNISON], syncstate[MAX_UNISON], rate[MAX_UNISON];
    Surge::Oscillator::DriftLFO driftLFO[MAX_UNISON];
    float panL[MAX_UNISON], panR[MAX_UNISON];
    int state[MAX_UNISON];
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_OSCILLATORBASE_H
