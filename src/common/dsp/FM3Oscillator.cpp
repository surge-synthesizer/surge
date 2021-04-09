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

#include "FM3Oscillator.h"
#include <cmath>
#include <algorithm>

using namespace std;

FM3Oscillator::FM3Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy)
{
}

void FM3Oscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    phase =
        (is_display || oscdata->retrigger.val.b) ? 0.f : (2.0 * M_PI * storage->rand_01() - M_PI);
    lastoutput = 0.f;
    driftLFO.init(nonzero_init_drift);
    fb_val = 0.f;
    AM.set_phase(phase);
    RM1.set_phase(phase);
    RM2.set_phase(phase);
}

FM3Oscillator::~FM3Oscillator() {}

void FM3Oscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
    auto driftlfo = driftLFO.next() * drift;
    fb_val = oscdata->p[fm3_feedback].get_extended(
        localcopy[oscdata->p[fm3_feedback].param_id_in_scene].f);

    double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));

    auto m1 = oscdata->p[fm3_m1ratio].get_extended(
        localcopy[oscdata->p[fm3_m1ratio].param_id_in_scene].f);

    if (m1 < 0)
    {
        m1 = -1.0 / m1;
    }

    if (oscdata->p[fm3_m1ratio].absolute)
    {
        float f = localcopy[oscdata->p[fm3_m1ratio].param_id_in_scene].f;
        float bpv = (f - 16.0) / 16.0;
        auto note = 69 + 69 * bpv;
        RM1.set_rate(min(M_PI, (double)pitch_to_omega(note)));
    }
    else
    {
        RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) * m1));
    }

    auto m2 = oscdata->p[fm3_m2ratio].get_extended(
        localcopy[oscdata->p[fm3_m2ratio].param_id_in_scene].f);

    if (m2 < 0)
    {
        m2 = -1.0 / m2;
    }

    if (oscdata->p[fm3_m2ratio].absolute)
    {
        float f = localcopy[oscdata->p[fm3_m2ratio].param_id_in_scene].f;
        float bpv = (f - 16.0) / 16.0;
        auto note = 69 + 69 * bpv;
        RM2.set_rate(min(M_PI, (double)pitch_to_omega(note)));
    }
    else
    {
        RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) * m2));
    }

    AM.set_rate(min(M_PI, (double)pitch_to_omega(
                              60.0 + localcopy[oscdata->p[fm3_m3freq].param_id_in_scene].f)));

    double d1 = localcopy[oscdata->p[fm3_m1amount].param_id_in_scene].f;
    double d2 = localcopy[oscdata->p[fm3_m2amount].param_id_in_scene].f;
    double d3 = localcopy[oscdata->p[fm3_m3amount].param_id_in_scene].f;

    RelModDepth1.newValue(32.0 * M_PI * d1 * d1 * d1);
    RelModDepth2.newValue(32.0 * M_PI * d2 * d2 * d2);
    AbsModDepth.newValue(32.0 * M_PI * d3 * d3 * d3);

    if (FM)
    {
        FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);
    }

    FeedbackDepth.newValue(abs(fb_val));

    for (int k = 0; k < BLOCK_SIZE_OS; k++)
    {
        RM1.process();
        RM2.process();
        AM.process();

        output[k] = phase + RelModDepth1.v * RM1.r + RelModDepth2.v * RM2.r + AbsModDepth.v * AM.r +
                    lastoutput;

        if (FM)
        {
            output[k] += FMdepth.v * master_osc[k];
        }

        output[k] = sin(output[k]);
        lastoutput =
            (fb_val < 0) ? output[k] * output[k] * FeedbackDepth.v : output[k] * FeedbackDepth.v;

        phase += omega;
        if (phase > 2.0 * M_PI)
        {
            phase -= 2.0 * M_PI;
        }

        RelModDepth1.process();
        RelModDepth2.process();
        AbsModDepth.process();

        if (FM)
        {
            FMdepth.process();
        }

        FeedbackDepth.process();
    }
    if (stereo)
    {
        memcpy(outputR, output, sizeof(float) * BLOCK_SIZE_OS);
    }
}

void FM3Oscillator::init_ctrltypes()
{
    oscdata->p[fm3_m1amount].set_name("M1 Amount");
    oscdata->p[fm3_m1amount].set_type(ct_percent);
    if (oscdata->p[fm3_m1ratio].absolute)
    {
        oscdata->p[fm3_m1ratio].set_name("M1 Frequency");
    }
    else
    {
        oscdata->p[fm3_m1ratio].set_name("M1 Ratio");
    }
    oscdata->p[fm3_m1ratio].set_type(ct_fmratio);

    oscdata->p[fm3_m2amount].set_name("M2 Amount");
    oscdata->p[fm3_m2amount].set_type(ct_percent);
    if (oscdata->p[fm3_m2ratio].absolute)
    {
        oscdata->p[fm3_m2ratio].set_name("M2 Frequency");
    }
    else
    {
        oscdata->p[fm3_m2ratio].set_name("M2 Ratio");
    }
    oscdata->p[fm3_m2ratio].set_type(ct_fmratio);

    oscdata->p[fm3_m3amount].set_name("M3 Amount");
    oscdata->p[fm3_m3amount].set_type(ct_percent);
    oscdata->p[fm3_m3freq].set_name("M3 Frequency");
    oscdata->p[fm3_m3freq].set_type(ct_freq_audible);

    oscdata->p[fm3_feedback].set_name("Feedback");
    oscdata->p[fm3_feedback].set_type(ct_osc_feedback_negative);
}
void FM3Oscillator::init_default_values()
{
    oscdata->p[fm3_m1amount].val.f = 0.f;
    if (oscdata->p[fm3_m1ratio].absolute || oscdata->p[fm3_m1ratio].extend_range)
    {
        oscdata->p[fm3_m1ratio].val_default.f = 16.f;
    }
    else
    {
        oscdata->p[fm3_m1ratio].val.f = 1.f;
    }
    oscdata->p[fm3_m2amount].val.f = 0.f;
    if (oscdata->p[fm3_m1ratio].absolute || oscdata->p[fm3_m1ratio].extend_range)
    {
        oscdata->p[fm3_m2ratio].val.f = 16.f;
    }
    else
    {
        oscdata->p[fm3_m2ratio].val.f = 1.f;
    }
    oscdata->p[fm3_m3amount].val.f = 0.f;
    oscdata->p[fm3_m3freq].val.f = 0.f;
    oscdata->p[fm3_feedback].val.f = 0.f;
}

void FM3Oscillator::handleStreamingMismatches(int streamingRevision,
                                              int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        oscdata->p[fm3_feedback].set_type(ct_osc_feedback);
    }

    if (streamingRevision <= 13)
    {
        oscdata->p[fm3_m1ratio].absolute = false;
        oscdata->p[fm3_m2ratio].absolute = false;
    }

    if (streamingRevision <= 15)
    {
        oscdata->retrigger.val.b = true;
    }
}
