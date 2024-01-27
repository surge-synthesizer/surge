/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#include "LFOModulationSource.h"
#include <cmath>
#include "DebugHelpers.h"
#include "MSEGModulationHelper.h"

#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "sst/basic-blocks/dsp/Interpolators.h"

namespace sdsp = sst::basic_blocks::dsp;

using namespace std;

LFOModulationSource::LFOModulationSource() { Surge::Formula::initEvaluatorState(formulastate); }
LFOModulationSource::~LFOModulationSource() { Surge::Formula::cleanEvaluatorState(formulastate); }

void LFOModulationSource::assign(SurgeStorage *storage, LFOStorage *lfo, pdata *localcopy,
                                 SurgeVoiceState *state, StepSequencerStorage *ss, MSEGStorage *ms,
                                 FormulaModulatorStorage *fs, bool is_display)
{
    this->storage = storage;
    this->lfo = lfo;
    this->state = state;
    this->localcopy = localcopy;
    this->ss = ss;
    this->ms = ms;
    this->fs = fs;
    this->is_display = is_display;

    Surge::Formula::cleanEvaluatorState(formulastate);
    if (is_display)
        msegstate = Surge::MSEG::EvaluatorState();

    iout = 0;
    output = 0;
    step = 0;
    env_state = lfoeg_stuck; // in the case we process this without an attack, we want to be 'done'
    env_val = 0.f;
    env_phase = 0;
    shuffle_id = 0;
    ratemult = 1.f;
    retrigger_FEG = false;
    retrigger_AEG = false;
    priorPhase = -1000;

    rate = lfo->rate.param_id_in_scene;
    magn = lfo->magnitude.param_id_in_scene;
    idelay = lfo->delay.param_id_in_scene;
    iattack = lfo->attack.param_id_in_scene;
    idecay = lfo->decay.param_id_in_scene;
    ihold = lfo->hold.param_id_in_scene;
    isustain = lfo->sustain.param_id_in_scene;
    irelease = lfo->release.param_id_in_scene;
    startphase = lfo->start_phase.param_id_in_scene;
    ideform = lfo->deform.param_id_in_scene;

    phaseInitialized = false;

    if (is_display)
    {
        gen = std::default_random_engine();
        gen.seed(46);
        distro = std::uniform_real_distribution<float>(-1.f, 1.f);
        urng = [this]() -> float { return distro(gen); };

        // this number is different than the one in the canvas on purpose
        // so since they are random the displays differ
        msegstate.seed(2112);
    }
    else
    {
        gen = std::default_random_engine();
        gen.seed(storage->rand_u32());
        distro = std::uniform_real_distribution<float>(-1.f, 1.f);
        urng = [this]() -> float { return distro(gen); };
    }

    noise = 0.f;
    noised1 = 0.f;
    target = 0.f;

    for (int i = 0; i < 4; i++)
    {
        wf_history[i] = 0.f;
    }

    for (int i = 0; i < 3; ++i)
        onepoleState[i] = 0.f;
}

float LFOModulationSource::bend1(float x)
{
    float a = 0.5f * limit_range(localcopy[ideform].f, -3.f, 3.f);

    // do twice for extra pleasure
    x = x - a * x * x + a;
    x = x - a * x * x + a;

    return x;
}

float LFOModulationSource::bend2(float x)
{
    float a = localcopy[ideform].f;

    x += 4.5 * a * sin(x * 2.f * M_PI) / (2.f * M_PI);

    return x;
}

float LFOModulationSource::bend3(float x)
{
    float a = localcopy[ideform].f;

    x += 0.25;
    x += 4.5 * a * sin(x * 2.f * M_PI) / (2.f * M_PI);
    x -= 0.25 + (localcopy[ideform].f / (2.f * M_PI));

    return x;
}

void LFOModulationSource::msegEnvelopePhaseAdjustment()
{
    // If we have an envelope MSEG length above 1 we want phase to span the duration
    if (lfo->shape.val.i == lt_mseg && ms->editMode == MSEGStorage::ENVELOPE &&
        ms->totalDuration > 1.0)
    {
        // extend the phase
        phase *= ms->totalDuration;
        double epi, epd;
        epd = modf(phase, &epi);
        phase = epd;
        unwrappedphase_intpart = epi;
    }
}

void LFOModulationSource::initPhaseFromStartPhase()
{
    phase = localcopy[startphase].f;
    phaseInitialized = true;

    if (lfo->shape.val.i == lt_tri && lfo->rate.deactivated && !lfo->unipolar.val.b)
    {
        phase += 0.25;
    }

    while (phase < 0.f)
    {
        phase += 1.f;
    }

    while (phase >= 1.f)
    {
        phase -= 1.f;
    }

    unwrappedphase_intpart = 0;

    msegEnvelopePhaseAdjustment();
}

