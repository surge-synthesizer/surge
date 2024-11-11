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

#include "MSEGModulationHelper.h"
#include <cmath>
#include <iostream>
#include "DebugHelpers.h"
#include "basic_dsp.h" // for limit_range
#include "sst/basic-blocks/dsp/FastMath.h"

namespace Surge
{
namespace MSEG
{

void rebuildCache(MSEGStorage *ms)
{
    forceToConstrainedNormalForm(ms);

    if (ms->loop_start > ms->n_activeSegments - 1)
    {
        ms->loop_start = -1;
    }

    if (ms->loop_end > ms->n_activeSegments - 1)
    {
        ms->loop_end = -1;
    }

    float totald = 0;

    for (int i = 0; i < ms->n_activeSegments; ++i)
    {
        ms->segmentStart[i] = totald;
        totald += ms->segments[i].duration;
        ms->segmentEnd[i] = totald;

        int nextseg = i + 1;

        if (nextseg >= ms->n_activeSegments)
        {
            if (ms->endpointMode == MSEGStorage::EndpointMode::LOCKED)
            {
                ms->segments[i].nv1 = ms->segments[0].v0;
            }
        }
        else
        {
            ms->segments[i].nv1 = ms->segments[nextseg].v0;
        }

        if (ms->segments[i].nv1 != ms->segments[i].v0)
        {
            ms->segments[i].dragcpratio = (ms->segments[i].cpv - ms->segments[i].v0) /
                                          (ms->segments[i].nv1 - ms->segments[i].v0);
        }
    }

    ms->totalDuration = totald;

    if (ms->editMode == MSEGStorage::ENVELOPE)
    {
        ms->envelopeModeDuration = totald;
        ms->envelopeModeNV1 = ms->segments[ms->n_activeSegments - 1].nv1;
    }

    if (ms->editMode == MSEGStorage::LFO && totald != 1.0)
    {
        if (fabs(totald - 1.0) > 1e-5)
        {
            // FIXME: Should never happen but WHAT TO DO HERE!
            // std::cout << "SOFTWARE ERROR" << std::endl;
        }

        ms->totalDuration = 1.0;
        ms->segmentEnd[ms->n_activeSegments - 1] = 1.0;
    }

    for (int i = 0; i < ms->n_activeSegments; ++i)
    {
        constrainControlPointAt(ms, i);
    }

    ms->durationToLoopEnd = ms->totalDuration;
    ms->durationLoopStartToLoopEnd = ms->totalDuration;

    if (ms->n_activeSegments > 0)
    {
        if (ms->loop_end >= 0)
        {
            ms->durationToLoopEnd = ms->segmentEnd[ms->loop_end];
        }

        ms->durationLoopStartToLoopEnd =
            ms->segmentEnd[(ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1)] -
            ms->segmentStart[(ms->loop_start >= 0 ? ms->loop_start : 0)];
    }
}

float valueAt(int ip, float fup, float df, MSEGStorage *ms, EvaluatorState *es, bool forceOneShot)
{
    if (ms->n_activeSegments <= 0)
    {
        return df;
    }

    es->has_triggered = false;
    es->retrigger_FEG = false;
    es->retrigger_AEG = false;

    // This still has some problems but lets try this for now
    double up = (double)ip + fup;

    // If a oneshot is done, it is done
    if (up >= ms->totalDuration &&
        (ms->loopMode == MSEGStorage::LoopMode::ONESHOT || forceOneShot) &&
        (ms->editMode != MSEGStorage::LFO))
    {
        return ms->segments[ms->n_activeSegments - 1].nv1;
    }

    df = limit_range(df, -1.f, 1.f);

    float timeAlongSegment = 0;

    if (es->loopState == EvaluatorState::PLAYING && es->released)
    {
        es->releaseStartPhase = up;
        es->releaseStartValue = es->lastOutput;
        es->loopState = EvaluatorState::RELEASING;
    }

    int idx = -1;

    if (es->loopState == EvaluatorState::PLAYING ||
        ms->loopMode != MSEGStorage::LoopMode::GATED_LOOP)
    {
        idx = timeToSegment(ms, up,
                            forceOneShot || ms->loopMode == MSEGStorage::ONESHOT ||
                                ms->editMode == MSEGStorage::LFO,
                            timeAlongSegment);

        if (idx < 0 || idx >= ms->n_activeSegments)
        {
            return 0;
        }
    }
    else
    {
        if (ms->loop_end == -1 || ms->loop_end >= ms->n_activeSegments)
        {
            return es->releaseStartValue;
        }

        if (es->releaseStartPhase == up)
        {
            // We just released. We know what to do but we might be off by epsilon so...
            idx = ms->loop_end + 1;
            timeAlongSegment = 0;
        }
        else
        {
            double adjustedPhase = up - es->releaseStartPhase + ms->segmentEnd[ms->loop_end];

            // so now find the index
            idx = -1;

            for (int ai = 0; ai < ms->n_activeSegments && idx < 0; ai++)
            {
                if (ms->segmentStart[ai] <= adjustedPhase && ms->segmentEnd[ai] > adjustedPhase)
                {
                    idx = ai;
                }
            }

            if (idx < 0)
            {
                return ms->segments[ms->n_activeSegments - 1].nv1; // We are past the end
            }

            timeAlongSegment = adjustedPhase - ms->segmentStart[idx];
        }
    }

    // detect if we have wrapped around when looping a single segment, see github issue #4546
    if (timeAlongSegment < es->timeAlongSegment)
    {
        es->has_triggered = true;
    }

    // std::cout << up << " " << idx << std::endl;

    auto r = ms->segments[idx];
    bool segInit = false;

    if (idx != es->lastEval || es->has_triggered)
    {
        segInit = true;
        es->lastEval = idx;
        es->retrigger_FEG = ms->segments[idx].retriggerFEG;
        es->retrigger_AEG = ms->segments[idx].retriggerAEG;
        es->has_triggered = false;
    }

    if (!ms->segments[idx].useDeform)
    {
        df = 0;
    }

    if (ms->segments[idx].invertDeform)
    {
        df = -df;
    }

    if (ms->segments[idx].duration <= MSEGStorage::minimumDuration)
    {
        return (ms->segments[idx].v0 + ms->segments[idx].nv1) * 0.5;
    }

    float res = r.v0;

    // we use local copies of these values so we can adjust them in the gated release phase
    float lv0 = r.v0;
    float lv1 = r.nv1;
    float lcpv = r.cpv;

    // So are we in the gated release segment?
    if (es->loopState == EvaluatorState::RELEASING && ms->loopMode == MSEGStorage::GATED_LOOP &&
        idx == ms->loop_end + 1)
    {
        float cpratio = 0.5;

        // Move the point at which we start
        lv0 = es->releaseStartValue;

        if (r.nv1 != r.v0)
        {
            cpratio = (r.cpv - r.v0) / (r.nv1 - r.v0);
        }

        lcpv = cpratio * (r.nv1 - lv0) + lv0;
    }

    switch (r.type)
    {
    case MSEGStorage::segment::HOLD:
    {
        res = lv0;
        break;
    }
    case MSEGStorage::segment::LINEAR:
    case MSEGStorage::segment::SCURVE: // Wuh? S-Curve and Linear are the same? Well kinda
    {
        if (lv0 == lv1)
        {
            es->timeAlongSegment = timeAlongSegment;
            return lv0;
        }

        float frac = timeAlongSegment / r.duration;
        float actualTime = frac;

        /*
         * Here is where they are the same. The S-Curve is just two copies of the
         * deformed line, with one inverted, pushed to the next quadrant.
         */
        bool scurveMirrored = false;

        if (r.type == MSEGStorage::segment::SCURVE)
        {
            /* There is a pathological case here where the deform is non-zero but very small
             * where this results in a floating point underflow. That is, if df is e1-7 then
             * e^df - 1 is basically the last decimal place and you get floating point rounding
             * quantization. So treat all deforms less than 1e-4 as zero.
             *
             * This is totally warranted because we work in float space.
             * Remember e^x = 1 + x + x^2/2 + ... we are in a situation where df = 1e-4
             * and adf = 1e-3 so adf^2 = 1e-6 == 0 at floating point. Safe to ignore on
             * the scale of values of 1. So
             *
             * (e^(af)-1) / (e^a - 1)
             * (1 + af - 1) / (1 + a - 1)
             * = f
             *
             * or: to floating point precision any deform below 1e-4 is the same as no
             * deform up to errors (and those errors are not what we want on the output).
             *
             * See issue #3708 for an example.
             */
            if (fabs(df) > 1e-4)
            {
                // Deform moves the center point
                auto adf = df * 10;
                frac = (exp(adf * frac) - 1) / (exp(adf) - 1);
            }

            if (frac > 0.5)
            {
                frac = 1 - (frac - 0.5) * 2;
                scurveMirrored = true;
            }
            else
            {
                frac = frac * 2;
            }
        }

        // So we want to handle control points
        auto cpd = r.cpv;

        /*
         * Alright so we have a functional form (e^ax-1)/(e^a-1) = y;
         * We also know that since we have vertical only motion here x = 1/2 and y is where we want
         * to hit ( specifically since we are generating a 0,1 line and cpv is -1,1 then
         * here we get y = 0.5 * cpv + 0.5.
         *
         * Fine so lets show our work. I'm going to use X and V for now
         *
         * (e^aX-1)/(e^a-1) = V  @ x=1/2
         * introduce Q = e^a/2
         * (Q - 1) / ( Q^2 - 1 ) = V
         * Q - 1 = V Q^2 - V
         * V Q^2 - Q + ( 1-V ) = 0
         *
         * OK cool we know how to solve that (for V != 0)
         *
         * Q = (1 +/- sqrt( 1 - 4 * V * (1-V) )) / 2 V
         *
         * and since Q = e^a/2
         *
         * a = 2 * log(Q)
         *
         */

        float V = 0.5 * r.cpv + 0.5;
        float amul = 1;

        if (V < 0.5)
        {
            amul = -1;
            V = 1 - V;
        }

        float disc = (1 - 4 * V * (1 - V));
        float a = 0;

        if (fabs(V) > 1e-3)
        {
            float Q = limit_range((1 - sqrt(disc)) / (2 * V), 0.00001f, 1000000.f);
            a = amul * 2 * log(Q);
        }

        // OK so frac is the 0,1 line point
        auto cpline = frac;

        if (fabs(a) > 1e-3)
        {
            cpline = (exp(a * frac) - 1) / (exp(a) - 1);
        }

        if (r.type == MSEGStorage::segment::LINEAR)
        {
            // This is the equivalent of LFOModulationSource.cpp::bend3
            float dfa = -0.5f * limit_range(df, -3.f, 3.f);

            float x = (2 * cpline - 1);
            x = x - dfa * x * x + dfa;
            x = x - dfa * x * x + dfa; // do twice because bend3 does it twice
            cpline = 0.5 * (x + 1);
        }

        // OK and now we have to dup it if we are in s-curve
        if (r.type == MSEGStorage::segment::SCURVE)
        {
            if (!scurveMirrored)
            {
                cpline *= 0.5;
            }
            else
            {
                cpline = 1.0 - 0.5 * cpline;
            }
        }

        // cpline will still be 0,1 so now we need to transform it
        res = cpline * (lv1 - lv0) + lv0;
        break;
    }
    case MSEGStorage::segment::QUAD_BEZIER:
    {
        /*
        ** First let's correct the control point so that the
        ** user specified one is through the curve. This means
        ** that the line from the center of the segment connecting
        ** the endpoints to the control point pushes out 2x
        */

        float cpv = lcpv;
        float cpt = r.cpduration * r.duration;

        // If we are positioned exactly at the midpoint our calculation below to find time will fail
        // so walk off a smidge
        if (fabs(cpt - r.duration * 0.5) < 1e-5)
        {
            cpt += 1e-4;
        }

        // here's the midpoint along the connecting curve
        float tp = r.duration / 2;
        float vp = (lv1 - lv0) / 2 + lv0;

        // The distance from tp,vp to cpt,cpv
        float dt = (cpt - tp), dy = (cpv - vp);

        cpv = vp + 2 * dy;
        cpt = tp + 2 * dt;

        // B = (1-t)^2 P0 + 2 t (1-t) P1 + t^2 P2
        float ttarget = timeAlongSegment;
        float px0 = 0, px1 = cpt, px2 = r.duration, py0 = lv0, py1 = cpv, py2 = lv1;

        /*
        ** OK so we want to find the bezier t corresponding to phase ttarget
        **
        ** (1-t)^2 px0 + 2 * (1-t) t px1 + t * t px2 = ttarget;
        **
        ** Luckily we know px0 = 0. So
        **
        ** 2 * ( 1-t ) * t * px1 + t * t * px2 - ttarget = 0;
        ** 2 * t * px1 - 2 * t^2 * px1 + t^2 * px2 - ttarget = 0;
        ** (px2 - 2 * px1) t^2 + 2 * px1 * t - ttarget = 0;
        **
        */
        float a = px2 - 2 * px1;
        float b = 2 * px1;
        float c = -ttarget;
        float disc = b * b - 4 * a * c;

        if (a == 0 || disc < 0)
        {
            // This means we have a line between v0 and nv1
            float frac = timeAlongSegment / r.duration;
            res = frac * lv1 + (1 - frac) * lv0;
        }
        else
        {
            float t = (-b + sqrt(b * b - 4 * a * c)) / (2 * a);

            if (df < 0)
            {
                t = pow(t, 1.0 + df * 0.7);
            }
            if (df > 0)
            {
                t = pow(t, 1.0 + df * 3);
            }

            /*
            ** And now evaluate the bezier in y with that t
            */
            res = (1 - t) * (1 - t) * py0 + 2 * (1 - t) * t * py1 + t * t * py2;
        }

        break;
    }
    case MSEGStorage::segment::BUMP:
    {
        auto t = timeAlongSegment / r.duration;

        auto d = (-df * 0.5) + 0.5;
        auto deform = 20.f + ((d * d * d) * 500.f);

        auto c = r.cpv;

        auto g = exp(-deform * (t - 0.5) * (t - 0.5));
        auto l = ((lv1 - lv0) * t) + lv0;
        auto q = c - ((lv0 + lv1) * 0.5);

        res = l + (q * g);

        break;
    }
    case MSEGStorage::segment::SINE:
    case MSEGStorage::segment::SAWTOOTH:
    case MSEGStorage::segment::TRIANGLE:
    case MSEGStorage::segment::SQUARE:
    {
        float pct = (r.cpv + 1) * 0.5;
        float as = 5.0;
        float scaledpct = (exp(as * pct) - 1) / (exp(as) - 1);
        int steps = (int)(scaledpct * 100);
        auto frac = timeAlongSegment / r.duration;
        float kernel = 0;

        /*
         * At this point you need to generate a waveform which starts at 1,
         * ends at -1, and has 'steps' oscillations in between, spanning
         * the phase space f=[0,1].
         */
        if (r.type == MSEGStorage::segment::SINE)
        {
            /*
             * To meet the -1, 1 constraint on the endpoints we use cos, not sine, so
             * the first point is one, and multiply the frequency by 3pi, 5pi, 7pi etc...
             * so we get one and a half waveforms
             */
            float mul = (1 + 2 * steps) * M_PI;
            kernel = cos(mul * frac);
        }

        if (r.type == MSEGStorage::segment::SAWTOOTH)
        {
            double mul = steps + 1;
            double phase = mul * frac;
            double dphase = phase - (int)phase;
            kernel = 1 - 2 * dphase;
        }

        if (r.type == MSEGStorage::segment::TRIANGLE)
        {
            double mul = (0.5 + steps);
            double phase = mul * frac;
            double dphase = phase - (int)phase;

            if (dphase < 0.5)
            {
                // line 1 @ 0 to -1 @ 1/2
                kernel = 1 - 4 * dphase;
            }
            else
            {
                auto qphase = dphase - 0.5;
                // line -1 @ 0 to 1 @ 1/2
                kernel = 4 * qphase - 1;
            }
        }

        if (r.type == MSEGStorage::segment::SQUARE)
        {
            // dDeform does PWM here
            float mul = steps + 1;
            float tphase = mul * frac;
            float phase = tphase - (int)tphase;
            float pw = (df + 1) / 2.0; // so now 0 -> 1
            kernel = (phase < pw ? 1 : -1);
        }

        // This is the equivalent of LFOModulationSource.cpp::bend3
        float a = -0.5f * limit_range(df, -3.f, 3.f);

        kernel = kernel - a * kernel * kernel + a;
        kernel = kernel - a * kernel * kernel + a; // do twice because bend3 does it twice

        res = (lv0 - lv1) * ((kernel + 1) * 0.5) + lv1;

        break;
    }

    case MSEGStorage::segment::STAIRS:
    {
        auto pct = (r.cpv + 1) * 0.5;
        auto as = 5.0;
        auto scaledpct = (exp(as * pct) - 1) / (exp(as) - 1);
        auto steps = (int)(scaledpct * 100) + 2;
        auto frac = (float)((int)(steps * timeAlongSegment / r.duration)) / (steps - 1);

        if (df < 0)
        {
            frac = pow(frac, 1.0 + df * 0.7);
        }
        if (df > 0)
        {
            frac = pow(frac, 1.0 + df * 3.0);
        }

        res = frac * lv1 + (1 - frac) * lv0;

        break;
    }
    case MSEGStorage::segment::SMOOTH_STAIRS:
    {
        auto pct = (r.cpv + 1) * 0.5;
        auto as = 5.0;
        auto scaledpct = (exp(as * pct) - 1) / (exp(as) - 1);
        auto steps = (int)(scaledpct * 100) + 2;
        auto frac = timeAlongSegment / r.duration;

        auto c = df < 0.f ? 1.0 + df * 0.7 : 1.0 + df * 3.0;
        auto z = pow(frac, c);

        auto q = floor(z * steps) / steps;

        auto r = ((z - q) * steps);
        auto b = r * 2 - 1;

        res = ((((b * b * b) + 1) / (2 * steps)) + q);
        res = (res * (lv1 - lv0)) + lv0;

        break;
    }
    case MSEGStorage::segment::BROWNIAN:
    {
        static constexpr int validx = 0, lasttime = 1, outidx = 2;

        if (segInit)
        {
            es->msegState[validx] = lv0;
            es->msegState[outidx] = lv0;
            es->msegState[lasttime] = 0;
        }

        float targetTime = timeAlongSegment / r.duration;

        if (targetTime >= 1)
        {
            res = lv1;
        }
        else if (targetTime <= 0)
        {
            res = lv0;
        }
        else if (targetTime <= es->msegState[lasttime])
        {
            res = es->msegState[outidx];
        }
        else
        {
            while (es->msegState[lasttime] < targetTime && es->msegState[lasttime] < 1)
            {
                auto ncd = r.cpduration; // 0-1

                // OK so the closer this is to 1 the more spiky we should be, which is basically
                // increasing the dt.

                // The slowest is 5 steps (max dt is 0.2)
                float sdt = 0.2f * (1 - ncd) * (1 - ncd);
                float dt = std::min(std::max(sdt, 0.0001f), 1 - es->msegState[lasttime]);

                if (es->msegState[lasttime] < 1)
                {
                    float lincoef = (lv1 - es->msegState[validx]) / (1 - es->msegState[lasttime]);
                    float randcoef = 0.1 * lcpv; // CPV is on -1,1
                    /*
                     * Modification to standard bridge. We want to move away from 1,-1 so lets
                     * explicitly bounce away from them if random will push us over
                     */
                    auto uniformRand = es->urd(es->gen);
                    auto linstp = es->msegState[validx] + lincoef * dt;
                    float randup = randcoef;
                    float randdn = randcoef;

                    if (linstp + randup > 1)
                    {
                        randup = 1 - linstp;
                    }
                    else if (linstp - randdn < -1)
                    {
                        randdn = linstp + 1;
                    }

                    auto randadj = (uniformRand > 0 ? randup * uniformRand : randdn * uniformRand);

                    es->msegState[validx] += limit_range(lincoef * dt + randadj, -1.f, 1.f);
                    es->msegState[lasttime] += dt;
                }
                else
                {
                    es->msegState[validx] = lv1;
                }
            }

            if (lcpv < 0)
            {
                // Snap to between 1 and 24 values.
                int a = floor(-lcpv * 24) + 1;
                es->msegState[outidx] = floor(es->msegState[validx] * a) * 1.f / a;
            }
            else
            {
                es->msegState[outidx] = es->msegState[validx];
            }

            // so basically softclip it
            es->msegState[outidx] = limit_range(es->msegState[outidx], -1.f, 1.f);

            res = es->msegState[outidx];
        }

        break;
    }
    case MSEGStorage::segment::RESERVED:
        // Should never occur
        break;
    }

    // std::cout << _D(timeAlongSegment) << _D(r.type) << _D(r.duration) << _D(lv0) << std::endl;

    es->timeAlongSegment = timeAlongSegment;

    res = limit_range(res, -1.f, 1.f);
    es->lastOutput = res;

    return res;
}

int timeToSegment(MSEGStorage *ms, double t)
{
    float x;
    return timeToSegment(ms, t, true, x);
}

int timeToSegment(MSEGStorage *ms, double t, bool ignoreLoops, float &amountAlongSegment)
{
    if (ms->totalDuration < MSEGStorage::minimumDuration)
    {
        return -1;
    }

    if (ignoreLoops)
    {
        if (t >= ms->totalDuration)
        {
            double nup = t / ms->totalDuration;

            t -= (int)nup * ms->totalDuration;

            if (t < 0)
            {
                t += ms->totalDuration;
            }
        }

        int idx = -1;

        for (int i = 0; i < ms->n_activeSegments; ++i)
        {
            if (t >= ms->segmentStart[i] && t < ms->segmentEnd[i])
            {
                idx = i;
                amountAlongSegment = t - ms->segmentStart[i];

                break;
            }
        }

        return idx;
    }
    else
    {
        // The loop case is more tedious
        int le = (ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1);
        int ls = (ms->loop_start >= 0 ? ms->loop_start : 0);

        // So are we before the first loop end point
        if (t <= ms->durationToLoopEnd)
        {
            for (int i = 0; i < ms->n_activeSegments; ++i)
                if (t >= ms->segmentStart[i] && t <= ms->segmentEnd[i])
                {
                    amountAlongSegment = t - ms->segmentStart[i];

                    return i;
                }
        }
        else if (ms->loop_start > ms->loop_end && ms->loop_start >= 0 && ms->loop_end >= 0)
        {
            // This basically means we just iterate around the loop_end endpoint which we do by
            // saying we are basically at the end point all the way along
            auto idx = ms->loop_end;

            amountAlongSegment = ms->segments[idx].duration;

            return idx;
        }
        else
        {
            double nt = t - ms->durationToLoopEnd;
            // OK so now we have a cycle which has length durationFromStartToEnd;
            double nup = nt / ms->durationLoopStartToLoopEnd;

            nt -= (int)nup * ms->durationLoopStartToLoopEnd;

            if (nt < 0)
            {
                nt += ms->durationLoopStartToLoopEnd;
            }

            // and we need to offset it by the starting point
            nt += ms->segmentStart[ls];

            for (int i = 0; i < ms->n_activeSegments; ++i)
                if (nt >= ms->segmentStart[i] && nt <= ms->segmentEnd[i])
                {
                    amountAlongSegment = nt - ms->segmentStart[i];

                    return i;
                }
        }

        return 0;
    }
}

void changeTypeAt(MSEGStorage *ms, float t, MSEGStorage::segment::Type type)
{
    auto idx = timeToSegment(ms, t);

    if (idx < ms->n_activeSegments)
    {
        ms->segments[idx].type = type;
    }
}

void insertAtIndex(MSEGStorage *ms, int insertIndex)
{
    for (int i = std::max(ms->n_activeSegments + 1, max_msegs - 1); i > insertIndex; --i)
    {
        ms->segments[i] = ms->segments[i - 1];
    }

    ms->segments[insertIndex].type = MSEGStorage::segment::LINEAR;
    ms->segments[insertIndex].v0 = 0;
    ms->segments[insertIndex].duration = 0.25;
    ms->segments[insertIndex].useDeform = true;
    ms->segments[insertIndex].invertDeform = false;
    ms->segments[insertIndex].retriggerFEG = false;
    ms->segments[insertIndex].retriggerAEG = false;

    int nxt = insertIndex + 1;

    if (nxt >= ms->n_activeSegments)
    {
        nxt = 0;
    }

    ms->segments[insertIndex].cpv = ms->segments[nxt].v0 * 0.5;
    ms->segments[insertIndex].cpduration = 0.125;

    /*
     * Handle the loops. We have just inserted at index so if start or end
     * is later we need to push them out
     */
    if (ms->loop_start >= insertIndex)
    {
        ms->loop_start++;
    }

    if (ms->loop_end >= insertIndex - 1)
    {
        ms->loop_end++;
    }

    ms->n_activeSegments++;
}

void insertAfter(MSEGStorage *ms, float t)
{
    auto idx = timeToSegment(ms, t);

    if (idx < 0)
    {
        idx = 0;
    }

    idx++;

    insertAtIndex(ms, idx);
}

void insertBefore(MSEGStorage *ms, float t)
{
    auto idx = timeToSegment(ms, t);

    if (idx < 0)
    {
        idx = 0;
    }

    insertAtIndex(ms, idx);
}

void extendTo(MSEGStorage *ms, float t, float nv)
{
    if (ms->editMode == MSEGStorage::LFO)
    {
        return;
    }

    if (t < ms->totalDuration)
    {
        return;
    }

    nv = limit_range(nv, -1.f, 1.f);

    // We want to keep the loop end at the same spot when we extend
    bool fixupLoopEnd = false;

    if (ms->loop_end < 0 || ms->loop_end == ms->n_activeSegments - 1)
    {
        fixupLoopEnd = true;
    }

    insertAtIndex(ms, ms->n_activeSegments);

    if (fixupLoopEnd)
    {
        if (ms->n_activeSegments > 1)
        {
            ms->loop_end = ms->n_activeSegments - 2;
        }
    }

    auto sn = ms->n_activeSegments - 1;

    ms->segments[sn].type = MSEGStorage::segment::LINEAR;

    if (sn == 0)
    {
        ms->segments[sn].v0 = 0;
    }
    else
    {
        ms->segments[sn].v0 = ms->segments[sn - 1].nv1;
    }

    float dt = t - ms->totalDuration;

    ms->segments[sn].duration = dt;
    ms->segments[sn].cpduration = 0.5;
    ms->segments[sn].cpv = 0;
    ms->segments[sn].nv1 = nv;

    if (ms->endpointMode == MSEGStorage::EndpointMode::LOCKED)
    {
        // The first point has to match where I just clicked. Adjust it and its control point
        float cpdratio = 0.5;

        if (ms->segments[0].duration > 0)
        {
            cpdratio = ms->segments[0].cpduration / ms->segments[0].duration;
        }

        float cpvratio = 0.5;

        if (ms->segments[0].nv1 != ms->segments[0].v0)
        {
            cpvratio = (ms->segments[0].cpv - ms->segments[0].v0) /
                       (ms->segments[0].nv1 - ms->segments[0].v0);
        }

        ms->segments[0].v0 = nv;
        ms->segments[0].cpduration = cpdratio * ms->segments[0].duration;
        ms->segments[0].cpv =
            (ms->segments[0].nv1 - ms->segments[0].v0) * cpvratio + ms->segments[0].v0;
    }
}

void splitSegment(MSEGStorage *ms, float t, float nv)
{
    int idx = timeToSegment(ms, t);

    nv = limit_range(nv, -1.f, 1.f);

    if (idx >= 0)
    {
        while (t > ms->totalDuration)
        {
            t -= ms->totalDuration;
        }

        while (t < 0)
        {
            t += ms->totalDuration;
        }

        float pv1 = ms->segments[idx].nv1;
        float dt = (t - ms->segmentStart[idx]) / (ms->segments[idx].duration);
        auto q = ms->segments[idx]; // take a copy

        insertAtIndex(ms, idx + 1);

        ms->segments[idx].nv1 = nv;
        ms->segments[idx].duration *= dt;

        ms->segments[idx + 1].v0 = nv;
        ms->segments[idx + 1].type = ms->segments[idx].type;
        ms->segments[idx + 1].nv1 = pv1;
        ms->segments[idx + 1].duration = q.duration * (1 - dt);
        ms->segments[idx + 1].useDeform = ms->segments[idx].useDeform;
        ms->segments[idx + 1].invertDeform = ms->segments[idx].invertDeform;
        ms->segments[idx + 1].retriggerFEG = ms->segments[idx].retriggerFEG;
        ms->segments[idx + 1].retriggerAEG = ms->segments[idx].retriggerAEG;

        // Now these are normalized this is easy
        ms->segments[idx].cpduration = q.cpduration;
        ms->segments[idx].cpv = q.cpv;
        ms->segments[idx + 1].cpduration = q.cpduration;
        ms->segments[idx + 1].cpv = q.cpv;
    }
}

void unsplitSegment(MSEGStorage *ms, float t, bool wrapTime)
{
    // Can't unsplit a single segment
    if (ms->n_activeSegments - 1 == 0)
    {
        return;
    }

    int idx = timeToSegment(ms, t);

    if (!wrapTime && t >= ms->totalDuration)
    {
        idx = ms->n_activeSegments - 1;
    }

    if (idx < 0)
    {
        idx = 0;
    }

    if (idx >= ms->n_activeSegments - 1)
    {
        idx = ms->n_activeSegments - 1;
    }

    int prior = idx;

    if ((ms->segmentEnd[idx] - t < t - ms->segmentStart[idx]) || t >= ms->totalDuration)
    {
        if (idx == ms->n_activeSegments - 1)
        {
            // We are just deleting the last segment
            deleteSegment(ms, idx);

            return;
        }

        idx++;
        prior = idx - 1;
    }
    else
    {
        prior = idx - 1;
    }

    if (prior < 0)
    {
        prior = ms->n_activeSegments - 1;
    }

    if (prior == idx)
    {
        return;
    }

    float cpdratio = ms->segments[prior].cpduration / ms->segments[prior].duration;
    float cpvratio = 0.5;

    if (ms->segments[prior].nv1 != ms->segments[prior].v0)
    {
        cpvratio = (ms->segments[prior].cpv - ms->segments[prior].v0) /
                   (ms->segments[prior].nv1 - ms->segments[prior].v0);
    }

    ms->segments[prior].duration += ms->segments[idx].duration;
    ms->segments[prior].nv1 = ms->segments[idx].nv1;
    ms->segments[prior].cpduration = cpdratio * ms->segments[prior].duration;

    for (int i = idx; i < ms->n_activeSegments - 1; ++i)
    {
        ms->segments[i] = ms->segments[i + 1];
    }

    ms->n_activeSegments--;

    if (ms->loop_start > idx)
    {
        ms->loop_start--;
    }

    if (ms->loop_end >= idx)
    {
        ms->loop_end--;
    }
}

void deleteSegment(MSEGStorage *ms, float t)
{
    // Can't delete the last segment
    if (ms->n_activeSegments <= 1)
    {
        return;
    }

    auto idx = timeToSegment(ms, t);

    deleteSegment(ms, idx);
}

void deleteSegment(MSEGStorage *ms, int idx)
{
    for (int i = idx; i < ms->n_activeSegments - 1; ++i)
    {
        ms->segments[i] = ms->segments[i + 1];
    }

    ms->n_activeSegments--;

    if (ms->editMode == MSEGStorage::LFO)
    {
        // We need to fill up the last one to span.
        auto ei = ms->n_activeSegments - 1;
        ms->segmentEnd[ei] = 1.0;
        auto cd = 0.f;
        for (int i = 0; i < ei; ++i)
            cd += ms->segments[i].duration;

        ms->segments[ei].duration = 1.0 - cd;
        ms->segments[ei].cpduration += 1.0 - cd;

        rebuildCache(ms);
    }

    if (ms->loop_start > idx)
    {
        ms->loop_start--;
    }

    if (ms->loop_end >= idx)
    {
        ms->loop_end--;
    }
}

void resetControlPoint(MSEGStorage *ms, float t)
{
    auto idx = timeToSegment(ms, t);

    if (idx >= 0 && idx < ms->n_activeSegments)
    {
        resetControlPoint(ms, idx);
    }
}

void resetControlPoint(MSEGStorage *ms, int idx)
{
    ms->segments[idx].cpduration = 0.5;
    ms->segments[idx].cpv = 0.0;

    if (ms->segments[idx].type == MSEGStorage::segment::QUAD_BEZIER)
    {
        ms->segments[idx].cpv = 0.5 * (ms->segments[idx].v0 + ms->segments[idx].nv1);
    }
}

void constrainControlPointAt(MSEGStorage *ms, int idx)
{
    if (!std::isfinite(ms->segments[idx].cpduration))
        ms->segments[idx].cpduration = 0.5;
    if (!std::isfinite(ms->segments[idx].cpv))
        ms->segments[idx].cpv = 0.0;

    ms->segments[idx].cpduration = limit_range(ms->segments[idx].cpduration, 0.f, 1.f);
    ms->segments[idx].cpv = limit_range(ms->segments[idx].cpv, -1.f, 1.f);
}

void forceToConstrainedNormalForm(MSEGStorage *ms) // removes any nans etc
{
    for (auto &seg : ms->segments)
    {
        if (!std::isfinite(seg.v0))
            seg.v0 = 0;
        if (!std::isfinite(seg.cpv))
            seg.cpv = 0;
        if (!std::isfinite(seg.duration))
            seg.duration = 0.1;
        if (!std::isfinite(seg.cpduration))
            seg.cpduration = 0.6;
    }
}

void scaleDurations(MSEGStorage *ms, float factor, float maxDuration)
{
    if (maxDuration > 0 && ms->totalDuration * factor > maxDuration)
    {
        factor = maxDuration / ms->totalDuration;
    }

    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].duration *= factor;
    }

