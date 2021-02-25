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

#include "DebugHelpers.h"
#include "globals.h"
#include "FastMath.h"

/*
 * The waveguide oscillator is a self-oscillating delay with various filters and
 * feedback options.
 *
 * The basic circuit is
 *
 * At Init:
 * - Excite the delay line with an input. In 'chirp' mode this is only pre-play and
 *   in 'continous' mode it is scaled by the amplitude during play
 *
 * At Runtime:
 * - run two delay lines seeded the same and take two taps, tap1 and tap2,
 *   and create an output which is (1-mix) * tap1 + mix * tap2
 * - create a feedback signal fb* tap + excitation in each line
 * - run that feedback signal through a tone filter in each line
 * - drive that feedback signal and run it through a soft clipper in each line
 * - write that feedback signal to the head of the delay line
 *
 */

#include "WaveguideOscillator.h"

enum ExModes
{
    burst_noise,
    burst_dust,
    burst_sine,
    burst_ramp,
    burst_square,
    burst_sweep,
    constant_noise,
    constant_dust,
};

int waveguide_excitations_count() { return 8; }

std::string waveguide_excitation_name(int i)
{
    auto m = (ExModes)i;

    switch (m)
    {
    case burst_noise:
        return "Burst Noise";
    case burst_dust:
        return "Burst Dust";
    case burst_sine:
        return "Burst Sine";
    case burst_ramp:
        return "Burst Ramp";
    case burst_square:
        return "Burst Square";
    case burst_sweep:
        return "Burst Sweep";
    case constant_noise:
        return "Constant Noise";
    case constant_dust:
        return "Constant Dust";
    }
    return "Unknown";
}

