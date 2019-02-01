#include "effect_defs.h"

/* rotary_speaker	*/

using namespace std;

enum rsparams
{
   rsp_rate = 0,
   rsp_doppler,
   rsp_amp,
   rsp_numparams
};

RotarySpeakerEffect::RotarySpeakerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), xover(storage), lowbass(storage)
{}

/*rotary_speaker::rotary_speaker(float *ep, timedata *td) : groupeffect(ep,td)
{
        strcpy(effectname,"rotary speaker");
        parameter_count = 3;
        strcpy(ctrllabel[0], "horn rate");	ctrlmode[0] = cm_frequency1hz;
        strcpy(ctrllabel[1], "depth");		ctrlmode[1] = cm_percent;
        strcpy(ctrllabel[2], "drive");		ctrlmode[2] = cm_decibel;

        buffer = new float[chtemp];
        
        bufferlength = chtemp;

        memset(buffer,chtemp,chtemp*sizeof(float));
        wpos = 0;

        // set constant parameters
        f_xover[0] = 0.862496f;		// 800Hz;
        f_xover[1] = 0;
        f_lowbass[0] = -1.14f;		// 200Hz
        f_lowbass[1] = 0;

        f_rotor_lp[0][1] = 0;
        f_rotor_lp[1][1] = 0;
        
        xover = new LP2A(f_xover);
        lowbass = new LP2A(f_lowbass);
        rotor_lp[0] = new LP2A(f_rotor_lp[0]);
        rotor_lp[1] = new LP2A(f_rotor_lp[1]);
}*/

RotarySpeakerEffect::~RotarySpeakerEffect()
{}

void RotarySpeakerEffect::init()
{
   memset(buffer, 0, max_delay_length * sizeof(float));

   // set constant parameters
   /*f_xover[0] = 0.862496f;		// 800Hz;
   f_xover[1] = 0;
   f_lowbass[0] = -1.14f;		// 200Hz
   f_lowbass[1] = 0;*/

   xover.suspend();
   lowbass.suspend();
   xover.coeff_LP2B(xover.calc_omega(0.862496f), 0.707);
   lowbass.coeff_LP2B(xover.calc_omega(-1.14f), 0.707);
   wpos = 0;
}

void RotarySpeakerEffect::suspend()
{
   memset(buffer, 0, max_delay_length * sizeof(float));
   xover.suspend();
   lowbass.suspend();
   wpos = 0;
}

void RotarySpeakerEffect::init_default_values()
{
   fxdata->p[0].val.f = 1.f;
   fxdata->p[1].val.f = 0.25f;
   fxdata->p[2].val.f = 0.5f;
}

const char* RotarySpeakerEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Rate";
   case 1:
      return "Depth";
   }
   return 0;
}
int RotarySpeakerEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 5;
   }
   return 0;
}

void RotarySpeakerEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[0].set_name("Horn Rate");
   fxdata->p[0].set_type(ct_lforate);
   fxdata->p[1].set_name("Doppler");
   fxdata->p[1].set_type(ct_percent);
   fxdata->p[2].set_name("Amp Mod");
   fxdata->p[2].set_type(ct_percent);

   fxdata->p[0].posy_offset = 1;

   fxdata->p[1].posy_offset = 3;
   fxdata->p[2].posy_offset = 3;
}

/*void rotary_speaker::init_params()
{
        if(!param) return;
        // preset values
        param[0] = 1.0f;
        param[1] = 1.0f;
}*/

void RotarySpeakerEffect::process_only_control()
{
   lfo.set_rate(2 * M_PI * powf(2, *f[rsp_rate]) * dsamplerate_inv * BLOCK_SIZE);
   lf_lfo.set_rate(0.7 * 2 * M_PI * powf(2, *f[rsp_rate]) * dsamplerate_inv * BLOCK_SIZE);

   lfo.process();
   lf_lfo.process();
}