    Surge::MSEG::rebuildCache(ms);
}

void scaleValues(MSEGStorage *ms, float factor)
{
    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].v0 *= factor;
    }

    if (ms->endpointMode == MSEGStorage::EndpointMode::FREE)
    {
        ms->segments[ms->n_activeSegments - 1].nv1 *= factor;
    }

    Surge::MSEG::rebuildCache(ms);
}

void setAllDurationsTo(MSEGStorage *ms, float value)
{
    if (ms->editMode == MSEGStorage::LFO)
    {
        value = 1.f / ms->n_activeSegments;
    }

    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].duration = value;
    }

    Surge::MSEG::rebuildCache(ms);
}

void mirrorMSEG(MSEGStorage *ms)
{
    int h = 0, t = ms->n_activeSegments - 1;
    auto v0 = ms->segments[0].v0;

    while (h < t)
    {
        // endpoints become starting points
        std::swap(ms->segments[h], ms->segments[t]);
        ms->segments[h].v0 = ms->segments[h].nv1;
        ms->segments[t].v0 = ms->segments[t].nv1;

        // move the tail and head closer
        h++;
        t--;
    };

    // special case when we have an odd number of nodes
    if (h == t)
    {
        ms->segments[h].v0 = ms->segments[h].nv1;
    }

    // special case end node in start/end unlinked mode
    if (ms->endpointMode == MSEGStorage::EndpointMode::FREE)
    {
        ms->segments[ms->n_activeSegments - 1].nv1 = v0;
    }

    // adjust curvature to flip it the other way around for curve types that need this
    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        switch (ms->segments[i].type)
        {
        case MSEGStorage::segment::Type::LINEAR:
            ms->segments[i].cpv *= -1.f;
            break;
        case MSEGStorage::segment::Type::QUAD_BEZIER:
            ms->segments[i].cpduration = 1.f - ms->segments[i].cpduration;
            break;
        default:
            // No need to adjust anyone else
            break;
        }
    }

    Surge::MSEG::rebuildCache(ms);
}

