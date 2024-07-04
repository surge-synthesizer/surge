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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_VOCODEREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_VOCODEREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include "VectorizedSVFilter.h"

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
    lipol_ps_blocksz mGain alignas(16);
    lipol_ps_blocksz mGainR alignas(16);
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

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_VOCODEREFFECT_H