void LFOModulationSource::attackFrom(float start)
{
    // For VLFO you don't need this but SLFO get recycled, so you do
    if (!is_display)
        msegstate = Surge::MSEG::EvaluatorState();

    if (!phaseInitialized)
    {
        initPhaseFromStartPhase();
    }

    env_state = lfoeg_delay;
    envelopeStart = start;

    env_val = 0.f;
    env_phase = 0;
    ratemult = 1.f;
    bool isFirstAttack = !everAttacked;
    everAttacked = true;

    if (localcopy[idelay].f == lfo->delay.val_min.f)
    {
        env_state = lfoeg_attack;

        if (localcopy[iattack].f == lfo->attack.val_min.f)
        {
            env_state = lfoeg_hold;
            env_val = 1.f;

            if (localcopy[ihold].f == lfo->hold.val_min.f)
            {
                env_state = lfoeg_decay;
            }
        }
    }

    if (is_display)
    {
        phase = lfo->start_phase.val.f;

        if (lfo->shape.val.i == lt_stepseq)
        {
            phase = 0.f;
        }

        step = 0;

        msegEnvelopePhaseAdjustment();
    }
    else
    {
        float phaseslider;

        // Use Phase as shuffle-parameter instead
        if (lfo->shape.val.i == lt_stepseq)
        {
            phaseslider = 0.f;
        }
        else
        {
            phaseslider = localcopy[startphase].f;
        }

        // With modulation the phaseslider can be outside [0, 1), as in #1524
        while (phaseslider < 0.f)
        {
            phaseslider += 1.f;
        }

        while (phaseslider >= 1.f)
        {
            phaseslider -= 1.f;
        }

        switch (lfo->trigmode.val.i)
        {
        case lm_keytrigger:
            phase = phaseslider;
            unwrappedphase_intpart = 0;

            step = 0;

            msegEnvelopePhaseAdjustment();

            break;
        case lm_random:
            phase = storage->rand_01();
            unwrappedphase_intpart = 0;

            msegEnvelopePhaseAdjustment();

            if (ss->loop_end == 0)
            {
                step = 0;
            }
            else
            {
                step = (storage->rand() % ss->loop_end) & (n_stepseqsteps - 1);
            }

            break;
        case lm_freerun:
        {
            double ipart;

            // Get our rate from the parameter and temposync
            float lrate = pow(2.0, (double)localcopy[rate].f);
            if (lfo->rate.temposync)
                lrate *= storage->temposyncratio;

            // Get our time from songpos. So songpos is in beats, which means
            // (songpos / tempo / 60) is the time. But temposyncratio is tempo / 120
            // so (songpos / (2 * temposyncratio)) is the time
            double timePassed = storage->songpos * storage->temposyncratio_inv * 0.5;

            // And so the total phase is timePassed * rate + phase0
            auto startPhase = phaseslider;

            if (lfo->shape.val.i == lt_mseg && ms->editMode == MSEGStorage::ENVELOPE &&
                ms->totalDuration > 1.0)
            {
                // extend the phase
                startPhase *= ms->totalDuration;
            }

            float totalPhase = startPhase + timePassed * lrate;

            // Mod that for phase; and get the step also by step length
            phase = (float)modf(totalPhase, &ipart);
            unwrappedphase_intpart = ipart;

            int i = (int)ipart;
            step = (i % max(1, (ss->loop_end - ss->loop_start + 1))) + ss->loop_start;
        }
        break;

        default:
            step = 0;
            phase = 0;
            unwrappedphase_intpart = 0;

            break;
        };
    }

    switch (lfo->shape.val.i)
    {
    case lt_snh:
        if (isFirstAttack || lfo->trigmode.val.i != lm_freerun)
        {
            noise = 0.f;
            noised1 = 0.f;
            target = 0.f;

            if (lfo->deform.deform_type == type_2)
            {
                wf_history[3] =
                    sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, 0.f, urng);
                wf_history[2] =
                    sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, 0.f, urng);
                wf_history[1] =
                    sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, 0.f, urng);
                wf_history[0] =
                    sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, 0.f, urng);

                iout = sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, 0.f, urng);
            }
            else
            {
                /*
                 * The SNH uses correlated noise with prior state. It used to be the
                 * attack always used prior state zero and only randomized once so
                 * the first value of SNH LFO was constant. This little loop fixes that.
                 */
                for (int i = 0; i < 3; ++i)
                    iout = sdsp::correlated_noise_o2mk2_suppliedrng(
                        target, noised1, limit_range(localcopy[ideform].f, -1.f, 1.f), urng);
            }
        }

        break;
    case lt_stepseq:
    {
        // fire up the engines
        wf_history[1] = ss->steps[step & (n_stepseqsteps - 1)];
        wf_history[2] = ss->steps[(step + n_stepseqsteps - 1) & (n_stepseqsteps - 1)];
        wf_history[3] = ss->steps[(step + n_stepseqsteps - 2) & (n_stepseqsteps - 1)];

        step++;

        if (ss->loop_end >= ss->loop_start)
        {
            if (step > ss->loop_end)
            {
                step = ss->loop_start;
            }
        }
        else
        {
            if (step >= ss->loop_start)
            {
                step = ss->loop_end + 1;
            }
        }

        // make sure we don't blow out ratemult with modulation
        auto shuffle_val =
            limit_range(lfo->start_phase.get_extended(localcopy[startphase].f), -1.99f, 1.99f);

        shuffle_id = (shuffle_id + 1) & 1;

        if (shuffle_id)
        {
            ratemult = 1.f / (1.f - 0.5f * shuffle_val);
        }
        else
        {
            ratemult = 1.f / (1.f + 0.5f * shuffle_val);
        }

        wf_history[0] = ss->steps[step & (n_stepseqsteps - 1)];
    }
    break;

    case lt_noise:
    {
        if (isFirstAttack || lfo->trigmode.val.i != lm_freerun)
        {
            auto lid = limit_range(localcopy[ideform].f, -1.f, 1.f);

            noise = 0.f;
            noised1 = 0.f;
            target = 0.f;

            /*
             * See the above comment in S&H; with noised1 set to 0, there isn't enough
             * randomness in wf_history[3] and so the outset cubic interpolation gets
             * less degrees of randomness than expected, so move us along a bit.
             *
             * This matters less than with S&H because of the (peculiar) behaviour
             * of the phase here. If phase is zero we always start at the same value
             * not a random value.
             */
            for (int i = 0; i < 3; ++i)
                wf_history[3] =
                    sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, lid, urng) * phase;
            wf_history[2] =
                sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, lid, urng) * phase;
            wf_history[1] =
                sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, lid, urng) * phase;
            wf_history[0] =
                sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, lid, urng) * phase;

            phase = 0.f;
        }
    }
    break;

    case lt_tri:
    {
        if (!lfo->unipolar.val.b)
        {
            phase += 0.25;

            if (phase >= 1.f)
            {
                phase -= 1.f;
                unwrappedphase_intpart++;
            }
        }
    }
    break;

    case lt_sine:
    {
        if (lfo->unipolar.val.b)
        {
            phase += 0.75;
            if (phase >= 1.f)
            {
                phase -= 1.f;
                unwrappedphase_intpart++;
            }
        }
    }
    break;

    case lt_formula:
    {
        formulastate.released = false;

        formulastate.del = lfo->delay.value_to_normalized(localcopy[idelay].f);
        formulastate.a = lfo->attack.value_to_normalized(localcopy[iattack].f);
        formulastate.h = lfo->hold.value_to_normalized(localcopy[ihold].f);
        formulastate.dec = lfo->decay.value_to_normalized(localcopy[idecay].f);
        formulastate.s = lfo->sustain.value_to_normalized(localcopy[isustain].f);
        formulastate.r = lfo->release.value_to_normalized(localcopy[irelease].f);

        formulastate.rate = localcopy[rate].f;
        formulastate.amp = localcopy[magn].f;
        formulastate.phase = localcopy[startphase].f;
        formulastate.deform = localcopy[ideform].f;
        formulastate.tempo = storage->temposyncratio * 120.0;
        formulastate.songpos = storage->songpos;

        formulastate.isVoice = isVoice;

        Surge::Formula::prepareForEvaluation(storage, fs, formulastate, is_display);
    }
    break;
    }
}