void clearMSEG(MSEGStorage *ms)
{
    ms->editMode = MSEGStorage::EditMode::ENVELOPE;
    ms->loopMode = MSEGStorage::LoopMode::LOOP;
    ms->endpointMode = MSEGStorage::EndpointMode::FREE;

    ms->n_activeSegments = 1;

    ms->segments[0].duration = 1.f;
    ms->segments[0].type = MSEGStorage::segment::LINEAR;
    ms->segments[0].cpv = 0.f;
    ms->segments[0].cpduration = 0.5;
    ms->segments[0].v0 = 1.f;
    ms->segments[0].nv1 = 0.f;
    ms->segments[0].useDeform = true;
    ms->segments[0].invertDeform = false;
    ms->segments[0].retriggerFEG = false;
    ms->segments[0].retriggerAEG = false;

    ms->loop_start = 0;
    ms->loop_end = ms->n_activeSegments - 1;

    Surge::MSEG::rebuildCache(ms);
}

void blankAllSegments(MSEGStorage *ms)
{
    for (int i = 0; i < max_msegs; ++i)
    {
        ms->segments[i].duration = 0;
        ms->segments[i].dragDuration = 0;
        ms->segments[i].v0 = 0;
        ms->segments[i].dragv0 = 0;
        ms->segments[i].nv1 = 0;
        ms->segments[i].dragv1 = 0;
        ms->segments[i].cpduration = 0.5;
        ms->segments[i].cpv = 0.0;
        ms->segments[i].dragcpv = 0.0;
        ms->segments[i].dragcpratio = 0.5;
        ms->segments[i].useDeform = true;
        ms->segments[i].invertDeform = false;
        ms->segments[i].retriggerFEG = false;
        ms->segments[i].retriggerAEG = false;
        ms->segments[i].type = MSEGStorage::segment::Type::LINEAR;
    }
}

