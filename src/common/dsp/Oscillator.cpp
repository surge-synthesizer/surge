//-------------------------------------------------------------------------------------------------------
//	Copyright 2005-2006 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "Oscillator.h"
#include "DspUtilities.h"

Oscillator*
spawn_osc(int osctype, SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
{
   Oscillator* osc = 0;
   switch (osctype)
   {
   case ot_classic:
      osc = (Oscillator*)_aligned_malloc(sizeof(SurgeSuperOscillator), 16);
      new (osc) SurgeSuperOscillator(storage, oscdata, localcopy);
      break;
   case ot_sinus:
   default:
      osc = (Oscillator*)_aligned_malloc(sizeof(osc_sine), 16);
      new (osc) osc_sine(storage, oscdata, localcopy);
      break;
   case ot_wavetable:
      osc = (Oscillator*)_aligned_malloc(sizeof(WavetableOscillator), 16);
      new (osc) WavetableOscillator(storage, oscdata, localcopy);
      break;
   case ot_WT2:
      osc = (Oscillator*)_aligned_malloc(sizeof(WindowOscillator), 16);
      new (osc) WindowOscillator(storage, oscdata, localcopy);
      break;
   case ot_shnoise:
      osc = (Oscillator*)_aligned_malloc(sizeof(SampleAndHoldOscillator), 16);
      new (osc) SampleAndHoldOscillator(storage, oscdata, localcopy);
      break;
   case ot_audioinput:
      osc = (Oscillator*)_aligned_malloc(sizeof(osc_audioinput), 16);
      new (osc) osc_audioinput(storage, oscdata, localcopy);
      break;
   case ot_FM:
      osc = (Oscillator*)_aligned_malloc(sizeof(FMOscillator), 16);
      new (osc) FMOscillator(storage, oscdata, localcopy);
      break;
   case ot_FM2:
      osc = (Oscillator*)_aligned_malloc(sizeof(FM2Oscillator), 16);
      new (osc) FM2Oscillator(storage, oscdata, localcopy);
      break;
   }
   return osc;
}

#if PPC
vector float vec_loadAndSplatScalar(float* scalarPtr)
{

   vUInt8 splatMap = vec_lvsl(0, scalarPtr);
   vector float result = vec_lde(0, scalarPtr);
   splatMap = (vUInt8)vec_splat((vector float)splatMap, 0);

   return vec_perm(result, result, splatMap);
}
#endif

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
}

osc_sine::~osc_sine()
{}

void osc_sine::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
   if (FM)
   {
      driftlfo = drift_noise(driftlfo2);
      double omega = min(M_PI, (double)pitch_to_omega(pitch + drift * driftlfo));
      FMdepth.newValue(fmdepth);

      for (int k = 0; k < block_size_os; k++)
      {
         output[k] = sin(phase);
         phase += omega + master_osc[k] * FMdepth.v;
         FMdepth.process();
      }
   }
   else
   {
      driftlfo = drift_noise(driftlfo2);
      sinus.set_rate(min(M_PI, (double)pitch_to_omega(pitch + drift * driftlfo)));

      for (int k = 0; k < block_size_os; k++)
      {
         sinus.process();
         output[k] = sinus.r;

         // const __m128 scale = _mm_set1_ps(0.000030517578125);

			// HACK för att testa sine(__m64)
			// TRANSLATE: HACK for testing sine(__m64)
         // const __m64 rate = _mm_set1_pi16(0x0040);

         /*m64phase = _mm_add_pi16(m64phase,rate);
         __m64 a = sine(m64phase);
         __m128 b = _mm_cvtpi16_ps(a);
         _mm_store_ss(&output[k],_mm_mul_ss(b,scale));*/

         // int
         /*m64phase.m64_i32[0] = (m64phase.m64_i32[0] + 0x40);
         int a = sine(m64phase.m64_i32[0]);
         __m128 b = _mm_cvtsi32_ss(b,a);
         _mm_store_ss(&output[k],_mm_mul_ss(b,scale));*/
      }
      //_mm_empty(); // HACK MMX
   }
   if (stereo)
   {
      memcpy(outputR, output, sizeof(float) * block_size_os);
   }
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

      for (int k = 0; k < block_size_os; k++)
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

      for (int k = 0; k < block_size_os; k++)
      {
         output[k] = a * storage->audio_in[0][k] + b * storage->audio_in[1][k];
      }
   }
}