void LFOModulationSource::release()
{
    if (lfo->release.val.f < lfo->release.val_max.f)
    {
        env_releasestart = env_val;
        env_phase = 0;
        // If we release during the attack and are scaling the envelope we need
        // to scale our release start point
        if (envRetrigMode == FROM_LAST && envelopeStart != 0 &&
            (env_state == lfoeg_attack || env_state == lfoeg_delay))
        {
            env_releasestart = env_releasestart * (1.0 - envelopeStart) + envelopeStart;
        }
        env_state = lfoeg_release;
    }
    else if (lfo->shape.val.i == lt_mseg || lfo->shape.val.i == lt_formula)
    {
        env_state = lfoeg_msegrelease;
    }
}

void LFOModulationSource::retriggerEnvelopeFrom(float start)
{
    env_state = lfoeg_delay;
    env_phase = 0;
    envelopeStart = start;
    if (localcopy[idelay].f == lfo->delay.val_min.f)
    {
        env_state = lfoeg_attack;

        if (localcopy[iattack].f == lfo->attack.val_min.f)
        {
            env_state = lfoeg_hold;
            env_val = 1.f;

            if (localcopy[ihold].f == lfo->hold.val_min.f)
            {
                env_state = lfoeg_decay;
            }
        }
    }
}

void LFOModulationSource::process_block()
{
    if ((!phaseInitialized) || (lfo->trigmode.val.i == lm_keytrigger && lfo->rate.deactivated))
    {
        initPhaseFromStartPhase();
    }

    retrigger_FEG = false;
    retrigger_AEG = false;

    int s = lfo->shape.val.i;
    float frate = 0;

    if (!lfo->rate.temposync)
    {
        frate = storage->envelope_rate_linear_nowrap(-localcopy[rate].f);
    }
    else
    {
        /*
        ** The approximation above drifts quite a lot especially on things like
        *  dotted-quarters-vs-beat-at-not-120 BPM
        ** See #2675 for sample patches. So do the calculation exactly.
        **
        ** envrate is blocksize / samplerate 2^-x
        ** so let's just do that
        */
        frate = (double)BLOCK_SIZE_OS * storage->dsamplerate_os_inv *
                pow(2.0, localcopy[rate].f); // since x = -localcopy, -x == localcopy
    }

    if (lfo->rate.deactivated)
    {
        frate = 0.0;
    }

    if (lfo->rate.temposync)
    {
        frate *= storage->temposyncratio;
    }

    phase += frate * ratemult;

    if (frate == 0 && phase == 0 && s == lt_stepseq)
    {
        phase = 0.001; // step forward a smidge
    }

    if (env_state != lfoeg_stuck && env_state != lfoeg_msegrelease)
    {
        float envrate = 0;

        switch (env_state)
        {
        case lfoeg_delay:
            envrate = storage->envelope_rate_linear_nowrap(localcopy[idelay].f);

            if (lfo->delay.temposync)
            {
                envrate *= storage->temposyncratio;
            }

            break;
        case lfoeg_attack:
            envrate = storage->envelope_rate_linear_nowrap(localcopy[iattack].f);

            if (lfo->attack.temposync)
            {
                envrate *= storage->temposyncratio;
            }

            break;
        case lfoeg_hold:
            envrate = storage->envelope_rate_linear_nowrap(localcopy[ihold].f);

            if (lfo->hold.temposync)
            {
                envrate *= storage->temposyncratio;
            }

            break;
        case lfoeg_decay:
            envrate = storage->envelope_rate_linear_nowrap(localcopy[idecay].f);

            if (lfo->decay.temposync)
            {
                envrate *= storage->temposyncratio;
            }

            break;
        case lfoeg_release:
            envrate = storage->envelope_rate_linear_nowrap(localcopy[irelease].f);

            if (lfo->release.temposync)
            {
                envrate *= storage->temposyncratio;
            }

            break;
        };

        env_phase += envrate;

        float sustainlevel = localcopy[isustain].f;

        if (env_phase > 1.f)
        {
            switch (env_state)
            {
            case lfoeg_delay:
                env_state = lfoeg_attack;
                env_phase = 0.f;
                break;
            case lfoeg_attack:
                env_state = lfoeg_hold;
                env_phase = 0.f;
                break;
            case lfoeg_hold:
                env_state = lfoeg_decay;
                env_phase = 0.f;
                break;
            case lfoeg_decay:
                env_state = lfoeg_stuck;
                env_phase = 0;
                env_val = sustainlevel;
                break;
            case lfoeg_release:
                env_state = lfoeg_stuck;
                env_phase = 0;
                env_val = 0.f;
                break;
            };
        }

        switch (env_state)
        {
        case lfoeg_delay:
            env_val = 0.f;
            break;
        case lfoeg_attack:
            env_val = env_phase;
            break;
        case lfoeg_hold:
            env_val = 1.f;
            break;
        case lfoeg_decay:
            env_val = (1.f - env_phase) + env_phase * sustainlevel;
            break;
        case lfoeg_release:
            env_val = (1.f - env_phase) * env_releasestart;
            break;
        };
    }

    if (phase >= 1 || phase < 0)
    {
        if (phase >= 2)
        {
            float ipart;

            phase = modf(phase, &ipart);
            unwrappedphase_intpart += ipart;
        }
        else if (phase < 0)
        {
            // -6.02 needs to go to .98
            int p = (int)phase - 1;
            float np = -p + phase;

            if (np >= 0 && np < 1)
            {
                phase = np;
                unwrappedphase_intpart += p;
            }
            else
            {
                // should never get here but something is already weird with the mod stack
                phase = 0;
            }
        }
        else
        {
            phase -= 1;
            unwrappedphase_intpart++;
        }

        switch (s)
        {
        case lt_snh:
        {
            if (lfo->deform.deform_type == type_2)
            {
                wf_history[3] = wf_history[2];
                wf_history[2] = wf_history[1];
                wf_history[1] = wf_history[0];

                wf_history[0] =
                    sdsp::correlated_noise_o2mk2_suppliedrng(target, noised1, 0.f, urng);
            }
            else
            {
                iout = sdsp::correlated_noise_o2mk2_suppliedrng(
                    target, noised1, limit_range(localcopy[ideform].f, -1.f, 1.f), urng);
            }
        }
        break;

        case lt_noise:
        {
            wf_history[3] = wf_history[2];
            wf_history[2] = wf_history[1];
            wf_history[1] = wf_history[0];

            wf_history[0] = sdsp::correlated_noise_o2mk2_suppliedrng(
                target, noised1, limit_range(localcopy[ideform].f, -1.f, 1.f), urng);
        }
        break;

        case lt_stepseq:
            // you might thing we don't need this and technically we don't
            // but I wanted to keep it here to retain compatibility with
            // versions of trigmask which were streamed in older sessions
            if (ss->trigmask & (UINT64_C(1) << step))
            {
                retrigger_FEG = true;
                retrigger_AEG = true;
            }

            if (ss->trigmask & (UINT64_C(1) << (16 + step)))
            {
                retrigger_FEG = true;
            }

            if (ss->trigmask & (UINT64_C(1) << (32 + step)))
            {
                retrigger_AEG = true;
            }

            step++;

            // make sure we don't blow out ratemult with modulation
            auto shuffle_val =
                limit_range(lfo->start_phase.get_extended(localcopy[startphase].f), -1.99f, 1.99f);

            shuffle_id = (shuffle_id + 1) & 1;

            if (shuffle_id)
            {
                ratemult = 1.f / (1.f - 0.5f * shuffle_val);
            }
            else
            {
                ratemult = 1.f / (1.f + 0.5f * shuffle_val);
            }

            if (ss->loop_end >= ss->loop_start)
            {
                if (step > ss->loop_end)
                {
                    step = ss->loop_start;
                }
            }
            else
            {
                if (step >= ss->loop_start)
                {
                    step = ss->loop_end + 1;
                }
            }

            wf_history[3] = wf_history[2];
            wf_history[2] = wf_history[1];
            wf_history[1] = wf_history[0];
            wf_history[0] = ss->steps[step & (n_stepseqsteps - 1)];

            break;
        };
    }

    float useenvval = env_val;

    if (lfo->delay.deactivated)
    {
        useenvval = 1.0; // constant envelope at 1
    }

    switch (s)
    {
    case lt_envelope:
    {
        switch (lfo->deform.deform_type)
        {
        case type_1:
            iout = (1.f - localcopy[ideform].f) + localcopy[ideform].f * env_val;

            break;

        case type_2:
            if (localcopy[ideform].f > 0.f)
            {
                iout = pow(env_val, 1.f + (9.0 * localcopy[ideform].f));
            }
            else
            {
                iout = pow(env_val, 1.f / (1.f + (4.0 * fabs(localcopy[ideform].f))));
            }

            if (env_val != 0.f)
            {
                iout /= env_val;
            }

            break;

        case type_3:

            if (localcopy[ideform].f < 0.f)
            {
                iout = env_val + (sdsp::correlated_noise_o2mk2_suppliedrng(
                                      target, noised1, 1.f - fabs(localcopy[ideform].f), urng) *
                                  0.2);
            }
            else
            {
                auto quant = localcopy[ideform].f * 24.f;

                if (quant > 1.f)
                {
                    iout = (float)(round(env_val * quant) / quant);
                }
                else
                {
                    iout = env_val;
                }
            }

            if (env_val != 0.f)
            {
                iout /= env_val;
            }

            break;
        }

        break;
    }

    case lt_sine:
    {
        constexpr auto wst_sine = sst::waveshapers::WaveshaperType::wst_sine;
        switch (lfo->deform.deform_type)
        {
        case type_1:
            iout = bend1(storage->lookup_waveshape_warp(wst_sine, 2.f - 4.f * phase));

            break;

        case type_2:
            if (localcopy[ideform].f >= -1 / 4.5)
            {
                iout = bend2(storage->lookup_waveshape_warp(wst_sine, 2.f - 4.f * phase));
            }
            else
            {
                iout = bend2(storage->lookup_waveshape_warp(wst_sine, 2.f - 4.f * phase)) /
                       (1 - ((localcopy[ideform].f + (1 / 4.5)) / 1.6));
            }

            break;

        case type_3:
            iout = (bend3(storage->lookup_waveshape_warp(wst_sine, 2.f - 4.f * phase)) /
                        (1.f + 0.5 * abs(localcopy[ideform].f)) -
                    (0.06 * localcopy[ideform].f));

            break;
        }

        break;
    }

    case lt_tri:
    {
        switch (lfo->deform.deform_type)
        {
        case type_1:
            iout = bend1(-1.f + 4.f * ((phase > 0.5) ? (1 - phase) : phase));

            break;

        case type_2:
            if (localcopy[ideform].f >= -1 / 4.5)
            {
                iout = bend2(-1.f + 4.f * ((phase > 0.5) ? (1 - phase) : phase));
            }
            else
            {
                iout = bend2(-1.f + 4.f * ((phase > 0.5) ? (1 - phase) : phase)) /
                       (1 - ((localcopy[ideform].f + (1 / 4.5)) / 1.6));
            }

            break;

        case type_3:
            iout = (bend3(-1.f + 4.f * ((phase > 0.5) ? (1 - phase) : phase)) /
                        (1.f + 0.5 * abs(localcopy[ideform].f)) -
                    (0.06 * localcopy[ideform].f));

            break;
        }

        break;
    }

    case lt_ramp:
    {
        switch (lfo->deform.deform_type)
        {
        case type_1:
            iout = bend1(1.f - 2.f * phase);

            break;

        case type_2:
            if (localcopy[ideform].f >= -1 / 4.5)
            {
                iout = bend2(1.f - 2.f * phase);
            }
            else
            {
                iout = bend2(1.f - 2.f * phase) / (1 - ((localcopy[ideform].f + (1 / 4.5)) / 1.6));
            }

            break;

        case type_3:;
            iout = (bend3(1.f - 2.f * phase) / (1.f + 0.5 * abs(localcopy[ideform].f)) -
                    (0.06 * localcopy[ideform].f));

            break;
        }

        break;
    }

    case lt_square:
    {
        iout = (phase > (0.5f + 0.5f * localcopy[ideform].f)) ? -1.f : 1.f;

        break;
    }

    case lt_snh:
    {
        if (lfo->deform.deform_type == type_2)
        {
            float df = localcopy[ideform].f;

            if (df > 0.5f)
            {
                float linear = (1.f - phase) * wf_history[2] + phase * wf_history[1];
                float cubic = sdsp::cubic_ipol(wf_history[3], wf_history[2], wf_history[1],
                                               wf_history[0], phase);

                iout = (2.f - 2.f * df) * linear + (2.f * df - 1.0f) * cubic;
            }
            else if (df > -0.0001f)
            {
                float cf = max(0.f, min(phase / (2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * wf_history[2] + cf * wf_history[1];
            }
            else if (df > -0.5f)
            {

                float cf = max(0.f, min((1.f - phase) / (-2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * 0 + cf * wf_history[1];
            }
            else
            {
                float cf = max(0.f, min(phase / (2.f + 2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * wf_history[1] + cf * 0;
            }
        }

        break;
    }

    case lt_noise:
    {
        iout = sdsp::cubic_ipol(wf_history[3], wf_history[2], wf_history[1], wf_history[0], phase);

        break;
    }

    case lt_stepseq:
    {
        // support 0 rate scrubbing across all 16 steps
        float calcPhase = phase;

        if (frate == 0)
        {
            // in 0 rate mode we want to scrub through the entire sequence. so:
            float p16 = phase * n_stepseqsteps;
            int pstep = ((int)p16) & (n_stepseqsteps - 1);
            float sphase = (p16 - pstep);

            // pstep is now between 0 and ns-1. But if that is beyond the end we want to wrap
            int last_step = std::max(ss->loop_end, ss->loop_start);
            int loop_len = std::max(1, abs(ss->loop_end - ss->loop_start) + 1);

            while (pstep > last_step && pstep >= 0)
            {
                pstep -= loop_len;
            }

            pstep = pstep & (n_stepseqsteps - 1);

            if (pstep != priorStep)
            {
                priorStep = pstep;

                if (ss->trigmask & (UINT64_C(1) << pstep))
                {
                    retrigger_FEG = true;
                    retrigger_AEG = true;
                }

                if (ss->trigmask & (UINT64_C(1) << (16 + pstep)))
                {
                    retrigger_FEG = true;
                }

                if (ss->trigmask & (UINT64_C(1) << (32 + pstep)))
                {
                    retrigger_AEG = true;
                }
            }

            if (priorPhase != phase)
            {
                priorPhase = phase;

                // So we want to load up the wf_history
                for (int i = 0; i < 4; ++i)
                {
                    wf_history[i] =
                        ss->steps[(pstep + 1 + n_stepseqsteps - i) & (n_stepseqsteps - 1)];
                }
            }

            calcPhase = sphase;
        }

        float df = localcopy[ideform].f;

        // LFO shape parameter from Shortcircuit XT!
        if (lfo->deform.deform_type == type_2)
        {
            if (df > 0.5f)
            {
                float linear;

                if (calcPhase > 0.5f)
                {
                    float ph = calcPhase - 0.5f;

                    linear = (1.f - ph) * wf_history[1] + ph * wf_history[0];
                }
                else
                {
                    float ph = calcPhase + 0.5f;

                    linear = (1.f - ph) * wf_history[2] + ph * wf_history[1];
                }

                float qbs =
                    sdsp::quad_bspline(wf_history[2], wf_history[1], wf_history[0], calcPhase);

                iout = (2.f - 2.f * df) * linear + (2.f * df - 1.0f) * qbs;
            }
            else if (df > -0.0001f)
            {
                if (calcPhase > 0.5f)
                {
                    float cf = 0.5f - (calcPhase - 1.f) / (2.f * df + 0.00001f);

                    cf = max(0.f, min(cf, 1.0f));
                    iout = (1.f - cf) * wf_history[0] + cf * wf_history[1];
                }
                else
                {
                    float cf = 0.5f - (calcPhase) / (2.f * df + 0.00001f);

                    cf = max(0.f, min(cf, 1.0f));
                    iout = (1.f - cf) * wf_history[1] + cf * wf_history[2];
                }
            }
            else if (df > -0.5f)
            {
                float cf = max(0.f, min((1.f - calcPhase) / (-2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * 0 + cf * wf_history[1];
            }
            else
            {
                float cf = max(0.f, min(calcPhase / (2.f + 2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * wf_history[1] + cf * 0;
            }
        }
        else
        {
            if (df > 0.5f)
            {
                float linear = (1.f - calcPhase) * wf_history[2] + calcPhase * wf_history[1];
                float cubic = sdsp::cubic_ipol(wf_history[3], wf_history[2], wf_history[1],
                                               wf_history[0], calcPhase);

                iout = (2.f - 2.f * df) * linear + (2.f * df - 1.0f) * cubic;
            }
            else if (df > -0.0001f)
            {
                float cf = max(0.f, min(calcPhase / (2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * wf_history[2] + cf * wf_history[1];
            }
            else if (df > -0.5f)
            {
                float cf = max(0.f, min((1.f - calcPhase) / (-2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * 0 + cf * wf_history[1];
            }
            else
            {
                float cf = max(0.f, min(calcPhase / (2.f + 2.f * df + 0.00001f), 1.0f));

                iout = (1.f - cf) * wf_history[1] + cf * 0;
            }
        }

        break;
    }

    case lt_mseg:
    {
        msegstate.released = (env_state == lfoeg_release || env_state == lfoeg_msegrelease);

        iout = Surge::MSEG::valueAt(unwrappedphase_intpart, phase, localcopy[ideform].f, ms,
                                    &msegstate);

        retrigger_FEG = msegstate.retrigger_FEG;
        retrigger_AEG = msegstate.retrigger_AEG;

        break;
    }

    case lt_formula:
    {
        formulastate.released = (env_state == lfoeg_release || env_state == lfoeg_msegrelease);

        formulastate.del = lfo->delay.value_to_normalized(localcopy[idelay].f);
        formulastate.a = lfo->attack.value_to_normalized(localcopy[iattack].f);
        formulastate.h = lfo->hold.value_to_normalized(localcopy[ihold].f);
        formulastate.dec = lfo->decay.value_to_normalized(localcopy[idecay].f);
        formulastate.s = lfo->sustain.value_to_normalized(localcopy[isustain].f);
        formulastate.r = lfo->release.value_to_normalized(localcopy[irelease].f);

        formulastate.rate = localcopy[rate].f;
        formulastate.amp = localcopy[magn].f;
        formulastate.phase = localcopy[startphase].f;
        formulastate.deform = localcopy[ideform].f;
        formulastate.tempo = storage->temposyncratio * 120.0;
        formulastate.songpos = storage->songpos;

        formulastate.isVoice = isVoice;

        float tmpout[Surge::Formula::max_formula_outputs] = {0, 0, 0, 0, 0, 0, 0, 0};

        Surge::Formula::valueAt(unwrappedphase_intpart, phase, storage, fs, &formulastate, tmpout);

        if (!formulastate.useEnvelope)
        {
            useenvval = 1.0;
        }

        retrigger_AEG = formulastate.retrigger_AEG;
        retrigger_FEG = formulastate.retrigger_FEG;

        if (formulastate.raisedError)
        {
            auto em = *formulastate.error;
            formulastate.error.reset();
            formulastate.raisedError = false;
            storage->reportError(em, "Formula Evaluator Error");
            std::cout << "ERROR: " << em << std::endl;
        }

        // Since I'm (right now) the only vector valued modulator just do a little
        // chute and ladder dance here on the output and return
        auto magnf = limit_range(lfo->magnitude.get_extended(localcopy[magn].f), -3.f, 3.f);
        auto uni = lfo->unipolar.val.b;

        for (auto i = 0; i < formulastate.activeoutputs; ++i)
        {
            if (uni)
            {
                tmpout[i] = 0.5f + 0.5f * tmpout[i];
            }

            output_multi[i] = useenvval * magnf * tmpout[i];
        }

        return;
    }
    };

    float io2 = iout;

    // change this? pls check formula
    if (lfo->unipolar.val.b)
    {
        if (s != lt_stepseq)
        {
            io2 = 0.5f + 0.5f * io2;
        }
        else if (io2 < 0.f)
        {
            io2 = 0.f;
        }
    }

    // Correct the envelope for the start case
    // (in a way which works for all sequencers except lt_envelope)
    float useenv0{0.0};
    float outputEnvVal{useenvval};
    if (envRetrigMode == FROM_LAST)
    {
        float off = envelopeStart;
        float scale = (1.0 - envelopeStart);
        if (env_state == lfoeg_delay || env_state == lfoeg_attack)
        {
            useenvval = useenvval * scale;
            useenv0 = off;
            outputEnvVal = useenvval;
        }
    }
    auto magnf = limit_range(lfo->magnitude.get_extended(localcopy[magn].f), -3.f, 3.f);

    output_multi[0] = (useenvval + useenv0) * magnf * io2;
    output_multi[1] = io2;
    output_multi[2] = outputEnvVal + useenv0; // env_val;

    if (lfo->lfoExtraAmplitude == LFOStorage::SCALED)
    {
        output_multi[1] *= magnf;
        output_multi[2] *= magnf;
    }

    if (s == lt_envelope)
    {
        // Additional start mode corrections required
        if (envelopeStart != 1 && envelopeStart != 0 && envRetrigMode == FROM_LAST &&
            (env_state == lfoeg_delay || env_state == lfoeg_attack))
        {
            // We need to back out the deforms if we can
            auto os = envelopeStart;
            auto dv = localcopy[ideform].f;
            if (lfo->deform.deform_type == type_1)
            {
                // type 1: r = (1-d)*p + d * p*p solve for p
                os = (1 - dv) * envelopeStart + dv * envelopeStart * envelopeStart;
            }
            else if (lfo->deform.deform_type == type_2)
            {
                // r = r^(1+0*dv) dv>0; r^(1/(-4*dv)) dv < 0
                if (dv > 0)
                    os = pow(os, 1 + 9 * dv);
                else
                    os = pow(os, 1 / (1 - 4 * dv));
            }
            // type 3 (random) is non-invertible. You just get a jump then.
            float scale = (1.0 - os);
            useenvval = useenvval * scale / (1 - envelopeStart);
            useenv0 = os;

            output_multi[0] = useenvval * magnf * io2 + useenv0;

            output_multi[1] *= useenvval;
            output_multi[1] += useenv0;
        }
        else
        {
            output_multi[1] *= useenvval;
            output_multi[1] += useenv0;
        }

        if (envRetrigMode == FROM_LAST && lfo->deform.deform_type != type_3)
        {
            // Now apply the one pole
            float alpha = onepoleFactor;
            output_multi[0] = (1.0 - alpha) * output_multi[0] + alpha * onepoleState[0];
            output_multi[1] = (1.0 - alpha) * output_multi[1] + alpha * onepoleState[1];
            onepoleState[0] = output_multi[0];
            onepoleState[1] = output_multi[1];
        }
    }
}

void LFOModulationSource::completedModulation()
{
    if (lfo->shape.val.i == lt_formula)
    {
        Surge::Formula::cleanEvaluatorState(formulastate);
    }
}