void createInitVoiceMSEG(MSEGStorage *ms)
{
    ms->editMode = MSEGStorage::EditMode::ENVELOPE;
    ms->loopMode = MSEGStorage::LoopMode::GATED_LOOP;
    ms->endpointMode = MSEGStorage::EndpointMode::FREE;

    blankAllSegments(ms);

    ms->n_activeSegments = 4;

    ms->segments[0].duration = 1.f;
    ms->segments[0].type = MSEGStorage::segment::LINEAR;
    ms->segments[0].cpv = 0.8;
    ms->segments[0].cpduration = 0.5;
    ms->segments[0].v0 = 0.f;

    ms->segments[1].duration = 1.f;
    ms->segments[1].type = MSEGStorage::segment::LINEAR;
    ms->segments[1].cpv = 0.8;
    ms->segments[1].cpduration = 0.5;
    ms->segments[1].v0 = 1.f;

    ms->segments[2].duration = 1.f;
    ms->segments[2].type = MSEGStorage::segment::LINEAR;
    ms->segments[2].cpv = 0.5;
    ms->segments[2].cpduration = 0.8;
    ms->segments[2].v0 = 0.5;

    ms->segments[3].duration = 1.f;
    ms->segments[3].type = MSEGStorage::segment::LINEAR;
    ms->segments[3].cpv = 0.5;
    ms->segments[3].cpduration = 0.8;
    ms->segments[3].v0 = 0.5;
    ms->segments[3].nv1 = 0.f;

    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].invertDeform = false;
        ms->segments[i].useDeform = true;
        ms->segments[i].retriggerFEG = false;
        ms->segments[i].retriggerAEG = false;
    }

    ms->loop_start = 2;
    ms->loop_end = 2;

    Surge::MSEG::rebuildCache(ms);
}

