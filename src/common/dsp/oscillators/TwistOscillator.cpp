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

#include "TwistOscillator.h"
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

#if SAMPLERATE_LANCZOS
#include "LanczosResampler.h"
#endif

std::string twist_engine_name(int i)
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
        engineLabels.push_back({"Detune", "Square Shape", "Saw Shape", "Sync"});
        // Waveshaper
        engineLabels.push_back({"Waveshaper", "Fold", "Asymmetry", "Variation"});
        // 2-Operator FM
        engineLabels.push_back({"Ratio", "Amount", "Feedback", "Sub"});
        // Formant/PD
        engineLabels.push_back({"Ratio/Type", "Formant", "Shape", "PD"});
        // Harmonic
        engineLabels.push_back({"Bump", "Peak", "Shape", "Organ"});
        // Wavetable
        engineLabels.push_back({"Bank", "Morph X", "Morph Y", "Lo-Fi"});
        // Chords
        engineLabels.push_back({"Type", "Inversion", "Shape", "Root"});
        // Vowels/Speech
        engineLabels.push_back({"Speak", "Species", "Segment", "Raw"});
        // Granular Cloud
        engineLabels.push_back({"Pitch Random", "Grain Density", "Grain Duration", "Sine"});
        // Filtered Noise
        engineLabels.push_back({"Type", "Clock Frequency", "Resonance", "Dual Peak"});
        // Particle Noise
        engineLabels.push_back({"Freq Random", "Density", "Filter Type", "Raw"});
        // Inharmonic String
        engineLabels.push_back({"Inharmonicity", "Brightness", "Decay Time", "Exciter"});
        // Modal Resonator
        engineLabels.push_back({"Material", "Brightness", "Decay Time", "Exciter"});
        // Analog Kick
        engineLabels.push_back({"Sharpness", "Brightness", "Decay Time", "Variation"});
        // Analog Snare
        engineLabels.push_back({"Tone<>Noise", "Model", "Decay Time", "Variation"});
        // Analog Hi-Hat
        engineLabels.push_back({"Tone<>Noise", "Low Cut", "Decay Time", "Variation"});
    }

    const char *getName(const Parameter *p) const override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);

        if (oscs->type.val.i != ot_twist)
        {
            return "ERROR";
        }

        auto engp = &(oscs->p[TwistOscillator::twist_engine]);
        if (engp->ctrltype != ct_twist_engine)
        {
            return "ERROR";
        }
        auto eng = engp->val.i;
        if (eng < 0 || eng >= engineLabels.size())
        {
            return "ERROR";
        }
        auto idx = (p - engp);
        auto lab = engineLabels[eng][idx - 1];

        if (idx == TwistOscillator::twist_aux_mix)
        {
            if (p->extend_range)
            {
                lab = "Main<>" + lab + " Pan";
            }
            else
            {
                lab += " Mix";
            }
        }

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
        engineBipolars.push_back({false, false, true, true});  // 2-Operator FM
        engineBipolars.push_back({false, false, false, true}); // Formant/PD
        engineBipolars.push_back({false, false, false, true}); // Harmonic
        engineBipolars.push_back({true, false, false, true});  // Wavetable
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

    const bool getValue(const Parameter *p) const override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);

        if (oscs->type.val.i != ot_twist)
        {
            return false;
        }

        auto engp = &(oscs->p[TwistOscillator::twist_engine]);
        if (engp->ctrltype != ct_twist_engine)
        {
            return "ERROR";
        }
        auto eng = engp->val.i;
        if (eng < 0 || eng >= engineBipolars.size())
        {
            return false;
        }
        auto idx = (p - engp);

        bool res = engineBipolars[eng][idx - 1];
        if (idx == TwistOscillator::twist_aux_mix)
            res = p->extend_range;

        return res;
    }
} etDynamicBipolar;

