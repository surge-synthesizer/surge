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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_SAMPLEANDHOLDOSCILLATOR_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_SAMPLEANDHOLDOSCILLATOR_H

#include "OscillatorBase.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"

class SampleAndHoldOscillator : public AbstractBlitOscillator
{
  private:
    lipol_ps li_hpf, li_DC, li_integratormult;

  public:
    enum shnoise_params
    {
        shn_correlation = 0,
        shn_width,
        shn_lowcut,
        shn_highcut,
        shn_sync,
        shn_unison_detune,
        shn_unison_voices,
    };

    SampleAndHoldOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~SampleAndHoldOscillator();
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    BiquadFilter lp, hp;
    void applyFilter();

    void convolute(int voice, bool FM, bool stereo);
    template <bool FM> void process_blockT(float pitch, float depth, float drift = 0);
    template <bool is_init> void update_lagvals();
    bool first_run;
    float dc, dc_uni[MAX_UNISON], elapsed_time[MAX_UNISON], last_level[MAX_UNISON],
        last_level2[MAX_UNISON], pwidth[MAX_UNISON];
    float pitch;
    lag<double> FMdepth, l_pw, l_shape, l_smooth, l_sub, l_sync;
    int id_pw, id_shape, id_smooth, id_sub, id_sync, id_detune;
    int FMdelay;
    float FMmul_inv;
    std::function<float()> urng; // A uniform -1,1 RNG
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_SAMPLEANDHOLDOSCILLATOR_H
