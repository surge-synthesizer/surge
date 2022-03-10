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

class FlangerEffect : public Effect
{
    enum fl_modes
    {
        flm_classic = 0,
        flm_doppler,
        flm_arp_mix,
        flm_arp_solo,
    };

    enum fl_waves
    {
        flw_sine = 0,
        flw_tri,
        flw_saw,
        flw_sng,
        flw_snh,
        flw_square,
    };

    enum fl_params
    {
        // Basic Control
        fl_mode = 0, // flange, phase-inverse-flange, arepeggio, vibrato
        fl_wave,     // what's the wave shape
        fl_rate,     // How quickly the oscillations happen
        fl_depth,    // How extreme the modulation of the delay is

        // Voices
        fl_voices,          // how many delay lines
        fl_voice_basepitch, // tune the first max delay line to this (M = sr / f)
        fl_voice_spacing,   // How far apart are the combs in pitch space

        // Feedback and EQ
        fl_feedback, // how much the output feeds back into the filters
        fl_damping,  // how much low pass damping in the feedback mechanism
        fl_width,    // how much to pan the delay lines ( 0 -> all even; 1 -> full spread)
        fl_mix,      // how much we add the comb into the mix

        fl_num_params,
    };

    static const int COMBS_PER_CHANNEL = 4;
    struct InterpDelay
    {
        // OK so lets say we want lowest tunable frequency to be 23.5hz at 96k
        // 96000/23.5 = 4084
        // And lets future proof a bit and make it a power of 2 so we can use & properly
        static const int DELAY_SIZE = 32768, DELAY_SIZE_MASK = DELAY_SIZE - 1;
        float line[DELAY_SIZE];
        int k = 0;
        InterpDelay() { reset(); }
        void reset()
        {
            memset(line, 0, DELAY_SIZE * sizeof(float));
            k = 0;
        }
        float value(float delayBy);
        void push(float nv)
        {
            k = (k + 1) & DELAY_SIZE_MASK;
            line[k] = nv;
        }
    };

  public:
    FlangerEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~FlangerEffect();
    virtual const char *get_effectname() override { return "flanger"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual int get_ringout_decay() override { return ringout_value; }

  private:
    int ringout_value = -1;
    InterpDelay idels[2];

    float lfophase[2][COMBS_PER_CHANNEL], longphase[2];
    float lpaL = 0.f, lpaR = 0.f; // state for the onepole LP filter

    lipol<float, true> lfoval[2][COMBS_PER_CHANNEL], delaybase[2][COMBS_PER_CHANNEL];
    lipol<float, true> depth, mix;
    lipol<float, true> voices, voice_detune, voice_chord;
    lipol<float, true> feedback, fb_hf_damping;
    lag<float> vzeropitch;
    float lfosandhtarget[2][COMBS_PER_CHANNEL];
    float vweights[2][COMBS_PER_CHANNEL];

    lipol_ps width;
    bool haveProcessed = false;

    const static int LFO_TABLE_SIZE = 8192;
    const static int LFO_TABLE_MASK = LFO_TABLE_SIZE - 1;
    float sin_lfo_table[LFO_TABLE_SIZE];
    float saw_lfo_table[LFO_TABLE_SIZE]; // don't make it analytic since I want to smooth the edges
};
