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

#include "globals.h"
#include "FastMath.h"

/*
 * String oscillator is a self-oscillating delay with various filters and
 * feedback options.
 *
 * The basic circuit is
 *
 * At init:
 * - Excite the delay line with an input. In 'chirp' mode this is only pre-play and
 *   in 'continuous' mode it is scaled by the amplitude during play
 *
 * At runtime:
 * - run two delay lines seeded the same and take two taps, tap1 and tap2,
 *   and create an output which is (1-mix) * tap1 + mix * tap2
 * - create a feedback signal fb* tap + excitation in each line
 * - run that feedback signal through a tone filter in each line
 * - drive that feedback signal and run it through a soft clipper in each line
 * - write that feedback signal to the head of the delay line
 *
 */

#include "StringOscillator.h"
#include "SurgeMemoryPools.h"

int stringosc_excitations_count() { return 15; }

std::string stringosc_excitation_name(int i)
{
    auto m = (StringOscillator::exciter_modes)i;

    switch (m)
    {
    case StringOscillator::burst_noise:
        return "Burst Noise";
    case StringOscillator::burst_pink_noise:
        return "Burst Pink Noise";
    case StringOscillator::burst_sine:
        return "Burst Sine";
    case StringOscillator::burst_ramp:
        return "Burst Ramp";
    case StringOscillator::burst_tri:
        return "Burst Triangle";
    case StringOscillator::burst_square:
        return "Burst Square";
    case StringOscillator::burst_sweep:
        return "Burst Sweep";
    case StringOscillator::constant_noise:
        return "Constant Noise";
    case StringOscillator::constant_pink_noise:
        return "Constant Pink Noise";
    case StringOscillator::constant_sine:
        return "Constant Sine";
    case StringOscillator::constant_tri:
        return "Constant Triangle";
    case StringOscillator::constant_ramp:
        return "Constant Ramp";
    case StringOscillator::constant_square:
        return "Constant Square";
    case StringOscillator::constant_sweep:
        return "Constant Sweep";
    case StringOscillator::constant_audioin:
        return "Audio In";
    }
    return "Unknown";
}

StringOscillator::~StringOscillator()
{
    if (storage && !ownDelayLines)
    {
        if (delayLine[0])
            storage->memoryPools->stringDelayLines.returnItem(delayLine[0]);
        if (delayLine[1])
            storage->memoryPools->stringDelayLines.returnItem(delayLine[1]);
    }
    else
    {
        if (delayLine[0])
            delete delayLine[0];
        if (delayLine[1])
            delete delayLine[1];
    }
};

