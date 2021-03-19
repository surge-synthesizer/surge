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
#include "DebugHelpers.h"

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
        return "2-Operator FM";
    case 3:
        return "Formant/PD";
    case 4:
        return "Harmonic";
    case 5:
        return "Wavetable";
    case 6:
        return "Chords";
    case 7:
        return "Vowels/Speech";
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

static struct EngineDynamicName : public ParameterDynamicNameFunction
{
    std::vector<std::vector<std::string>> engineLabels;
    std::vector<std::string> defaultLabels = {"Harmonics", "Timbre", "Morph", "Aux Mix"};

    EngineDynamicName() noexcept
    {
        // Waveforms
        engineLabels.push_back({"Detune", "Square Shape", "Saw Shape", "Sync Mix"});
        // Waveshaper
        engineLabels.push_back({"Waveshaper", "Fold", "Asymmetry", "Variation Mix"});
        // 2-Operator FM
        engineLabels.push_back({"Ratio", "Amount", "Feedback", "Sub Mix"});
        // Formant/PD
        engineLabels.push_back({"Ratio/Type", "Formant/Cutoff", "Shape", "PD Mix"});
        // Harmonic
        engineLabels.push_back({"Bump", "Peak", "Shape", "Organ Mix"});
        // Wavetable
        engineLabels.push_back({"Bank", "Morph Row", "Morph Column", "Lo-Fi Mix"});
        // Chords
        engineLabels.push_back({"Type", "Inversion", "Shape", "Root Mix"});
        // Vowels/Speech
        engineLabels.push_back({"Speak", "Species", "Segment", "Raw Mix"});
        // Granular Cloud
        engineLabels.push_back({"Pitch Random", "Grain Density", "Grain Duration", "Sine Mix"});
        // Filtered Noise
        engineLabels.push_back({"Type", "Clock Frequency", "Resonance", "Dual Peak Mix"});
        // Particle Noise
        engineLabels.push_back({"Freq Random", "Density", "Filter Type", "Raw Mix"});
        // Inharmonic String
        engineLabels.push_back({"Inharmonicity", "Brightness", "Decay Time", "Exciter Mix"});
        // Modal Resonator
        engineLabels.push_back({"Material", "Brightness", "Decay Time", "Exciter Mix"});
        // Analog Kick
        engineLabels.push_back({"Sharpness", "Brightness", "Decay Time", "Variation Mix"});
        // Analog Snare
        engineLabels.push_back({"Tone<>Noise", "Model", "Decay Time", "Variation Mix"});
        // Analog Hi-Hat
        engineLabels.push_back({"Tone<>Noise", "Low Cut", "Decay Time", "Variation Mix"});
    }

    const char *getName(Parameter *p) override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);

        if (oscs->type.val.i != ot_eurotwist)
        {
            return "ERROR";
        }

        auto engp = &(oscs->p[EuroTwist::et_engine]);
        auto eng = engp->val.i;
        auto idx = (p - engp);

        auto lab = engineLabels[eng][idx - 1];

        static char result[TXT_SIZE];
        snprintf(result, TXT_SIZE, "%s", lab.c_str());
        return result;
    }
} etDynamicName;

static struct EngineDynamicBipolar : public ParameterDynamicBoolFunction
{
    std::vector<std::vector<bool>> engineBipolars;

    EngineDynamicBipolar() noexcept
    {
        engineBipolars.push_back({true, true, true, true});    // Waveforms
        engineBipolars.push_back({true, false, false, true});  // Waveshaper
        engineBipolars.push_back({true, false, true, true});   // 2-Operator FM
        engineBipolars.push_back({false, false, false, true}); // Formant/PD
        engineBipolars.push_back({false, false, false, true}); // Harmonic
        engineBipolars.push_back({true, true, true, true});    // Wavetable
        engineBipolars.push_back({false, true, true, true});   // Chords
        engineBipolars.push_back({false, true, false, true});  // Vowels/Speech
        engineBipolars.push_back({false, false, false, true}); // Granular Cloud
        engineBipolars.push_back({false, false, false, true}); // Filtered Noise
        engineBipolars.push_back({false, false, true, true});  // Particle Noise
        engineBipolars.push_back({false, false, false, true}); // Inharmonic String
        engineBipolars.push_back({false, false, false, true}); // Modal Resonator
        engineBipolars.push_back({false, false, false, true}); // Analog Kick
        engineBipolars.push_back({true, false, false, true});  // Analog Snare
        engineBipolars.push_back({true, false, false, true});  // Analog Hi-Hat
    }

    const bool getValue(Parameter *p) override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);

        if (oscs->type.val.i != ot_eurotwist)
        {
            return false;
        }

        auto engp = &(oscs->p[EuroTwist::et_engine]);
        auto eng = engp->val.i;
        auto idx = (p - engp);

        auto res = engineBipolars[eng][idx - 1];

        return res;
    }
} etDynamicBipolar;

/*
 * The only place we use dynamic deactivation is on the LPG sliders where
 * we bind this object to the decay and make it follow the response, which
 * is why we don't have crafty ifs here. If we do more deactivation obviously
 * we will want crafty ifs.
 */
