#include "FreqshiftEffect.h"

using namespace std;

/* freqshift			*/

enum freqshiftparams
{
   fsp_shift = 0,
   fsp_rmult,
   fsp_delay,
   fsp_feedback,
   fsp_mix,
   fsp_numparams
};

FreqshiftEffect::FreqshiftEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), time(0.0001), shiftL(0.01),
      shiftR(0.01), // fiL(true), frL(true), fiR(true), frR(true)
      fr(6, true), fi(6, true)
{}

FreqshiftEffect::~FreqshiftEffect()
{}

void FreqshiftEffect::init()
{
   memset(buffer, 0, 2 * max_delay_length * sizeof(float));
   wpos = 0;
   /*frL.reset();
   frR.reset();
   fiL.reset();
   fiR.reset();*/
   fr.reset();
   fi.reset();
   ringout = 10000000;

   // See issue #1444 and the fix for this stuff
   inithadtempo = (storage->temposyncratio_inv != 0);
   setvars(true);
   inithadtempo = (storage->temposyncratio_inv != 0);
}

void FreqshiftEffect::setvars(bool init)
{
   if( ! inithadtempo && storage->temposyncratio_inv != 0 )
   {
      init = true;
      inithadtempo = true;
   }

   feedback.newValue(amp_to_linear(*f[fsp_feedback]));

   if (init)
      time.newValue((fxdata->p[fsp_delay].temposync ? storage->temposyncratio_inv : 1.f) *
                        samplerate * storage->note_to_pitch_ignoring_tuning(12 * fxdata->p[fsp_delay].val.f) -
                    FIRoffset);
   else
      time.newValue((fxdata->p[fsp_delay].temposync ? storage->temposyncratio_inv : 1.f) *
                        samplerate * storage->note_to_pitch_ignoring_tuning(12 * *f[fsp_delay]) -
                    FIRoffset);
   mix.set_target_smoothed(*f[fsp_mix]);

   double shift = *f[fsp_shift] * (fxdata->p[fsp_shift].extend_range ? 1000.0 : 10.0);
   double omega = shift * M_PI * 2.0 * dsamplerate_inv;
   o1L.set_rate(M_PI * 0.5 - min(0.0, omega));
   o2L.set_rate(M_PI * 0.5 + max(0.0, omega));

   // phase lock oscillators
   if (*f[fsp_rmult] == 1.f)
   {
      const double a = 0.01;
      o1R.r = a * o1L.r + (1 - a) * o1R.r;
      o1R.i = a * o1L.i + (1 - a) * o1R.i;
      o2R.r = a * o2L.r + (1 - a) * o2R.r;
      o2R.i = a * o2L.i + (1 - a) * o2R.i;
   }
   else
      omega *= *f[fsp_rmult];

   o1R.set_rate(M_PI * 0.5 - min(0.0, omega));
   o2R.set_rate(M_PI * 0.5 + max(0.0, omega));

   const float db96 = powf(10.f, 0.05f * -96.f);
   float maxfb = max(db96, feedback.v);
   if (maxfb < 1.f)
   {
      float f = BLOCK_SIZE_INV * time.v * (1.f + log(db96) / log(maxfb));
      ringout_time = (int)f;
   }
   else
   {
      ringout_time = -1;
      ringout = 0;
   }
}

