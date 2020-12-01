#include "PhaserEffect.h"

using namespace vt_dsp;

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

inline void PhaserEffect::init_stages()
{
   n_stages = fxdata->p[ph_stages].val.i;
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
}

void PhaserEffect::process_only_control()
{
   init_stages();
   
   float rate = envelope_rate_linear(-*f[ph_mod_rate]) *
                (fxdata->p[ph_mod_rate].temposync ? storage->temposyncratio : 1.f);

   lfophase += (float)slowrate * rate;
   if (lfophase > 1)
      lfophase = fmod( lfophase, 1.0 ); // lfophase could be > 2 also at very high modulation rates so -=1 doesn't work
   float lfophaseR = lfophase + 0.5 * *f[ph_stereo];
   if (lfophaseR > 1)
      lfophaseR = fmod( lfophaseR, 1.0 );
}

// in the original phaser we had {1.5 / 12, 19.5 / 12, 35 / 12, 50 / 12}
float legacy_freq[4] = {
   1.5  / 12,
   19.5 / 12,
   35   / 12,
   50   / 12,
};

float legacy_span[4] = {
   2.0,
   1.5,
   1.0,
   0.5,
};


void PhaserEffect::setvars()
{
   init_stages();
   
   double rate = envelope_rate_linear(-*f[ph_mod_rate]) *
                 (fxdata->p[ph_mod_rate].temposync ? storage->temposyncratio : 1.f);

   lfophase += (float)slowrate * rate;
   if (lfophase > 1)
      lfophase = fmod( lfophase, 1.0 ); // lfophase could be > 2 also at very high modulation rates so -=1 doesn't work
   float lfophaseR = lfophase + 0.5 * *f[ph_stereo];
   if (lfophaseR > 1)
      lfophaseR = fmod( lfophaseR, 1.0 );
   
   double lfoout = 1.f - fabs(2.0 - 4.0 * lfophase);
   double lfooutR = 1.f - fabs(2.0 - 4.0 * lfophaseR);
   
   
   // if stages is set to 1 to indicate we are in legacy mode, use legacy freqs and spans
   if (n_stages < 2)
   {
      // 4 stages in original phaser mode
      for (int i = 0; i < 2; i++)
      {
         double omega = biquad[2 * i]->calc_omega(2 * *f[ph_center] + legacy_freq[i] + legacy_span[i] * lfoout * *f[ph_mod_depth]);
         biquad[2 * i]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
         omega = biquad[2 * i + 1]->calc_omega(2 * *f[ph_center] + legacy_freq[i] + legacy_span[i] * lfooutR * *f[ph_mod_depth]);
         biquad[2 * i + 1]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
      }
   }
   else
   {
      for (int i = 0; i < n_stages; i++)
       {
          double centre = powf(2, (i + 1.0) * 2  / n_stages);
          double omega = biquad[2 * i]->calc_omega(2 * *f[ph_center] + *f[ph_spread] * centre + 2.0 / (i + 1) * lfoout * *f[ph_mod_depth]);
          biquad[2 * i]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
          omega = biquad[2 * i + 1]->calc_omega(2 * *f[ph_center] + *f[ph_spread] * centre * (2.0 / (i + 1) * lfooutR * *f[ph_mod_depth]));
          biquad[2 * i + 1]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
       }
   }
   
   feedback.newValue(0.95f * *f[ph_feedback]);
   width.set_target_smoothed(db_to_linear(*f[ph_width]));
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

   mix.set_target_smoothed(limit_range(*f[ph_mix], 0.f, 1.f));
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
      return "Stages";
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
      return 13;
   case 2:
      return 21;
   }
   return 0;
}

void PhaserEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();
   fxdata->p[ph_stages].set_name("Count");
   fxdata->p[ph_stages].set_type(ct_phaser_stages);
   fxdata->p[ph_center].set_name("Center");
   fxdata->p[ph_center].set_type(ct_percent_bidirectional);
   fxdata->p[ph_spread].set_name("Spread");
   fxdata->p[ph_spread].set_type(ct_percent);
   fxdata->p[ph_sharpness].set_name("Sharpness");
   fxdata->p[ph_sharpness].set_type(ct_percent_bidirectional);
   fxdata->p[ph_feedback].set_name("Feedback");
   fxdata->p[ph_feedback].set_type(ct_percent_bidirectional);

   fxdata->p[ph_mod_rate].set_name("Rate");
   fxdata->p[ph_mod_rate].set_type(ct_lforate);
   fxdata->p[ph_mod_depth].set_name("Depth");
   fxdata->p[ph_mod_depth].set_type(ct_percent);
   fxdata->p[ph_stereo].set_name("Stereo");
   fxdata->p[ph_stereo].set_type(ct_percent);

   fxdata->p[ph_width].set_name("Width");
   fxdata->p[ph_width].set_type(ct_decibel_narrow);
   fxdata->p[ph_mix].set_name("Mix");
   fxdata->p[ph_mix].set_type(ct_percent);

   fxdata->p[ph_stages].posy_offset = -15;
   fxdata->p[ph_center].posy_offset = 3;
   fxdata->p[ph_spread].posy_offset = -13;
   fxdata->p[ph_sharpness].posy_offset = 3;
   fxdata->p[ph_feedback].posy_offset = 7;

   fxdata->p[ph_mod_rate].posy_offset = 7;
   fxdata->p[ph_mod_depth].posy_offset = 7;
   fxdata->p[ph_stereo].posy_offset = 7;

   fxdata->p[ph_width].posy_offset = 7;
   fxdata->p[ph_mix].posy_offset = 11;

}
void PhaserEffect::init_default_values()
{
   fxdata->p[ph_stages].val.i = 4;
   fxdata->p[ph_width].val.f = 0.f;
   fxdata->p[ph_spread].val.f = 0.f;
}

void PhaserEffect::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision < 14)
   {
      fxdata->p[ph_stages].val.i = 4;
      fxdata->p[ph_width].val.f = 0.f;
   }
}

int PhaserEffect::get_ringout_decay()
{
   auto fb = *f[ph_feedback];
   // The ringout is longer at high feedbacks. This is just a heuristic based on
   // testing with the patch in #2663. Note that at feedbacks above 1 (from
   // modulation or control pushes) you can get infinite self modulation
   // so run forever then
   if( fb > 1 || fb < -1 )
      return -1;
   if( fb > 0.9 || fb < -0.9 )
      return 5000;
   if( fb > 0.5 || fb < -0.5 )
      return 3000;
   return 1000;
}