void StringOscillator::init(float pitch, bool is_display, bool nzi)
{
    // fixme - alloc in is_display but for now just deal with the race
    // delayLine[0] = std::make_unique<SSESincDelayLine<16384>>();
    // delayLine[1] = std::make_unique<SSESincDelayLine<16384>>();
    if (is_display)
    {
        ownDelayLines = true;
        delayLine[0] = new SSESincDelayLine<16384>(storage->sinctable);
        delayLine[1] = new SSESincDelayLine<16384>(storage->sinctable);
    }
    else
    {
        ownDelayLines = false;
        delayLine[0] = storage->memoryPools->stringDelayLines.getItem(storage->sinctable);
        delayLine[1] = storage->memoryPools->stringDelayLines.getItem(storage->sinctable);
    }

    memset((void *)dustBuffer, 0, 2 * (BLOCK_SIZE_OS) * sizeof(float));

    id_exciterlvl = oscdata->p[str_exciter_level].param_id_in_scene;
    id_str1decay = oscdata->p[str_str1_decay].param_id_in_scene;
    id_str2decay = oscdata->p[str_str2_decay].param_id_in_scene;
    id_str2detune = oscdata->p[str_str2_detune].param_id_in_scene;
    id_strbalance = oscdata->p[str_str_balance].param_id_in_scene;
    id_stiffness = oscdata->p[str_stiffness].param_id_in_scene;

    if (is_display)
    {
        gen = std::minstd_rand(8675309);
    }
    else
    {
        gen = std::minstd_rand(storage->rand());
    }

    urd = std::uniform_real_distribution<float>(0.0, 1.0);

    auto pitch_t = std::min(148.f, pitch);
    auto pitchmult_inv = std::max(1.0, storage->dsamplerate_os * (1 / 8.175798915) *
                                           storage->note_to_pitch_inv(pitch_t));
    auto p2off = oscdata->p[str_str2_detune].get_extended(localcopy[id_str2detune].f);
    double pitch2_t = 1, pitchmult2_inv = 1;

    if (oscdata->p[str_str2_detune].absolute)
    {
        pitch2_t = std::min(148.f, pitch);

        auto frequency = Tunings::MIDI_0_FREQ * storage->note_to_pitch(pitch2_t);
        auto fac = oscdata->p[str_str2_detune].extend_range ? 12 * 16 : 16;
        auto detune = localcopy[id_str2detune].f * fac;

        frequency = std::max(10.0, frequency + detune);
        pitchmult2_inv = std::max(1.0, storage->dsamplerate_os / frequency);
    }
    else
    {
        pitch2_t = std::min(148.f, pitch + p2off);
        pitchmult2_inv = std::max(1.0, storage->dsamplerate_os * (1 / 8.175798915) *
                                           storage->note_to_pitch_inv(pitch2_t));
    }

    noiseLp.coeff_LP2B(noiseLp.calc_omega(0) * OSC_OVERSAMPLING, 0.9);
    for (int i = 0; i < 3; ++i)
    {
        fillDustBuffer(pitchmult_inv, pitchmult2_inv);
    }

    tap[0].startValue(pitchmult_inv);
    tap[1].startValue(pitchmult2_inv);
    t2level.startValue(0.5 * limit_range(localcopy[id_strbalance].f, -1.f, 1.f) + 0.5);

    // we need a big prefill to support the delay line for FM
    auto prefill = (int)floor(10 * std::max(pitchmult_inv, pitchmult2_inv) * getOversampleLevel());

    for (int i = 0; i < 2; ++i)
    {
        delayLine[i]->clear();
        driftLFO[i].init(nzi);
    }

    auto mode = (exciter_modes)oscdata->p[str_exciter_mode].val.i;
    phase1 = 0.0, phase2 = 0.0;

    if (!oscdata->retrigger.val.b && !is_display)
    {
        phase1 = urd(gen);
        phase2 = urd(gen);
    }

    auto r1 = 1.0 / (pitchmult_inv * getOversampleLevel());
    auto r2 = 1.0 / (pitchmult2_inv * getOversampleLevel());
    auto d0 = limit_range(localcopy[id_exciterlvl].f, 0.f, 1.f);
    d0 *= d0 * d0 * d0;

    int dustrp = BLOCK_SIZE_OS;
    configureLpAndHpFromTone(pitch);

    for (int i = 0; i < prefill; ++i)
    {
        float dlv[2] = {0, 0};

        switch (mode)
        {
        case burst_sine:
            d0 = 1;
        case constant_sine:
        {
            dlv[0] = (0.707 * d0 * std::sin(2.0 * M_PI * phase1));
            dlv[1] = (0.707 * d0 * std::sin(2.0 * M_PI * phase2));

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
            auto t2 = (phase2 < 0.5) ? (phase2 * 4 - 1) : ((1 - phase2)) * 4 - 1;

            dlv[0] = (0.707 * d0 * t1);
            dlv[1] = (0.707 * d0 * t2);

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
            {
                phase1 -= 1;
            }

            if (phase2 > 1)
            {
                phase2 -= 1;
            }
        }
        break;
        case burst_square:
            d0 = 1;
        case constant_square:
        {
            dlv[0] = (0.707 * d0 * ((phase1 > 0.5) * 2 - 1));
            dlv[1] = (0.707 * d0 * ((phase2 > 0.5) * 2 - 1));

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
            {
                phase1 -= 1;
            }

            if (phase2 > 1)
            {
                phase2 -= 1;
            }
        }
        break;
        case burst_ramp:
            d0 = 1;
        case constant_ramp:
        {
            dlv[0] = (0.707 * d0 * (phase1 * 2 - 1));
            dlv[1] = (0.707 * d0 * (phase2 * 2 - 1));

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
            {
                phase1 -= 1;
            }

            if (phase2 > 1)
            {
                phase2 -= 1;
            }
        }
        break;
        case burst_sweep:
            d0 = 1;
        case constant_sweep:
        {
            if (phase1 == 0)
            {
                dlv[0] = (0.707 * d0);
            }
            else
            {
                dlv[0] = (0.707 * d0 * std::sin(2.0 * M_PI / phase1));
            }

            if (phase2 == 0)
            {
                dlv[1] = (0.707 * d0);
            }
            else
            {
                dlv[1] = (0.707 * d0 * std::sin(2.0 * M_PI / phase2));
            }

            phase1 += r1;
            phase2 += r2;

            if (phase1 > 1)
            {
                phase1 -= 1;
            }

            if (phase2 > 1)
            {
                phase2 -= 1;
            }
        }
        break;

        case constant_audioin:
            dlv[0] = (0);
            dlv[1] = (0);
            break;

        case burst_pink_noise:
            d0 = 1.0;
        case constant_pink_noise:
        {
            if (dustrp >= BLOCK_SIZE_OS)
            {
                dustrp = 0;
                fillDustBuffer(pitchmult_inv, pitchmult2_inv);
            }

            dlv[0] = (d0 * dustBuffer[0][dustrp]);
            dlv[1] = (d0 * dustBuffer[1][dustrp]);

            dustrp++;
        }
        break;
        case burst_noise:
            d0 = 1;
        case constant_noise:
        default:
            dlv[0] = (d0 * (urd(gen) * 2 - 1));
            dlv[1] = (d0 * (urd(gen) * 2 - 1));
            break;
        }

        float lpt[2], hpt[2];
        lp.process_sample(dlv[0], dlv[1], lpt[0], lpt[1]);
        hp.process_sample(dlv[0], dlv[1], hpt[0], hpt[1]);

        for (int t = 0; t < 2; ++t)
        {
            delayLine[t]->write(tone.v < 0 ? lpt[t] : hpt[t]);
        }
    }

    for (int t = 0; t < 2; ++t)
    {
        priorSample[t] = delayLine[t]->buffer[(delayLine[t]->wp - 1) & delayLine[t]->comb_size];
    }

    charFilt.init(storage->getPatch().character.val.i);
}

