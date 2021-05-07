/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

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