void createInitSceneMSEG(MSEGStorage *ms)
{
    ms->editMode = MSEGStorage::EditMode::LFO;
    ms->loopMode = MSEGStorage::LoopMode::LOOP;

    ms->n_activeSegments = 4;

    blankAllSegments(ms);

    ms->segments[0].duration = 0.25f;
    ms->segments[0].type = MSEGStorage::segment::LINEAR;
    ms->segments[0].cpv = 0.f;
    ms->segments[0].cpduration = 0.5;
    ms->segments[0].v0 = 0.f;
    ms->segments[0].invertDeform = false;
    ms->segments[0].retriggerFEG = false;
    ms->segments[0].retriggerAEG = false;

    ms->segments[1].duration = 0.25f;
    ms->segments[1].type = MSEGStorage::segment::LINEAR;
    ms->segments[1].cpv = 0.f;
    ms->segments[1].cpduration = 0.5;
    ms->segments[1].v0 = 1.f;
    ms->segments[1].invertDeform = true;
    ms->segments[1].retriggerFEG = false;
    ms->segments[1].retriggerAEG = false;

    ms->segments[2].duration = 0.25f;
    ms->segments[2].type = MSEGStorage::segment::LINEAR;
    ms->segments[2].cpv = 0.f;
    ms->segments[2].cpduration = 0.5;
    ms->segments[2].v0 = 0.f;
    ms->segments[2].invertDeform = false;
    ms->segments[2].retriggerFEG = false;
    ms->segments[2].retriggerAEG = false;

    ms->segments[3].duration = 0.25f;
    ms->segments[3].type = MSEGStorage::segment::LINEAR;
    ms->segments[3].cpv = 0.f;
    ms->segments[3].cpduration = 0.5;
    ms->segments[3].v0 = -1.f;
    ms->segments[3].invertDeform = true;
    ms->segments[3].retriggerFEG = false;
    ms->segments[3].retriggerAEG = false;

    ms->loop_start = 0;
    ms->loop_end = ms->n_activeSegments - 1;

    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].useDeform = true;
    }

    Surge::MSEG::rebuildCache(ms);
}

