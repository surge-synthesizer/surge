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

#include "ExciterEffect.h"

namespace
{
constexpr double low_freq = 500.0;
constexpr double high_freq = 10000.0;
constexpr double q_val = 0.7071;
} // namespace

namespace chowdsp
{

ExciterEffect::ExciterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), toneFilter(storage)
{
    wet_gain.set_blocksize(BLOCK_SIZE);
    drive_gain.set_blocksize(BLOCK_SIZE);
}

ExciterEffect::~ExciterEffect() {}

void ExciterEffect::init()
{
    toneFilter.suspend();
    toneFilter.coeff_HP(M_PI, q_val);
    toneFilter.coeff_instantize();

    levelDetector.reset(storage->samplerate);

    drive_gain.set_target(1.0f);
    wet_gain.set_target(0.0f);
}

void ExciterEffect::process(float *dataL, float *dataR)
{
    set_params();

    // copy dry signal
    copy_block(dataL, dryL, BLOCK_SIZE_QUAD);
    copy_block(dataR, dryR, BLOCK_SIZE_QUAD);

    drive_gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
    os.upsample(dataL, dataR);

    for (int k = 0; k < os.getUpBlockSize(); k++)
        process_sample(os.leftUp[k], os.rightUp[k]);

    os.downsample(dataL, dataR);

    // dry/wet process
    wet_gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
    add_block(dataL, dryL, dataL, BLOCK_SIZE_QUAD);
    add_block(dataR, dryR, dataR, BLOCK_SIZE_QUAD);
}

void ExciterEffect::set_params()
{
    // "Tone" param
    auto cutoff = low_freq * std::pow(high_freq / low_freq, clamp01(*f[exciter_tone]));
    cutoff = limit_range(cutoff, 10.0, storage->samplerate * 0.48);
    auto omega_factor = storage->samplerate_inv * 2.0 * M_PI / (double)os.getOSRatio();
    toneFilter.coeff_HP(cutoff * omega_factor, q_val);

    // "Drive" param
    auto drive_makeup = std::pow(0.2f, 1.f - clamp01(*f[exciter_tone]));
    auto drive = 8.f * std::pow(clamp01(*f[exciter_drive]), 1.5f) * drive_makeup;
    drive_gain.set_target_smoothed(drive);

    // attack/release params
    auto attack_ms = std::pow(2.0f, fxdata->p[exciter_att].displayInfo.b * *f[exciter_att]);
    auto release_ms =
        10.0f * std::pow(2.0f, fxdata->p[exciter_rel].displayInfo.b * *f[exciter_rel]);

    attack_ms = limit_range(attack_ms, 2.5f, 40.0f);
    release_ms = limit_range(release_ms, 25.0f, 400.0f);

    levelDetector.set_attack_time(attack_ms);
    levelDetector.set_release_time(release_ms);

    // "Mix" param
    wet_gain.set_target_smoothed(clamp01(*f[exciter_mix]));
}

void ExciterEffect::suspend() { init(); }

void ExciterEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[exciter_drive].set_name("Drive");
    fxdata->p[exciter_drive].set_type(ct_percent);
    fxdata->p[exciter_drive].val_default.f = 0.5f;
    fxdata->p[exciter_drive].posy_offset = 1;

    fxdata->p[exciter_tone].set_name("Tone");
    fxdata->p[exciter_tone].set_type(ct_percent);
    fxdata->p[exciter_tone].val_default.f = 0.5f;
    fxdata->p[exciter_tone].posy_offset = 1;

    fxdata->p[exciter_tone].set_name("Tone");
    fxdata->p[exciter_tone].set_type(ct_percent);
    fxdata->p[exciter_tone].val_default.f = 0.5f;
    fxdata->p[exciter_tone].posy_offset = 1;

    fxdata->p[exciter_att].set_name("Attack");
    fxdata->p[exciter_att].set_type(ct_comp_attack_ms);
    fxdata->p[exciter_att].val_max.f = std::log2(20.0f) / fxdata->p[exciter_att].displayInfo.b;
    fxdata->p[exciter_att].val_min.f = std::log2(5.f) / fxdata->p[exciter_att].displayInfo.b;
    fxdata->p[exciter_att].val_default.f = 0.5f;
    fxdata->p[exciter_att].posy_offset = 3;

    fxdata->p[exciter_rel].set_name("Release");
    fxdata->p[exciter_rel].set_type(ct_comp_release_ms);
    fxdata->p[exciter_rel].val_max.f = std::log2(20.f) / fxdata->p[exciter_rel].displayInfo.b;
    fxdata->p[exciter_rel].val_min.f = std::log2(5.f) / fxdata->p[exciter_rel].displayInfo.b;
    fxdata->p[exciter_rel].val_default.f = 0.5f;
    fxdata->p[exciter_rel].posy_offset = 3;

    fxdata->p[exciter_mix].set_name("Mix");
    fxdata->p[exciter_mix].set_type(ct_percent);
    fxdata->p[exciter_mix].val_default.f = 0.5f;
    fxdata->p[exciter_mix].posy_offset = 5;
}

void ExciterEffect::init_default_values()
{
    fxdata->p[exciter_drive].val.f = 0.5f;
    fxdata->p[exciter_tone].val.f = 0.5f;
    fxdata->p[exciter_att].val.f = 0.5f;
    fxdata->p[exciter_rel].val.f = 0.5f;
    fxdata->p[exciter_mix].val.f = 0.5f;
}

const char *ExciterEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Exciter";
    case 1:
        return "Shape";
    case 2:
        return "Output";
    }

    return 0;
}

int ExciterEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 13;
    }

    return 0;
}

} // namespace chowdsp