float StringOscillator::pitchAdjustmentForStiffness()
{
    auto filter_mode = oscdata->p[str_stiffness].deform_type & StringOscillator::filter_all;
    if (!(filter_mode & StringOscillator::filter_compensate))
        return 0;

    auto tv = limit_range(localcopy[id_stiffness].f, -1.f, 1.f);
    if (tv < 0)
    {
        // I just whacked at it in a tuner at levels and came up with this. These are pitch shifts
        // so basically i ran A/69/440 into a tuner with the burst chirp and saw how far we were
        // off in frequency at 0, 25, 50 etcc then converted to notes using 12TET
        static constexpr float retunes[] = {-0.0591202, -0.122405, -0.225738, -0.406056,
                                            -0.7590243};
        float fidx = limit_range(-4 * tv, 0.f, 4.f);
        int idx = limit_range((int)fidx, 0, 3);
        float frac = fidx - idx;

        auto res = retunes[idx] * (1 - frac) + retunes[idx + 1] * frac;

        return -res;
    }
    else
    {
        // I just whacked at it in a tuner at levels and came up with this
        static constexpr float retunes[] = {0.02752047, 0.09026062, 0.31, 0.615, 0.87};
        float fidx = limit_range(4 * tv, 0.f, 4.f);
        int idx = limit_range((int)fidx, 0, 3);
        float frac = fidx - idx;

        auto res = retunes[idx] * (1 - frac) + retunes[idx + 1] * frac;

        return -res;
    }
    return 0;
}

