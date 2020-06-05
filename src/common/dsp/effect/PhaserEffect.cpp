#include "effect_defs.h"

/* phaser			*/

using namespace vt_dsp;

enum phaseparam
{
   pp_base = 0,
   // pp_spread,
   // pp_distribution,
   // pp_count,
   pp_feedback,
   pp_q,
   pp_lforate,
   pp_lfodepth,
   pp_stereo,
   pp_mix,
   pp_nparams,
};

float bend(float x, float b)
{
   return (1.f + b) * x - b * x * x * x;
}

PhaserEffect::PhaserEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   for (int i = 0; i < n_bq_units; i++)
   {
      biquad[i] = (BiquadFilter*)_aligned_malloc(sizeof(BiquadFilter), 16);
      memset(biquad[i], 0, sizeof(BiquadFilter));
      new (biquad[i]) BiquadFilter(storage);
   }
   mix.set_blocksize(BLOCK_SIZE);
   feedback.setBlockSize(BLOCK_SIZE * slowrate);
   bi = 0;
}

PhaserEffect::~PhaserEffect()
{
   for (int i = 0; i < n_bq_units; i++)
      _aligned_free(biquad[i]);
}

void PhaserEffect::init()
{
   lfophase = 0.25f;
   // setvars(true);
   for (int i = 0; i < n_bq_units; i++)
   {
      // notch[i]->coeff_LP(1.0,1.0);
      biquad[i]->suspend();
   }
   clear_block(L, BLOCK_SIZE_QUAD);
   clear_block(R, BLOCK_SIZE_QUAD);
   mix.set_target(1.f);
   mix.instantize();
   bi = 0;
   dL = 0;
   dR = 0;
   bi = 0;
}

void PhaserEffect::process_only_control()
{
   float rate = envelope_rate_linear(-*f[pp_lforate]) *
                (fxdata->p[pp_lforate].temposync ? storage->temposyncratio : 1.f);

   lfophase += (float)slowrate * rate;
   if (lfophase > 1)
      lfophase = fmod( lfophase, 1.0 ); // lfophase could be > 2 also at very high modulation rates so -=1 doesn't work
   float lfophaseR = lfophase + 0.5 * *f[pp_stereo];
   if (lfophaseR > 1)
      lfophaseR = fmod( lfophaseR, 1.0 );
}

float basefreq[4] = {1.5 / 12, 19.5 / 12, 35 / 12, 50 / 12};
float basespan[4] = {2.0, 1.5, 1.0, 0.5};

void PhaserEffect::setvars()
{
   double rate = envelope_rate_linear(-*f[pp_lforate]) *
                 (fxdata->p[pp_lforate].temposync ? storage->temposyncratio : 1.f);

   lfophase += (float)slowrate * rate;
   if (lfophase > 1)
      lfophase = fmod( lfophase, 1.0 ); // lfophase could be > 2 also at very high modulation rates so -=1 doesn't work
   float lfophaseR = lfophase + 0.5 * *f[pp_stereo];
   if (lfophaseR > 1)
      lfophaseR = fmod( lfophaseR, 1.0 );
   
   double lfoout = 1.f - fabs(2.0 - 4.0 * lfophase);
   double lfooutR = 1.f - fabs(2.0 - 4.0 * lfophaseR);

   for (int i = 0; i < n_bq; i++)
   {
      double omega = biquad[0]->calc_omega(2.0 * *f[pp_base] + basefreq[i] +
                                           basespan[i] * lfoout * *f[pp_lfodepth]);
      biquad[i]->coeff_APF(omega, 1.0 + 0.8 * *f[pp_q]);
      omega = biquad[0]->calc_omega(2.0 * *f[pp_base] + basefreq[i] +
                                    basespan[i] * lfooutR * *f[pp_lfodepth]);
      biquad[i + n_bq]->coeff_APF(omega, 1.0 + 0.8 * *f[pp_q]);
   }

   feedback.newValue(0.95f * *f[pp_feedback]);
}

void PhaserEffect::process(float* dataL, float* dataR)
{
   if (bi == 0)
      setvars();
   bi = (bi + 1) & slowrate_m1;
   // feedback.multiply_2_blocks((__m128*)L,(__m128*)R, BLOCK_SIZE_QUAD);
   // accumulate_block((__m128*)dataL, (__m128*)L, BLOCK_SIZE_QUAD);
   // accumulate_block((__m128*)dataR, (__m128*)R, BLOCK_SIZE_QUAD);
   for (int i = 0; i < BLOCK_SIZE; i++)
   {
      feedback.process();
      dL = dataL[i] + dL * feedback.v;
      dR = dataR[i] + dR * feedback.v;
      dL = limit_range(dL, -32.f, 32.f);
      dR = limit_range(dR, -32.f, 32.f);
      dL = biquad[0]->process_sample(dL);
      dL = biquad[1]->process_sample(dL);
      dL = biquad[2]->process_sample(dL);
      dL = biquad[3]->process_sample(dL);
      dR = biquad[4]->process_sample(dR);
      dR = biquad[5]->process_sample(dR);
      dR = biquad[6]->process_sample(dR);
      dR = biquad[7]->process_sample(dR);
      L[i] = dL;
      R[i] = dR;
   }

   mix.set_target_smoothed(limit_range(*f[pp_mix], 0.f, 1.f));
   mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void PhaserEffect::suspend()
{
   init();
}

const char* PhaserEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Phaser";
   case 1:
      return "Modulation";
   case 2:
      return "Output";
   }
   return 0;
}
int PhaserEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 9;
   case 2:
      return 17;
   }
   return 0;
}

void PhaserEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[pp_base].set_name("Base Frequency");
   fxdata->p[pp_base].set_type(ct_percent_bidirectional);
   fxdata->p[pp_feedback].set_name("Feedback");
   fxdata->p[pp_feedback].set_type(ct_percent_bidirectional);
   fxdata->p[pp_q].set_name("Q");
   fxdata->p[pp_q].set_type(ct_percent_bidirectional);

   fxdata->p[pp_lforate].set_name("Rate");
   fxdata->p[pp_lforate].set_type(ct_lforate);
   fxdata->p[pp_lfodepth].set_name("Depth");
   fxdata->p[pp_lfodepth].set_type(ct_percent);
   fxdata->p[pp_stereo].set_name("Stereo");
   fxdata->p[pp_stereo].set_type(ct_percent);

   fxdata->p[pp_mix].set_name("Mix");
   fxdata->p[pp_mix].set_type(ct_percent);

   for (int i = 0; i < pp_nparams; i++)
      fxdata->p[i].posy_offset = 1 + ((i >= pp_lforate) ? 2 : 0) + ((i >= pp_mix) ? 2 : 0);
}
void PhaserEffect::init_default_values()
{
   // fxdata->p[0].val.f = 0.f;
}
