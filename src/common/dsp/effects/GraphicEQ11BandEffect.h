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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_GRAPHICEQ11BANDEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_GRAPHICEQ11BANDEFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>

class GraphicEQ11BandEffect : public Effect
{
    lipol_ps_blocksz gain alignas(16);
    lipol_ps_blocksz mix alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    enum geq11_params
    {
        geq11_30 = 0,
        geq11_60,
        geq11_120,
        geq11_250,
        geq11_500,
        geq11_1k,
        geq11_2k,
        geq11_4k,
        geq11_8k,
        geq11_12k,
        geq11_16k,

        geq11_gain,

        geq11_num_ctrls,
    };

    GraphicEQ11BandEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~GraphicEQ11BandEffect();
    virtual const char *get_effectname() override { return "Graphic EQ"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

  private:
    float freqs[11] = {30.f,   60.f,   120.f,  250.f,   500.f,  1000.f,
                       2000.f, 4000.f, 8000.f, 12000.f, 16000.f};
    std::string band_names[11] = {
        "30 Hz", "60 Hz", "120 Hz", "250 Hz", "500 Hz", "1 kHz",
        "2 kHz", "4 kHz", "8 kHz",  "12 kHz", "16 kHz",
    };
    BiquadFilter band1, band2, band3, band4, band5, band6, band7, band8, band9, band10, band11;
    int bi; // block increment (to keep track of events not occurring every n blocks)
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_GRAPHICEQ11BANDEFFECT_H
