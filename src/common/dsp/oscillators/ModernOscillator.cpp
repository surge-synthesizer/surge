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

#include "ModernOscillator.h"
#include "DebugHelpers.h"

/*
 * Alright so what the heck is this thing? Well this is the "Modern" oscillator
 * based on differentiated polynomial waveforms, described in this paper:
 *
 * https://www.researchgate.net/publication/224557976_Alias-Suppressed_Oscillators_Based_on_Differentiated_Polynomial_Waveforms
 *
 * but with some caveats.
 *
 * So the whole idea of that paper is as follows. Imagine phase runs from -1 to 1
 * then you can implement a sawtooth as simply "saw = phase". Cool. But if you do
 * that when the saw turns over you get either the prior or the next cycle but
 * you don't get the blend of them. That creates aliasing. That aliasing is unpleasant.
 * There's a lot of ways to overcome that (BLIT, BLEP, etc...) but DPW works as follows:
 *
 * Create a polynomial which is the n-th integral of the desired function
 * Differentiate it n times
 * Output that
 *
 * Basically the numerical derivative operator acts as very specific sort of
 * filter that averages you over the discontinuities to greatly reduce aliasing.
 * The paper above goes up to n=5 or 6 or so, but leaves "some items as exercises
 * to the reader" as they say.
 *
 * The trick, of course, is that the function you differentiate needs to be
 * continuous and so forth, otherwise you get bad numerical derivatives at the
 * edges.
 *
 * For this implementation we use the 'cubic' representation (the second
 * anti-derivative) and take the numerical second derivative. More on that later.
 *
 * So first let's talk about the waveforms. This entire document is written assuming
 * that phase is normalized between -1 and 1.
 *
 * Sawtooth: output = p
 * 2nd anti-derivative g = p^3 / 6 + a * p^2 + b * p + c
 *
 * so now we need to solve for a and b. We know that g(-1) = g(1).
 *
 * g(-1) = -1/6 + a - b + c
 * g(1)  = 1/6 + a + b + c
 *
 * g(-1) = g(-1) -> -1/6 + a - b + c = 1/6 + a + b + c
 *
 * Solution to this is a = 0; b = -1/6; c = 0.
 *
 * So if we take p^3 / 6 - p / 6, its second derivative on -1, 1 cycled pulse will be
 * a saw with the edges anti-aliasing nicely. Cool!
 *
 * Square: we want p < 0 = 1; p > 0 = -1. So clearly our functional form has to be
 * at most quadratic.
 *
 * g = a * x^2 + b * x + c
 *
 * so if we want this to have:
 *
 * g'' = p < 0 ? -1 : 1 = Q
 * g'' = 2 * a
 * a = Q / 2
 *
 * and great now do continuity at -1 / 1 for the generator subbing in Q on either side:
 *
 * 1/2 * 1 (-1)^2 + b * -1 + v = 1/2 * -1 * (1)^2 + b + 1 + c
 * 1/2 - b = -1/2 + b
 * b = 1/2
 *
 * so g = (Q * p^2 + p) / 2
 *
 * Pulse wave is done a bit differently. For that we use the fact
 * that two sawtooths phase shifted by pulse width factor and subtracted gives you a pulse.
 *
 * Finally, the "sine" wave. The sine wave we use here is not actually
 * a sine wave, but is a normalized parabola pair. That is, the sine wave is
 * a 0/1 parabola centered at 0.5 and the opposite centered at -0.5.
 * The math for that is worked out in the comment below.
 *
 * So now we have our generators. How do we actually define the signal?
 * Well, in the paper above, or any implmeentation with constant pitch, you
 * could simply record the generators at a series of points, roll your pointer,
 * differentiate, etc... But Surge can radically modulate pitch, so that gives
 * us a very 'non-constant' grid between frequency and sample inside the engine.
 *
 * So rather than that approach, what we do is: at every sample, figure out the
 * frequency desired *at that sample*. We then figure out the 3 generators
 * for that frequency at that sample (prior, prior - 1, and prior - 2) and differentiate
 * using a numercial second derivative. This is more CPU intensive, but it is rock
 * solid under all sorts of frequency modulation, including FM, pitch shifts, and sync.
 *
 * Finally, that numerical integration. I use the standard second derivative form
 * (u_i - 2 u_i-1 + u i-2 ) / dt^2. *But*, since i am calculating at i, we can either
 * think that that is second-order-accurate-in-dt and lagged by one sample or
 * first order accurate-in-dt and not lagged. It doesn't really matter. I also considered
 * having the candidate phases be +1/0/-1 rather than 0/-1/-2 but that somehow felt
 * a wee bit like cheating. Anyway, the difference is negligible.
 *
 * Other than that, FM is obivous, sync runs two clocks and resets obviously,
 * and the rest is just mixing and lagging. All pretty obvious.
 */

void ModernOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    // we need a tiny little portamento since the derivative is pretty
    // unstable under super big pitch changes
    pitchlag.setRate(0.5);
    pitchlag.startValue(pitch);
    pwidth.setRate(0.001); // 4x slower
    sync.setRate(0.001 * BLOCK_SIZE_OS);

    n_unison = is_display ? 1 : oscdata->p[mo_unison_voices].val.i;

    auto us = Surge::Oscillator::UnisonSetup<double>(n_unison);

    double atten = us.attenuation();

    for (int u = 0; u < n_unison; ++u)
    {
        unisonOffsets[u] = us.detune(u);
        us.attenuatedPanLaw(u, mixL[u], mixR[u]);

        phase[u] = oscdata->retrigger.val.b || is_display ? 0.f : storage->rand_01();
        sphase[u] = phase[u];
        sTurnFrac[u] = 0;
        sprior[u] = 0;
        sTurnVal[u] = 0;

        driftLFO[u].init(nonzero_init_drift);

        sReset[u] = false;
    }

    subphase = 0;
    subsphase = 0;

    // This is the same implementation as ClassicOscillator, just as doubles
    charFilt.init(storage->getPatch().character.val.i);
}

template <ModernOscillator::mo_multitypes multitype, bool subOctave, bool FM>
void ModernOscillator::process_sblk(float pitch, float drift, bool stereo, float fmdepthV)
{
    float submul = 1;
    if (subOctave)
    {
        submul = 0.5;
    }

    float ud = oscdata->p[mo_unison_detune].get_extended(
        localcopy[oscdata->p[mo_unison_detune].param_id_in_scene].f);
    pitchlag.startValue(pitch);
    sync.newValue(std::max(0.f, localcopy[oscdata->p[mo_sync].param_id_in_scene].f));

    float absOff = 0;
    if (oscdata->p[mo_unison_detune].absolute)
    {
        absOff = ud * 16;
        ud = 0;
    }

    for (int u = 0; u < n_unison; ++u)
    {
        auto dval = driftLFO[u].next();
        auto lfodetune = drift * dval;

        dpbase[u].newValue(std::min(
            0.5, pitch_to_dphase_with_absolute_offset(
                     pitchlag.v + lfodetune + ud * unisonOffsets[u], absOff * unisonOffsets[u])));
        dspbase[u].newValue(
            std::min(0.5, pitch_to_dphase_with_absolute_offset(pitchlag.v + lfodetune + sync.v +
                                                                   ud * unisonOffsets[u],
                                                               absOff * unisonOffsets[u])));
    }

    auto subdt = drift * driftLFO[0].val();

    subdpbase.newValue(std::min(0.5, pitch_to_dphase(pitchlag.v + subdt) * submul));
    subdpsbase.newValue(std::min(0.5, pitch_to_dphase(pitchlag.v + subdt + sync.v) * submul));
    sync.process();

    // Let people modulate outside the sliders a bit. but not catastrophically
    sawmix.newValue(0.5 *
                    limit_range(localcopy[oscdata->p[mo_saw_mix].param_id_in_scene].f, -2.f, 2.f));
    sqrmix.newValue(
        0.5 * limit_range(localcopy[oscdata->p[mo_pulse_mix].param_id_in_scene].f, -2.f, 2.f));
    trimix.newValue(0.5 *
                    limit_range(localcopy[oscdata->p[mo_tri_mix].param_id_in_scene].f, -2.f, 2.f));

    // Since we always use this multiplied by 2, put the mul here to save it later
    pwidth.newValue(2 * limit_range(1.f - localcopy[oscdata->p[mo_pulse_width].param_id_in_scene].f,
                                    0.01f, 0.99f));
    pitchlag.process();

    double fv = 16 * fmdepthV * fmdepthV * fmdepthV;
    fmdepth.newValue(fv);

    const double oneOverSix = 1.0 / 6.0;
    // We only use 3 of these
    double sBuff alignas(16)[4] = {0, 0, 0, 0}, sOffBuff alignas(16)[4] = {0, 0, 0, 0},
                 triBuff alignas(16)[4] = {0, 0, 0, 0}, phases alignas(16)[4] = {0, 0, 0, 0};

    bool subsyncskip =
        oscdata->p[mo_tri_mix].deform_type & ModernOscillator::mo_submask::mo_subskipsync;

    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        double vL = 0.0, vR = 0.0;
        double fmPhaseShift = 0.0;

        if (FM)
        {
            fmPhaseShift = FM * fmdepth.v * master_osc[i];
        }

        for (int u = 0; u < n_unison; ++u)
        {
            auto dp = dpbase[u].v;
            auto dsp = dspbase[u].v;
            double pfm = sphase[u];

            // Since this is a template param compiler should not eject branch
            if (FM)
            {
                pfm += fmPhaseShift;

                // Have to use floor/ceil here because FM could be big
                if (pfm > 1)
                {
                    pfm -= floor(pfm);
                }
                else if (pfm < 0)
                {
                    pfm += -ceil(pfm) + 1;
                }
            }

            phases[0] = pfm;
            phases[1] = pfm - dsp + (pfm < dsp);
            phases[2] = pfm - 2 * dsp + (pfm < 2 * dsp);

            for (int s = 0; s < 3; ++s)
            {
                // Saw component (p^3 - p) / 6
                double p01 = phases[s];
                double p = (p01 - 0.5) * 2;
                double p3 = p * p * p;
                double sawcub = (p3 - p) * oneOverSix;

                sBuff[s] = sawcub;

                /*
                 * Remember these ifs are now on template params so won't
                 * eject branches
                 */
                if (subOctave)
                {
                    triBuff[s] = 0.0;
                }
                else
                {
                    if (multitype == momt_square)
                    {
                        // double Q = std::signbit(p) * 2 - 1;
                        double Q = (p < 0) * 2 - 1;
                        triBuff[s] = p * (Q * p + 1) * 0.5;
                    }
                    if (multitype == ModernOscillator::momt_sine)
                    {
                        // double pos = 1.0 - std::signbit(p);
                        double modpos = 2.0 * (p < 0) - 1.0;
                        double p4 = p3 * p;
                        constexpr double oo3 = 1.0 / 3.0;

                        /*
                         * So...
                         *
                         * -(pos * (-p4 + 2 * p3 - p) + (pos - 1) * (-p4 - 2 * p3 + p)) * oo3
                         *
                         * Alright so p4 is:
                         *
                         * (pos * -p4 + (pos - 1) * -p4) == ( 1 - 2 * pos ) * p4
                         *
                         * p3 is:
                         *
                         * pos * 2 * p3 + (pos - 1) * -2 * p3
                         * pos * 2 * p3 - pos * 2 + p3 + 2 * p3
                         *       2 * p3
                         *
                         * p is:
                         *
                         * pos * -p + (pos - 1) + p
                         * -pos * p + pos * p - p
                         * or -p
                         *
                         * so our term is actually:
                         *
                         * -((1 - 2 * pos) * p4 + 2 * p3 - p) * oo3
                         *
                         * Moreover, pos is 1-signbit so (1 - 2 * pos) == (1 - 2 + 2 * signbit)
                         * or 2 * signbit - 1
                         */
                        triBuff[s] = -(modpos * p4 + 2 * p3 - p) * oo3;
                    }
                    if (multitype == ModernOscillator::momt_triangle)
                    {
                        double tp = p + 0.5;
                        tp -= (tp > 1.0) * 2;

                        double Q = 1 - (tp < 0) * 2;
                        triBuff[s] = (2.0 + tp * tp * (3.0 - 2.0 * Q * tp)) * oneOverSix;
                    }
                }

                double pwp = p + pwidth.v; // that's actually pw * 2, but we lag the width * 2
                pwp += (pwp > 1) * -2;     // (pwp > 1 ? -2 : (pwp < -1 ? 2 : 0));
                sOffBuff[s] = (pwp * pwp * pwp - pwp) * oneOverSix;
            }

            double denom = 0.25 / (dsp * dsp);
            double saw = (sBuff[0] + sBuff[2] - 2.0 * sBuff[1]);
            double sawoff = (sOffBuff[0] + sOffBuff[2] - 2.0 * sOffBuff[1]);
            double tri = (triBuff[0] + triBuff[2] - 2.0 * triBuff[1]);
            double sqr = sawoff - saw;

            // super important - you have to mix after differentiating to avoid zipper noise
            // but I can save a multiply by putting it here
            double res = (sawmix.v * saw + trimix.v * tri + sqrmix.v * sqr) * denom;
            res = res * (1.0 - sTurnFrac[u]) + sTurnFrac[u] * sTurnVal[u];

            vL += res * mixL[u];
            vR += res * mixR[u];

            // we know phase is in 0,1 and dp is in 0,0.5
            phase[u] += dp;
            sphase[u] += dsp;
            sTurnFrac[u] = 0.0;

            // If we try to unbranch this, we get the divide at every tick which is painful
            if (phase[u] > 1)
            {
                phase[u] -= 1;

                if (sReset[u])
                {
                    sphase[u] = phase[u] * dsp / dp;
                    sphase[u] -= floor(sphase[u]); // just in case we have a very high sync

                    /*
                     * So the way we do synch can be a bit aliasy. Basically we move the phase
                     * forward and then difference over the new phase. WHat we should really do is
                     * figure out continuous generators with sync in but ugh that's super hard and
                     * it is late in the 1.9 cycle. So instead what we do is a little compensating
                     * turnover where we linearly itnerpolate the prior phase forward one sample
                     * (that is sTurnVal) and then average it into the next sample. Resetting
                     * sTurnFrac above means this only happens at the turnover sample. Only do this
                     * if sync is on of course
                     */
                    if (sync.v > 1e-4)
                        sTurnFrac[u] = 0.5; // std::min(std::max(0.1, osp-sphase[u]), 0.9);
                    sTurnVal[u] = res + (sprior[u] - res) * dsp;
                }

                sReset[u] = !sReset[u];
            }

            sprior[u] = res;

            sphase[u] -= (sphase[u] > 1) * 1.0;

            dpbase[u].process();
            dspbase[u].process();
        }

        if (subOctave)
        {
            auto dp = subdpbase.v;
            auto dsp = ((1 - subsyncskip) * subdpsbase.v) + (subsyncskip * dp);

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

                if (multitype == momt_square)
                {
                    double Q = (p < 0) * 2 - 1;
                    triBuff[s] = p * (Q * p + 1) * 0.5;
                }
                if (multitype == momt_sine)
                {
                    double modpos = 2.0 * (p < 0) - 1.0;
                    double p4 = p3 * p;
                    constexpr double oo3 = 1.0 / 3.0;

                    triBuff[s] = -(modpos * p4 + 2 * p3 - p) * oo3;
                }
                if (multitype == momt_triangle)
                {
                    double tp = p + 0.5;

                    tp -= (tp > 1.0) * 2;

                    double Q = 1 - (tp < 0) * 2;
                    double tricub = (2.0 + tp * tp * (3.0 - 2.0 * Q * tp)) * oneOverSix;

                    triBuff[s] = tricub;
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

        output[i] = vL;
        outputR[i] = vR;

        sawmix.process();
        trimix.process();
        sqrmix.process();
        pwidth.process();
        fmdepth.process();
        subdpbase.process();
        subdpsbase.process();
    }

    if (!stereo)
    {
        for (int s = 0; s < BLOCK_SIZE_OS; ++s)
        {
            output[s] = 0.5 * (output[s] + outputR[s]);
        }
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

    starting = false;
}

void ModernOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepthV)
{
    if (oscdata->p[mo_tri_mix].deform_type != cachedDeform)
    {
        cachedDeform = oscdata->p[mo_tri_mix].deform_type;
        multitype = ((ModernOscillator::mo_multitypes)(cachedDeform & 0xF));
    }

    bool subOct = false;

    if (cachedDeform & mo_subone)
    {
        subOct = true;
    }

    if (!FM)
    {
        switch (multitype)
        {
        case momt_sine:
            if (subOct)
                return process_sblk<momt_sine, true, false>(pitch, drift, stereo, fmdepthV);
            else
                return process_sblk<momt_sine, false, false>(pitch, drift, stereo, fmdepthV);
        case momt_square:
            if (subOct)
                return process_sblk<momt_square, true, false>(pitch, drift, stereo, fmdepthV);
            else
                return process_sblk<momt_square, false, false>(pitch, drift, stereo, fmdepthV);
        case momt_triangle:
            if (subOct)
                return process_sblk<momt_triangle, true, false>(pitch, drift, stereo, fmdepthV);
            else
                return process_sblk<momt_triangle, false, false>(pitch, drift, stereo, fmdepthV);
        }
    }
    else
    {
        switch (multitype)
        {
        case momt_sine:
            if (subOct)
                return process_sblk<momt_sine, true, true>(pitch, drift, stereo, fmdepthV);
            else
                return process_sblk<momt_sine, false, true>(pitch, drift, stereo, fmdepthV);
        case momt_square:
            if (subOct)
                return process_sblk<momt_square, true, true>(pitch, drift, stereo, fmdepthV);
            else
                return process_sblk<momt_square, false, true>(pitch, drift, stereo, fmdepthV);
        case momt_triangle:
            if (subOct)
                return process_sblk<momt_triangle, true, true>(pitch, drift, stereo, fmdepthV);
            else
                return process_sblk<momt_triangle, false, true>(pitch, drift, stereo, fmdepthV);
        }
    }
}

static struct ModernTriName : public ParameterDynamicNameFunction
{
    const char *getName(const Parameter *p) const override
    {
        auto flag = p->deform_type;
        int mt = flag & 0xF;
        bool sub = flag & ModernOscillator::mo_submask::mo_subone;
        static char tx[1024];

        std::string subs = sub ? " Sub" : "";
        std::string res = mo_multitype_names[mt] + subs;

        strncpy(tx, res.c_str(), 1024);
        tx[1023] = 0;
        return tx;
    }
} dpwTriName;

void ModernOscillator::init_ctrltypes()
{
    oscdata->p[mo_saw_mix].set_name("Sawtooth");
    oscdata->p[mo_saw_mix].set_type(ct_percent_bipolar);

    oscdata->p[mo_pulse_mix].set_name("Pulse");
    oscdata->p[mo_pulse_mix].set_type(ct_percent_bipolar);

    oscdata->p[mo_tri_mix].set_name("--DYNAMIC-NAME--");
    oscdata->p[mo_tri_mix].set_type(ct_modern_trimix);
    oscdata->p[mo_tri_mix].dynamicName = &dpwTriName;

    oscdata->p[mo_pulse_width].set_name("Width");
    oscdata->p[mo_pulse_width].set_type(ct_percent);
    oscdata->p[mo_pulse_width].val_default.f = 0.5;

    oscdata->p[mo_sync].set_name("Sync");
    oscdata->p[mo_sync].set_type(ct_syncpitch);

    oscdata->p[mo_unison_detune].set_name("Unison Detune");
    oscdata->p[mo_unison_detune].set_type(ct_oscspread);
    oscdata->p[mo_unison_voices].set_name("Unison Voices");
    oscdata->p[mo_unison_voices].set_type(ct_osccount);
}

void ModernOscillator::init_default_values()
{
    oscdata->p[mo_saw_mix].val.f = 0.5;
    oscdata->p[mo_tri_mix].val.f = 0.0;
    oscdata->p[mo_tri_mix].deform_type = 0;
    oscdata->p[mo_pulse_mix].val.f = 0.0;
    oscdata->p[mo_pulse_width].val.f = 0.5;
    oscdata->p[mo_sync].val.f = 0.0;
    oscdata->p[mo_unison_detune].val.f = 0.2;
    oscdata->p[mo_unison_voices].val.i = 1;
}
