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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_NIMBUSEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_NIMBUSEFFECT_H

#include "Effect.h"

#include <memory>
#include <vembertech/lipol.h>

namespace clouds
{
class GranularProcessor;
}

struct SRC_STATE_tag;

class NimbusEffect : public Effect
{
  public:
    enum nmb_params
    {
        nmb_mode,
        nmb_quality,

        nmb_position,
        nmb_size,
        nmb_pitch,
        nmb_density,
        nmb_texture,
        nmb_spread,

        nmb_freeze,
        nmb_feedback,

        nmb_reverb,
        nmb_mix,

        nmb_num_params,
    };

  private:
    lipol_ps_blocksz mix alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    NimbusEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~NimbusEffect();
    virtual const char *get_effectname() override { return "Nimbus"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual int get_ringout_decay() override { return -1; }

    // Only used by rack
    void setNimbusTrigger(bool b) { nimbusTrigger = b; }

  private:
    uint8_t *block_mem, *block_ccm;
    clouds::GranularProcessor *processor;
    static constexpr int processor_sr = 32000;
    static constexpr float processor_sr_inv = 1.f / 32000;
    int old_nmb_mode = 0;
    bool nimbusTrigger{false};

    SRC_STATE_tag *surgeSR_to_euroSR, *euroSR_to_surgeSR;

    static constexpr int raw_out_sz = BLOCK_SIZE_OS << 5; // power of 2 pls
    float resampled_output[raw_out_sz][2];                // at sr
    size_t resampReadPtr = 0, resampWritePtr = 1;         // see comment in init

    static constexpr int nimbusprocess_blocksize = 8;
    float stub_input[2][nimbusprocess_blocksize]; // This is the extra sample we have around
    size_t numStubs{0};
    int consumed = 0, created = 0;
    bool builtBuffer{false};
};

#endif // SURGE_NIMBUSEFFECT_H
