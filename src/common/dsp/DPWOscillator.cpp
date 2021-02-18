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

#include "DPWOscillator.h"
#include "DebugHelpers.h"

void DPWOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    // We need a tiny little portamento since the derivative is pretty
    // unstable under super big pitch changes
    pitchlag.setRate(0.5);
    pitchlag.startValue(pitch);
    pwidth.setRate(0.001); // 4x slower
    sync.setRate(0.001 * BLOCK_SIZE_OS);

    n_unison = is_display ? 1 : oscdata->p[dpw_unison_voices].val.i;
    double oosun = 1.0 / sqrt(n_unison);

    for (int u = 0; u < n_unison; ++u)
    {
        if (n_unison > 1)
        {
            float mx = 1.f * u / (n_unison - 1); // between 0 and 1
            unisonOffsets[u] = (mx - 0.5) * 2;
            mixL[u] = (1.0 - mx) * oosun * 2;
            mixR[u] = mx * oosun * 2;
        }
        else
        {
            unisonOffsets[0] = 0;
            mixL[0] = 1;
            mixR[0] = 1;
        }
        phase[u] = oscdata->retrigger.val.b || is_display ? 0.f : ((float)rand() / (float)RAND_MAX);
        sphase[u] = phase[u];
        subphase = 0;
        subsphase = 0;

        driftlfo[u] = 0.f;
        driftlfo2[u] = 0.f;
        if (nonzero_init_drift)
            driftlfo2[u] = 0.0005 * ((float)rand() / (float)(RAND_MAX));
    }

    // This is the same implementation as SurgeSuperOscillator just as doubles
    switch (storage->getPatch().character.val.i)
    {
    case 0:
    {
        double filt = 1.0 - 2.0 * 5000.0 * samplerate_inv;
        filt *= filt;
        charfiltB0 = 1.f - filt;
        charfiltB1 = 0.f;
        charfiltA1 = filt;
        dofilter = true;
    }
    break;
    case 1:
        charfiltB0 = 1.f;
        charfiltB1 = 0.f;
        charfiltA1 = 0.f;
        dofilter = false; // since that is just output = output
        break;
    case 2:
    {
        double filt = 1.0 - 2.0 * 5000.0 * samplerate_inv;
        filt *= filt;
        auto A0 = 1.0 / (1.0 - filt);
        charfiltB0 = 1.f * A0;
        charfiltB1 = -filt * A0;
        charfiltA1 = 0.f;
        dofilter = true;
    }
    break;
    }
}

void DPWOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepthV)
{
    if ((oscdata->p[dpw_tri_mix].deform_type & 0xF) != (int)multitype)
    {
        multitype = ((DPWOscillator::dpw_multitypes)(oscdata->p[dpw_tri_mix].deform_type & 0xF));

        // FIXME - this is not the right place for this and it has bugs
        std::string nm = std::string("Multi - ") + dpw_multitype_names[(int)multitype];
        oscdata->p[dpw_tri_mix].set_name(nm.c_str());
    }
    int subOctave = 0;
    float submul = 1;
    if (oscdata->p[dpw_tri_mix].deform_type & dpw_subone)
    {
        subOctave = -1;
        submul = 0.5;
    }

    float ud = oscdata->p[dpw_unison_detune].get_extended(
        localcopy[oscdata->p[dpw_unison_detune].param_id_in_scene].f);
    pitchlag.startValue(pitch);
    sync.newValue(localcopy[oscdata->p[dpw_sync].param_id_in_scene].f);
    for (int u = 0; u < n_unison; ++u)
    {
        driftlfo[u] = drift_noise(driftlfo2[u]);
        auto lfodetune = drift * driftlfo[u];

        dpbase[u].newValue(
            std::min(0.5, pitch_to_dphase(pitchlag.v + lfodetune + ud * unisonOffsets[u])));
        dspbase[u].newValue(std::min(
            0.5, pitch_to_dphase(pitchlag.v + lfodetune + sync.v + ud * unisonOffsets[u])));
    }
    auto subdt = drift * driftlfo[0];
    subdpbase.newValue(std::min(0.5, pitch_to_dphase(pitchlag.v + subdt) * submul));
    subdpsbase.newValue(std::min(0.5, pitch_to_dphase(pitchlag.v + subdt + sync.v) * submul));
    sync.process();
    sawmix.newValue(localcopy[oscdata->p[dpw_saw_mix].param_id_in_scene].f);
    trimix.newValue(localcopy[oscdata->p[dpw_tri_mix].param_id_in_scene].f);
    sqrmix.newValue(localcopy[oscdata->p[dpw_pulse_mix].param_id_in_scene].f);
    pwidth.newValue(limit_range(1.f - localcopy[oscdata->p[dpw_pulse_width].param_id_in_scene].f,
                                0.01f, 0.99f));

    pitchlag.process();

    double fv = 16 * fmdepthV * fmdepthV * fmdepthV;
    fmdepth.newValue(fv);

    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        double vL = 0.0, vR = 0.0;
        double fmPhaseShift = 0.0;
        if (FM)
            fmPhaseShift = fmdepth.v * master_osc[i];
        for (int u = 0; u < n_unison; ++u)
        {
            auto dp = dpbase[u].v;
            auto dsp = dspbase[u].v;

            /*
             * So rather than lagging, back compute with the current dPhase
             * which makes us way more stable under phase changes
             */
            double sBuff[3], sOffBuff[3], triBuff[3];
            for (int s = 0; s < 3; ++s)
            {
                // Saw Component (p^3-p)/6
                double p01 = sphase[u] + fmPhaseShift - s * dsp;
                if (p01 > 1)
                {
                    p01 -= floor(p01);
                }
                if (p01 < 0)
                {
                    p01 += -ceil(p01) + 1;
                }
                double p = (p01 - 0.5) * 2;

                double p3 = p * p * p;
                double sawcub = (p3 - p) / 6.0;
                sBuff[s] = sawcub;

                if (subOctave != 0)
                {
                    triBuff[s] = 0.0;
                }
                else
                {
                    switch (multitype)
                    {
                    case dpwm_square:
                    {
                        double Q = p < 0 ? 1 : -1;
                        triBuff[s] = Q * p * p / 2 + p / 2;
                    }
                    break;
                    case dpwm_sine:
                    {
                        double pos = p < 0 ? 0 : 1;
                        double p4 = p3 * p;

                        triBuff[s] = -(pos * (-p4 / 3.0 + 2 * p3 / 3.0 - p / 3.0) +
                                       (pos - 1) * (-p4 / 3.0 - 2 * p3 / 3.0 + p / 3.0));
                    }
                    break;
                    case dpwm_triangle:
                    {
                        double tp = p + 0.5;
                        if (tp > 1.0)
                            tp -= 2;
                        if (tp < -1.0)
                            tp += 2;
                        double Q = tp < 0 ? -1 : 1;
                        double tricub = -(2.0 * Q * tp * tp * tp - 3.0 * tp * tp - 2.0) / 6.0;
                        triBuff[s] = tricub;
                    }
                    break;
                    }
                }

                double pwp = p + pwidth.v * 2;
                if (pwp > 1)
                    pwp -= 2;
                if (pwp < -1)
                    pwp += 2;
                sOffBuff[s] = (pwp * pwp * pwp - pwp) / 6.0;
            }

            double denom = 0.25 / (dsp * dsp);
            double saw = (sBuff[0] + sBuff[2] - 2.0 * sBuff[1]) * denom;
            double tri = (triBuff[0] + triBuff[2] - 2.0 * triBuff[1]) * denom;
            double sawoff = (sOffBuff[0] + sOffBuff[2] - 2.0 * sOffBuff[1]) * denom;
            double sqr = sawoff - saw;

            // Super important - you have to mix after differentiating to avoid zipper noise
            double res = sawmix.v * saw + trimix.v * tri + sqrmix.v * sqr;

            vL += res * mixL[u];
            vR += res * mixR[u];
            phase[u] += dp;
            sphase[u] += dsp;

            if (phase[u] > 1)
            {
                phase[u] -= floor(phase[u]);
                if (sReset[u])
                    sphase[u] = phase[u] * dsp / dp;
                sReset[u] = !sReset[u];
            }

            if (sphase[u] > 1)
                sphase[u] -= floor(sphase[u]);

            dpbase[u].process();
            dspbase[u].process();
        }

        if (subOctave != 0)
        {
            auto dp = subdpbase.v;
            auto dsp = subdpsbase.v;
            double triBuff[3];

            for (int s = 0; s < 3; ++s)
            {
                double p01 = subsphase + fmPhaseShift - s * dsp;
                if (p01 > 1)
                {
                    p01 -= floor(p01);
                }
                if (p01 < 0)
                {
                    p01 += -ceil(p01) + 1;
                }
                double p = (p01 - 0.5) * 2;
                double p3 = p * p * p;

                switch (multitype)
                {
                case dpwm_square:
                {
                    double Q = p < 0 ? 1 : -1;
                    triBuff[s] = Q * p * p / 2 + p / 2;
                }
                break;
                case dpwm_sine:
                {
                    double pos = p < 0 ? 0 : 1;
                    double p4 = p3 * p;

                    triBuff[s] = -(pos * (-p4 / 3.0 + 2 * p3 / 3.0 - p / 3.0) +
                                   (pos - 1) * (-p4 / 3.0 - 2 * p3 / 3.0 + p / 3.0));
                }
                break;
                case dpwm_triangle:
                {
                    double tp = p + 0.5;
                    if (tp > 1.0)
                        tp -= 2;
                    if (tp < -1.0)
                        tp += 2;
                    double Q = tp < 0 ? -1 : 1;
                    double tricub = -(2.0 * Q * tp * tp * tp - 3.0 * tp * tp - 2.0) / 6.0;
                    triBuff[s] = tricub;
                }
                break;
                }
            }

            double sub = (triBuff[0] + triBuff[2] - 2.0 * triBuff[1]) / (4 * dsp * dsp);

            vL += trimix.v * sub;
            vR += trimix.v * sub;

            subphase += dp;
            subsphase += dsp;

            if (subphase > 1)
            {
                subphase -= floor(subphase);
                subsphase = subphase * dsp / dp;
            }

            if (subsphase > 1)
                subsphase -= floor(subsphase);
        }

        if (stereo)
        {
            output[i] = vL;
            outputR[i] = vR;
        }
        else
        {
            output[i] = (vL + vR) / 2;
        }
        sawmix.process();
        trimix.process();
        sqrmix.process();
        pwidth.process();
        fmdepth.process();
        subdpbase.process();
        subdpsbase.process();
    }

    if (dofilter)
    {
        if (starting)
        {
            priorY_L = output[0];
            priorX_L = output[0];

            priorY_R = outputR[0];
            priorX_R = outputR[0];
        }

        for (int i = 0; i < BLOCK_SIZE_OS; ++i)
        {
            auto pfL = charfiltA1 * priorY_L + charfiltB0 * output[i] + charfiltB1 * priorX_L;
            priorY_L = pfL;
            priorX_L = output[i];
            output[i] = pfL;

            if (stereo)
            {
                auto pfR = charfiltA1 * priorY_R + charfiltB0 * outputR[i] + charfiltB1 * priorX_R;
                priorY_R = pfR;
                priorX_R = outputR[i];
                outputR[i] = pfR;
            }
        }
    }

    starting = false;
}