static struct EngineDynamicDeact : public ParameterDynamicDeactivationFunction
{
    EngineDynamicDeact() noexcept {}

    const bool getValue(Parameter *p) override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);
        return oscs->p[EuroTwist::et_lpg_response].deactivated;
    }

    Parameter *getPrimaryDeactivationDriver(Parameter *p) override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);
        return &(oscs->p[EuroTwist::et_lpg_response]);
    }
} etDynamicDeact;

EuroTwist::EuroTwist(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy)
{
    voice = std::make_unique<plaits::Voice>();
    alloc = std::make_unique<stmlib::BufferAllocator>(shared_buffer, sizeof(shared_buffer));
    voice->Init(alloc.get());
    patch = std::make_unique<plaits::Patch>();
    int error;
    srcstate = src_new(SRC_SINC_FASTEST, 2, &error);

    if (error != 0)
    {
        srcstate = nullptr;
    }

    // FM downsampling with a linear interpolator is absolutely fine
    fmdownsamplestate = src_new(SRC_LINEAR, 1, &error);
    if (error != 0)
    {
        fmdownsamplestate = nullptr;
    }
}

float EuroTwist::tuningAwarePitch(float pitch)
{
    if (storage->tuningApplicationMode == SurgeStorage::RETUNE_ALL &&
        !(storage->oddsound_mts_client && storage->oddsound_mts_active) &&
        !(storage->isStandardTuning))
    {
        auto idx = (int)floor(pitch);
        float frac = pitch - idx; // frac is 0 means use idx; frac is 1 means use idx+1
        float b0 = storage->currentTuning.logScaledFrequencyForMidiNote(idx) * 12;
        float b1 = storage->currentTuning.logScaledFrequencyForMidiNote(idx + 1) * 12;
        return (1.f - frac) * b0 + frac * b1;
    }
    return pitch;
}

void EuroTwist::init(float pitch, bool is_display, bool nonzero_drift)
{
    charFilt.init(storage->getPatch().character.val.i);

    float tpitch = tuningAwarePitch(pitch);
    memset((void *)patch.get(), 0, sizeof(plaits::Patch));
    driftLFO.init(nonzero_drift);

    // Lets run forward a cycle
    int throwaway = 0;
    double cycleInSamples = std::max(1.0, 1.0 / pitch_to_dphase(tpitch));

    while (cycleInSamples < 10)
        cycleInSamples *= 2;

    if (!(oscdata->retrigger.val.b || is_display))
    {
        cycleInSamples *= (1.0 + (float)rand() / (float)RAND_MAX);
    }

    memset(fmlagbuffer, 0, (BLOCK_SIZE_OS << 1) * sizeof(float));
    fmrp = 0;
    fmwp = (int)(BLOCK_SIZE_OS * 48000 * dsamplerate_os_inv);

    process_block_internal<false, true>(pitch, 0, false, 0, std::ceil(cycleInSamples));
}
EuroTwist::~EuroTwist()
{
    if (srcstate)
        srcstate = src_delete(srcstate);

    if (fmdownsamplestate)
        fmdownsamplestate = src_delete(fmdownsamplestate);
}

template <bool FM> inline constexpr int getBlockSize() { return 4; }

template <> inline constexpr int getBlockSize<true>() { return 1; }

