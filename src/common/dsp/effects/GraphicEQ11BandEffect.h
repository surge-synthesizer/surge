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

#include <vembertech/halfratefilter.h>
#include <vembertech/lipol.h>

class GraphicEQ11BandEffect : public Effect
{
    lipol_ps gain alignas(16);
    lipol_ps mix alignas(16);

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