void WaveguideOscillator::init(float pitch, bool is_display, bool nzi)
{
    auto pitch_t = std::min(148.f, pitch);
    auto pitchmult_inv =
        std::max(1.0, dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch_t));

    auto pitch2_t =
        std::min(148.f, pitch + localcopy[oscdata->p[wg_str2_detune].param_id_in_scene].f);
    auto pitchmult2_inv =
        std::max(1.0, dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch2_t));

    tap[0].startValue(pitchmult_inv);
    tap[1].startValue(pitchmult2_inv);
    t2level.startValue(
        0.5 * limit_range(localcopy[oscdata->p[wg_str_balance].param_id_in_scene].f, -1.f, 1.f) +
        0.5);

    // we need a big prefill to supprot the delay line for FM
    auto prefill = (int)floor(10 * std::max(pitchmult_inv, pitchmult2_inv));

    for (int i = 0; i < 2; ++i)
    {
        delayLine[i].clear();
        driftlfo[i] = 0.f;
        driftlfo2[i] = 0.f;
        if (nzi)
            driftlfo2[i] = 0.0005 * ((float)rand() / (float)(RAND_MAX));
    }

    auto mode = (ExModes)oscdata->p[wg_exciter_mode].val.i;
    phase1 = 0.0, phase2 = 0.0;

    if (!oscdata->retrigger.val.b && !is_display)
    {
        phase1 = (float)rand() / (float)RAND_MAX;
        phase2 = (float)rand() / (float)RAND_MAX;
    }

    auto r1 = 1.0 / pitchmult_inv;
    auto r2 = 1.0 / pitchmult2_inv;
    auto d0 = limit_range(localcopy[oscdata->p[wg_exciter_level].param_id_in_scene].f, 0.f, 1.f);
    d0 = powf(d0, 0.125);

    for (int i = 0; i < prefill; ++i)
    {
        switch (mode)
        {
        case burst_sine:
        {
            delayLine[0].write(0.707 * std::sin(2.0 * M_PI * phase1));
            delayLine[1].write(0.707 * std::sin(2.0 * M_PI * phase2));
            phase1 += r1;
            phase2 += r2;
        }
        break;
        case burst_square:
        {
            delayLine[0].write(0.707 * ((phase1 > 0.5) * 2 - 1));
            delayLine[1].write(0.707 * ((phase2 > 0.5) * 2 - 1));

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;
        case burst_ramp:
        {
            delayLine[0].write(0.707 * (phase1 * 2 - 1));
            delayLine[1].write(0.707 * (phase2 * 2 - 1));

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;
        case burst_sweep:
        {
            if (phase1 == 0)
            {
                delayLine[0].write(0.707);
            }
            else
            {
                delayLine[0].write(0.707 * std::sin(2.0 * M_PI / phase1));
            }

            if (phase2 == 0)
            {
                delayLine[1].write(0.707);
            }
            else
            {
                delayLine[1].write(0.707 * std::sin(2.0 * M_PI / phase2));
            }

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;
        case burst_dust:
            d0 = DUST_VOLADJUST;
        case constant_dust:
        {
            auto rn1 = (float)rand() / (float)RAND_MAX;
            auto rn2 = (float)rand() / (float)RAND_MAX;
            auto ds1 = (rn1 > DUST_THRESHOLD) ? 1.0 : (rn1 < (1.f - DUST_THRESHOLD)) ? -1.0 : 0;
            auto ds2 = (rn2 > DUST_THRESHOLD) ? 1.0 : (rn2 < (1.f - DUST_THRESHOLD)) ? -1.0 : 0;

            delayLine[0].write(d0 * ds1);
            delayLine[1].write(d0 * ds2);
        }
        break;
        case burst_noise:
            d0 = 1;
        case constant_noise:
        default:
            delayLine[0].write(d0 * (((float)rand() / (float)RAND_MAX) * 2 - 1));
            delayLine[1].write(d0 * (((float)rand() / (float)RAND_MAX) * 2 - 1));
            break;
        }
    }
    priorSample[0] = delayLine[0].buffer[(delayLine[0].wp - 1) & delayLine[0].comb_size];
    priorSample[1] = delayLine[1].buffer[(delayLine[1].wp - 1) & delayLine[1].comb_size];
}

void WaveguideOscillator::process_block(float pitch, float drift, bool stereo, bool FM,
                                        float fmdepthV)
{
    auto mode = (ExModes)oscdata->p[wg_exciter_mode].val.i;
    driftlfo[0] = drift_noise(driftlfo2[0]);
    auto lfodetune = drift * driftlfo[0];
    auto pitch_t = std::min(148.f, pitch + lfodetune);
    auto pitchmult_inv = std::max((FIRipol_N >> 1) + 1.0, dsamplerate_os * (1 / 8.175798915) *
                                                              storage->note_to_pitch_inv(pitch_t));
    auto d0 = limit_range(localcopy[oscdata->p[wg_exciter_level].param_id_in_scene].f, 0.f, 1.f);
    examp.newValue(d0 * d0 * d0);
    auto p2off = oscdata->p[wg_str2_detune].get_extended(
        localcopy[oscdata->p[wg_str2_detune].param_id_in_scene].f);
    driftlfo[1] = drift_noise(driftlfo2[1]);
    lfodetune = drift * driftlfo[1];
    auto pitch2_t = std::min(148.f, pitch + p2off + lfodetune);
    auto pitchmult2_inv =
        std::max((FIRipol_N >> 1) + 1.0,
                 dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch2_t));

    pitchmult_inv = std::min(pitchmult_inv, (delayLine[0].comb_size - 100) * 1.0);
    pitchmult2_inv = std::min(pitchmult2_inv, (delayLine[0].comb_size - 100) * 1.0);

    tap[0].newValue(pitchmult_inv);
    tap[1].newValue(pitchmult2_inv);

    t2level.newValue(
        0.5 * limit_range(localcopy[oscdata->p[wg_str_balance].param_id_in_scene].f, -1.f, 1.f) +
        0.5);

    auto fbp = limit_range(localcopy[oscdata->p[wg_str1_decay].param_id_in_scene].f, 0.f, 1.f);
    fbp = sqrt(fbp);
    auto fb0 = 48000 * dsamplerate_inv * 0.95; // just picked by listening
    auto fb1 = 1.0;
    feedback[0].newValue(fb1 * fbp + fb0 * (1 - fbp));

    auto fbp2 = limit_range(localcopy[oscdata->p[wg_str2_decay].param_id_in_scene].f, 0.f, 1.f);
    fbp2 = sqrt(fbp2);
    feedback[1].newValue(fb1 * fbp2 + fb0 * (1 - fbp2));

    // fmdepthV basically goes 0 -> 16 so lets push it 0,1 for now
    double fv = fmdepthV / 16.0;
    fmdepth.newValue(fv);

    tone.newValue(limit_range(localcopy[oscdata->p[wg_stiffness].param_id_in_scene].f, -1.f, 1.f));
    float clo = -12, cmid = 100, chi = -33;
    float hpCutoff = chi;
    float lpCutoff = cmid;

    if (tone.v > 0)
    {
        // OK so cool scale the hp cutoff
        auto tv = tone.v;
        hpCutoff = tv * (cmid - chi) + chi;
    }
    else
    {
        auto tv = -tone.v;
        lpCutoff = tv * (clo - cmid) + cmid;
    }

    lp.coeff_LP(lp.calc_omega((lpCutoff / 12.0) - 2.f) * OSC_OVERSAMPLING, 0.707);
    // hp.coeff_HP(hp.calc_omega((hpCutoff / 12.0) - 2.f) * OSC_OVERSAMPLING, 0.707);

    float val[2] = {0.f, 0.f};
    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            auto v = tap[t].v;
            if (FM)
                v *= Surge::DSP::fastexp(limit_range(fmdepth.v * master_osc[i] * 3, -6.f, 4.f));

            val[t] = delayLine[t].read(v);

            // Add continuous excitation
            switch (mode)
            {
            case constant_noise:
            {
                val[t] += examp.v * ((float)rand() / (float)RAND_MAX * 2 - 1);
            }
            break;
            case constant_dust:
            {
                auto rn1 = (float)rand() / (float)RAND_MAX;
                auto ds1 = DUST_VOLADJUST * examp.v *
                           ((rn1 > DUST_THRESHOLD)           ? 1.0
                            : (rn1 < (1.f - DUST_THRESHOLD)) ? -1.0
                                                             : 0);
                val[t] += ds1;
            }
            break;
            default:
                val[t] *= examp.v;
                break;
            }
            // precautionary hard clip
            auto fbv = limit_range(val[t], -1.f, 1.f);

            // one pole filter the feedback value
            // auto filtv = tone.v * fbv + (1 - tone.v) * priorSample[t];
            auto lpfiltv = lp.process_sample(fbv);

            auto tv1 = std::max(tone.v * 1.6f, 0.f) * 0.62; // if this was 625 i would go to 1
            float hptv = tv1 * tv1 * tv1;
            auto ophpfiltv = (1 - hptv) * fbv - (hptv)*priorSample[t];
            ophpfiltv = (ophpfiltv < 1e-20 && ophpfiltv > -1e-20) ? 0.0 : ophpfiltv;
            priorSample[t] = ophpfiltv;

            auto filtv = (tone.v > 0) ? ophpfiltv : lpfiltv;
            // This is basically the flush_denormals from the biquad
            filtv = (filtv < 1e-20 && filtv > -1e-20) ? 0.0 : filtv;

            delayLine[t].write(filtv * feedback[t].v);
        }

        // float out = (1.0 - t2level.v) * val[0] + t2level.v * val[1];
        float out = val[0] + t2level.v * (val[1] - val[0]);
        // softclip the ouptput
        out = out * (1.5 - 0.5 * out * out);

        output[i] = out;
        outputR[i] = out;

        tap[0].process();
        tap[1].process();
        t2level.process();
        feedback[0].process();
        feedback[1].process();
        tone.process();
        examp.process();
        fmdepth.process();
    }
    lp.flush_sample_denormal();
}