template <bool FM, bool throwaway>
void EuroTwist::process_block_internal(float pitch, float drift, bool stereo, float FMdepth,
                                       int throwawayBlocks)
{
    if (!srcstate)
        return;

    if (FM && !fmdownsamplestate)
        return;

    pitch = tuningAwarePitch(pitch);

    auto driftv = driftLFO.next();
    patch->note = pitch + drift * driftv;
    patch->engine = oscdata->p[et_engine].val.i;

    harm.newValue(fvbp(et_harmonics));
    timb.newValue(fvbp(et_timbre));
    morph.newValue(fvbp(et_morph));
    lpgcol.newValue(fv(et_lpg_response));
    lpgdec.newValue(fv(et_lpg_decay));
    auxmix.newValue(limit_range(fv(et_aux_mix), 0.f, 1.f));

    plaits::Modulations mod = {};
    // for now
    memset((void *)&mod, 0, sizeof(plaits::Modulations));

    constexpr int subblock = getBlockSize<FM>();
    float src_in[subblock][2];
    float src_out[BLOCK_SIZE_OS][2];

    SRC_DATA sdata;
    sdata.end_of_input = 0;
    sdata.src_ratio = dsamplerate_os / 48000.0;

    float normFMdepth = 0;

    if (FM)
    {
        float dsmaster[BLOCK_SIZE_OS << 2];
        SRC_DATA fmdata;
        fmdata.end_of_input = 0;
        fmdata.src_ratio = 48000.0 / dsamplerate_os; // going INTO the plaits rate
        fmdata.data_in = master_osc;
        fmdata.data_out = &(dsmaster[0]);
        fmdata.input_frames = BLOCK_SIZE_OS;
        fmdata.output_frames = BLOCK_SIZE_OS << 2;
        src_process(fmdownsamplestate, &fmdata);

        const float bl = -143.5, bhi = 71.7, oos = 1.0 / (bhi - bl);
        float adb = limit_range(amp_to_db(FMdepth), bl, bhi);
        float nfm = (adb - bl) * oos;

        normFMdepth = limit_range(nfm, 0.f, 1.f);

        for (int i = 0; i < fmdata.output_frames_gen; ++i)
        {
            fmlagbuffer[fmwp] = dsmaster[i];
            fmwp = (fmwp + 1) & ((BLOCK_SIZE_OS << 1) - 1);
        }
    }

    for (int i = 0; i < carrover_size; ++i)
    {
        output[i] = auxmix.v * carryover[i][1] + (1.0 - auxmix.v) * carryover[i][0];
        outputR[i] = output[i];
    }

    int total_generated = carrover_size;
    carrover_size = 0;
    int required_blocks = throwaway ? throwawayBlocks : BLOCK_SIZE_OS;

    bool lpgIsOn = !oscdata->p[et_lpg_response].deactivated;

    while (total_generated < required_blocks)
    {
        plaits::Voice::Frame poutput[subblock];
        patch->harmonics = harm.v;
        patch->timbre = timb.v;
        patch->morph = morph.v;
        patch->decay = lpgdec.v;
        patch->lpg_colour = lpgcol.v;

        harm.process();
        timb.process();
        morph.process();
        lpgdec.process();
        lpgcol.process();
        auxmix.process();
        if (lpgIsOn)
        {
            mod.trigger = gate ? 1.0 : 0.0;
            mod.trigger_patched = true;
        }

        if (FM)
        {
            mod.frequency_patched = true;
            mod.frequency = 137 * fmlagbuffer[fmrp]; // this is in 'notes'
            fmrp = (fmrp + 1) & ((BLOCK_SIZE_OS << 1) - 1);
            patch->frequency_modulation_amount = normFMdepth;
        }
        else
        {
            patch->frequency_modulation_amount = 0;
        }

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
        // FIXME - check res

        for (int i = 0; i < sdata.output_frames_gen; ++i)
        {
            if (i + total_generated >= required_blocks)
            {
                carryover[carrover_size][0] = src_out[i][0];
                carryover[carrover_size][1] = src_out[i][1];
                carrover_size++;
            }
            else if (!throwaway)
            {
                output[total_generated + i] =
                    auxmix.v * src_out[i][1] + (1 - auxmix.v) * src_out[i][0];
                outputR[total_generated + i] = output[total_generated + i];
            }
        }
        total_generated += sdata.output_frames_gen;
        if (sdata.input_frames_used != subblock)
        {
            // FIXME
            std::cout << "DEAL " << std::endl;
        }
    }

    if (!throwaway && charFilt.doFilter)
    {
        if (charFilt.doFilter)
        {
            if (stereo)
            {
                charFilt.process_block_stereo(output, outputR, BLOCK_SIZE_OS);
            }
            else
            {
                charFilt.process_block(output, BLOCK_SIZE_OS);
            }
        }
    }
}

void EuroTwist::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
    if (FM)
        EuroTwist::process_block_internal<true>(pitch, drift, stereo, FMdepth);
    else
        EuroTwist::process_block_internal<false>(pitch, drift, stereo, FMdepth);
}

void EuroTwist::init_ctrltypes()
{
    oscdata->p[et_engine].set_name("Engine");
    oscdata->p[et_engine].set_type(ct_eurotwist_engine);

    oscdata->p[et_harmonics].set_name("Harmonics");
    oscdata->p[et_harmonics].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    oscdata->p[et_harmonics].dynamicName = &etDynamicName;
    oscdata->p[et_harmonics].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[et_timbre].set_name("Timbre");
    oscdata->p[et_timbre].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    oscdata->p[et_timbre].dynamicName = &etDynamicName;
    oscdata->p[et_timbre].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[et_morph].set_name("Morph");
    oscdata->p[et_morph].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    oscdata->p[et_morph].dynamicName = &etDynamicName;
    oscdata->p[et_morph].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[et_aux_mix].set_name("Aux Mix");
    oscdata->p[et_aux_mix].set_type(ct_percent);
    oscdata->p[et_aux_mix].dynamicName = &etDynamicName;
    oscdata->p[et_aux_mix].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[et_lpg_response].set_name("LPG Response");
    oscdata->p[et_lpg_response].set_type(ct_percent_deactivatable);

    oscdata->p[et_lpg_decay].set_name("LPG Decay");
    oscdata->p[et_lpg_decay].set_type(ct_percent);
    oscdata->p[et_lpg_decay].dynamicDeactivation = &etDynamicDeact;
}

void EuroTwist::init_default_values()
{
    oscdata->p[et_engine].val.i = 0;
    oscdata->p[et_harmonics].val.f = 0;
    oscdata->p[et_timbre].val.f = 0;
    oscdata->p[et_morph].val.f = 0;
    oscdata->p[et_aux_mix].val.f = 0;
    oscdata->p[et_lpg_response].val.f = 0;
    oscdata->p[et_lpg_response].deactivated = true;
    oscdata->p[et_lpg_decay].val.f = 0;
}