void StringOscillator::configureLpAndHpFromTone(float pitch)
{
    auto filter_mode = oscdata->p[str_stiffness].deform_type & StringOscillator::filter_all;

    tone.newValue(limit_range(localcopy[id_stiffness].f, -1.f, 1.f));

    float clo = 10, cmidhi = 60, cmid = 100, chi = -70;
    float hpCutoff = chi;
    float lpCutoff = cmid;

    if (filter_mode & StringOscillator::filter_fixed)
    {
        if (tone.v > 0)
        {
            // OK so cool scale the HP cutoff
            auto tv = tone.v;
            hpCutoff = tv * (cmidhi - chi) + chi;
        }
        else
        {
            auto tv = -tone.v;
            lpCutoff = tv * (clo - cmid) + cmid;
        }
    }
    else
    {
        // If you change this you also need to recallibrate the tuning corrections!
        if (tone.v > 0)
        {
            // We want a smaller range but chi stays the same so cmidhi - chi = 60 + 70 = 130
            // and we want about 60% to knock us out so
            cmidhi = 10, chi = -70;
            // OK so cool scale the HP cutoff
            auto tv = tone.v;
            hpCutoff = tv * (cmidhi - chi) + chi + pitch - 60;
        }
        else
        {
            auto tv = -tone.v;
            lpCutoff = pitch - 40 * tv;
        }
    }
    // Inefficient - copy coefficients later
    lp.coeff_LP(lp.calc_omega((lpCutoff / 12.0) - 2.f) * OSC_OVERSAMPLING * getOversampleLevel(),
                0.707);
    hp.coeff_HP(hp.calc_omega((hpCutoff / 12.0) - 2.f) * OSC_OVERSAMPLING * getOversampleLevel(),
                0.707);
}

int StringOscillator::getOversampleLevel()
{
    if (oscdata->p[str_exciter_level].deform_type & StringOscillator::os_twox)
        return 2;
    return 1;
}

void StringOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepthV)
{
#define P(m)                                                                                       \
    case m:                                                                                        \
        if (FM)                                                                                    \
        {                                                                                          \
            if (oss & StringOscillator::os_onex)                                                   \
            {                                                                                      \
                process_block_internal<true, m, 1>(pitch, drift, stereo, fmdepthV);                \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                process_block_internal<true, m, 2>(pitch, drift, stereo, fmdepthV);                \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            if (oss & StringOscillator::os_onex)                                                   \
            {                                                                                      \
                process_block_internal<false, m, 1>(pitch, drift, stereo, fmdepthV);               \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                process_block_internal<false, m, 2>(pitch, drift, stereo, fmdepthV);               \
            }                                                                                      \
        }                                                                                          \
        break;

    auto mode = (exciter_modes)oscdata->p[str_exciter_mode].val.i;
    auto oss = oscdata->p[str_exciter_level].deform_type & StringOscillator::os_all;

    switch (mode)
    {
        P(burst_noise)
        P(burst_pink_noise)
        P(burst_sine)
        P(burst_tri)
        P(burst_ramp)
        P(burst_square)
        P(burst_sweep)

        P(constant_noise)
        P(constant_pink_noise)
        P(constant_sine)
        P(constant_tri)
        P(constant_ramp)
        P(constant_square)
        P(constant_sweep)

        P(constant_audioin)
    }

#undef P
}

template <bool FM, StringOscillator::exciter_modes mode, int OS>
void StringOscillator::process_block_internal(float pitch, float drift, bool stereo, float fmdepthV)
{
    auto lfodetune = drift * driftLFO[0].next();
    auto pitchadj = pitchAdjustmentForStiffness();
    auto pitch_t = std::min(148.f, pitch + lfodetune + pitchadj);
    auto pitchmult_inv =
        std::max((FIRipol_N >> 1) + 1.0,
                 storage->dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch_t));
    auto d0 = limit_range(localcopy[id_exciterlvl].f, 0.f, 1.f);

    if (mode >= constant_noise)
    {
        examp.newValue(d0 * d0 * d0 * d0);
    }
    else
    {
        if (d0 < 0.1)
        {
            // 5.6234 = powf(0.1, 0.25) * 10 to match it where powf(d0, 0.25) starts below
            examp.newValue(d0 * 5.6234);
        }
        else
        {
            examp.newValue(powf(d0, 0.25));
        }
    }

    auto dp1 = pitch_to_dphase(pitch_t);

    lfodetune = drift * driftLFO[1].next();

    double dp2;
    double pitch2_t = 1, pitchmult2_inv = 1;
    auto p2off = oscdata->p[str_str2_detune].get_extended(localcopy[id_str2detune].f);

    if (oscdata->p[str_str2_detune].absolute)
    {
        pitch2_t = std::min(148.f, pitch + pitchadj);
        auto frequency = Tunings::MIDI_0_FREQ * storage->note_to_pitch(pitch2_t);

        auto fac = oscdata->p[str_str2_detune].extend_range ? 12 * 16 : 16;
        auto detune = localcopy[id_str2detune].f * fac;

        frequency = std::max(10.0, frequency + detune);
        pitchmult2_inv = std::max(1.0, storage->dsamplerate_os / frequency);
        dp2 = frequency * storage->dsamplerate_os_inv;
    }
    else
    {
        pitch2_t = std::min(148.f, pitch + p2off + pitchadj);
        pitchmult2_inv = std::max(1.0, storage->dsamplerate_os * (1 / 8.175798915) *
                                           storage->note_to_pitch_inv(pitch2_t));
        dp2 = pitch_to_dphase(pitch2_t);
    }

    dp1 /= OS;
    dp2 /= OS;

    pitchmult_inv = std::min(pitchmult_inv, (delayLine[0]->comb_size - 100) * 1.0);
    pitchmult2_inv = std::min(pitchmult2_inv, (delayLine[0]->comb_size - 100) * 1.0);

    tap[0].newValue(pitchmult_inv);
    tap[1].newValue(pitchmult2_inv);

    t2level.newValue(0.5 * limit_range(localcopy[id_strbalance].f, -1.f, 1.f) + 0.5);

    auto fbp = limit_range(localcopy[id_str1decay].f, 0.f, 1.f);

    if (oscdata->p[str_str1_decay].extend_range)
    {
        fbp = 2 * fbp - 1;
        auto absfbp = fabs(fbp);
        auto sgn = 1.f;
        if (fbp < 0)
            sgn = -1.f;
        if (absfbp < 0.2)
        {
            // go from 0.85 to 0.95
            feedback[0].newValue(sgn * (0.85f + (0.5f * absfbp)));
        }
        else
        {
            // go from 0.95 to 1.0
            feedback[0].newValue(sgn * (0.9375f + (0.0625f * absfbp)));
        }
    }
    else
    {
        if (fbp < 0.2)
        {
            // go from 0.85 to 0.95
            feedback[0].newValue(0.85f + (0.5f * fbp));
        }
        else
        {
            // go from 0.95 to 1.0
            feedback[0].newValue(0.9375f + (0.0625f * fbp));
        }
    }

    auto fbp2 = limit_range(localcopy[id_str2decay].f, 0.f, 1.f);

    if (oscdata->p[str_str2_decay].extend_range)
    {
        fbp2 = 2 * fbp2 - 1;
        auto absfbp = fabs(fbp2);
        auto sgn = 1.f;
        if (fbp2 < 0)
            sgn = -1.f;
        if (absfbp < 0.2)
        {
            // go from 0.85 to 0.95
            feedback[1].newValue(sgn * (0.85f + (0.5f * absfbp)));
        }
        else
        {
            // go from 0.95 to 1.0
            feedback[1].newValue(sgn * (0.9375f + (0.0625f * absfbp)));
        }
    }
    else
    {
        if (fbp2 < 0.2)
        {
            feedback[1].newValue(0.85f + (0.5f * fbp2));
        }
        else
        {
            feedback[1].newValue(0.9375f + (0.0625f * fbp2));
        }
    }
    // fmdepthV basically goes 0 -> 16 so lets push it 0,1 for now
    double fv = fmdepthV / 16.0;

    fmdepth.newValue(fv);

    configureLpAndHpFromTone(pitch);

    float val[2] = {0.f, 0.f}, fbNoOutVal[2] = {0.f, 0.f}, fbv[2] = {0, 0};

    if (mode == constant_pink_noise)
    {
        fillDustBuffer(pitchmult_inv, pitchmult2_inv);
    }

    auto interp_mode = oscdata->p[str_exciter_level].deform_type & StringOscillator::interp_all;

    float *useOutL, *useOutR;

    float osOutL[BLOCK_SIZE_OS * max_oversample], osOutR[BLOCK_SIZE_OS * max_oversample];

    if (OS == 1)
    {
        useOutL = output;
        useOutR = outputR;
    }
    else
    {
        useOutL = osOutL;
        useOutR = osOutR;
    }

    for (int i = 0; i < BLOCK_SIZE_OS * OS; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            auto v = tap[t].v;
            float *phs = (t == 0) ? &phase1 : &phase2;
            float dp = (t == 0) ? dp1 : dp2;

            if (FM)
            {
                v *= Surge::DSP::fastexp(limit_range(fmdepth.v * master_osc[i] * 3, -6.f, 4.f));
            }

            v *= OS;

            switch (interp_mode)
            {
            case StringOscillator::interp_sinc:
                val[t] = delayLine[t]->read(v);
                break;
            case StringOscillator::interp_lin:
                val[t] = delayLine[t]->readLinear(v);
                break;
            case StringOscillator::interp_zoh:
                val[t] = delayLine[t]->readZOH(v);
                break;
            }

            fbNoOutVal[t] = 0.f;

            // Add continuous excitation
            switch (mode)
            {
            case constant_noise:
            {
                val[t] += examp.v * (urd(gen) * 2 - 1);
            }
            break;
            case constant_pink_noise:
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
                *phs -= (*phs > 1);
            }
            break;
            case constant_tri:
            {
                auto rn = 0.707 * ((*phs < 0.5) ? (*phs * 4 - 1) : ((1 - *phs) * 4 - 1));

                val[t] += examp.v * rn;
                *phs += dp;
                *phs -= (*phs > 1);
            }
            break;
            case constant_sweep:
            {
                float sv = 1.0;

                if (*phs != 0)
                {
                    sv = std::sin(2.0 * M_PI / *phs);
                }

                val[t] += examp.v * 0.707 * sv;
                *phs += dp;
                *phs -= (*phs > 1);
            }
            break;
            case constant_sine:
            {
                float sv = std::sin(2.0 * M_PI * *phs);

                val[t] += examp.v * 0.707 * sv;
                *phs += dp;
                *phs -= (*phs > 1);
            }
            break;
            case constant_square:
            {
                auto rn = 0.707 * ((*phs > 0.5) ? 1 : -1);

                val[t] += examp.v * rn;
                *phs += dp;
                *phs -= (*phs > 1);
            }
            break;
            case constant_audioin:
            {
                // in OS case on audio in do a simple ZOH for now
                if (OS == 1)
                    fbNoOutVal[t] = examp.v * storage->audio_in[t][i];
                if (OS == 2)
                    fbNoOutVal[t] = examp.v * storage->audio_in[t][i / 2];
            }
            break;
            default:
                // We should do something else with amplitude here
                val[t] *= examp.v;
                break;
            }

            // precautionary hard clip
            fbv[t] = limit_range(val[t] + fbNoOutVal[t], -1.f, 1.f);
        }

        float lpv[2], hpv[2];
        lp.process_sample(fbv[0], fbv[1], lpv[0], lpv[1]);
        hp.process_sample(fbv[0], fbv[1], hpv[0], hpv[1]);

        for (int t = 0; t < 2; ++t)
        {
            auto filtv = (tone.v > 0) ? hpv[t] : lpv[t];

            if (fabs(filtv) < 1e-16)
                filtv = 0;
            delayLine[t]->write(filtv * feedback[t].v);
        }

        float out = val[0] + t2level.v * (val[1] - val[0]);

        // softclip the output
        out = out * (1.5 - 0.5 * out * out);

        tap[0].process();
        tap[1].process();
        t2level.process();
        feedback[0].process();
        feedback[1].process();
        tone.process();
        examp.process();
        fmdepth.process();

        useOutL[i] = out;
        useOutR[i] = out;
    }

    if (OS == 2)
    {
        halfband.process_block_D2(useOutL, useOutR, BLOCK_SIZE_OS * OS, output, outputR);
    }

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

