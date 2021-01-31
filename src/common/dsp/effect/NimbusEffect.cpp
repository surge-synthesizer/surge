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

#ifdef _MSC_VER
#define __attribute__(x)
#endif

#include "NimbusEffect.h"

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
}

NimbusEffect::~NimbusEffect()
{
    delete[] block_mem;
    delete[] block_ccm;
    delete processor;
}

void NimbusEffect::init() {}

void NimbusEffect::setvars(bool init) {}

void NimbusEffect::process(float *dataL, float *dataR)
{

    clouds::ShortFrame input[BLOCK_SIZE];
    clouds::ShortFrame output[BLOCK_SIZE];

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        input[i].l = (short)(limit_range(dataL[i], -1.f, 1.f) * 32767.0f);
        input[i].r = (short)(limit_range(dataR[i], -1.f, 1.f) * 32767.0f);
    }

    processor->set_playback_mode(
        (clouds::PlaybackMode)((int)clouds::PLAYBACK_MODE_GRANULAR + *pdata_ival[nmb_mode]));
    processor->set_quality(*pdata_ival[nmb_quality]);
    auto parm = processor->mutable_parameters();

    parm->dry_wet = *f[nmb_mix_aka_blend];
    parm->freeze = *f[nmb_freeze] > 0.5;

    parm->position = limit_range(*f[nmb_position], 0.f, 1.f);
    parm->size = limit_range(*f[nmb_size], 0.f, 1.f);
    parm->density = limit_range(*f[nmb_density], 0.f, 1.f);
    parm->pitch = limit_range(24 * *f[nmb_pitch], -48.f, 48.f); // FIXME obviously use a pitch
    parm->texture = limit_range(*f[nmb_texture], 0.f, 1.f);
    parm->stereo_spread = limit_range(*f[nmb_spread], 0.f, 1.f);
    parm->feedback = limit_range(*f[nmb_feedback], 0.f, 1.f);
    parm->reverb = limit_range(*f[nmb_reverb], 0.f, 1.f);

    // nmb_in_gain,
    //   nmb_mode,

    parm->trigger = true;
    parm->gate = true;

    processor->Prepare();
    processor->Process(input, output, BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        dataL[i] = output[i].l / 32767.0f;
        dataR[i] = output[i].r / 32767.0f;
    }
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
        return 19;
    case 3:
        return 25;
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
    fxdata->p[nmb_position].set_name("Grain Position");
    fxdata->p[nmb_position].set_type(ct_percent);
    fxdata->p[nmb_position].posy_offset = ypos;
    fxdata->p[nmb_size].set_name("Grain Size");
    fxdata->p[nmb_size].set_type(ct_percent);
    fxdata->p[nmb_size].posy_offset = ypos;
    fxdata->p[nmb_density].set_name("Grain Density");
    fxdata->p[nmb_density].set_type(ct_percent);
    fxdata->p[nmb_density].posy_offset = ypos;
    fxdata->p[nmb_texture].set_name("Grain Texture");
    fxdata->p[nmb_texture].set_type(ct_percent);
    fxdata->p[nmb_texture].posy_offset = ypos;

    fxdata->p[nmb_pitch].set_name("Grain Pitch");
    fxdata->p[nmb_pitch].set_type(ct_percent_bidirectional);
    fxdata->p[nmb_pitch].val_max.f = 2.0;
    fxdata->p[nmb_pitch].val_min.f = -2.0;
    fxdata->p[nmb_pitch].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_freeze].set_name("Freeze");
    fxdata->p[nmb_freeze].set_type(ct_percent); // so it is modulatable. 50% above is frozed
    fxdata->p[nmb_freeze].posy_offset = ypos;
    fxdata->p[nmb_feedback].set_name("Feedback");
    fxdata->p[nmb_feedback].set_type(ct_percent);
    fxdata->p[nmb_feedback].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_reverb].set_name("Reverb");
    fxdata->p[nmb_reverb].set_type(ct_percent);
    fxdata->p[nmb_reverb].posy_offset = ypos;

    fxdata->p[nmb_spread].set_name("Spread");
    fxdata->p[nmb_spread].set_type(ct_percent);
    fxdata->p[nmb_spread].posy_offset = ypos;

    fxdata->p[nmb_mix_aka_blend].set_name("Blend / Mix");
    fxdata->p[nmb_mix_aka_blend].set_type(ct_percent);
    fxdata->p[nmb_mix_aka_blend].posy_offset = ypos;
}

void NimbusEffect::init_default_values()
{
    fxdata->p[nmb_mode].val.i = 0;
    fxdata->p[nmb_quality].val.i = 0;
    fxdata->p[nmb_position].val.f = 0.5;
    fxdata->p[nmb_size].val.f = 0.5;
    fxdata->p[nmb_density].val.f = 0.5;
    fxdata->p[nmb_texture].val.f = 0.5;
    fxdata->p[nmb_pitch].val.f = 0;
    fxdata->p[nmb_freeze].val.f = 0;
    fxdata->p[nmb_feedback].val.f = 0;
    fxdata->p[nmb_reverb].val.f = 0;
    fxdata->p[nmb_spread].val.f = 0;
    fxdata->p[nmb_mix_aka_blend].val.f = 0.5;
}