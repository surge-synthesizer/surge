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
