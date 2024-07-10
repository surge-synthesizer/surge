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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_WINDOWOSCILLATOR_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_WINDOWOSCILLATOR_H

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

    template <bool FM, bool Full16> void ProcessWindowOscs(bool stereo);
    lag<double> FMdepth[MAX_UNISON];
    lag<float> l_morph;

    float OutAttenuation;
    float DetuneBias, DetuneOffset;
    int NumUnison;
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_WINDOWOSCILLATOR_H
