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
 * Running TODO
 * - the softclip is dumb. Use that param differently. Perhaps per-tap feedback?
 * - The detune as a spread around 0 rather than an exact?
 *
 */
#include "WaveguideOscillator.h"

enum ExModes
{
    burst_noise,
    burst_dust,
    burst_sine,
    burst_tri,
    burst_ramp,
    burst_square,
    burst_sweep,

    constant_noise,
    constant_dust,
    constant_sine,
    constant_tri,
    constant_ramp,
    constant_square,
    constant_sweep,
    constant_audioin,
};

int waveguide_excitations_count() { return 15; }

std::string waveguide_excitation_name(int i)
{
    auto m = (ExModes)i;

    switch (m)
    {
    case burst_noise:
        return "Burst Noise";
    case burst_dust:
        return "Burst Dark Noise";
    case burst_sine:
        return "Burst Sine";
    case burst_ramp:
        return "Burst Ramp";
    case burst_tri:
        return "Burst Triangle";
    case burst_square:
        return "Burst Square";
    case burst_sweep:
        return "Burst Sweep";
    case constant_noise:
        return "Constant Noise";
    case constant_dust:
        return "Constant Dark Noise";
    case constant_sine:
        return "Constant Sine";
    case constant_tri:
        return "Constant Triangle";
    case constant_ramp:
        return "Constant Ramp";
    case constant_square:
        return "Constant Square";
    case constant_sweep:
        return "Constant Sweep";
    case constant_audioin:
        return "Audio In";
    }
    return "Unknown";
}

