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

#include "VectorizedSVFilter.h"

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

const int n_vocoder_bands = 20;
const int voc_vector_size = n_vocoder_bands >> 2;

class VocoderEffect : public Effect
{
  public:
    enum vocoder_input_modes
    {
        vim_mono,
        vim_left,
        vim_right,
        vim_stereo,
    };

    enum vocoder_params
    {
        voc_input_gain,
        voc_input_gate,

        voc_envfollow,
        voc_q,
        voc_shift,

        voc_num_bands,
        voc_minfreq,
        voc_maxfreq,

        voc_mod_input,
        voc_mod_range,
        voc_mod_center,

        voc_mix,

        // voc_unvoiced_threshold,

        voc_num_params,
    };

    VocoderEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~VocoderEffect();
    virtual const char *get_effectname() override { return "vocoder"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    virtual int get_ringout_decay() override { return 500; }
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

  private:
    VectorizedSVFilter mCarrierL alignas(16)[voc_vector_size];
    VectorizedSVFilter mCarrierR alignas(16)[voc_vector_size];
    VectorizedSVFilter mModulator alignas(16)[voc_vector_size];
    VectorizedSVFilter mModulatorR alignas(16)[voc_vector_size];
    vFloat mEnvF alignas(16)[voc_vector_size];
    vFloat mEnvFR alignas(16)[voc_vector_size];
    lipol_ps mGain alignas(16);
    lipol_ps mGainR alignas(16);
    int modulator_mode;
    float wet;
    int mBI; // block increment (to keep track of events not occurring every n blocks)
    int active_bands;

    /*
    float mVoicedLevel;
    float mUnvoicedLevel;

    BiquadFilter
            mVoicedDetect,
            mUnvoicedDetect;
    */
};
