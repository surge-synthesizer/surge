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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_AUDIOINPUTOSCILLATOR_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_AUDIOINPUTOSCILLATOR_H

#include "OscillatorBase.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"

class AudioInputOscillator : public Oscillator
{
  public:
    enum audioin_params
    {
        audioin_channel = 0,
        audioin_gain,
        audioin_sceneAchan,
        audioin_sceneAgain,
        audioin_sceneAmix,
        audioin_lowcut,
        audioin_highcut,
    };

    /* add controls:
    limiter?
    */

    AudioInputOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~AudioInputOscillator();
    virtual void init_ctrltypes(int scene, int osc) override;
    virtual void init_default_values() override;
    virtual bool allow_display() override { return false; }
    bool isInSceneB;
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    BiquadFilter lp, hp;
    void applyFilter();
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_AUDIOINPUTOSCILLATOR_H
