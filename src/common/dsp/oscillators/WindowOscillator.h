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
#include "OscillatorCommonFunctions.h"

class WindowOscillator : public Oscillator
{
  public:
    enum window_params
    {
        win_morph = 0,
        win_formant,
        win_window,
        win_lowcut,
        win_highcut,
        win_unison_detune,
        win_unison_voices,
    };

    WindowOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~WindowOscillator();
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    int IOutputL alignas(16)[BLOCK_SIZE_OS];
    int IOutputR alignas(16)[BLOCK_SIZE_OS];

    struct
    {
        unsigned int Pos[MAX_UNISON];
        unsigned int SubPos[MAX_UNISON];
        unsigned int Ratio[MAX_UNISON];
        unsigned int Table[2][MAX_UNISON];
        unsigned int FormantMul[MAX_UNISON];
        unsigned char Gain[MAX_UNISON][2];
        // samples until playback should start (for per-sample scheduling)
        unsigned int DispatchDelay[MAX_UNISON];
        Surge::Oscillator::DriftLFO driftLFO[MAX_UNISON];

        int FMRatio[MAX_UNISON][BLOCK_SIZE_OS];
    } Window alignas(16);

    BiquadFilter lp, hp;
    void applyFilter();
    template <bool is_init> void update_lagvals();

    void ProcessWindowOscs(bool stereo, bool FM);
    lag<double> FMdepth[MAX_UNISON];
    lag<float> l_morph;

    float OutAttenuation;
    float DetuneBias, DetuneOffset;
    int NumUnison;
};
