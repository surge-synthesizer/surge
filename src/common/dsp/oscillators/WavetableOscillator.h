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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_WAVETABLEOSCILLATOR_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_WAVETABLEOSCILLATOR_H

#include "OscillatorBase.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"

class WavetableOscillator : public AbstractBlitOscillator
{
  public:
    enum wt_params
    {
        wt_morph = 0,
        wt_skewv,
        wt_saturate,
        wt_formant,
        wt_skewh,
        wt_unison_detune,
        wt_unison_voices,
    };

    enum FeatureDeform
    {
        XT_134_EARLIER = 0,
        XT_14 = 1 << 0
    };

    // Deform on the Unison Voices parameter, deciding what the count means once the
    // wavetable is a sample. It lives on the oscillator rather than on the wavetable so
    // that two oscillators pointed at the same table can read it differently, and so the
    // choice belongs to the patch rather than riding along in the .wt header.
    enum SampleUnisonDeform
    {
        UNISON_VOICES = 0,
        SAMPLE_PLAY_COUNT = 1
    };

    // Play count value meaning "never stop", which wtf_loop_sample pins the countdown to.
    // It sits above MAX_UNISON deliberately, so that when the SAMPLE_PLAY_COUNT deform makes
    // the unison voice count double as the play count, the whole 1 to MAX_UNISON range stays
    // a literal play count. Public because the Window oscillator counts plays the same way.
    static constexpr int infinite_sampleloop = MAX_UNISON + 1;

    lipol_ps li_hpf, li_DC, li_integratormult;
    WavetableOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy,
                        pdata *localcopyUnmod);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~WavetableOscillator();
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    void processSamplesForDisplay(float *samples, int size, bool real) override;

    static const int SAMPLES_FOR_DISPLAY = 60;
    static double skewHPhaseResponse[SAMPLES_FOR_DISPLAY];

  private:
    void convolute(int voice, bool FM, bool stereo);
    template <bool is_init> void update_lagvals();
    inline float distort_level(float);
    void readDeformType();
    void selectDeform();
    float getMorph();
    float deformLegacy(float, int);
    float deformContinuous(float, int);
    float deformMorph(float, int);

    float (WavetableOscillator::*deformSelected)(float, int);
    bool first_run;
    float oscpitch[MAX_UNISON];
    float dc, dc_uni[MAX_UNISON], last_level[MAX_UNISON];
    float pitch;
    int mipmap[MAX_UNISON], mipmap_ofs[MAX_UNISON];
    lag<float> FMdepth, hpf_coeff, integrator_mult, l_hskew, l_vskew, l_clip, l_shape;
    float formant_t, formant_last, pitch_last, pitch_t;
    // Frame position. Per unison voice because a sample advances each voice through the table
    // on its own, which is what makes real unison on a sample possible at all; these were
    // scalars when a sample could only ever run one voice.
    //
    // A wavetable still has just one position, taken from Morph and therefore the same for
    // every voice. process_block works it out once and writes it to each sounding voice, so
    // the deform functions can index by voice without asking which mode they are in.
    float tableipol[MAX_UNISON], last_tableipol[MAX_UNISON];
    float hskew, last_hskew;
    int id_shape, id_vskew, id_hskew, id_clip, id_detune, id_formant;
    int tableid[MAX_UNISON]; // per voice for the same reason as tableipol above
    // Read only by the pre-1.4 morph clamp and the init/reset paths, never in sample mode,
    // so this one stays shared
    int last_tableid;
    int FMdelay;
    int nointerp;
    float FMmul_inv;
    // Plays remaining for sample-mode playback, per voice, counted down each time the sample
    // wraps. At infinite_sampleloop the countdown is skipped entirely.
    int sampleloop[MAX_UNISON];

    pdata *unmodulatedLocalcopy;
    FeatureDeform deformType;
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_WAVETABLEOSCILLATOR_H