static struct EngineDisplayFormatter : ParameterExternalFormatter
{
    bool formatValue(const Parameter *p, float value, char *txt, int txtlen) const override
    {
        return false;
    }
    bool stringToValue(const Parameter *p, const char *txt, float &outVal) const override
    {
        return false;
    }
    bool formatAltValue(const Parameter *p, float value, char *txt, int txtlen) const override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);

        if (oscs->type.val.i != ot_twist)
        {
            return false;
        }

        auto engp = &(oscs->p[TwistOscillator::twist_engine]);
        if (engp->ctrltype != ct_twist_engine)
        {
            return false;
        }
        auto eng = engp->val.i;
        if (eng == 6)
        {
            // This is the chords engine
            static std::vector<std::string> chords = {"oct", "5",   "sus4", "m",  "m7", "m9",
                                                      "m11", "6/9", "M9",   "M7", "M"};

            float univalue = (value + 1) * 0.5;
            int whichC = floor(univalue * (chords.size()));
            if (whichC < 0)
                whichC = 0;
            if (whichC >= chords.size())
                whichC = chords.size() - 1;
            snprintf(txt, txtlen, "%s", chords[whichC].c_str());
            return true;
        }
        return false;
    }
} etDynamicFormatter;

/*
 * The only place we use dynamic deactivation is on the LPG sliders where
 * we bind this object to the decay and make it follow the response, which
 * is why we don't have crafty ifs here. If we do more deactivation obviously
 * we will want crafty ifs.
 */
static struct EngineDynamicDeact : public ParameterDynamicDeactivationFunction
{
    EngineDynamicDeact() noexcept {}

    const bool getValue(const Parameter *p) const override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);
        return oscs->p[TwistOscillator::twist_lpg_response].deactivated;
    }

    Parameter *getPrimaryDeactivationDriver(const Parameter *p) const override
    {
        auto oscs = &(p->storage->getPatch().scene[p->scene - 1].osc[p->ctrlgroup_entry]);
        return &(oscs->p[TwistOscillator::twist_lpg_response]);
    }
} etDynamicDeact;

TwistOscillator::TwistOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                 pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy), charFilt(storage)
{
#if SAMPLERATE_LANCZOS
    lancRes = std::make_unique<LanczosResampler>(48000, storage->dsamplerate_os);
    srcstate = nullptr;
#else
    int error;
    srcstate = src_new(SRC_SINC_FASTEST, 2, &error);
    // srcstate = src_new(SRC_LINEAR, 2, &error);
    if (error != 0)
    {
        srcstate = nullptr;
    }
#endif
    voice = std::make_unique<plaits::Voice>();
    shared_buffer = new char[16384];
    alloc = std::make_unique<stmlib::BufferAllocator>(shared_buffer, 16384);
    voice->Init(alloc.get());
    patch = std::make_unique<plaits::Patch>();
    mod = std::make_unique<plaits::Modulations>();

    // FM downsampling with a linear interpolator is absolutely fine
    int error;
    fmdownsamplestate = src_new(SRC_LINEAR, 1, &error);
    if (error != 0)
    {
        fmdownsamplestate = nullptr;
    }
}

float TwistOscillator::tuningAwarePitch(float pitch)
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

void TwistOscillator::init(float pitch, bool is_display, bool nonzero_drift)
{
    charFilt.init(storage->getPatch().character.val.i);

    float tpitch = tuningAwarePitch(pitch);
    memset((void *)patch.get(), 0, sizeof(plaits::Patch));
    memset((void *)mod.get(), 0, sizeof(plaits::Modulations));

    driftLFO.init(nonzero_drift);

    // Lets run forward a cycle
    int throwaway = 0;
    double cycleInSamples = std::max(1.0, 1.0 / pitch_to_dphase(tpitch));

    while (cycleInSamples < 10)
        cycleInSamples *= 2;

    if (!(oscdata->retrigger.val.b || is_display))
    {
        cycleInSamples *= (1.0 + storage->rand_01());
    }

    memset(fmlagbuffer, 0, (BLOCK_SIZE_OS << 1) * sizeof(float));
    fmrp = 0;
    fmwp = (int)(BLOCK_SIZE_OS * 48000 * storage->dsamplerate_os_inv);

    process_block_internal<false, true>(pitch, 0, false, 0, std::ceil(cycleInSamples));
}
TwistOscillator::~TwistOscillator()
{
    if (shared_buffer)
        delete[] shared_buffer;

    if (srcstate)
        srcstate = src_delete(srcstate);

    if (fmdownsamplestate)
        fmdownsamplestate = src_delete(fmdownsamplestate);
}

