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

void DPWOscillator::init(float pitch, bool is_display)
{
    // We need a tiny little portamento since the derivative is pretty
    // unstable under super big pitch changes
    pitchlag.setRate(0.5);
    pitchlag.startValue(pitch);

    n_unison = is_display ? 1 : oscdata->p[dpw_unison_voices].val.i;

    for (int u = 0; u < n_unison; ++u)
    {
        if (n_unison > 1)
        {
            float mx = 1.f * u / (n_unison - 1); // between 0 and 1
            unisonOffsets[u] = (mx - 0.5);
            mixL[u] = 1.0 - mx;
            mixR[u] = mx;
        }
        else
        {
            unisonOffsets[0] = 0;
        }
        phase[u] = oscdata->retrigger.val.b || is_display ? 0.f : ((float)rand() / (float)RAND_MAX);
    }
}

void DPWOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
    float ud = localcopy[oscdata->p[dpw_unison_detune].param_id_in_scene].f;
    pitchlag.newValue(pitch);
    for (int u = 0; u < n_unison; ++u)
    {
        dpbase[u].newValue(std::min(0.5, pitch_to_dphase(pitchlag.v + ud * unisonOffsets[u])));
    }
    sawmix.newValue(localcopy[oscdata->p[dpw_saw_mix].param_id_in_scene].f);
    trimix.newValue(localcopy[oscdata->p[dpw_tri_mix].param_id_in_scene].f);
    sqrmix.newValue(localcopy[oscdata->p[dpw_pulse_mix].param_id_in_scene].f);
    pwidth.newValue(limit_range(1.f - localcopy[oscdata->p[dpw_pulse_width].param_id_in_scene].f,
                                0.01f, 0.99f));

    pitchlag.process();

    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        double vL = 0.0, vR = 0.0;
        for (int u = 0; u < n_unison; ++u)
        {
            auto dp = dpbase[u].v;

            for (int qx = sigbuf_len - 1; qx > 0; --qx)
            {
                sawBuffer[u][qx] = sawBuffer[u][qx - 1];
                triBuffer[u][qx] = triBuffer[u][qx - 1];
                sawOffBuffer[u][qx] = sawOffBuffer[u][qx - 1];
            }

            // Saw Component (p^3-p)/6
            double p = (phase[u] - 0.5) * 2;
            double p3 = p * p * p;
            double sawcub = (p3 - p) / 6.0;
            sawBuffer[u][0] = sawcub;
            double saw =
                (sawBuffer[u][0] + sawBuffer[u][2] - 2.0 * sawBuffer[u][1]) / (4.0 * dp * dp);

            double tp = p + 0.5;
            if (tp > 1.0)
                tp -= 2;
            double Q = tp < 0 ? -1 : 1;
            double tricub = (2.0 * Q * tp * tp * tp - 3.0 * tp * tp - 2.0) / 6.0;
            triBuffer[u][0] = tricub;
            double tri =
                (triBuffer[u][0] + triBuffer[u][2] - 2.0 * triBuffer[u][1]) / (4.0 * dp * dp);

            double pwp = (phase[u] - 0.5 + pwidth.v) * 2;
            if (pwp > 1)
                pwp -= 2;
            sawcub = (pwp * pwp * pwp - pwp) / 6.0;
            sawOffBuffer[u][0] = sawcub;
            double sawoff = (sawOffBuffer[u][0] + sawOffBuffer[u][2] - 2.0 * sawOffBuffer[u][1]) /
                            (4.0 * dp * dp);
            double sqr = sawoff - saw + (1 - pwidth.v * 2);
            // sawBuffer[u][0] = sawmix.v * sawcub + trimix.v * tricub;

            // double res = (sawBuffer[u][0] + sawBuffer[u][2] - 2.0 * sawBuffer[u][1]) /
            //             (4.0 * dp * dp);

            // Super important - you have to mix after differentiating to avoid
            // zipper noise
            double res = sawmix.v * saw + trimix.v * tri + sqrmix.v * sqr;
            vL += res * mixL[u];
            vR += res * mixR[u];
            phase[u] += dp;

            while (phase[u] > 1)
                phase[u] -= 1;

            dpbase[u].process();
        }

        output[i] = vL;
        outputR[i] = vR;

        sawmix.process();
        trimix.process();
        sqrmix.process();
        pwidth.process();
    }

    if (starting)
    {
        for (int i = 0; i < 2; ++i)
        {
            output[i] = output[2];
            outputR[i] = outputR[2];
        }
        starting = false;
    }
}

void DPWOscillator::init_ctrltypes()
{
    oscdata->p[dpw_saw_mix].set_name("Saw Mix");
    oscdata->p[dpw_saw_mix].set_type(ct_percent);
    oscdata->p[dpw_saw_mix].val_default.f = 0.5;

    oscdata->p[dpw_pulse_mix].set_name("Pulse Mix");
    oscdata->p[dpw_pulse_mix].set_type(ct_percent);
    oscdata->p[dpw_pulse_mix].val_default.f = 0.5;

    oscdata->p[dpw_tri_mix].set_name("Triangle Mix");
    oscdata->p[dpw_tri_mix].set_type(ct_percent);
    oscdata->p[dpw_tri_mix].val_default.f = 0.5;

    oscdata->p[dpw_pulse_width].set_name("Pulse Width");
    oscdata->p[dpw_pulse_width].set_type(ct_percent);
    oscdata->p[dpw_pulse_width].val_default.f = 0.5;

    oscdata->p[dpw_sync].set_name("Currently Unimplemented");
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
    oscdata->p[dpw_pulse_mix].val.f = 0.0;
    oscdata->p[dpw_pulse_width].val.f = 0.5;
    oscdata->p[dpw_sync].val.f = 0.0;
    oscdata->p[dpw_unison_detune].val.f = 0.2;
    oscdata->p[dpw_unison_voices].val.i = 1;
}