void createStepseqMSEG(MSEGStorage *ms, int numSegments)
{
    ms->endpointMode = MSEGStorage::EndpointMode::FREE;
    ms->loopMode = MSEGStorage::LOOP;

    ms->n_activeSegments = numSegments;

    auto stepLen = (ms->editMode == MSEGStorage::EditMode::ENVELOPE) ? 1.f : (1.f / numSegments);

    for (int i = 0; i < numSegments; i++)
    {
        ms->segments[i].duration = stepLen;
        ms->segments[i].type = MSEGStorage::segment::HOLD;
        ms->segments[i].v0 = (1.f / (numSegments - 1)) * i;
    }

    ms->segments[numSegments - 1].nv1 = ms->segments[0].v0;

    ms->loop_start = 0;
    ms->loop_end = numSegments - 1;

    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].useDeform = true;
        ms->segments[i].invertDeform = false;
        ms->segments[i].retriggerFEG = false;
        ms->segments[i].retriggerAEG = false;
    }

    Surge::MSEG::rebuildCache(ms);
}

void createSawMSEG(MSEGStorage *ms, int numSegments, float curve)
{
    bool isEnvMode = (ms->editMode == MSEGStorage::EditMode::ENVELOPE);
    auto stepLen = isEnvMode ? 1.f : (1.f / numSegments);

    if (isEnvMode)
    {
        ms->endpointMode = MSEGStorage::EndpointMode::FREE;
    }

    ms->loopMode = MSEGStorage::LOOP;

    ms->n_activeSegments = (numSegments * 2) - isEnvMode;

    for (int i = 0; i < numSegments; i++)
    {
        ms->segments[i * 2].duration = stepLen;
        ms->segments[i * 2].type = MSEGStorage::segment::LINEAR;
        ms->segments[i * 2].cpduration = 0.5;
        ms->segments[i * 2].cpv = curve;
        ms->segments[i * 2].v0 = 1.f;

        ms->segments[(i * 2) + 1].duration = 0.f;
        ms->segments[(i * 2) + 1].type = MSEGStorage::segment::LINEAR;
        ms->segments[(i * 2) + 1].cpduration = 0.5;
        ms->segments[(i * 2) + 1].cpv = 0.f;
        ms->segments[(i * 2) + 1].v0 = -1.f;
    }

    ms->segments[ms->n_activeSegments - 1].nv1 = -1.f;

    ms->loop_start = 0;
    ms->loop_end = ms->n_activeSegments - 1;

    for (int i = 0; i < ms->n_activeSegments; i++)
    {
        ms->segments[i].useDeform = true;
        ms->segments[i].invertDeform = false;
        ms->segments[i].retriggerFEG = false;
        ms->segments[i].retriggerAEG = false;
    }

    Surge::MSEG::rebuildCache(ms);
}

