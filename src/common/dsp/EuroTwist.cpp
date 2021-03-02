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

#include "EuroTwist.h"

#define TEST
#ifndef _MSC_VER
#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#endif
#include "plaits/dsp/voice.h"
#include "samplerate.h"

std::string eurotwist_engine_name(int i)
{
    switch (i)
    {
    case 0:
        return "Waveforms";
    case 1:
        return "Waveshaper";
    case 2:
        return "2-OP FM";
    case 3:
        return "Formant/PD";
    case 4:
        return "Harmonic";
    case 5:
        return "Wavetable";
    case 6:
        return "Chords";
    case 7:
        return "Spech";
    case 8:
        return "Granular Cloud";
    case 9:
        return "Filtered Noise";
    case 10:
        return "Particle Noise";
    case 11:
        return "Inharmonic String";
    case 12:
        return "Modal Resonator";
    case 13:
        return "Analog Kick";
    case 14:
        return "Analog Snare";
    case 15:
        return "Analog Hi-Hat";
    }
    return "Error " + std::to_string(i);
}

EuroTwist::EuroTwist(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy)
{
    voice = std::make_unique<plaits::Voice>();
    alloc = std::make_unique<stmlib::BufferAllocator>(shared_buffer, sizeof(shared_buffer));
    voice->Init(alloc.get());
    patch = std::make_unique<plaits::Patch>();
    int error;
    srcstate = src_new(SRC_SINC_MEDIUM_QUALITY, 2, &error);
    if (error != 0)
    {
        srcstate = nullptr;
    }
}

void EuroTwist::init(float pitch, bool is_display, bool nonzero_drift)
{
    memset((void *)patch.get(), 0, sizeof(plaits::Patch));
}
EuroTwist::~EuroTwist()
{
    if (srcstate)
        srcstate = src_delete(srcstate);
}
void EuroTwist::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
    if (!srcstate)
        return;

    patch->note = pitch; // fixme - alternate tuning goes here
    patch->engine = oscdata->p[et_engine].val.i;

    harm.newValue(fvbp(et_harmonics));
    timb.newValue(fvbp(et_timbre));
    morph.newValue(fvbp(et_morph));

    plaits::Modulations mod = {};
    // for now
    memset((void *)&mod, 0, sizeof(plaits::Modulations));

    constexpr int subblock = 4;
    float src_in[subblock][2];
    float src_out[BLOCK_SIZE_OS][2];

    SRC_DATA sdata;
    sdata.end_of_input = 0;
    sdata.src_ratio = dsamplerate_os / 48000.0;

    for (int i = 0; i < carrover_size; ++i)
    {
        output[i] = carryover[i][0];
        outputR[i] = carryover[i][0];
    }

    int total_generated = carrover_size;
    carrover_size = 0;
    while (total_generated < BLOCK_SIZE_OS)
    {
        plaits::Voice::Frame poutput[subblock];
        patch->harmonics = harm.v;
        patch->timbre = timb.v;
        patch->morph = morph.v;

        harm.process();
        timb.process();
        morph.process();

        voice->Render(*patch, mod, poutput, subblock);
        for (int i = 0; i < subblock; ++i)
        {
            src_in[i][0] = poutput[i].out / 32768.f;
            src_in[i][1] = poutput[i].aux / 32768.f;
        }
        sdata.data_in = &(src_in[0][0]);
        sdata.data_out = &(src_out[0][0]);
        sdata.input_frames = subblock;
        sdata.output_frames = BLOCK_SIZE_OS;
        auto res = src_process(srcstate, &sdata);
        for (int i = 0; i < sdata.output_frames_gen; ++i)
        {
            if (i + total_generated >= BLOCK_SIZE_OS)
            {
                carryover[carrover_size][0] = src_out[i][0];
                carryover[carrover_size][1] = src_out[i][1];
                carrover_size++;
            }
            else
            {
                output[total_generated + i] = src_out[i][0];
                outputR[total_generated + i] = src_out[i][0];
            }
        }
        total_generated += sdata.output_frames_gen;
        if (sdata.input_frames_used != subblock)
        {
            // FIXME
            // std::cout << "DEAL " << std::endl;
        }
    }
}

void EuroTwist::init_ctrltypes()
{
    oscdata->p[et_engine].set_name("Engine");
    oscdata->p[et_engine].set_type(ct_eurotwist_engine);

    oscdata->p[et_harmonics].set_name("Harmonics");
    oscdata->p[et_harmonics].set_type(ct_percent_bipolar);

    oscdata->p[et_timbre].set_name("Timbre");
    oscdata->p[et_timbre].set_type(ct_percent_bipolar);

    oscdata->p[et_morph].set_name("Morph");
    oscdata->p[et_morph].set_type(ct_percent_bipolar);

    oscdata->p[et_aux_mix].set_name("Aux Mix");
    oscdata->p[et_aux_mix].set_type(ct_percent);

    oscdata->p[et_lpg_response].set_name("LPG Response");
    oscdata->p[et_lpg_response].set_type(ct_percent);

    oscdata->p[et_lpg_decay].set_name("LPG Decay");
    oscdata->p[et_lpg_decay].set_type(ct_percent); // FIXME ct_percent_deactivatable
}

void EuroTwist::init_default_values()
{
    oscdata->p[et_engine].val.i = 0;
    oscdata->p[et_harmonics].val.f = 0;
    oscdata->p[et_timbre].val.f = 0;
    oscdata->p[et_morph].val.f = 0;
    oscdata->p[et_aux_mix].val.f = 0;
    oscdata->p[et_lpg_response].val.f = 0;
    oscdata->p[et_lpg_decay].val.f = 0;
}