void DPWOscillator::init_ctrltypes()
{
    oscdata->p[dpw_saw_mix].set_name("Sawtooth");
    oscdata->p[dpw_saw_mix].set_type(ct_percent_bidirectional);

    oscdata->p[dpw_pulse_mix].set_name("Pulse");
    oscdata->p[dpw_pulse_mix].set_type(ct_percent_bidirectional);

    std::string nm = std::string("Multi - ") + dpw_multitype_names[(int)multitype];
    oscdata->p[dpw_tri_mix].set_name(nm.c_str());

    oscdata->p[dpw_tri_mix].set_type(ct_dpw_trimix);

    oscdata->p[dpw_pulse_width].set_name("Width");
    oscdata->p[dpw_pulse_width].set_type(ct_percent);
    oscdata->p[dpw_pulse_width].val_default.f = 0.5;

    oscdata->p[dpw_sync].set_name("Sync");
    oscdata->p[dpw_sync].set_type(ct_syncpitch);

    oscdata->p[dpw_unison_detune].set_name("Unison Detune");
    oscdata->p[dpw_unison_detune].set_type(ct_oscspread);
    oscdata->p[dpw_unison_voices].set_name("Unison Voices");
    oscdata->p[dpw_unison_voices].set_type(ct_osccount);
}

void DPWOscillator::init_default_values()
{
    oscdata->p[dpw_saw_mix].val.f = 0.5;
    oscdata->p[dpw_tri_mix].val.f = 0.0;
    oscdata->p[dpw_tri_mix].deform_type = 0;
    oscdata->p[dpw_pulse_mix].val.f = 0.0;
    oscdata->p[dpw_pulse_width].val.f = 0.5;
    oscdata->p[dpw_sync].val.f = 0.0;
    oscdata->p[dpw_unison_detune].val.f = 0.2;
    oscdata->p[dpw_unison_voices].val.i = 1;
}