void createSinLineMSEG(MSEGStorage *ms, int numSegments)
{
    float dp = 2.0 * M_PI / numSegments;
    float dt = 1.0 / numSegments;

    ms->loopMode = MSEGStorage::LOOP;
    ms->endpointMode = MSEGStorage::EndpointMode::LOCKED;

    ms->n_activeSegments = numSegments;

    for (int i = 0; i < numSegments; ++i)
    {
        ms->segments[i].duration = dt;
        ms->segments[i].v0 = sin(i * dp);
        ms->segments[i].nv1 = sin((i + 1) * dp);
        ms->segments[i].cpduration = 0.5;
        ms->segments[i].cpv = 0.0;
        ms->segments[i].type = MSEGStorage::segment::LINEAR;
        ms->segments[i].useDeform = true;
        ms->segments[i].invertDeform = false;
        ms->segments[i].retriggerFEG = false;
        ms->segments[i].retriggerAEG = false;
    }

    ms->loop_start = 0;
    ms->loop_end = ms->n_activeSegments - 1;

    Surge::MSEG::rebuildCache(ms);
}

void modifyEditMode(MSEGStorage *ms, MSEGStorage::EditMode em)
{
    if (em == ms->editMode)
    {
        return;
    }

    float targetDuration = 1.0;

    if (ms->editMode == MSEGStorage::LFO && em == MSEGStorage::ENVELOPE)
    {
        if (ms->envelopeModeDuration > 0)
        {
            targetDuration = ms->envelopeModeDuration;
        }

        if (ms->envelopeModeNV1 >= -1)
        {
            ms->segments[ms->n_activeSegments - 1].nv1 = ms->envelopeModeNV1;
        }
    }

    float durRatio = targetDuration / ms->totalDuration;

    for (auto &s : ms->segments)
    {
        s.duration *= durRatio;
    }

    ms->editMode = em;
    rebuildCache(ms);
}

