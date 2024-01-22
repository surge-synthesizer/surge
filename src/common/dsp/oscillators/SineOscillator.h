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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_SINEOSCILLATOR_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_SINEOSCILLATOR_H

#include "OscillatorBase.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"
#include "OscillatorCommonFunctions.h"
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

class SineOscillator : public Oscillator
{
  public:
    enum sine_params
    {
        sine_shape,
        sine_feedback,
        sine_FMmode,
        sine_lowcut,
        sine_highcut,
        sine_unison_detune,
        sine_unison_voices,
    };

    SineOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    template <int mode, bool stereo, bool FM>
    void process_block_internal(float pitch, float drift, float FMdepth);

    template <int mode>
    void process_block_legacy(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                              float FMdepth = 0.f);
    virtual ~SineOscillator();
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc sine[MAX_UNISON];
    double phase[MAX_UNISON];
    Surge::Oscillator::DriftLFO driftLFO[MAX_UNISON];
    Surge::Oscillator::CharacterFilter<float> charFilt;
    float fb_val;
    float playingramp[MAX_UNISON], dplaying;
    lag<double> FMdepth;
    lag<double> FB;
    void prepare_unison(int voices);
    int n_unison;
    float out_attenuation, out_attenuation_inv, detune_bias, detune_offset;
    float panL alignas(16)[MAX_UNISON], panR alignas(16)[MAX_UNISON];

    int id_mode, id_fb, id_fmlegacy, id_detune;
    float lastvalue alignas(16)[2][MAX_UNISON];
    bool firstblock = true;

    BiquadFilter lp, hp;
    void applyFilter();

    inline float valueFromSinAndCos(float svalue, float cvalue)
    {
        return valueFromSinAndCos(svalue, cvalue, localcopy[id_mode].i);
    }
    static float valueFromSinAndCos(float svalue, float cvalue, int mode);

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_SINEOSCILLATOR_H
