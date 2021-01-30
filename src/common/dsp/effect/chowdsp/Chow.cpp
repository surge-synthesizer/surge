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

#include "Chow.h"

namespace chowdsp
{

Chow::Chow(SurgeStorage *storage, FxStorage *fxdata, pdata *pd) : Effect(storage, fxdata, pd)
{
    makeup.set_blocksize(BLOCK_SIZE);
}

Chow::~Chow() {}

void Chow::init()
{
    os.reset();
    makeup.set_target(1.0f);
    thresh_smooth.reset(SmoothSteps);
    ratio_smooth.reset(SmoothSteps);
}

static inline float chowProcess(float x, const float threshGain, const float ratio)
{
    float y = x;

    if (x > threshGain)
    {
        y = threshGain;
        y += (x - threshGain) / ratio;
    }

    return y;
}

void Chow::process(float *dataL, float *dataR)
{
    set_params();

    if (cur_os)
        process_block_os(dataL, dataR);
    else
        process_block(dataL, dataR);

    makeup.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
}

void Chow::set_params()
{
    const auto threshGain = db_to_linear(*f[chow_thresh]);
    const auto ratio = 9.0f * std::pow(*f[chow_ratio] / -48.f, 2.584f) +
                       1.0f; // hand-tuned from several years ago...
    auto makeup_gain = db_to_linear((*f[chow_thresh] / 12.f) * ((1.0f / ratio) - 1.0f) - 1.0f);

    const auto oversample = static_cast<bool>(*pdata_ival[chow_os]);
    makeup_gain *= oversample ? 4.0f : 1.0f;
    makeup.set_target_smoothed(makeup_gain);

    cur_os = oversample;
    thresh_smooth.setTargetValue(threshGain);
    ratio_smooth.setTargetValue(ratio);
}

void Chow::process_block(float *dataL, float *dataR)
{
    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float cur_thresh = thresh_smooth.getNextValue();
        float cur_ratio = ratio_smooth.getNextValue();

        dataL[k] = chowProcess(dataL[k], cur_thresh, cur_ratio);
        dataR[k] = chowProcess(dataR[k], cur_thresh, cur_ratio);
    }
}

void Chow::process_block_os(float *dataL, float *dataR)
{
    os.upsample(dataL, dataR);

    float cur_thresh, cur_ratio;
    for (int k = 0; k < os.getUpBlockSize(); k++)
    {
        if (k % os.getOSRatio() == 0)
        {
            cur_thresh = thresh_smooth.getNextValue();
            cur_ratio = ratio_smooth.getNextValue();
        }

        os.leftUp[k] = chowProcess(os.leftUp[k], cur_thresh, cur_ratio);
        os.rightUp[k] = chowProcess(os.rightUp[k], cur_thresh, cur_ratio);
    }

    os.downsample(dataL, dataR);
}

void Chow::suspend() {}

void Chow::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[chow_thresh].set_name("Threshold");
    fxdata->p[chow_thresh].set_type(ct_decibel_attenuation_large);
    fxdata->p[chow_thresh].posy_offset = 1;

    fxdata->p[chow_ratio].set_name("Ratio");
    fxdata->p[chow_ratio].set_type(ct_decibel_attenuation);
    fxdata->p[chow_ratio].posy_offset = 1;

    fxdata->p[chow_os].set_name("Oversampling");
    fxdata->p[chow_os].set_type(ct_bool);
    fxdata->p[chow_os].posy_offset = 1;
}

void Chow::init_default_values()
{
    fxdata->p[chow_thresh].val.f = -27.f;
    fxdata->p[chow_ratio].val.f = -10.f;
    fxdata->p[chow_os].val.f = 0.f;
}

const char *Chow::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "CHOW";
    }

    return 0;
}

int Chow::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    }
    return 0;
}

} // namespace chowdsp