void StringOscillator::init_ctrltypes()
{
    oscdata->p[str_exciter_mode].set_name("Exciter");
    oscdata->p[str_exciter_mode].set_type(ct_stringosc_excitation_model);

    oscdata->p[str_exciter_level].set_name("Exciter Level");
    oscdata->p[str_exciter_level].set_type(ct_percent_with_string_deform_hook);
    oscdata->p[str_exciter_level].val_default.f = 1.f;

    oscdata->p[str_str1_decay].set_name("String 1 Decay");
    oscdata->p[str_str1_decay].set_type(ct_percent_with_extend_to_bipolar);
    oscdata->p[str_str1_decay].val_default.f = 0.95;

    oscdata->p[str_str2_decay].set_name("String 2 Decay");
    oscdata->p[str_str2_decay].set_type(ct_percent_with_extend_to_bipolar);
    oscdata->p[str_str2_decay].val_default.f = 0.95;

    oscdata->p[str_str2_detune].set_name("String 2 Detune");
    oscdata->p[str_str2_detune].set_type(ct_oscspread_bipolar);

    oscdata->p[str_str_balance].set_name("String Balance");
    oscdata->p[str_str_balance].set_type(ct_percent_bipolar_stringbal);

    oscdata->p[str_stiffness].set_name("Stiffness");
    oscdata->p[str_stiffness].set_type(ct_percent_bipolar_with_string_filter_hook);
}

