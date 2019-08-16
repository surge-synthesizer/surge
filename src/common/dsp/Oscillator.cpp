//-------------------------------------------------------------------------------------------------------
//	Copyright 2005-2006 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "Oscillator.h"
#include "DspUtilities.h"

using namespace std;

Oscillator*
spawn_osc(int osctype, SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
{
   Oscillator* osc = 0;
   switch (osctype)
   {
   case ot_classic:
      return new SurgeSuperOscillator(storage, oscdata, localcopy);
   case ot_wavetable:
      return new WavetableOscillator(storage, oscdata, localcopy);
   case ot_WT2:
      return new WindowOscillator(storage, oscdata, localcopy);
   case ot_shnoise:
      return new SampleAndHoldOscillator(storage, oscdata, localcopy);
   case ot_audioinput:
      return new osc_audioinput(storage, oscdata, localcopy);
   case ot_FM:
      return new FMOscillator(storage, oscdata, localcopy);
   case ot_FM2:
      return new FM2Oscillator(storage, oscdata, localcopy);
   case ot_sinus:
   default:
      return new osc_sine(storage, oscdata, localcopy);
   }
   return osc;
}

Oscillator::Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : master_osc(0)
{
   // assert(storage);
   assert(oscdata);
   this->storage = storage;
   this->oscdata = oscdata;
   this->localcopy = localcopy;
   ticker = 0;
}

Oscillator::~Oscillator()
{}

/* sine osc */

osc_sine::osc_sine(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy)
{}

void osc_sine::init(float pitch, bool is_display)
{
   // phase = oscdata->retrigger.val.b ? ((oscdata->startphase.val.f) * M_PI * 2) : 0.f;
   phase = 0.f;
   // m64phase = _mm_set1_pi16(0);
   // m64phase.m64_i32[0] = 0;
   sinus.set_phase(phase);
   driftlfo = 0;
   driftlfo2 = 0;

   id_mode = oscdata->p[0].param_id_in_scene;
   id_fb = oscdata->p[1].param_id_in_scene;
   lastvalue = 0;
}

osc_sine::~osc_sine()
{}

void osc_sine::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   driftlfo = drift_noise(driftlfo2);
   double omega = min(M_PI, (double)pitch_to_omega(pitch + drift * driftlfo));
   // FMdepth.newValue(fmdepth);
   FMdepth.newValue(32.0 * M_PI * fmdepth * fmdepth * fmdepth);
   FB.newValue(localcopy[id_fb].f);

   for (int k = 0; k < BLOCK_SIZE_OS; k++)
   {
      // Replicate FM2 exactly
      auto p = phase + lastvalue;
      if (FM)
         p += FMdepth.v * master_osc[k];
      output[k] = valueFromSinAndCos(sin(p), cos(p));
      phase += omega;
      lastvalue = output[k] * FB.v;
      FMdepth.process();
      FB.process();
   }

   if (stereo)
   {
      memcpy(outputR, output, sizeof(float) * BLOCK_SIZE_OS);
   }
}

float osc_sine::valueFromSinAndCos(float svalue, float cvalue)
{
   int wfMode = localcopy[id_mode].i;
   float pvalue = svalue;

   int quadrant;
   if (svalue > 0)
      if (cvalue > 0)
         quadrant = 1;
      else
         quadrant = 2;
   else if (cvalue < 0)
      quadrant = 3;
   else
      quadrant = 4;

   switch (wfMode)
   {
   case 1:
      if (quadrant == 3 || quadrant == 4)
         pvalue = 0;
      pvalue = 2 * pvalue - 1;
      break;
   case 2:
      if (quadrant == 1 || quadrant == 3)
         pvalue = 0;
      break;
   case 3:
      if (quadrant == 2 || quadrant == 4)
         pvalue = 0;
      break;
   case 4:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - cvalue;
         break;
      case 2:
         pvalue = 1 + cvalue;
         break;
      case 3:
         pvalue = -1 - cvalue;
         break;
      case 4:
         pvalue = -1 + cvalue;
         break;
      }
      break;
   case 5:
      switch (quadrant)
      {
      case 1:
         pvalue = 1 - cvalue;
         break;
      case 2:
         pvalue = 1 + cvalue;
         break;
      default:
         pvalue = 0;
         break;
      }
      pvalue = 2 * pvalue - 1;
      break;
   case 6:
      if (quadrant <= 2)
         pvalue = 2 * svalue * cvalue; // remember sin 2x = 2 * sinx * cosx
      else
         pvalue = 0;
      break;
   case 7:
      pvalue = 2 * svalue * cvalue;
      if (quadrant == 2 || quadrant == 3)
         pvalue = -pvalue;
      break;
   case 8:
      pvalue = 2 * svalue * cvalue;
      if (quadrant == 2 || quadrant == 4)
         pvalue = 0;
      if (quadrant == 3)
         pvalue = -pvalue;
      break;

   default:
      break;
   }
   return pvalue;
}

void osc_sine::init_ctrltypes()
{
   oscdata->p[0].set_name("WaveShape");
   oscdata->p[0].set_type(ct_sineoscmode);

   oscdata->p[1].set_name("Feedback");
   oscdata->p[1].set_type(ct_percent);
}

void osc_sine::init_default_values()
{
   oscdata->p[0].val.i = 0;
   oscdata->p[1].val.f = 0;
}
/* audio input osc */

/* add controls:
input L/R
gain
limiter?
*/

osc_audioinput::osc_audioinput(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : Oscillator(storage, oscdata, localcopy)
{}

void osc_audioinput::init(float pitch, bool is_display)
{}

osc_audioinput::~osc_audioinput()
{}

void osc_audioinput::init_ctrltypes()
{
   oscdata->p[0].set_name("Input");
   oscdata->p[0].set_type(ct_percent_bidirectional);
   oscdata->p[1].set_name("Gain");
   oscdata->p[1].set_type(ct_decibel);
}
void osc_audioinput::init_default_values()
{
   oscdata->p[0].val.f = 0.0f;
   oscdata->p[1].val.f = 0.0f;
}

void osc_audioinput::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
   if (stereo)
   {
      float g = db_to_linear(localcopy[oscdata->p[1].param_id_in_scene].f);
      float inp = limit_range(localcopy[oscdata->p[0].param_id_in_scene].f, -1.f, 1.f);

      float a = g * (1.f - inp);
      float b = g * (1.f + inp);

      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         output[k] = a * storage->audio_in[0][k];
         outputR[k] = b * storage->audio_in[1][k];
      }
   }
   else
   {
      float g = db_to_linear(localcopy[oscdata->p[1].param_id_in_scene].f);
      float inp = limit_range(localcopy[oscdata->p[0].param_id_in_scene].f, -1.f, 1.f);

      float a = g * (1.f - inp);
      float b = g * (1.f + inp);

      for (int k = 0; k < BLOCK_SIZE_OS; k++)
      {
         output[k] = a * storage->audio_in[0][k] + b * storage->audio_in[1][k];
      }
   }
}