void adjustDurationInternal(MSEGStorage *ms, int idx, float d, float snapResolution,
                            float upperBound = 0)
{
    if (snapResolution <= 0)
    {
        ms->segments[idx].duration = std::max(0.f, ms->segments[idx].duration + d);
    }
    else
    {
        ms->segments[idx].dragDuration = std::max(0.f, ms->segments[idx].dragDuration + d);

        auto target = (float)(round((ms->segmentStart[idx] + ms->segments[idx].dragDuration) /
                                    snapResolution) *
                              snapResolution) -
                      ms->segmentStart[idx];

        if (upperBound > 0 && target > upperBound)
        {
            target = ms->segments[idx].duration;
        }

        if (target < MSEGStorage::minimumDuration)
        {
            target = ms->segments[idx].duration;
        }

        ms->segments[idx].duration = target;
    }
}

void adjustDurationShiftingSubsequent(MSEGStorage *ms, int idx, float dx, float snap,
                                      float maxDuration)
{
    if (ms->editMode == MSEGStorage::LFO)
    {
        if (ms->segmentEnd[idx] + dx > 1.0)
        {
            dx = 1.0 - ms->segmentEnd[idx];
        }

        if (ms->segmentEnd[idx] + dx < 0.0)
        {
            dx = ms->segmentEnd[idx];
        }

        if (-dx > ms->segments[idx].duration)
        {
            dx = -ms->segments[idx].duration;
        }
    }

    if (maxDuration > 0 && dx > 0 && ms->totalDuration + dx > maxDuration)
    {
        dx = maxDuration - ms->totalDuration;
    }

    float durationChange = dx;

    if (idx >= 0)
    {
        auto rcv = 0.5;

        if (ms->segments[idx].duration > 0)
        {
            rcv = ms->segments[idx].cpduration / ms->segments[idx].duration;
        }

        float prior = ms->segments[idx].duration;

        adjustDurationInternal(ms, idx, dx, snap);

        float after = ms->segments[idx].duration;

        durationChange = after - prior;

        ms->segments[idx].cpduration = ms->segments[idx].duration * rcv;
    }

    if (ms->editMode == MSEGStorage::LFO)
    {
        dx = durationChange;

        if (dx > 0)
        {
            /*
             * Collapse the subsequent by 0 to squash me in
             */
            float toConsume = dx;

            for (int i = ms->n_activeSegments - 1; i > idx && toConsume > 0; --i)
            {
                if (ms->segments[i].duration >= toConsume)
                {
                    ms->segments[i].duration -= toConsume;
                    toConsume = 0;
                }
                else
                {
                    toConsume -= ms->segments[i].duration;
                    ms->segments[i].duration = 0;
                }
            }
        }
        else
        {
            /*
             * Need to expand the back
             */
            ms->segments[ms->n_activeSegments - 1].duration -= dx;
        }
    }

    rebuildCache(ms);
}

void adjustDurationConstantTotalDuration(MSEGStorage *ms, int idx, float dx, float snap)
{
    int prior = idx, next = idx + 1;
    auto csum = 0.f, pd = 0.f;

    if (prior >= 0 && (ms->segments[prior].duration + dx) <= MSEGStorage::minimumDuration && dx < 0)
    {
        dx = 0;
    }

    if (next < ms->n_activeSegments &&
        (ms->segments[next].duration - dx) <= MSEGStorage::minimumDuration && dx > 0)
    {
        dx = 0;
    }

    if (prior >= 0)
    {
        csum += ms->segments[prior].duration;
        pd = ms->segments[prior].duration;
    }

    if (next < ms->n_activeSegments)
    {
        csum += ms->segments[next].duration;
    }

    if (prior >= 0)
    {
        auto rcv = 0.5;

        if (ms->segments[prior].duration > 0)
        {
            rcv = ms->segments[prior].cpduration / ms->segments[prior].duration;
        }

        adjustDurationInternal(ms, prior, dx, snap, csum);
        ms->segments[prior].cpduration = ms->segments[prior].duration * rcv;
        pd = ms->segments[prior].duration;
    }

    if (next < ms->n_activeSegments)
    {
        auto rcv = ms->segments[next].cpduration / ms->segments[next].duration;

        // offsetDuration( ms->segments[next].duration, -dx );
        ms->segments[next].duration = csum - pd;
        ms->segments[next].cpduration = ms->segments[next].duration * rcv;
    }

    rebuildCache(ms);
}

void setLoopStart(MSEGStorage *ms, int seg)
{
    ms->loop_start = seg;

    if (ms->loop_end >= 0 && ms->loop_end < ms->loop_start)
    {
        ms->loop_end = std::max(0, seg - 1);
    }
}
void setLoopEnd(MSEGStorage *ms, int seg)
{
    ms->loop_end = seg;

    if (ms->loop_start >= 0 && ms->loop_start > ms->loop_end)
    {
        ms->loop_start = std::min(ms->n_activeSegments - 1, seg + 1);
    }
}
} // namespace MSEG
} // namespace Surge
