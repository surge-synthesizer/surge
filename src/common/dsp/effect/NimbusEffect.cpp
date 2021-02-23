/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "NimbusEffect.h"

#ifdef _MSC_VER
#define __attribute__(x)
#endif

#define TEST // remember this is how you tell the eurorack code to use dsp not hardware
#include "clouds/dsp/granular_processor.h"

NimbusEffect::NimbusEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    const int memLen = 118784;
    const int ccmLen = 65536 - 128;
    block_mem = new uint8_t[memLen]();
    block_ccm = new uint8_t[ccmLen]();
    processor = new clouds::GranularProcessor();
    memset(processor, 0, sizeof(*processor));

    processor->Init(block_mem, memLen, block_ccm, ccmLen);
    mix.set_blocksize(BLOCK_SIZE);
}

NimbusEffect::~NimbusEffect()
{
    delete[] block_mem;
    delete[] block_ccm;
    delete processor;
}

void NimbusEffect::init()
{
    mix.set_target(1.f);
    mix.instantize();
}

void NimbusEffect::setvars(bool init)
{
    if (*pdata_ival[nmb_mode] != old_nmb_mode)
    {
        switch (*pdata_ival[nmb_mode])
        {
        case 0:
            fxdata->p[nmb_density].set_name("Density");
            fxdata->p[nmb_density].set_type(ct_percent_bidirectional);

            fxdata->p[nmb_texture].set_name("Texture");
            fxdata->p[nmb_texture].set_type(ct_percent);

            break;
        case 1:
        case 2:
            fxdata->p[nmb_density].set_name("Diffusion");
            fxdata->p[nmb_density].set_type(ct_percent);

            fxdata->p[nmb_texture].set_name("Filter");
            fxdata->p[nmb_texture].set_type(ct_percent_bidirectional);
            break;
        case 3:
            fxdata->p[nmb_density].set_name("Smear");
            fxdata->p[nmb_density].set_type(ct_percent_bidirectional);

            fxdata->p[nmb_texture].set_name("Texture");
            fxdata->p[nmb_texture].set_type(ct_percent_bidirectional);
            break;
        }

        fxdata->p[nmb_size].set_name((*pdata_ival[nmb_mode] == 3) ? "Warp" : "Size");

        fxdata->p[nmb_density].posy_offset = 3;
        fxdata->p[nmb_texture].posy_offset = 3;

        old_nmb_mode = *pdata_ival[nmb_mode];
        hasInvalidated = true;
    }
}

void NimbusEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    clouds::ShortFrame input[BLOCK_SIZE];
    clouds::ShortFrame output[BLOCK_SIZE];

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        input[i].l = (short)(clamp1bp(dataL[i]) * 32767.0f);
        input[i].r = (short)(clamp1bp(dataR[i]) * 32767.0f);
    }

    processor->set_playback_mode(
        (clouds::PlaybackMode)((int)clouds::PLAYBACK_MODE_GRANULAR + *pdata_ival[nmb_mode]));
    processor->set_quality(*pdata_ival[nmb_quality]);

    auto parm = processor->mutable_parameters();
    float den_val, tex_val;

    den_val = (fxdata->p[nmb_density].ctrltype == ct_percent) ? *f[nmb_density]
                                                              : (*f[nmb_density] + 1.f) * 0.5;

    tex_val = (fxdata->p[nmb_texture].ctrltype == ct_percent) ? *f[nmb_texture]
                                                              : (*f[nmb_texture] + 1.f) * 0.5;

    // nmb_in_gain,
    parm->position = limit_range(*f[nmb_position], 0.f, 1.f);
    parm->size = limit_range(*f[nmb_size], 0.f, 1.f);
    parm->density = limit_range(den_val, 0.f, 1.f);
    parm->texture = limit_range(tex_val, 0.f, 1.f);
    parm->pitch = limit_range(*f[nmb_pitch], -48.f, 48.f);
    parm->stereo_spread = limit_range(*f[nmb_spread], 0.f, 1.f);
    parm->feedback = limit_range(*f[nmb_feedback], 0.f, 1.f);
    parm->freeze = *f[nmb_freeze] > 0.5;
    parm->reverb = limit_range(*f[nmb_reverb], 0.f, 1.f);
    parm->dry_wet = 1.f;

    parm->trigger = true;
    parm->gate = true;

    processor->Prepare();
    processor->Process(input, output, BLOCK_SIZE);

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        L[i] = output[i].l / 32767.0f;
        R[i] = output[i].r / 32767.0f;
    }

    mix.set_target_smoothed(limit_range(*f[nmb_mix], 0.f, 1.f));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void NimbusEffect::suspend() { init(); }

