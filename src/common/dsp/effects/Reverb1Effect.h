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
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"
#include "AllpassFilter.h"

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

const int revbits = 15;
const int max_rev_dly = 1 << revbits;
const int rev_tap_bits = 4;
const int rev_taps = 1 << rev_tap_bits;

class Reverb1Effect : public Effect
{
    enum rev1_params
    {
        rev1_predelay = 0,
        rev1_shape,
        rev1_roomsize,
        rev1_decaytime,
        rev1_damping,
        rev1_lowcut,
        rev1_freq1,
        rev1_gain1,
        rev1_highcut,
        rev1_mix,
        rev1_width,
        // rev1_variation,
    };

    float delay_pan_L alignas(16)[rev_taps], delay_pan_R alignas(16)[rev_taps];
    float delay_fb alignas(16)[rev_taps];
    float delay alignas(16)[rev_taps * max_rev_dly];
    float out_tap alignas(16)[rev_taps];
    float predelay alignas(16)[max_rev_dly];
    int delay_time alignas(16)[rev_taps];
    lipol_ps mix alignas(16), width alignas(16);

  public:
    Reverb1Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~Reverb1Effect();
    virtual const char *get_effectname() override { return "reverb"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;
    virtual int get_ringout_decay() override { return ringout_time; }

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    void update_rtime();
    void update_rsize();
    void clear_buffers();
    void loadpreset(int id);
    /*int delay_time_mod[rev_taps];
    int delay_time_dv[rev_taps];*/
    int delay_pos;
    /*	float noise[rev_taps];
            float noise_target[rev_taps];*/
    double modphase;
    int shape;
    float lastf[n_fx_params];
    BiquadFilter band1, locut, hicut;
    int ringout_time;
    int b;
};
