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

/* FM osc */

enum fmparam
{
   fm_m1amt = 0,
   fm_m1ratio,
   fm_m2amt,
   fm_m2ratio,
   fm_m3amt,
   fm_m3freq,
   fm_fb,
};

FM3Oscillator::FM3Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy)
{}

void FM3Oscillator::init(float pitch, bool is_display)
{
   // phase = oscdata->retrigger.val.b ? ((oscdata->startphase.val.f) * M_PI * 2) : 0.f;
   phase = 0.0;
   lastoutput = 0.0;
   driftlfo = 0;
   driftlfo2 = 0;
   fb_val = 0.0;
   AM.set_phase(0.0);
   RM1.set_phase(0.0);
   RM2.set_phase(0.0);
}

FM3Oscillator::~FM3Oscillator()
{}

void FM3Oscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   driftlfo = drift_noise(driftlfo2) * drift;
   fb_val = oscdata->p[fm_fb].get_extended(localcopy[oscdata->p[fm_fb].param_id_in_scene].f);

   double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));

   auto m1 = oscdata->p[fm_m1ratio].get_extended(localcopy[oscdata->p[fm_m1ratio].param_id_in_scene].f);
   if( m1 < 0 ) m1 = 1.0 / m1;
   RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) * m1 ) );

   auto m2 = oscdata->p[fm_m2ratio].get_extended(localcopy[oscdata->p[fm_m2ratio].param_id_in_scene].f);
   if( m2 < 0 ) m2 = 1.0 / m2;
   RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) * m2 ) );
   AM.set_rate(min(
       M_PI, (double)pitch_to_omega(60.0 + localcopy[oscdata->p[fm_m3freq].param_id_in_scene].f)));

   double d1 = localcopy[oscdata->p[fm_m1amt].param_id_in_scene].f;
   double d2 = localcopy[oscdata->p[fm_m2amt].param_id_in_scene].f;
   double d3 = localcopy[oscdata->p[fm_m3amt].param_id_in_scene].f;

   RelModDepth1.newValue(32.0 * M_PI * d1 * d1 * d1);
   RelModDepth2.newValue(32.0 * M_PI * d2 * d2 * d2);
   AbsModDepth.newValue(32.0 * M_PI * d3 * d3 * d3);

   if (FM)
      FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);

   FeedbackDepth.newValue(abs(fb_val));

   for (int k = 0; k < BLOCK_SIZE_OS; k++)
   {
      RM1.process();
      RM2.process();
      AM.process();

      output[k] = phase + RelModDepth1.v * RM1.r + RelModDepth2.v * RM2.r + AbsModDepth.v * AM.r +
                  lastoutput;

      if (FM)
         output[k] += FMdepth.v * master_osc[k];

      output[k] = sin(output[k]);
      lastoutput = (fb_val < 0) ? output[k] * output[k] * FeedbackDepth.v : output[k] * FeedbackDepth.v;
      
      phase += omega;
      if (phase > 2.0 * M_PI)
         phase -= 2.0 * M_PI;

      RelModDepth1.process();
      RelModDepth2.process();
      AbsModDepth.process();
      if (FM)
         FMdepth.process();
      FeedbackDepth.process();
   }
   if (stereo)
   {
      memcpy(outputR, output, sizeof(float) * BLOCK_SIZE_OS);
   }
}

void FM3Oscillator::init_ctrltypes()
{
   oscdata->p[fm_m1amt].set_name("M1 Amount");
   oscdata->p[fm_m1amt].set_type(ct_percent);
   oscdata->p[fm_m1ratio].set_name("M1 Ratio");
   oscdata->p[fm_m1ratio].set_type(ct_fmratio);

   oscdata->p[fm_m2amt].set_name("M2 Amount");
   oscdata->p[fm_m2amt].set_type(ct_percent);
   oscdata->p[fm_m2ratio].set_name("M2 Ratio");
   oscdata->p[fm_m2ratio].set_type(ct_fmratio);

   oscdata->p[fm_m3amt].set_name("M3 Amount");
   oscdata->p[fm_m3amt].set_type(ct_percent);
   oscdata->p[fm_m3freq].set_name("M3 Frequency");
   oscdata->p[fm_m3freq].set_type(ct_freq_audible);

   oscdata->p[fm_fb].set_name("Feedback");
   oscdata->p[fm_fb].set_type(ct_osc_feedback_negative);
}
void FM3Oscillator::init_default_values()
{
   oscdata->p[fm_m1amt].val.f = 0.0f;
   oscdata->p[fm_m1ratio].val.f = 1.0f;
   oscdata->p[fm_m2amt].val.f = 0.0f;
   oscdata->p[fm_m2ratio].val.f = 1.0f;
   oscdata->p[fm_m3amt].val.f = 0.0f;
   oscdata->p[fm_m3freq].val.f = 0.0f;
   oscdata->p[fm_fb].val.f = 0.0f;
}

void FM3Oscillator::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision <= 12)
   {
      oscdata->p[fm_fb].set_type(ct_osc_feedback);
   }
}