const char *NimbusEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Configuration";
    case 1:
        return "Grain";
    case 2:
        return "Playback";
    case 3:
        return "Output";
    }
    return 0;
}

int NimbusEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 21;
    case 3:
        return 27;
    }
    return 0;
}

void NimbusEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    int ypos = 1;
    fxdata->p[nmb_mode].set_name("Mode");
    fxdata->p[nmb_mode].set_type(ct_nimbusmode);
    fxdata->p[nmb_mode].posy_offset = ypos;

    fxdata->p[nmb_quality].set_name("Quality");
    fxdata->p[nmb_quality].set_type(ct_nimbusquality);
    fxdata->p[nmb_quality].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_position].set_name("Position");
    fxdata->p[nmb_position].set_type(ct_percent);
    fxdata->p[nmb_position].posy_offset = ypos;
    fxdata->p[nmb_size].set_name("Size");
    fxdata->p[nmb_size].set_type(ct_percent);
    fxdata->p[nmb_size].val_default.f = 0.5;
    fxdata->p[nmb_size].posy_offset = ypos;
    fxdata->p[nmb_pitch].set_name("Pitch");
    fxdata->p[nmb_pitch].set_type(ct_pitch4oct);
    fxdata->p[nmb_pitch].posy_offset = ypos;
    fxdata->p[nmb_density].set_name("Density");
    fxdata->p[nmb_density].set_type(ct_percent_bidirectional);
    fxdata->p[nmb_density].posy_offset = ypos;
    fxdata->p[nmb_texture].set_name("Texture");
    fxdata->p[nmb_texture].set_type(ct_percent);
    fxdata->p[nmb_texture].posy_offset = ypos;
    fxdata->p[nmb_spread].set_name("Spread");
    fxdata->p[nmb_spread].set_type(ct_percent);
    fxdata->p[nmb_spread].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_freeze].set_name("Freeze");
    fxdata->p[nmb_freeze].set_type(ct_float_toggle); // so it is modulatable. 50% above is frozen
    fxdata->p[nmb_freeze].posy_offset = ypos;
    fxdata->p[nmb_feedback].set_name("Feedback");
    fxdata->p[nmb_feedback].set_type(ct_percent);
    fxdata->p[nmb_feedback].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_reverb].set_name("Reverb");
    fxdata->p[nmb_reverb].set_type(ct_percent);
    fxdata->p[nmb_reverb].posy_offset = ypos;

    fxdata->p[nmb_mix].set_name("Mix");
    fxdata->p[nmb_mix].set_type(ct_percent);
    fxdata->p[nmb_mix].val_default.f = 0.5;
    fxdata->p[nmb_mix].posy_offset = ypos;
}

void NimbusEffect::init_default_values()
{
    fxdata->p[nmb_mode].val.i = 0;
    fxdata->p[nmb_quality].val.i = 0;
    fxdata->p[nmb_position].val.f = 0.f;
    fxdata->p[nmb_size].val.f = 0.5;
    fxdata->p[nmb_density].val.f = 0.f;
    fxdata->p[nmb_texture].val.f = 0.f;
    fxdata->p[nmb_pitch].val.f = 0.f;
    fxdata->p[nmb_freeze].val.f = 0.f;
    fxdata->p[nmb_feedback].val.f = 0.f;
    fxdata->p[nmb_reverb].val.f = 0.f;
    fxdata->p[nmb_spread].val.f = 0.f;
    fxdata->p[nmb_mix].val.f = 0.5;
}
