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
 * - run that feedback signal through a onepole filter in each line
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
    chirp_noise,
    chirp_dust,
    chirp_sine,
    chirp_ramp,
    chirp_square,
    chirp_sine1over,
    continuous_noise,
    continuous_dust,

};

int waveguide_excitations_count() { return 8; }
std::string waveguide_excitation_name(int i)
{
    auto m = (ExModes)i;
    switch (m)
    {
    case chirp_noise:
        return "Chirp Noise";
    case chirp_dust:
        return "Chirp Dust";
    case chirp_sine:
        return "Chirp Sine";
    case chirp_ramp:
        return "Chirp Ramp";
    case chirp_square:
        return "Chirp Square";
    case chirp_sine1over:
        return "Chirp Sine 1/t";
    case continuous_noise:
        return "Continuous Noise";
    case continuous_dust:
        return "Continuous Dust";
    }
    return "Unknown";
}

void WaveguideOscillator::init(float pitch, bool is_display, bool nzi)
{
    auto pitch_t = std::min(148.f, pitch);
    auto pitchmult_inv =
        std::max(1.0, dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch_t));

    auto pitch2_t =
        std::min(148.f, pitch + localcopy[oscdata->p[wg_tap2_offset].param_id_in_scene].f);
    auto pitchmult2_inv =
        std::max(1.0, dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch2_t));

    tap[0].startValue(pitchmult_inv);
    tap[1].startValue(pitchmult2_inv);
    t2level.startValue(
        limit_range(localcopy[oscdata->p[wg_tap2_mix].param_id_in_scene].f, 0.f, 1.f));

    auto prefill = (int)floor(3 * std::max(pitchmult_inv, pitchmult2_inv));
    delayLine[0].clear();
    delayLine[1].clear();

    auto mode = (ExModes)oscdata->p[wg_ex_mode].val.i;
    auto ph1 = 0.0, ph2 = 0.0;
    auto r1 = 1.0 / pitchmult_inv;
    auto r2 = 1.0 / pitchmult2_inv;
    auto d0 = limit_range(localcopy[oscdata->p[wg_ex_amp].param_id_in_scene].f, 0.f, 1.f);
    for (int i = 0; i < prefill; ++i)
    {
        switch (mode)
        {
        case chirp_sine:
        {
            delayLine[0].write(0.707 * std::sin(2.0 * M_PI * ph1));
            delayLine[1].write(0.707 * std::sin(2.0 * M_PI * ph2));
            ph1 += r1;
            ph2 += r2;
        }
        break;
        case chirp_square:
        {
            delayLine[0].write(0.707 * ((ph1 > 0.5) * 2 - 1));
            delayLine[1].write(0.707 * ((ph2 > 0.5) * 2 - 1));
            ph1 += r1;
            ph2 += r2;
            if (ph1 > 1)
                ph1 -= 1;
            if (ph2 > 1)
                ph2 -= 1;
        }
        break;
        case chirp_ramp:
        {
            delayLine[0].write(0.707 * (ph1 * 2 - 1));
            delayLine[1].write(0.707 * (ph2 * 2 - 1));
            ph1 += r1;
            ph2 += r2;
            if (ph1 > 1)
                ph1 -= 1;
            if (ph2 > 1)
                ph2 -= 1;
        }
        break;
        case chirp_sine1over:
        {
            if (ph1 == 0)
            {
                delayLine[0].write(0.707);
            }
            else
            {
                delayLine[0].write(0.707 * std::sin(2.0 * M_PI / ph1));
            }

            if (ph2 == 0)
            {
                delayLine[1].write(0.707);
            }
            else
            {
                delayLine[1].write(0.707 * std::sin(2.0 * M_PI / ph2));
            }
            ph1 += r1;
            ph2 += r2;
            if (ph1 > 1)
                ph1 -= 1;
            if (ph2 > 1)
                ph2 -= 1;
        }
        break;
        case chirp_dust:
            d0 = 1;
        case continuous_dust:
        {
            auto rn1 = (float)rand() / (float)RAND_MAX;
            auto rn2 = (float)rand() / (float)RAND_MAX;
            auto ds1 = (rn1 > 0.98) ? 1.0 : (rn1 < 0.02) ? -1.0 : 0;
            auto ds2 = (rn2 > 0.98) ? 1.0 : (rn2 < 0.02) ? -1.0 : 0;
            delayLine[0].write(d0 * ds1);
            delayLine[1].write(d0 * ds2);
        }
        break;
        case chirp_noise:
            d0 = 1;
        case continuous_noise:
        default:
            delayLine[0].write(d0 * (((float)rand() / (float)RAND_MAX) * 2 - 1));
            delayLine[1].write(d0 * (((float)rand() / (float)RAND_MAX) * 2 - 1));
            break;
        }
    }
    priorSample[0] = delayLine[0].buffer[prefill - 1];
    priorSample[1] = delayLine[1].buffer[prefill - 1];
}

