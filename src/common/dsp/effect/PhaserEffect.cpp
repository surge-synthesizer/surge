#include "effect_defs.h"

/* phaser			*/

using namespace vt_dsp;

enum phaseparam
{
   pp_base = 0,
   pp_feedback,
   pp_q,
   pp_lforate,
   pp_lfodepth,
   pp_stereo,
   pp_mix,
   pp_width,
   pp_stages,
   // pp_spread,
   // pp_distribution,
   pp_nparams
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
   n_bq_units_initialised = n_bq_units;
   feedback.setBlockSize(BLOCK_SIZE * slowrate);
   width.set_blocksize(BLOCK_SIZE);
   mix.set_blocksize(BLOCK_SIZE);
   bi = 0;
}

PhaserEffect::~PhaserEffect()
{
   for (int i = 0; i < n_bq_units_initialised; i++)
      _aligned_free(biquad[i]);
}

void PhaserEffect::init()
{
   bi = 0;
   dL = 0;
   dR = 0;
   lfophase = 0.25f;
   
   for (int i = 0; i < n_bq_units_initialised; i++)
   {
      biquad[i]->suspend();
   }
   clear_block(L, BLOCK_SIZE_QUAD);
   clear_block(R, BLOCK_SIZE_QUAD);
   mix.set_target(1.f);
   width.instantize();
   mix.instantize();
}

void PhaserEffect::process_only_control()
{
    n_stages = ceil(*f[pp_stages] * 16);
    n_bq_units = n_stages * 2;
    
    if (n_bq_units_initialised < n_bq_units)
    {
       // we need to increase the number of stages
       for (int k = n_bq_units_initialised; k < n_bq_units; k++)
       {
          biquad[k] = (BiquadFilter*)_aligned_malloc(sizeof(BiquadFilter), 16);
          memset(biquad[k], 0, sizeof(BiquadFilter));
          new (biquad[k]) BiquadFilter(storage);
       }
       n_bq_units_initialised = n_bq_units;
    }
   
   float rate = envelope_rate_linear(-*f[pp_lforate]) *
                (fxdata->p[pp_lforate].temposync ? storage->temposyncratio : 1.f);

   lfophase += (float)slowrate * rate;
   if (lfophase > 1)
      lfophase = fmod( lfophase, 1.0 ); // lfophase could be > 2 also at very high modulation rates so -=1 doesn't work
   float lfophaseR = lfophase + 0.5 * *f[pp_stereo];
   if (lfophaseR > 1)
      lfophaseR = fmod( lfophaseR, 1.0 );
}
// in the original phaser we had {1.5 / 12, 19.5 / 12, 35 / 12, 50 / 12}
float basefreq[16] = {
   1.5  / 12,
   19.5 / 12,
   35   / 12,
   50   / 12,
   2.0,
   3.80735492206,
   1.0,
   4.32192809489,
   3.16992500144,
   3.90689059561,
   2.80735492206,
   4.16992500144,
   2.32192809489,
   4.24792751344,
   2.32192809489,
   1.58496250072,
};
   
// log_2((12000 - freq) / 2900) retaining first four from original code
float basespan[16] = {
   2.0,
   1.5,
   1.0,
   0.5,
   1.7858751946471525,
   0.9233787183970875,
   1.890211854461888,
   0.04890960048094651,
   1.4639470997597905,
   0.7858751946471525,
   1.5932301167047567,
   0.4639470997597903,
   1.7118746132033758,
   0.2713020218173943,
   1.7118746132033758,
   1.8699394594356273
};

void PhaserEffect::setvars()
{
   n_stages = fxdata->p[pp_stages].val.i;
   n_bq_units = n_stages * 2;
   
   if (n_bq_units_initialised < n_bq_units)
   {
      // we need to increase the number of stages
      for (int k = n_bq_units_initialised; k < n_bq_units; k++)
      {
         biquad[k] = (BiquadFilter*)_aligned_malloc(sizeof(BiquadFilter), 16);
         memset(biquad[k], 0, sizeof(BiquadFilter));
         new (biquad[k]) BiquadFilter(storage);
      }
      n_bq_units_initialised = n_bq_units;
   }
   
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

   for (int i = 0; i < n_stages; i++)
   {
      double omega = biquad[2 * i]->calc_omega(2.0 * *f[pp_base] + basefreq[i] +
                                           basespan[i] * lfoout * *f[pp_lfodepth]);
      biquad[2 * i]->coeff_APF(omega, 1.0 + 0.8 * *f[pp_q]);
      omega = biquad[2 * i + 1]->calc_omega(2.0 * *f[pp_base] + basefreq[i] +
                                    basespan[i] * lfooutR * *f[pp_lfodepth]);
      biquad[2 * i + 1]->coeff_APF(omega, 1.0 + 0.8 * *f[pp_q]);
   }

   feedback.newValue(0.95f * *f[pp_feedback]);
   width.set_target_smoothed(db_to_linear(*f[pp_width]));
}

void PhaserEffect::process(float* dataL, float* dataR)
{
   if (bi == 0)
      setvars();
   bi = (bi + 1) & slowrate_m1;

   for (int i = 0; i < BLOCK_SIZE; i++)
   {
      feedback.process();
      dL = dataL[i] + dL * feedback.v;
      dR = dataR[i] + dR * feedback.v;
      dL = limit_range(dL, -32.f, 32.f);
      dR = limit_range(dR, -32.f, 32.f);
      
      for (int curr_stage = 0; curr_stage < n_stages; curr_stage++)
      {
         dL = biquad[2 * curr_stage]->process_sample(dL);
         dR = biquad[2 * curr_stage + 1]->process_sample(dR);
      }
      L[i] = dL;
      R[i] = dR;
   }

   // scale width
   float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
   encodeMS(L, R, M, S, BLOCK_SIZE_QUAD);
   width.multiply_block(S, BLOCK_SIZE_QUAD);
   decodeMS(M, S, L, R, BLOCK_SIZE_QUAD);

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
      return 11;
   case 2:
      return 19;
   }
   return 0;
}

void PhaserEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();
   fxdata->p[pp_stages].set_name("Stages");
   fxdata->p[pp_stages].set_type(ct_phaser_stages);
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

   fxdata->p[pp_width].set_name("Width");
   fxdata->p[pp_width].set_type(ct_decibel_narrow);
   fxdata->p[pp_mix].set_name("Mix");
   fxdata->p[pp_mix].set_type(ct_percent);

   fxdata->p[pp_stages].posy_offset = -15;
   fxdata->p[pp_base].posy_offset = 3;
   fxdata->p[pp_feedback].posy_offset = 3;
   fxdata->p[pp_q].posy_offset = 3;
   fxdata->p[pp_lforate].posy_offset = 5;
   fxdata->p[pp_lfodepth].posy_offset = 5;
   fxdata->p[pp_stereo].posy_offset = 5;
   fxdata->p[pp_width].posy_offset = 5;
   fxdata->p[pp_mix].posy_offset = 9;

}
void PhaserEffect::init_default_values()
{
   fxdata->p[pp_stages].val.i = 4;
   fxdata->p[pp_width].val.f = 0.f;
}

void PhaserEffect::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision < 14)
   {
      fxdata->p[pp_stages].val.i = 4;
      fxdata->p[pp_width].val.f = 0.f;
   }
}
