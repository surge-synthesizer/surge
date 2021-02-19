/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_DPWOSCILLATOR_H
#define SURGE_DPWOSCILLATOR_H

#include "OscillatorBase.h"
#include "DspUtilities.h"

class DPWOscillator : public Oscillator
{
  public:
    enum dpw_params
    {
        dpw_saw_mix = 0,
        dpw_pulse_mix,
        dpw_tri_mix,
        dpw_pulse_width,
        dpw_sync,
        dpw_unison_detune,
        dpw_unison_voices,
    };

    enum dpw_multitypes
    {
        dpwm_triangle,
        dpwm_square,
        dpwm_sine,
    } multitype = dpwm_triangle;

    enum dpw_submask
    {
        dpw_subone = 1U << 10,
    };

    static constexpr int sigbuf_len = 6;
    DPWOscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p) : Oscillator(s, o, p)
    {
        for (auto u = 0; u < MAX_UNISON; ++u)
        {
            phase[u] = 0;
            sphase[u] = 0;
            sReset[u] = false;
            mixL[u] = 1.f;
            mixR[u] = 1.f;
        }
    }

    virtual void init(float pitch, bool is_display = false, bool nonzero_init_drift = true);
    virtual void init_ctrltypes(int scene, int oscnum) { init_ctrltypes(); };
    virtual void init_ctrltypes();
    virtual void init_default_values();
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f);

    lag<double, true> sawmix, trimix, sqrmix, pwidth, sync, dpbase[MAX_UNISON], dspbase[MAX_UNISON],
        subdpbase, subdpsbase, detune, pitchlag, fmdepth;

    // character filter
    bool dofilter = true;
    double charfiltB0 = 0.0, charfiltB1 = 0.0, charfiltA1 = 0.0;
    double priorY_L = 0.0, priorY_R = 0.0, priorX_L = 0.0, priorX_R = 0.0;

    int n_unison = 1;
    bool starting = true;
    double phase[MAX_UNISON], sphase[MAX_UNISON], subphase, subsphase;
    bool sReset[MAX_UNISON], subReset;
    double unisonOffsets[MAX_UNISON];
    double mixL[MAX_UNISON], mixR[MAX_UNISON];

    float driftlfo[MAX_UNISON], driftlfo2[MAX_UNISON];

    int cachedDeform = -1;
};

const char dpw_multitype_names[3][16] = {"Triangle", "Square", "Sine"};

#endif // SURGE_DPWOSCILLATOR_H