void RotarySpeakerEffect::process(float* dataL, float* dataR)
{
   lfo.set_rate(2 * M_PI * powf(2, *f[rsp_rate]) * dsamplerate_inv * BLOCK_SIZE);
   lf_lfo.set_rate(0.7 * 2 * M_PI * powf(2, *f[rsp_rate]) * dsamplerate_inv);

   float precalc0 = (-2 - (float)lfo.i);
   float precalc1 = (-1 - (float)lfo.r);
   float precalc2 = (+1 - (float)lfo.r);
   float lenL = sqrt(precalc0 * precalc0 + precalc1 * precalc1);
   float lenR = sqrt(precalc0 * precalc0 + precalc2 * precalc2);

   float delay = samplerate * 0.0018f * *f[rsp_doppler];
   dL.newValue(delay * lenL);
   dR.newValue(delay * lenR);
   float dotp_L = (precalc1 * (float)lfo.r + precalc0 * (float)lfo.i) / lenL;
   float dotp_R = (precalc2 * (float)lfo.r + precalc0 * (float)lfo.i) / lenR;

   float a = *f[rsp_amp] * 0.6f;
   hornamp[0].newValue((1.f - a) + a * dotp_L);
   hornamp[1].newValue((1.f - a) + a * dotp_R);

   lfo.process();

   float upper[BLOCK_SIZE];
   float lower[BLOCK_SIZE];
   float lower_sub[BLOCK_SIZE];
   float tbufferL[BLOCK_SIZE];
   float tbufferR[BLOCK_SIZE];

   int k;

   for (k = 0; k < BLOCK_SIZE; k++)
   {
      // float input = (float)tanh_fast(0.5f*dataL[k]+dataR[k]*drive.v);
      float input = 0.5f * (dataL[k] + dataR[k]);
      upper[k] = input;
      lower[k] = input;
      // drive.process();
   }

   xover.process_block(lower);
   // xover->process(lower,0);

   for (k = 0; k < BLOCK_SIZE; k++)
   {
      // feed delay input
      int wp = (wpos + k) & (max_delay_length - 1);
      lower_sub[k] = lower[k];
      upper[k] -= lower[k];
      buffer[wp] = upper[k];

      /*int i_dtL = max(BLOCK_SIZE,min((int)dL.v,max_delay_length-1)),
              i_dtR = max(BLOCK_SIZE,min((int)dR.v,max_delay_length-1)),
              sincL = FIRipol_N*(int)(FIRipol_M*(ceil(dL.v)-dL.v)),
              sincR = FIRipol_N*(int)(FIRipol_M*(ceil(dR.v)-dR.v));*/

      int i_dtimeL = max(BLOCK_SIZE, min((int)dL.v, max_delay_length - FIRipol_N - 1));
      int i_dtimeR = max(BLOCK_SIZE, min((int)dR.v, max_delay_length - FIRipol_N - 1));

      int rpL = (wpos - i_dtimeL + k);
      int rpR = (wpos - i_dtimeR + k);

      int sincL = FIRipol_N *
                  limit_range((int)(FIRipol_M * (float(i_dtimeL + 1) - dL.v)), 0, FIRipol_M - 1);
      int sincR = FIRipol_N *
                  limit_range((int)(FIRipol_M * (float(i_dtimeR + 1) - dR.v)), 0, FIRipol_M - 1);

      // get delay output
      tbufferL[k] = 0;
      tbufferR[k] = 0;
      for (int i = 0; i < FIRipol_N; i++)
      {
         tbufferL[k] +=
             buffer[(rpL - i) & (max_delay_length - 1)] * sinctable1X[sincL + FIRipol_N - i];
         tbufferR[k] +=
             buffer[(rpR - i) & (max_delay_length - 1)] * sinctable1X[sincR + FIRipol_N - i];
      }
      dL.process();
      dR.process();
   }

   // f_rotor_lp[0][0] = 3.0f + dotp_L;
   // f_rotor_lp[1][0] = 3.0f + dotp_R;

   // rotor_lp[0]->process(tbufferL,0);
   // rotor_lp[1]->process(tbufferR,0);
   // lowbass->process(lower_sub,0);
   lowbass.process_block(lower_sub);

   for (k = 0; k < BLOCK_SIZE; k++)
   {
      lower[k] -= lower_sub[k];

      float bass = lower_sub[k] + lower[k] * (lf_lfo.r * 0.6f + 0.3f);

      dataL[k] = hornamp[0].v * tbufferL[k] + bass;
      dataR[k] = hornamp[1].v * tbufferR[k] + bass;
      lf_lfo.process();
      hornamp[0].process();
      hornamp[1].process();
   }

   wpos += BLOCK_SIZE;
   wpos = wpos & (max_delay_length - 1);
}