void FreqshiftEffect::process(float* dataL, float* dataR)
{
   setvars(false);

   int k;
   float L alignas(16)[BLOCK_SIZE],
         R alignas(16)[BLOCK_SIZE],
         Li alignas(16)[BLOCK_SIZE],
         Ri alignas(16)[BLOCK_SIZE],
         Lr alignas(16)[BLOCK_SIZE],
         Rr alignas(16)[BLOCK_SIZE];

   for (k = 0; k < BLOCK_SIZE; k++)
   {
      time.process();

      int i_dtime = max(FIRipol_N + BLOCK_SIZE, min((int)time.v, max_delay_length - FIRipol_N - 1));

      int rp = (wpos - i_dtime + k);

      int sinc = FIRipol_N *
                 limit_range((int)(FIRipol_M * (float(i_dtime + 1) - time.v)), 0, FIRipol_M - 1);

      L[k] = 0;
      R[k] = 0;
      for (int i = 0; i < FIRipol_N; i++)
      {
         L[k] += buffer[0][(rp - i) & (max_delay_length - 1)] * sinctable1X[sinc + FIRipol_N - i];
         R[k] += buffer[1][(rp - i) & (max_delay_length - 1)] * sinctable1X[sinc + FIRipol_N - i];
      }

      // do freqshift (part I)
      o1L.process();
      Lr[k] = L[k] * o1L.r;
      Li[k] = L[k] * o1L.i;
      o1R.process();
      Rr[k] = R[k] * o1R.r;
      Ri[k] = R[k] * o1R.i;
   }

   fr.process_block(Lr, Rr, BLOCK_SIZE);
   fi.process_block(Li, Ri, BLOCK_SIZE);

   // do freqshift
   /*{
           // quadrature oscillator 1
           double r,i;
           o1L.process();
           r = L*o1L.r;
           i = L*o1L.i;
           // filter the sections (bad-ass mofo dp 20-pole filters!)
           r = frL.process(r);
           i = fiL.process(i);
           // quadrature oscillator 2
           o2L.process();
           r *= o2L.r;
           i *= o2L.i;
           L = 2*(r+i);

           // right channel
           o1R.process();
           r = R*o1R.r;
           i = R*o1R.i;
           // filter the sections (bad-ass mofo dp 20-pole filters!)
           r = frR.process(r);
           i = fiR.process(i);
           // quadrature oscillator 2
           o2R.process();
           r *= o2R.r;
           i *= o2R.i;
           R = 2*(r+i);
   }*/

   for (k = 0; k < BLOCK_SIZE; k++)
   {
      o2L.process();
      Lr[k] *= o2L.r;
      Li[k] *= o2L.i;
      o2R.process();
      Rr[k] *= o2R.r;
      Ri[k] *= o2R.i;

      L[k] = 2 * (Lr[k] + Li[k]);
      R[k] = 2 * (Rr[k] + Ri[k]);

      int wp = (wpos + k) & (max_delay_length - 1);

      feedback.process();

      buffer[0][wp] = dataL[k] + (float)lookup_waveshape(0, (L[k] * feedback.v));
      buffer[1][wp] = dataR[k] + (float)lookup_waveshape(0, (R[k] * feedback.v));
   }

   mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);

   wpos += BLOCK_SIZE;
   wpos = wpos & (max_delay_length - 1);
}

void FreqshiftEffect::suspend()
{
   init();
   ringout = 10000000;
}

const char* FreqshiftEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Shift";
   case 1:
      return "Delay";
   case 2:
      return "Output";
   }
   return 0;
}
int FreqshiftEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 7;
   case 2:
      return 13;
   }
   return 0;
}

void FreqshiftEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[fsp_shift].set_name("Left");
   fxdata->p[fsp_shift].set_type(ct_freq_shift);
   fxdata->p[fsp_rmult].set_name("Right");
   fxdata->p[fsp_rmult].set_type(ct_percent_bidirectional);
   fxdata->p[fsp_delay].set_name("Time");
   fxdata->p[fsp_delay].set_type(ct_envtime);
   fxdata->p[fsp_feedback].set_name("Feedback");
   fxdata->p[fsp_feedback].set_type(ct_percent);
   fxdata->p[fsp_mix].set_name("Mix");
   fxdata->p[fsp_mix].set_type(ct_percent);

   fxdata->p[fsp_shift].posy_offset = 1;
   fxdata->p[fsp_rmult].posy_offset = 1;

   fxdata->p[fsp_delay].posy_offset = 3;
   fxdata->p[fsp_feedback].posy_offset = 3;

   fxdata->p[fsp_mix].posy_offset = 5;
}
void FreqshiftEffect::init_default_values()
{
   fxdata->p[fsp_shift].val.f = 0.f;
   fxdata->p[fsp_rmult].val.f = 1.f;
   fxdata->p[fsp_delay].val.f = fxdata->p[fsp_delay].val_min.f;
   fxdata->p[fsp_feedback].val.f = 0.0f;
   fxdata->p[fsp_mix].val.f = 1.f;
}
