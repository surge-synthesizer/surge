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
#include "samplerate.h"
#include "DebugHelpers.h"

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

    int error;
    surgeSR_to_euroSR = src_new(SRC_SINC_FASTEST, 2, &error);
    if (error != 0)
    {
        surgeSR_to_euroSR = nullptr;
    }

    euroSR_to_surgeSR = src_new(SRC_SINC_FASTEST, 2, &error);
    if (error != 0)
    {
        euroSR_to_surgeSR = nullptr;
    }
}

NimbusEffect::~NimbusEffect()
{
    delete[] block_mem;
    delete[] block_ccm;
    delete processor;

    if (surgeSR_to_euroSR)
        surgeSR_to_euroSR = src_delete(surgeSR_to_euroSR);
    if (euroSR_to_surgeSR)
        euroSR_to_surgeSR = src_delete(euroSR_to_surgeSR);
}

void NimbusEffect::init()
{
    mix.set_target(1.f);
    mix.instantize();

    memset(resampled_output, 0, raw_out_sz * 2 * sizeof(float));

    consumed = 0;
    created = 0;
    builtBuffer = false;
    resampReadPtr = 0;
    resampWritePtr = 1; // why 1? well while we are stalling we want to output 0 so write 1 ahead
}

void NimbusEffect::setvars(bool init) {}

void NimbusEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    if (!surgeSR_to_euroSR || !euroSR_to_surgeSR)
        return;

    /* Resample Temp Buffers */
    float resample_this[BLOCK_SIZE << 3][2];
    float resample_into[BLOCK_SIZE << 3][2];

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        resample_this[i][0] = dataL[i];
        resample_this[i][1] = dataR[i];
    }

    SRC_DATA sdata;
    sdata.end_of_input = 0;
    sdata.src_ratio = processor_sr * storage->samplerate_inv;
    sdata.data_in = &(resample_this[0][0]);
    sdata.data_out = &(resample_into[0][0]);
    sdata.input_frames = BLOCK_SIZE;
    sdata.output_frames = (BLOCK_SIZE << 3);
    auto res = src_process(surgeSR_to_euroSR, &sdata);
    consumed += sdata.input_frames_used;

    if (sdata.output_frames_gen)
    {
        clouds::ShortFrame input[BLOCK_SIZE << 3];
        clouds::ShortFrame output[BLOCK_SIZE << 3];

        int frames_to_go = sdata.output_frames_gen;
        int outpos = 0;

        processor->set_playback_mode(
            (clouds::PlaybackMode)((int)clouds::PLAYBACK_MODE_GRANULAR + *pdata_ival[nmb_mode]));
        processor->set_quality(*pdata_ival[nmb_quality]);

        int consume_ptr = 0;
        while (frames_to_go >= nimbusprocess_blocksize)
        {
            int sp = 0;
            while (numStubs > 0)
            {
                input[sp].l = (short)(clamp1bp(stub_input[0][sp]) * 32767.0f);
                input[sp].r = (short)(clamp1bp(stub_input[1][sp]) * 32767.0f);
                sp++;
                numStubs--;
            }

            for (int i = sp; i < nimbusprocess_blocksize; ++i)
            {
                input[i].l = (short)(clamp1bp(resample_into[consume_ptr][0]) * 32767.0f);
                input[i].r = (short)(clamp1bp(resample_into[consume_ptr][1]) * 32767.0f);
                consume_ptr++;
            }

            int inputSz = nimbusprocess_blocksize; // sdata.output_frames_gen + sp;

            auto parm = processor->mutable_parameters();

            float den_val, tex_val;

            den_val = (*f[nmb_density] + 1.f) * 0.5;
            tex_val = (*f[nmb_texture] + 1.f) * 0.5;

            parm->position = clamp01(*f[nmb_position]);
            parm->size = clamp01(*f[nmb_size]);
            parm->density = clamp01(den_val);
            parm->texture = clamp01(tex_val);
            parm->pitch = limit_range(*f[nmb_pitch], -48.f, 48.f);
            parm->stereo_spread = clamp01(*f[nmb_spread]);
            parm->feedback = clamp01(*f[nmb_feedback]);
            parm->freeze = *f[nmb_freeze] > 0.5;
            parm->reverb = clamp01(*f[nmb_reverb]);
            parm->dry_wet = 1.f;

            parm->trigger = false;     // this is an external granulating source. Skip it
            parm->gate = parm->freeze; // This is the CV for the freeze button

            processor->Prepare();
            processor->Process(input, output, inputSz);

            for (int i = 0; i < inputSz; ++i)
            {
                resample_this[outpos + i][0] = output[i].l / 32767.0f;
                resample_this[outpos + i][1] = output[i].r / 32767.0f;
            }
            outpos += inputSz;
            frames_to_go -= (nimbusprocess_blocksize - sp);
        }

        if (frames_to_go > 0)
        {
            numStubs = frames_to_go;
            for (int i = 0; i < numStubs; ++i)
            {
                stub_input[0][i] = resample_into[consume_ptr][0];
                stub_input[1][i] = resample_into[consume_ptr][1];
                consume_ptr++;
            }
        }

        SRC_DATA odata;
        odata.end_of_input = 0;
        odata.src_ratio = processor_sr_inv * storage->samplerate;
        odata.data_in = &(resample_this[0][0]);
        odata.data_out = &(resample_into[0][0]);
        odata.input_frames = outpos;
        odata.output_frames = BLOCK_SIZE << 3;
        auto reso = src_process(euroSR_to_surgeSR, &odata);
        if (!builtBuffer)
            created += odata.output_frames_gen;

        size_t w = resampWritePtr;
        for (int i = 0; i < odata.output_frames_gen; ++i)
        {
            resampled_output[w][0] = resample_into[i][0];
            resampled_output[w][1] = resample_into[i][1];

            w = (w + 1U) & (raw_out_sz - 1U);
        }
        resampWritePtr = w;
    }

    bool rpi = (created) > (BLOCK_SIZE + 8); // leave some buffer
    if (rpi)
        builtBuffer = true;

    size_t rp = resampReadPtr;
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        L[i] = resampled_output[rp][0];
        R[i] = resampled_output[rp][1];
        rp = (rp + rpi) & (raw_out_sz - 1);
    }
    resampReadPtr = rp;

    mix.set_target_smoothed(clamp01(*f[nmb_mix]));
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

    // Dynamic names and bipolarity support
    static struct DynTexDynamicNameBip : public ParameterDynamicNameFunction,
                                         ParameterDynamicBoolFunction
    {
        const char *getName(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            static char res[TXT_SIZE];
            res[0] = 0;
            auto mode = fx->p[nmb_mode].val.i;
            switch (mode)
            {
            case 0:
                if (idx == nmb_density)
                    snprintf(res, TXT_SIZE, "%s", "Density");
                if (idx == nmb_texture)
                    snprintf(res, TXT_SIZE, "%s", "Texture");
                if (idx == nmb_size)
                    snprintf(res, TXT_SIZE, "%s", "Size");
                break;
            case 1:
            case 2:
                if (idx == nmb_density)
                    snprintf(res, TXT_SIZE, "%s", "Diffusion");
                if (idx == nmb_texture)
                    snprintf(res, TXT_SIZE, "%s", "Filter");
                if (idx == nmb_size)
                    snprintf(res, TXT_SIZE, "%s", "Size");
                break;
            case 3:
                if (idx == nmb_density)
                    snprintf(res, TXT_SIZE, "%s", "Smear");
                if (idx == nmb_texture)
                    snprintf(res, TXT_SIZE, "%s", "Texture");
                if (idx == nmb_size)
                    snprintf(res, TXT_SIZE, "%s", "Warp");
                break;
            }

            return res;
        }

        const bool getValue(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            auto mode = fx->p[nmb_mode].val.i;

            auto isBipolar = false;
            switch (mode)
            {
            case 0:
                if (idx == nmb_density)
                    isBipolar = true;
                break;
            case 1:
            case 2:
                if (idx == nmb_texture)
                    isBipolar = true;
                break;
            case 3:
                if (idx != nmb_size)
                    isBipolar = true;
                break;
            }

            return isBipolar;
        }
    } dynTexDynamicNameBip;

    static struct SpreadDeactivator : public ParameterDynamicDeactivationFunction
    {
        const bool getValue(const Parameter *p) const
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto mode = fx->p[nmb_mode].val.i;

            return mode != 0;
        }
    } spreadDeact;

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
    fxdata->p[nmb_size].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    fxdata->p[nmb_size].dynamicName = &dynTexDynamicNameBip;
    fxdata->p[nmb_size].dynamicBipolar = &dynTexDynamicNameBip;

    fxdata->p[nmb_size].val_default.f = 0.5;
    fxdata->p[nmb_size].posy_offset = ypos;
    fxdata->p[nmb_pitch].set_name("Pitch");
    fxdata->p[nmb_pitch].set_type(ct_pitch4oct);
    fxdata->p[nmb_pitch].posy_offset = ypos;

    fxdata->p[nmb_density].set_name("Density");
    fxdata->p[nmb_density].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    fxdata->p[nmb_density].posy_offset = ypos;
    fxdata->p[nmb_density].dynamicName = &dynTexDynamicNameBip;
    fxdata->p[nmb_density].dynamicBipolar = &dynTexDynamicNameBip;

    fxdata->p[nmb_texture].set_name("Texture");
    fxdata->p[nmb_texture].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    fxdata->p[nmb_texture].posy_offset = ypos;
    fxdata->p[nmb_texture].dynamicName = &dynTexDynamicNameBip;
    fxdata->p[nmb_texture].dynamicBipolar = &dynTexDynamicNameBip;

    fxdata->p[nmb_spread].set_name("Spread");
    fxdata->p[nmb_spread].set_type(ct_percent);
    fxdata->p[nmb_spread].dynamicDeactivation = &spreadDeact;
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
