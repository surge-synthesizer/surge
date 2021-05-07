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

    lipol_ps li_hpf, li_DC, li_integratormult;
    WavetableOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~WavetableOscillator();
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    void convolute(int voice, bool FM, bool stereo);
    template <bool is_init> void update_lagvals();
    inline float distort_level(float);
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
};
