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

class ClassicOscillator : public AbstractBlitOscillator
{
  public:
    enum co_params
    {
        co_shape = 0,
        co_width1,
        co_width2,
        co_mainsubmix,
        co_sync,
        co_unison_detune,
        co_unison_voices,
    };

    ClassicOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    template <bool FM> void convolute(int voice, bool stereo);
    virtual ~ClassicOscillator();

  private:
    bool first_run;
    float dc, dc_uni[MAX_UNISON], elapsed_time[MAX_UNISON], last_level[MAX_UNISON],
        pwidth[MAX_UNISON], pwidth2[MAX_UNISON];
    template <bool is_init> void update_lagvals();
    float pitch;
    lipol_ps li_hpf, li_DC;
    lag<float> FMdepth, integrator_mult, l_pw, l_pw2, l_shape, l_sub, l_sync;
    int id_pw, id_pw2, id_shape, id_smooth, id_sub, id_sync, id_detune;
    int FMdelay;
    float FMmul_inv;
    float FMphase alignas(16)[BLOCK_SIZE_OS + 4];
    Surge::Oscillator::CharacterFilter<float> charFilt;
};
