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

#include "CHOWEffect.h"

namespace chowdsp
{

CHOWEffect::CHOWEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    makeup.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);
}

CHOWEffect::~CHOWEffect() {}

void CHOWEffect::init()
{
    os.reset();
    makeup.set_target(1.0f);
    mix.set_target(1.0f);

    thresh_smooth.reset(SmoothSteps);
    ratio_smooth.reset(SmoothSteps);

    mix.instantize();
}

static inline float chowProcess(float x, const float threshGain, const float ratio, const bool flip)
{
    float y = x;

    if (!flip && x > threshGain)
    {
        y = threshGain;
        y += (x - threshGain) / ratio;
    }
    else if (flip && x < -threshGain)
    {
        y = -threshGain;
        y += (x + threshGain) / ratio;
    }

    return y;
}

void CHOWEffect::process(float *dataL, float *dataR)
{
    set_params();

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        L[i] = dataL[i];
        R[i] = dataR[i];
    }

    if (cur_os)
        process_block_os(dataL, dataR);
    else
        process_block(dataL, dataR);

    makeup.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(clamp01(*f[chow_mix]));
    mix.fade_2_blocks_to(L, dataL, R, dataR, dataL, dataR, BLOCK_SIZE_QUAD);
}

void CHOWEffect::set_params()
{
    auto thresh_clamped = limit_range(*f[chow_thresh], fxdata->p[chow_thresh].val_min.f,
                                      fxdata->p[chow_thresh].val_max.f);
    const auto threshGain = storage->db_to_linear(thresh_clamped);

    // hand-tuned from several years ago...
    const auto ratio = limit_range(*f[chow_ratio], fxdata->p[chow_ratio].val_min.f,
                                   fxdata->p[chow_ratio].val_max.f);
    auto makeup_gain =
        storage->db_to_linear((thresh_clamped / 12.f) * ((1.0f / ratio) - 1.0f) - 1.0f);

    makeup_gain *= cur_os ? 4.0f : 1.0f;
    makeup.set_target_smoothed(makeup_gain);

    thresh_smooth.setTargetValue(threshGain);
    ratio_smooth.setTargetValue(ratio);
}

void CHOWEffect::process_block(float *dataL, float *dataR)
{
    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float cur_thresh = thresh_smooth.getNextValue();
        float cur_ratio = ratio_smooth.getNextValue();
        bool cur_flip = fxdata->p[chow_flip].val.b;

        dataL[k] = chowProcess(dataL[k], cur_thresh, cur_ratio, cur_flip);
        dataR[k] = chowProcess(dataR[k], cur_thresh, cur_ratio, cur_flip);
    }
}

void CHOWEffect::process_block_os(float *dataL, float *dataR)
{
    os.upsample(dataL, dataR);

    float cur_thresh, cur_ratio;
    bool cur_flip;

    for (int k = 0; k < os.getUpBlockSize(); k++)
    {
        if (k % os.getOSRatio() == 0)
        {
            cur_thresh = thresh_smooth.getNextValue();
            cur_ratio = ratio_smooth.getNextValue();
            cur_flip = fxdata->p[chow_flip].val.b;
        }

        os.leftUp[k] = chowProcess(os.leftUp[k], cur_thresh, cur_ratio, cur_flip);
        os.rightUp[k] = chowProcess(os.rightUp[k], cur_thresh, cur_ratio, cur_flip);
    }

    os.downsample(dataL, dataR);
}

void CHOWEffect::suspend() { init(); }

void CHOWEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[chow_thresh].set_name("Threshold");
    fxdata->p[chow_thresh].set_type(ct_decibel_attenuation_large);
    fxdata->p[chow_thresh].posy_offset = 1;

    fxdata->p[chow_ratio].set_name("Ratio");
    fxdata->p[chow_ratio].set_type(ct_chow_ratio);
    fxdata->p[chow_ratio].posy_offset = 1;

    fxdata->p[chow_flip].set_name("Flip");
    fxdata->p[chow_flip].set_type(ct_bool);
    fxdata->p[chow_flip].posy_offset = 1;

    fxdata->p[chow_mix].set_name("Mix");
    fxdata->p[chow_mix].set_type(ct_percent);
    fxdata->p[chow_mix].posy_offset = 3;
    fxdata->p[chow_mix].val_default.f = 1.f;
}

void CHOWEffect::init_default_values()
{
    fxdata->p[chow_thresh].val.f = -24.f;
    fxdata->p[chow_ratio].val.f = 10.f;

    fxdata->p[chow_mix].val.f = 1.f;
}

const char *CHOWEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Truculate";
    case 1:
        return "Output";
    }

    return 0;
}

int CHOWEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 9;
    }
    return 0;
}

} // namespace chowdsp