void StringOscillator::init_default_values()
{
    oscdata->p[str_exciter_mode].val.i = 0;
    oscdata->p[str_exciter_level].val.f = 1.f;
    oscdata->p[str_exciter_level].deform_type = os_onex | interp_sinc;

    oscdata->p[str_str1_decay].val.f = 0.95f;
    oscdata->p[str_str1_decay].set_extend_range(false);
    oscdata->p[str_str2_decay].val.f = 0.95f;
    oscdata->p[str_str2_decay].set_extend_range(false);

    oscdata->p[str_str2_detune].val.f = 0.1f;
    oscdata->p[str_str2_detune].set_extend_range(false);
    oscdata->p[str_str_balance].val.f = 0.f;

    oscdata->p[str_stiffness].val.f = 0.f;
    oscdata->p[str_stiffness].deform_type = filter_fixed;
}

void StringOscillator::fillDustBuffer(float tap0, float tap1)
{
    for (int i = 0; i < BLOCK_SIZE_OS * getOversampleLevel(); ++i)
    {
        auto v0 = urd(gen) * 2 - 1;
        auto v1 = urd(gen) * 2 - 1;
        noiseLp.process_sample_nolag(v0, v1);
        dustBuffer[0][i] = v0 * 1.7;
        dustBuffer[1][i] = v1 * 1.7;
    }
}
void StringOscillator::handleStreamingMismatches(int streamingRevision,
                                                 int currentSynthStreamingRevision)
{
    if (streamingRevision < 19)
    {
        oscdata->p[str_exciter_level].deform_type = os_onex | interp_sinc;
        oscdata->p[str_stiffness].deform_type = filter_fixed;
        oscdata->p[str_str1_decay].set_extend_range(false);
        oscdata->p[str_str2_decay].set_extend_range(false);
    }
}