void WaveguideOscillator::process_block(float pitch, float drift, bool stereo, bool FM,
                                        float FMdepth)
{
    auto mode = (ExModes)oscdata->p[wg_ex_mode].val.i;
    auto pitch_t = std::min(148.f, pitch);
    auto pitchmult_inv = std::max((FIRipol_N >> 1) + 1.0, dsamplerate_os * (1 / 8.175798915) *
                                                              storage->note_to_pitch_inv(pitch_t));

    examp.newValue(limit_range(localcopy[oscdata->p[wg_ex_amp].param_id_in_scene].f, 0.f, 1.f));
    auto p2off = oscdata->p[wg_tap2_offset].get_extended(
        localcopy[oscdata->p[wg_tap2_offset].param_id_in_scene].f);
    auto pitch2_t = std::min(148.f, pitch + p2off);
    auto pitchmult2_inv =
        std::max((FIRipol_N >> 1) + 1.0,
                 dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch2_t));

    tap[0].newValue(pitchmult_inv);
    tap[1].newValue(pitchmult2_inv);

    t2level.newValue(limit_range(localcopy[oscdata->p[wg_tap2_mix].param_id_in_scene].f, 0.f, 1.f));

    auto fbp = limit_range(localcopy[oscdata->p[wg_feedback].param_id_in_scene].f, 0.f, 1.f);
    fbp = sqrt(fbp);
    auto fb0 = 48000 * dsamplerate_inv * 0.9; // just picked by listening
    auto fb1 = 1.0;
    feedback[0].newValue(fb1 * fbp + fb0 * (1 - fbp));

    auto fbp2 = limit_range(localcopy[oscdata->p[wg_feedback2].param_id_in_scene].f, 0.f, 1.f);
    fbp2 = sqrt(fbp2);
    feedback[1].newValue(fb1 * fbp2 + fb0 * (1 - fbp2));

    onepole.newValue(
        limit_range(localcopy[oscdata->p[wg_inloop_onepole].param_id_in_scene].f, 0.f, 1.f));

    float val[2];
    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            auto v = tap[t].v;
            val[t] = delayLine[t].read(v - 0.5);

            // precautionary hard clip
            auto fbv = limit_range(val[t], -1.f, 1.f);

            // one pole filter the feedback value
            auto filtv = onepole.v * fbv + (1 - onepole.v) * priorSample[t];
            priorSample[t] = filtv;

            switch (mode)
            {
            case continuous_noise:
            {
                filtv += examp.v * ((float)rand() / (float)RAND_MAX * 2 - 1);
            }
            break;
            case continuous_dust:
            {
                auto rn1 = (float)rand() / (float)RAND_MAX;
                auto ds1 = examp.v * ((rn1 > 0.98) ? 1.0 : (rn1 < 0.02) ? -1.0 : 0);
                filtv += ds1;
            }
            break;
            default:
                break;
            }
            delayLine[t].write(filtv * feedback[t].v);
        }

        float out = (1.0 - t2level.v) * val[0] + t2level.v * val[1];
        // softclip the ouptput
        out = 1.5 * out - 0.5 * out * out * out;

        output[i] = out;
        outputR[i] = out;

        tap[0].process();
        tap[1].process();
        t2level.process();
        feedback[0].process();
        feedback[1].process();
        onepole.process();
        examp.process();
    }
}

void WaveguideOscillator::init_ctrltypes()
{
    oscdata->p[wg_ex_mode].set_name("Mode");
    oscdata->p[wg_ex_mode].set_type(ct_waveguide_excitation_model);

    oscdata->p[wg_ex_amp].set_name("Excitation Amplitude");
    oscdata->p[wg_ex_amp].set_type(ct_percent);

    oscdata->p[wg_feedback].set_name("Tap 1 FB");
    oscdata->p[wg_feedback].set_type(ct_percent);

    oscdata->p[wg_feedback2].set_name("Tap 2 FB");
    oscdata->p[wg_feedback2].set_type(ct_percent);

    oscdata->p[wg_tap2_offset].set_name("Tap 2 Detune");
    oscdata->p[wg_tap2_offset].set_type(ct_oscspread);

    oscdata->p[wg_tap2_mix].set_name("Tap 2 Mix");
    oscdata->p[wg_tap2_mix].set_type(ct_percent);

    oscdata->p[wg_inloop_onepole].set_name("InLoop OnePole");
    oscdata->p[wg_inloop_onepole].set_type(ct_percent);
}

void WaveguideOscillator::init_default_values()
{
    oscdata->p[wg_ex_mode].val.i = 0;
    oscdata->p[wg_ex_amp].val.f = 1;

    oscdata->p[wg_feedback].val.f = 0.99;
    oscdata->p[wg_feedback2].val.f = 0.99;

    oscdata->p[wg_tap2_offset].val.f = 0.1;
    oscdata->p[wg_tap2_offset].extend_range = false;
    oscdata->p[wg_tap2_mix].val.f = 0.1;

    oscdata->p[wg_inloop_onepole].val.f = 0.96;
}