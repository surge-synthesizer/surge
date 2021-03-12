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
#include "BiquadFilter.h"
#include "OscillatorCommonFunctions.h"
#include <random>

class WaveguideOscillator : public Oscillator
{
  public:
    enum wg_params
    {
        wg_exciter_mode,
        wg_exciter_level,

        wg_str1_decay,
        wg_str2_decay,
        wg_str2_detune,
        wg_str_balance,

        wg_stiffness,
    };

    WaveguideOscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p)
        : Oscillator(s, o, p), lp(s), noiseLp(s)
    {
    }

    virtual void init(float pitch, bool is_display = false, bool nonzero_drift = true);
    virtual void init_ctrltypes(int scene, int oscnum) { init_ctrltypes(); };
    virtual void init_ctrltypes();
    virtual void init_default_values();
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f);

    float phase1 = 0, phase2 = 0;

    static constexpr float dustpos = 0.998, dustneg = 1.0 - dustpos;

    lag<float, true> examp, tap[2], t2level, feedback[2], tone, fmdepth;

    SSESincDelayLine<16384> delayLine[2];
    float priorSample[2] = {0, 0};
    Surge::Oscillator::DriftLFO driftLFO[2];
    Surge::Oscillator::CharacterFilter<float> charFilt;

    std::minstd_rand gen;
    std::uniform_real_distribution<float> urd;

    float dustBuffer[2][BLOCK_SIZE_OS];
    void fillDustBuffer(float tap0, float tap1);

    BiquadFilter lp, noiseLp;
};

#endif // SURGE_WAVEGUIDEOSCILLATOR_H