void WaveguideOscillator::init_ctrltypes()
{
    oscdata->p[wg_exciter_mode].set_name("Exciter");
    oscdata->p[wg_exciter_mode].set_type(ct_waveguide_excitation_model);

    oscdata->p[wg_exciter_level].set_name("Exciter Level");
    oscdata->p[wg_exciter_level].set_type(ct_percent);
    oscdata->p[wg_exciter_level].val_default.f = 1.f;

    oscdata->p[wg_str1_decay].set_name("String 1 Decay");
    oscdata->p[wg_str1_decay].set_type(ct_percent);
    oscdata->p[wg_str1_decay].val_default.f = 0.95;

    oscdata->p[wg_str2_decay].set_name("String 2 Decay");
    oscdata->p[wg_str2_decay].set_type(ct_percent);
    oscdata->p[wg_str2_decay].val_default.f = 0.95;

    oscdata->p[wg_str2_detune].set_name("String 2 Detune");
    oscdata->p[wg_str2_detune].set_type(ct_oscspread_bipolar);

    oscdata->p[wg_str_balance].set_name("String Balance");
    oscdata->p[wg_str_balance].set_type(ct_percent_bipolar_stringbal);

    oscdata->p[wg_stiffness].set_name("Stiffness");
    oscdata->p[wg_stiffness].set_type(ct_percent_bipolar);
}

void WaveguideOscillator::init_default_values()
{
    oscdata->p[wg_exciter_mode].val.i = 0;
    oscdata->p[wg_exciter_level].val.f = 1.f;

    oscdata->p[wg_str1_decay].val.f = 0.95f;
    oscdata->p[wg_str2_decay].val.f = 0.95f;

    oscdata->p[wg_str2_detune].val.f = 0.1f;
    oscdata->p[wg_str2_detune].extend_range = false;
    oscdata->p[wg_str_balance].val.f = 0.f;

    oscdata->p[wg_stiffness].val.f = 0.f;
}
