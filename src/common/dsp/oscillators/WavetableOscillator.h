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
    float tableipol, last_tableipol;
    float hskew, last_hskew;
    int id_shape, id_vskew, id_hskew, id_clip, id_detune, id_formant, tableid, last_tableid;
    int FMdelay;
    int nointerp;
    float FMmul_inv;
    int sampleloop;
    pdata *unmodulatedLocalcopy;
    FeatureDeform deformType;
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_WAVETABLEOSCILLATOR_H
