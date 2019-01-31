//-------------------------------------------------------------------------------------------------------
//	Copyright 2006 Claes Johanson & vember audio
//-------------------------------------------------------------------------------------------------------
#include "Oscillator.h"

using namespace std;

/* FM osc */

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
   AM.set_phase(0.0);
   RM1.set_phase(0.0);
   RM2.set_phase(0.0);
}

FMOscillator::~FMOscillator()
{}

void FMOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   driftlfo = drift_noise(driftlfo2) * drift;
   double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));
   RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                              localcopy[oscdata->p[1].param_id_in_scene].f));
   RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                              localcopy[oscdata->p[3].param_id_in_scene].f));
   AM.set_rate(
       min(M_PI, (double)pitch_to_omega(60.0 + localcopy[oscdata->p[5].param_id_in_scene].f)));
   double d1 = localcopy[oscdata->p[0].param_id_in_scene].f;
   double d2 = localcopy[oscdata->p[2].param_id_in_scene].f;
   double d3 = localcopy[oscdata->p[4].param_id_in_scene].f;
   RelModDepth1.newValue(32.0 * M_PI * d1 * d1 * d1);
   RelModDepth2.newValue(32.0 * M_PI * d2 * d2 * d2);
   AbsModDepth.newValue(32.0 * M_PI * d3 * d3 * d3);
   if (FM)
      FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);
   FeedbackDepth.newValue(localcopy[oscdata->p[6].param_id_in_scene].f);

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
      lastoutput = output[k] * FeedbackDepth.v;
      phase += omega;
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
   oscdata->p[0].set_name("M1 Amount");
   oscdata->p[0].set_type(ct_percent);
   oscdata->p[1].set_name("M1 Ratio");
   oscdata->p[1].set_type(ct_fmratio);

   oscdata->p[2].set_name("M2 Amount");
   oscdata->p[2].set_type(ct_percent);
   oscdata->p[3].set_name("M2 Ratio");
   oscdata->p[3].set_type(ct_fmratio);

   oscdata->p[4].set_name("M3 Amount");
   oscdata->p[4].set_type(ct_percent);
   oscdata->p[5].set_name("M3 Freq");
   oscdata->p[5].set_type(ct_freq_audible);

   oscdata->p[6].set_name("Feedback");
   oscdata->p[6].set_type(ct_percent);
}
void FMOscillator::init_default_values()
{
   oscdata->p[0].val.f = 0.0f;
   oscdata->p[1].val.f = 1.0f;
   oscdata->p[2].val.f = 0.0f;
   oscdata->p[3].val.f = 1.0f;
   oscdata->p[4].val.f = 0.0f;
   oscdata->p[5].val.f = 0.0f;
   oscdata->p[6].val.f = 0.0f;
}

/* FM osc 2 */
// 2 modulators

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
   double ph = localcopy[oscdata->p[5].param_id_in_scene].f * 2.0 * M_PI;
   RM1.set_phase(ph);
   RM2.set_phase(ph);
   phase = -sin(ph) * (calcmd(localcopy[oscdata->p[0].param_id_in_scene].f) +
                       calcmd(localcopy[oscdata->p[2].param_id_in_scene].f)) -
           ph;
}

FM2Oscillator::~FM2Oscillator()
{}

void FM2Oscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   driftlfo = drift_noise(driftlfo2) * drift;
   double omega = min(M_PI, (double)pitch_to_omega(pitch + driftlfo));
   double shift = localcopy[oscdata->p[4].param_id_in_scene].f * dsamplerate_inv;
   RM1.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                                  (double)localcopy[oscdata->p[1].param_id_in_scene].i +
                              shift));
   RM2.set_rate(min(M_PI, (double)pitch_to_omega(pitch + driftlfo) *
                                  (double)localcopy[oscdata->p[3].param_id_in_scene].i -
                              shift));
   double d1 = localcopy[oscdata->p[0].param_id_in_scene].f;
   double d2 = localcopy[oscdata->p[2].param_id_in_scene].f;
   RelModDepth1.newValue(calcmd(d1));
   RelModDepth2.newValue(calcmd(d2));
   if (FM)
      FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);
   FeedbackDepth.newValue(localcopy[oscdata->p[6].param_id_in_scene].f);
   PhaseOffset.newValue(2.0 * M_PI * localcopy[oscdata->p[5].param_id_in_scene].f);

   for (int k = 0; k < BLOCK_SIZE_OS; k++)
   {
      RM1.process();
      RM2.process();
      output[k] =
          phase + RelModDepth1.v * RM1.r + RelModDepth2.v * RM2.r + lastoutput + PhaseOffset.v;
      if (FM)
         output[k] += FMdepth.v * master_osc[k];
      output[k] = sin(output[k]);
      lastoutput = output[k] * FeedbackDepth.v;
      phase += omega;
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
   oscdata->p[0].set_name("M1 Amount");
   oscdata->p[0].set_type(ct_percent);
   oscdata->p[1].set_name("M1 Ratio");
   oscdata->p[1].set_type(ct_fmratio_int);
   oscdata->p[2].set_name("M2 Amount");
   oscdata->p[2].set_type(ct_percent);
   oscdata->p[3].set_name("M2 Ratio");
   oscdata->p[3].set_type(ct_fmratio_int);
   oscdata->p[4].set_name("Mx Shift");
   oscdata->p[4].set_type(ct_freq_shift);
   oscdata->p[5].set_name("Mx Start Phase");
   oscdata->p[5].set_type(ct_percent);
   oscdata->p[6].set_name("Feedback");
   oscdata->p[6].set_type(ct_percent);
}
void FM2Oscillator::init_default_values()
{
   oscdata->p[0].val.f = 0.0f;
   oscdata->p[1].val.i = 1;
   oscdata->p[2].val.f = 0.0f;
   oscdata->p[3].val.i = 1;
   oscdata->p[4].val.f = 0.0f;
   oscdata->p[5].val.f = 0.0f;
   oscdata->p[6].val.f = 0.0f;
}
