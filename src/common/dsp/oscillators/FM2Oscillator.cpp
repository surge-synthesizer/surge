/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
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
    lastoutput = 0.0;
    driftLFO.init(nonzero_init_drift);
    fb_val = 0.0;
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
    auto driftlfo = driftLFO.next() * drift;
    fb_val = oscdata->p[fm2_feedback].get_extended(
        localcopy[oscdata->p[fm2_feedback].param_id_in_scene].f);

    double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));
    double sh = localcopy[oscdata->p[fm2_m12offset].param_id_in_scene].f * storage->dsamplerate_inv;

    RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                                   (double)localcopy[oscdata->p[fm2_m1ratio].param_id_in_scene].i +
                               sh));
    RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                                   (double)localcopy[oscdata->p[fm2_m2ratio].param_id_in_scene].i -
                               sh));

    double d1 = localcopy[oscdata->p[fm2_m1amount].param_id_in_scene].f;
    double d2 = localcopy[oscdata->p[fm2_m2amount].param_id_in_scene].f;

    RelModDepth1.newValue(calcmd(d1));
    RelModDepth2.newValue(calcmd(d2));

    if (FM)
        FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);

    FeedbackDepth.newValue(abs(fb_val));
    PhaseOffset.newValue(2.0 * M_PI * localcopy[oscdata->p[fm2_m12phase].param_id_in_scene].f);

    for (int k = 0; k < BLOCK_SIZE_OS; k++)
    {
        RM1.process();
        RM2.process();

        output[k] =
            phase + RelModDepth1.v * RM1.r + RelModDepth2.v * RM2.r + lastoutput + PhaseOffset.v;
        if (FM)
            output[k] += FMdepth.v * master_osc[k];
        output[k] = sin(output[k]);
        lastoutput =
            (fb_val < 0) ? output[k] * output[k] * FeedbackDepth.v : output[k] * FeedbackDepth.v;

        phase += omega;
        if (phase > 2.0 * M_PI)
            phase -= 2.0 * M_PI;

        RelModDepth1.process();
        RelModDepth2.process();
        FeedbackDepth.process();
        if (FM)
            FMdepth.process();
        PhaseOffset.process();
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
    oscdata->p[fm2_m12offset].set_type(ct_freq_shift);
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
}