template <bool FM, bool throwaway>
void TwistOscillator::process_block_internal(float pitch, float drift, bool stereo, float FMdepth,
                                             int throwawayBlocks)
{
#if SAMPLERATE_SRC
    if (!srcstate)
        return;
#endif

    if (FM && !fmdownsamplestate)
        return;

    pitch = tuningAwarePitch(pitch);

    auto driftv = driftLFO.next();
    patch->note = pitch + drift * driftv;
    patch->engine = oscdata->p[twist_engine].val.i;

    harm.newValue(fvbp(twist_harmonics));
    timb.newValue(fvbp(twist_timbre));
    morph.newValue(fvbp(twist_morph));
    lpgcol.newValue(fv(twist_lpg_response));
    lpgdec.newValue(fv(twist_lpg_decay));
    auxmix.newValue(limit_range(fvbp(twist_aux_mix), -1.f, 1.f));

    bool lpgIsOn = !oscdata->p[twist_lpg_response].deactivated;

    constexpr int max_subblock = 4;
    int subblock = (lpgIsOn || FM) ? 1 : max_subblock;
#if SAMPLERATE_SRC
    float src_in[subblock][2];
    float src_out[BLOCK_SIZE_OS][2];

    SRC_DATA sdata;
    sdata.end_of_input = 0;
    sdata.src_ratio = dsamplerate_os / 48000.0;
#endif

    float normFMdepth = 0;

    if (FM)
    {
        float dsmaster[BLOCK_SIZE_OS << 2];
        SRC_DATA fmdata;
        fmdata.end_of_input = 0;
        fmdata.src_ratio = 48000.0 / storage->dsamplerate_os; // going INTO the plaits rate
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

    int required_blocks = throwaway ? throwawayBlocks : BLOCK_SIZE_OS;

#if SAMPLERATE_SRC
    for (int i = 0; i < carrover_size; ++i)
    {
        if (oscdata->p[twist_aux_mix].extend_range)
        {
            output[i] = auxmix.v * carryover[i][1] + (1.0 - auxmix.v) * carryover[i][0];
            outputR[i] = auxmix.v * carryover[i][0] + (1.0 - auxmix.v) * carryover[i][1];
        }
        else
        {
            output[i] = auxmix.v * carryover[i][1] + (1.0 - auxmix.v) * carryover[i][0];
            outputR[i] = output[i];
        }
    }
    int total_generated = carrover_size;
    carrover_size = 0;
#else
    int total_generated =
        required_blocks - lancRes->inputsRequiredToGenerateOutputs(required_blocks);
#endif

    if (lpgIsOn)
    {
        mod->trigger = gate ? 1.0 : 0.0;
        mod->trigger_patched = true;
    }

    while (total_generated < required_blocks)
    {
        plaits::Voice::Frame poutput[max_subblock];
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
#if SAMPLERATE_SRC
        auxmix.process();
#endif

        if (FM)
        {
            mod->frequency_patched = true;
            mod->frequency = 137 * fmlagbuffer[fmrp]; // this is in 'notes'
            fmrp = (fmrp + 1) & ((BLOCK_SIZE_OS << 1) - 1);
            patch->frequency_modulation_amount = normFMdepth;
        }
        else
        {
            mod->frequency_patched = false;
            patch->frequency_modulation_amount = 0;
        }

        voice->Render(*patch, *mod, poutput, subblock);

#if SAMPLERATE_LANCZOS
        for (int i = 0; i < subblock; ++i)
        {
            lancRes->push(poutput[i].out / 32768.f, poutput[i].aux / 32768.f);
        }
        total_generated =
            required_blocks - lancRes->inputsRequiredToGenerateOutputs(required_blocks);
#else
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
                if (oscdata->p[twist_aux_mix].extend_range)
                {
                    output[total_generated + i] =
                        auxmix.v * src_out[i][1] + (1 - auxmix.v) * src_out[i][0];
                    outputR[total_generated + i] =
                        auxmix.v * src_out[i][0] + (1 - auxmix.v) * src_out[i][1];
                }
                else
                {
                    output[total_generated + i] =
                        auxmix.v * src_out[i][1] + (1 - auxmix.v) * src_out[i][0];
                    outputR[total_generated + i] = output[total_generated + i];
                }
            }
        }
        total_generated += sdata.output_frames_gen;
        if (sdata.input_frames_used != subblock)
        {
            // FIXME
            std::cout << "DEAL " << std::endl;
        }
#endif
    }

