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

#ifndef SURGE_WAVEGUIDEOSCILLATOR_H
#define SURGE_WAVEGUIDEOSCILLATOR_H

#include "OscillatorBase.h"
#include "SurgeStorage.h"
#include "DspUtilities.h"
#include "SSESincDelayLine.h"

class WaveguideOscillator : public Oscillator
{
  public:
    enum
    {
        wg_ex_mode,
        wg_ex_amp,
        wg_feedback,
        wg_feedback2,
        wg_tap2_offset,
        wg_tap2_mix,
        wg_inloop_onepole,
    };
    WaveguideOscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p) : Oscillator(s, o, p) {}

    virtual void init(float pitch, bool is_display = false, bool nonzero_drift = true);
    virtual void init_ctrltypes(int scene, int oscnum) { init_ctrltypes(); };
    virtual void init_ctrltypes();
    virtual void init_default_values();
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f);

    float phase = 0;

    lag<float, true> examp, tap[2], t2level, feedback[2], onepole;

    SSESincDelayLine<16384> delayLine[2];
    float priorSample[2];
};

#endif // SURGE_WAVEGUIDEOSCILLATOR_H