void WaveguideOscillator::init(float pitch, bool is_display, bool nzi)
{
    memset((void *)dustBuffer, 0, 2 * (BLOCK_SIZE_OS) * sizeof(float));

    if (is_display)
    {
        gen = std::minstd_rand(8675309);
    }
    else
    {
        std::random_device rd;
        gen = std::minstd_rand(rd());
    }
    urd = std::uniform_real_distribution<float>(0.0, 1.0);

    auto pitch_t = std::min(148.f, pitch);
    auto pitchmult_inv =
        std::max(1.0, dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch_t));

    auto pitch2_t =
        std::min(148.f, pitch + localcopy[oscdata->p[wg_str2_detune].param_id_in_scene].f);
    auto pitchmult2_inv =
        std::max(1.0, dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch2_t));

    noiseLp.coeff_LP2B(noiseLp.calc_omega(0) * OSC_OVERSAMPLING, 0.9);
    for (int i = 0; i < 3; ++i)
        fillDustBuffer(pitchmult_inv, pitchmult2_inv);
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
        driftLFO[i].init(nzi);
    }

    auto mode = (ExModes)oscdata->p[wg_exciter_mode].val.i;
    phase1 = 0.0, phase2 = 0.0;

    if (!oscdata->retrigger.val.b && !is_display)
    {
        phase1 = urd(gen);
        phase2 = urd(gen);
    }

    auto r1 = 1.0 / pitchmult_inv;
    auto r2 = 1.0 / pitchmult2_inv;
    auto d0 = limit_range(localcopy[oscdata->p[wg_exciter_level].param_id_in_scene].f, 0.f, 1.f);
    d0 = powf(d0, 0.125);

    int dustrp = BLOCK_SIZE_OS;
    for (int i = 0; i < prefill; ++i)
    {
        switch (mode)
        {
        case burst_sine:
            d0 = 1;
        case constant_sine:
        {
            delayLine[0].write(0.707 * d0 * std::sin(2.0 * M_PI * phase1));
            delayLine[1].write(0.707 * d0 * std::sin(2.0 * M_PI * phase2));
            phase1 += r1;
            phase2 += r2;
        }
        break;
        case burst_tri:
            d0 = 1;
        case constant_tri:
        {
            // phase is 0,1 so tri is
            auto t1 = (phase1 < 0.5) ? (phase1 * 4 - 1) : ((1 - phase1)) * 4 - 1;
            delayLine[0].write(0.707 * d0 * t1);
            auto t2 = (phase2 < 0.5) ? (phase2 * 4 - 1) : ((1 - phase2)) * 4 - 1;
            delayLine[1].write(0.707 * d0 * t2);
            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;
        case burst_square:
            d0 = 1;
        case constant_square:
        {
            delayLine[0].write(0.707 * d0 * ((phase1 > 0.5) * 2 - 1));
            delayLine[1].write(0.707 * d0 * ((phase2 > 0.5) * 2 - 1));

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;
        case burst_ramp:
            d0 = 1;
        case constant_ramp:
        {
            delayLine[0].write(0.707 * d0 * (phase1 * 2 - 1));
            delayLine[1].write(0.707 * d0 * (phase2 * 2 - 1));

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;
        case burst_sweep:
            d0 = 1;
        case constant_sweep:
        {
            if (phase1 == 0)
            {
                delayLine[0].write(0.707 * d0);
            }
            else
            {
                delayLine[0].write(0.707 * d0 * std::sin(2.0 * M_PI / phase1));
            }

            if (phase2 == 0)
            {
                delayLine[1].write(0.707 * d0);
            }
            else
            {
                delayLine[1].write(0.707 * d0 * std::sin(2.0 * M_PI / phase2));
            }

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
                phase1 -= 1;
            if (phase2 > 1)
                phase2 -= 1;
        }
        break;

        case constant_audioin:
            delayLine[0].write(0);
            delayLine[1].write(0);
            break;

        case burst_dust:
            d0 = 1.0;
        case constant_dust:
        {
            if (dustrp >= BLOCK_SIZE_OS)
            {
                dustrp = 0;
                fillDustBuffer(pitchmult_inv, pitchmult2_inv);
            }

            delayLine[0].write(d0 * dustBuffer[0][dustrp]);
            delayLine[1].write(d0 * dustBuffer[1][dustrp]);
            dustrp++;
        }
        break;
        case burst_noise:
            d0 = 1;
        case constant_noise:
        default:
            delayLine[0].write(d0 * (urd(gen) * 2 - 1));
            delayLine[1].write(d0 * (urd(gen) * 2 - 1));
            break;
        }
    }
    priorSample[0] = delayLine[0].buffer[(delayLine[0].wp - 1) & delayLine[0].comb_size];
    priorSample[1] = delayLine[1].buffer[(delayLine[1].wp - 1) & delayLine[1].comb_size];

    charFilt.init(storage->getPatch().character.val.i);
}

void WaveguideOscillator::process_block(float pitch, float drift, bool stereo, bool FM,
                                        float fmdepthV)
{
    auto mode = (ExModes)oscdata->p[wg_exciter_mode].val.i;

    auto lfodetune = drift * driftLFO[0].next();
    auto pitch_t = std::min(148.f, pitch + lfodetune);
    auto pitchmult_inv = std::max((FIRipol_N >> 1) + 1.0, dsamplerate_os * (1 / 8.175798915) *
                                                              storage->note_to_pitch_inv(pitch_t));
    double d0 = limit_range(localcopy[oscdata->p[wg_exciter_level].param_id_in_scene].f, 0.f, 1.f);

    if (mode >= constant_noise)
    {
        examp.newValue(d0 * d0 * d0);
    }
    else
    {
        if (d0 < 0.1)
        {
            examp.newValue(d0 * 4.6415); // cbrt(0.1) * 10 to match it where cbrt(d0) starts below
        }
        else
        {
            examp.newValue(cbrt(d0));
        }
    }

    lfodetune = drift * driftLFO[1].next();
    auto p2off = oscdata->p[wg_str2_detune].get_extended(
        localcopy[oscdata->p[wg_str2_detune].param_id_in_scene].f);
    auto pitch2_t = std::min(148.f, pitch + p2off + lfodetune);
    auto pitchmult2_inv =
        std::max((FIRipol_N >> 1) + 1.0,
                 dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch2_t));

    pitchmult_inv = std::min(pitchmult_inv, (delayLine[0].comb_size - 100) * 1.0);
    pitchmult2_inv = std::min(pitchmult2_inv, (delayLine[0].comb_size - 100) * 1.0);

    auto dp1 = pitch_to_dphase(pitch_t);
    auto dp2 = pitch_to_dphase(pitch2_t);

    tap[0].newValue(pitchmult_inv);
    tap[1].newValue(pitchmult2_inv);

    t2level.newValue(
        0.5 * limit_range(localcopy[oscdata->p[wg_str_balance].param_id_in_scene].f, -1.f, 1.f) +
        0.5);

    auto fbp = limit_range(localcopy[oscdata->p[wg_str1_decay].param_id_in_scene].f, 0.f, 1.f);
    fbp = sqrt(fbp);
    auto fb0 = 0.85; // just picked by listening
    feedback[0].newValue(fbp + fb0 * (1 - fbp));

    auto fbp2 = limit_range(localcopy[oscdata->p[wg_str2_decay].param_id_in_scene].f, 0.f, 1.f);
    fbp2 = sqrt(fbp2);
    feedback[1].newValue(fbp2 + fb0 * (1 - fbp2));

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

    float val[2] = {0.f, 0.f}, fbNoOutVal[2] = {0.f, 0.f};
    if (mode == constant_dust)
    {
        fillDustBuffer(pitchmult_inv, pitchmult2_inv);
    }
    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            auto v = tap[t].v;
            float *phs = (t == 0) ? &phase1 : &phase2;
            float dp = (t == 0) ? dp1 : dp2;
            if (FM)
                v *= Surge::DSP::fastexp(limit_range(fmdepth.v * master_osc[i] * 3, -6.f, 4.f));

            val[t] = delayLine[t].read(v);
            fbNoOutVal[t] = 0.f;

            // Add continuous excitation
            switch (mode)
            {
            case constant_noise:
            {
                val[t] += examp.v * (urd(gen) * 2 - 1);
            }
            break;
            case constant_dust:
            {

                auto ds1 = examp.v * dustBuffer[t][i];
                val[t] += ds1;
            }
            break;
            case constant_ramp:
            {
                auto rn = 0.707 * (*phs * 2 - 1);
                val[t] += examp.v * rn;
                *phs += dp;
                if (*phs > 1)
                    *phs -= 1;
            }
            break;
            case constant_tri:
            {
                auto rn = 0.707 * ((*phs < 0.5) ? (*phs * 4 - 1) : ((1 - *phs) * 4 - 1));
                val[t] += examp.v * rn;
                *phs += dp;
                if (*phs > 1)
                    *phs -= 1;
            }
            break;
            case constant_sweep:
            {
                float sv = 0;
                if (*phs == 0)
                {
                    sv = 1.0;
                }
                else
                {
                    sv = std::sin(2.0 * M_PI / *phs);
                }
                val[t] += examp.v * 0.707 * sv;
                *phs += dp;
                if (*phs > 1)
                    *phs -= 1;
            }
            break;
            case constant_sine:
            {
                float sv = std::sin(2.0 * M_PI * *phs);
                val[t] += examp.v * 0.707 * sv;
                *phs += dp;
                if (*phs > 1)
                    *phs -= 1;
            }
            break;
            case constant_square:
            {
                auto rn = 0.707 * ((*phs > 0.5) ? 1 : -1);
                val[t] += examp.v * rn;
                *phs += dp;
                if (*phs > 1)
                    *phs -= 1;
            }
            break;
            case constant_audioin:
            {
                fbNoOutVal[t] = examp.v * storage->audio_in[t][i];
            }
            break;
            default:
                // We should do something else with amplitude here
                // val[t] *= examp.v;
                break;
            }
            // precautionary hard clip
            auto fbv = limit_range(val[t] + fbNoOutVal[t], -1.f, 1.f);

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

void WaveguideOscillator::fillDustBuffer(float tap0, float tap1)
{
    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        auto v0 = urd(gen) * 2 - 1;
        auto v1 = urd(gen) * 2 - 1;
        noiseLp.process_sample_nolag(v0, v1);
        dustBuffer[0][i] = v0 * 1.7;
        dustBuffer[1][i] = v1 * 1.7;
    }
}
