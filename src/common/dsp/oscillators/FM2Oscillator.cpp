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

#include "FM2Oscillator.h"
#include <algorithm>

using std::max;
using std::min;

FM2Oscillator::FM2Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy)
{
}

double calcmd(double x) { return x * x * x * 8.0 * M_PI; }

void FM2Oscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    phase =
        (is_display || oscdata->retrigger.val.b) ? 0.f : (2.0 * M_PI * storage->rand_01() - M_PI);
    oldout1 = 0.0;
    oldout2 = 0.0;
    driftLFO.init(nonzero_init_drift);
    fb_val = 0.0;
    fb_mode = 0;
    double ph = (localcopy[oscdata->p[fm2_m12phase].param_id_in_scene].f + phase) * 2.0 * M_PI;
    RM1.set_phase(ph);
    RM2.set_phase(ph);
    phase = -sin(ph) * (calcmd(localcopy[oscdata->p[fm2_m1amount].param_id_in_scene].f) +
                        calcmd(localcopy[oscdata->p[fm2_m2amount].param_id_in_scene].f)) -
            ph;
}

FM2Oscillator::~FM2Oscillator() {}

void FM2Oscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
    fb_mode = oscdata->p[fm2_feedback].deform_type;

    if (stereo)
        if (FM)
            if (fb_mode == 0)
                process_block_internal<0, true, true>(pitch, drift, fmdepth);
            else
                process_block_internal<1, true, true>(pitch, drift, fmdepth);
        else if (fb_mode == 0)
            process_block_internal<0, true, false>(pitch, drift, fmdepth);
        else
            process_block_internal<1, true, false>(pitch, drift, fmdepth);
    else if (FM)
        if (fb_mode == 0)
            process_block_internal<0, false, true>(pitch, drift, fmdepth);
        else
            process_block_internal<1, false, true>(pitch, drift, fmdepth);
    else if (fb_mode == 0)
        process_block_internal<0, false, false>(pitch, drift, fmdepth);
    else
        process_block_internal<1, false, false>(pitch, drift, fmdepth);
}

template <int mode, bool stereo, bool FM>
void FM2Oscillator::process_block_internal(float pitch, float drift, float fmdepth)
{
    auto driftlfo = driftLFO.next() * drift;
    double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));
    double sh = oscdata->p[fm2_m12offset].get_extended(
                    localcopy[oscdata->p[fm2_m12offset].param_id_in_scene].f) *
                storage->dsamplerate_inv;

    fb_val = oscdata->p[fm2_feedback].get_extended(
        localcopy[oscdata->p[fm2_feedback].param_id_in_scene].f);

    RM1.set_rate(
        min(M_PI,
            (double)pitch_to_omega(pitch + driftlfo) * (double)oscdata->p[fm2_m1ratio].val.i + sh));
    RM2.set_rate(
        min(M_PI,
            (double)pitch_to_omega(pitch + driftlfo) * (double)oscdata->p[fm2_m2ratio].val.i - sh));

    double d1 = localcopy[oscdata->p[fm2_m1amount].param_id_in_scene].f;
    double d2 = localcopy[oscdata->p[fm2_m2amount].param_id_in_scene].f;

    RelModDepth1.newValue(calcmd(d1));
    RelModDepth2.newValue(calcmd(d2));
    FeedbackDepth.newValue(abs(fb_val));
    PhaseOffset.newValue(2.0 * M_PI * localcopy[oscdata->p[fm2_m12phase].param_id_in_scene].f);

    if (FM)
        FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);

    for (int k = 0; k < BLOCK_SIZE_OS; k++)
    {
        RM1.process();
        RM2.process();

        double avg = mode == 1 ? ((oldout1 + oldout2) / 2.0) : oldout1;
        double fb_amt = (fb_val < 0) ? avg * avg * FeedbackDepth.v : avg * FeedbackDepth.v;

        output[k] =
            phase + RelModDepth1.v * RM1.r + RelModDepth2.v * RM2.r + fb_amt + PhaseOffset.v;

        if (FM)
            output[k] += FMdepth.v * master_osc[k];

        oldout2 = oldout1;
        oldout1 = sin(output[k]);
        output[k] = oldout1;

        phase += omega;

        if (phase > 2.0 * M_PI)
            phase -= 2.0 * M_PI;

        RelModDepth1.process();
        RelModDepth2.process();
        FeedbackDepth.process();
        PhaseOffset.process();

        if (FM)
            FMdepth.process();
    }

    if (stereo)
    {
        memcpy(outputR, output, sizeof(float) * BLOCK_SIZE_OS);
    }
}

void FM2Oscillator::init_ctrltypes()
{
    oscdata->p[fm2_m1amount].set_name("M1 Amount");
    oscdata->p[fm2_m1amount].set_type(ct_percent);
    oscdata->p[fm2_m1ratio].set_name("M1 Ratio");
    oscdata->p[fm2_m1ratio].set_type(ct_fmratio_int);
    oscdata->p[fm2_m2amount].set_name("M2 Amount");
    oscdata->p[fm2_m2amount].set_type(ct_percent);
    oscdata->p[fm2_m2ratio].set_name("M2 Ratio");
    oscdata->p[fm2_m2ratio].set_type(ct_fmratio_int);
    oscdata->p[fm2_m12offset].set_name("M1/2 Offset");
    oscdata->p[fm2_m12offset].set_type(ct_freq_fm2_offset);
    oscdata->p[fm2_m12phase].set_name("M1/2 Phase");
    oscdata->p[fm2_m12phase].set_type(ct_percent);
    oscdata->p[fm2_feedback].set_name("Feedback");
    oscdata->p[fm2_feedback].set_type(ct_osc_feedback_negative);
}

void FM2Oscillator::init_default_values()
{
    oscdata->p[fm2_m1amount].val.f = 0.0f;
    oscdata->p[fm2_m1ratio].val.i = 1;
    oscdata->p[fm2_m2amount].val.f = 0.0f;
    oscdata->p[fm2_m2ratio].val.i = 1;
    oscdata->p[fm2_m12offset].val.f = 0.0f;
    oscdata->p[fm2_m12phase].val.f = 0.0f;
    oscdata->p[fm2_feedback].val.f = 0.0f;
}

void FM2Oscillator::handleStreamingMismatches(int streamingRevision,
                                              int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        oscdata->p[fm2_feedback].set_type(ct_osc_feedback);
    }

    if (streamingRevision <= 15)
    {
        oscdata->retrigger.val.b = true;
    }

    if (streamingRevision <= 21)
    {
        oscdata->p[fm2_feedback].deform_type = 0;
    }

    if (streamingRevision <= 23)
    {
        oscdata->p[fm2_m12offset].extend_range = false;
    }
}
