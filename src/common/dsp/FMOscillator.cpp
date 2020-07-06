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

#include "Oscillator.h"
#include <cmath>

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

FMOscillator::FMOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy)
{}

void FMOscillator::init(float pitch, bool is_display)
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

FMOscillator::~FMOscillator()
{}

void FMOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   driftlfo = drift_noise(driftlfo2) * drift;
   fb_val = oscdata->p[fm_fb].get_extended(localcopy[oscdata->p[fm_fb].param_id_in_scene].f);

   double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));

   RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                              localcopy[oscdata->p[fm_m1ratio].param_id_in_scene].f));
   RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                              localcopy[oscdata->p[fm_m2ratio].param_id_in_scene].f));
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

void FMOscillator::init_ctrltypes()
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
void FMOscillator::init_default_values()
{
   oscdata->p[fm_m1amt].val.f = 0.0f;
   oscdata->p[fm_m1ratio].val.f = 1.0f;
   oscdata->p[fm_m2amt].val.f = 0.0f;
   oscdata->p[fm_m2ratio].val.f = 1.0f;
   oscdata->p[fm_m3amt].val.f = 0.0f;
   oscdata->p[fm_m3freq].val.f = 0.0f;
   oscdata->p[fm_fb].val.f = 0.0f;
}

void FMOscillator::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision <= 12)
   {
      oscdata->p[fm_fb].set_type(ct_osc_feedback);
   }
}

/* FM osc 2 */
// 2 modulators

enum fm2param
{
   fm2_m1amt = 0,
   fm2_m1ratio,
   fm2_m2amt,
   fm2_m2ratio,
   fm2_m12ofs,
   fm2_m12ph,
   fm2_fb,
};

FM2Oscillator::FM2Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy)
{}

double calcmd(double x)
{
   return x * x * x * 8.0 * M_PI;
}

void FM2Oscillator::init(float pitch, bool is_display)
{
   // phase = oscdata->retrigger.val.b ? ((oscdata->startphase.val.f) * M_PI * 2) : 0.f;
   lastoutput = 0.0;
   driftlfo = 0;
   driftlfo2 = 0;
   fb_val = 0.0;
   double ph = localcopy[oscdata->p[fm2_m12ph].param_id_in_scene].f * 2.0 * M_PI;
   RM1.set_phase(ph);
   RM2.set_phase(ph);
   phase = -sin(ph) * (calcmd(localcopy[oscdata->p[fm2_m1amt].param_id_in_scene].f) +
                       calcmd(localcopy[oscdata->p[fm2_m2amt].param_id_in_scene].f)) -
           ph;
}

FM2Oscillator::~FM2Oscillator()
{}

void FM2Oscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   driftlfo = drift_noise(driftlfo2) * drift;
   fb_val = oscdata->p[fm2_fb].get_extended(localcopy[oscdata->p[fm2_fb].param_id_in_scene].f);

   double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));
   double shift = localcopy[oscdata->p[fm2_m12ofs].param_id_in_scene].f * dsamplerate_inv;

   RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                                  (double)localcopy[oscdata->p[fm2_m1ratio].param_id_in_scene].i + shift));
   RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                                  (double)localcopy[oscdata->p[fm2_m2ratio].param_id_in_scene].i - shift));

   double d1 = localcopy[oscdata->p[fm2_m1amt].param_id_in_scene].f;
   double d2 = localcopy[oscdata->p[fm2_m2amt].param_id_in_scene].f;

   RelModDepth1.newValue(calcmd(d1));
   RelModDepth2.newValue(calcmd(d2));

   if (FM)
      FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);

   FeedbackDepth.newValue(abs(fb_val));
   PhaseOffset.newValue(2.0 * M_PI * localcopy[oscdata->p[fm2_m12ph].param_id_in_scene].f);

   for (int k = 0; k < BLOCK_SIZE_OS; k++)
   {
      RM1.process();
      RM2.process();

      output[k] = phase + RelModDepth1.v * RM1.r + RelModDepth2.v * RM2.r + lastoutput + PhaseOffset.v;
      if (FM)
         output[k] += FMdepth.v * master_osc[k];
      output[k] = sin(output[k]);
      lastoutput = (fb_val < 0) ? output[k] * output[k] * FeedbackDepth.v : output[k] * FeedbackDepth.v;

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
   oscdata->p[fm2_m1amt].set_name("M1 Amount");
   oscdata->p[fm2_m1amt].set_type(ct_percent);
   oscdata->p[fm2_m1ratio].set_name("M1 Ratio");
   oscdata->p[fm2_m1ratio].set_type(ct_fmratio_int);
   oscdata->p[fm2_m2amt].set_name("M2 Amount");
   oscdata->p[fm2_m2amt].set_type(ct_percent);
   oscdata->p[fm2_m2ratio].set_name("M2 Ratio");
   oscdata->p[fm2_m2ratio].set_type(ct_fmratio_int);
   oscdata->p[fm2_m12ofs].set_name("M1/2 Offset");
   oscdata->p[fm2_m12ofs].set_type(ct_freq_shift);
   oscdata->p[fm2_m12ph].set_name("M1/2 Phase");
   oscdata->p[fm2_m12ph].set_type(ct_percent);
   oscdata->p[fm2_fb].set_name("Feedback");
   oscdata->p[fm2_fb].set_type(ct_osc_feedback_negative);
}
void FM2Oscillator::init_default_values()
{
   oscdata->p[fm2_m1amt].val.f = 0.0f;
   oscdata->p[fm2_m1ratio].val.i = 1;
   oscdata->p[fm2_m2amt].val.f = 0.0f;
   oscdata->p[fm2_m2ratio].val.i = 1;
   oscdata->p[fm2_m12ofs].val.f = 0.0f;
   oscdata->p[fm2_m12ph].val.f = 0.0f;
   oscdata->p[fm2_fb].val.f = 0.0f;
}

void FM2Oscillator::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision <= 12)
   {
      oscdata->p[fm2_fb].set_type(ct_osc_feedback);
   }
}