#if SAMPLERATE_LANCZOS
    if (throwaway)
    {
        lancRes->advanceReadPointer(required_blocks);
    }
    else
    {
        float tL[BLOCK_SIZE_OS], tR[BLOCK_SIZE_OS];
        lancRes->populateNextBlockSizeOS(tL, tR);

        for (int i = 0; i < BLOCK_SIZE_OS; ++i)
        {
            if (oscdata->p[twist_aux_mix].extend_range)
            {
                output[i] = auxmix.v * tL[i] + (1 - auxmix.v) * tR[i];
                outputR[i] = auxmix.v * tR[i] + (1 - auxmix.v) * tL[i];
            }
            else
            {
                output[i] = auxmix.v * tR[i] + (1 - auxmix.v) * tL[i];
                outputR[i] = output[i];
            }
            auxmix.process();
        }
    }
    lancRes->renormalizePhases();
#endif

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

void TwistOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
    if (FM)
    {
        TwistOscillator::process_block_internal<true>(pitch, drift, stereo, FMdepth);
    }
    else
    {
        TwistOscillator::process_block_internal<false>(pitch, drift, stereo, FMdepth);
    }
}

void TwistOscillator::init_ctrltypes()
{
    oscdata->p[twist_engine].set_name("Engine");
    oscdata->p[twist_engine].set_type(ct_twist_engine);

    oscdata->p[twist_harmonics].set_name("Harmonics");
    oscdata->p[twist_harmonics].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    oscdata->p[twist_harmonics].dynamicName = &etDynamicName;
    oscdata->p[twist_harmonics].dynamicBipolar = &etDynamicBipolar;
    oscdata->p[twist_harmonics].set_user_data(&etDynamicFormatter);

    oscdata->p[twist_timbre].set_name("Timbre");
    oscdata->p[twist_timbre].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    oscdata->p[twist_timbre].dynamicName = &etDynamicName;
    oscdata->p[twist_timbre].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[twist_morph].set_name("Morph");
    oscdata->p[twist_morph].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    oscdata->p[twist_morph].dynamicName = &etDynamicName;
    oscdata->p[twist_morph].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[twist_aux_mix].set_name("Aux Mix");
    oscdata->p[twist_aux_mix].set_type(ct_twist_aux_mix);
    oscdata->p[twist_aux_mix].dynamicName = &etDynamicName;
    oscdata->p[twist_aux_mix].dynamicBipolar = &etDynamicBipolar;

    oscdata->p[twist_lpg_response].set_name("LPG Response");
    oscdata->p[twist_lpg_response].set_type(ct_percent_deactivatable);

    oscdata->p[twist_lpg_decay].set_name("LPG Decay");
    oscdata->p[twist_lpg_decay].set_type(ct_percent);
    oscdata->p[twist_lpg_decay].dynamicDeactivation = &etDynamicDeact;
}

void TwistOscillator::init_default_values()
{
    oscdata->p[twist_engine].val.i = 0.f;
    oscdata->p[twist_harmonics].val.f = 0.f;
    oscdata->p[twist_timbre].val.f = 0.f;
    oscdata->p[twist_morph].val.f = 0.f;
    oscdata->p[twist_aux_mix].val.f = -1.f;
    oscdata->p[twist_aux_mix].set_extend_range(false);
    oscdata->p[twist_lpg_response].val.f = 0.f;
    oscdata->p[twist_lpg_response].deactivated = true;
    oscdata->p[twist_lpg_decay].val.f = 0.f;
}
