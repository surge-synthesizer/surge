#include "ConditionerEffect.h"
#include <vt_dsp/basic_dsp.h>
#include "SurgeParamConfig.h"

using namespace std;

using namespace vt_dsp;

ConditionerEffect::ConditionerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), band1(storage), band2(storage)
{
   bufpos = 0;

   ampL.set_blocksize(BLOCK_SIZE);
   ampR.set_blocksize(BLOCK_SIZE);
   width.set_blocksize(BLOCK_SIZE);
   postamp.set_blocksize(BLOCK_SIZE);
}

ConditionerEffect::~ConditionerEffect()
{}

void ConditionerEffect::init()
{
   setvars(true);
   ef = 0;
   bufpos = 0;
   filtered_lamax = 1.f;
   filtered_lamax2 = 1.f;
   gain = 1.f;
   memset(lamax, 0, sizeof(float) * (lookahead << 1));
   memset(delayed[0], 0, sizeof(float) * lookahead);
   memset(delayed[1], 0, sizeof(float) * lookahead);

   vu[0] = 0.f;
   vu[1] = 0.f;
   vu[2] = 1.f;
   vu[4] = 0.f;
   vu[5] = 0.f;
   assert(KNumVuSlots >= 5);
}

void ConditionerEffect::setvars(bool init)
{
   band1.coeff_peakEQ(band1.calc_omega(-2.5), 2, *f[cond_bass]);
   band2.coeff_peakEQ(band2.calc_omega(4.75), 2, *f[cond_treble]);
   if (init)
   {
   }
}

void ConditionerEffect::process_only_control()
{
   float am = 1.0f + 0.9f * *f[cond_attack];
   float rm = 1.0f + 0.9f * *f[cond_release];
   float attack = 0.001f * am * am;
   float release = 0.0001f * rm * rm;

   float a = storage->vu_falloff;
   vu[0] = min(8.f, a * vu[0]);
   vu[1] = min(8.f, a * vu[1]);
   vu[4] = min(8.f, a * vu[4]);
   vu[5] = min(8.f, a * vu[5]);

   for (int k = 0; k < BLOCK_SIZE; k++)
   {
      filtered_lamax = (1 - attack) * filtered_lamax + attack;
      filtered_lamax2 = (1 - release) * filtered_lamax2 + (release)*filtered_lamax;
      if (filtered_lamax > filtered_lamax2)
         filtered_lamax2 = filtered_lamax;

      gain = 1.f / filtered_lamax2;
   }

   vu[2] = gain;
}

void ConditionerEffect::process(float* dataL, float* dataR)
{
   float am = 1.0f + 0.9f * *f[cond_attack];
   float rm = 1.0f + 0.9f * *f[cond_release];
   float attack = 0.001f * am * am;
   float release = 0.0001f * rm * rm;

   float a = storage->vu_falloff;
   vu[0] = min(8.f, a * vu[0]);
   vu[1] = min(8.f, a * vu[1]);
   vu[4] = min(8.f, a * vu[4]);
   vu[5] = min(8.f, a * vu[5]);

   setvars(false);
   band1.process_block(dataL, dataR);
   band2.process_block(dataL, dataR);
   float pregain = db_to_linear(-*f[cond_threshold]);
   ampL.set_target_smoothed(pregain * 0.5f * clamp1bp(1 - *f[cond_balance]));
   ampR.set_target_smoothed(pregain * 0.5f * clamp1bp(1 + *f[cond_balance]));
   width.set_target_smoothed(clamp1bp(*f[cond_width]));
   postamp.set_target_smoothed(db_to_linear(*f[cond_gain]));

   float M alignas(16)[BLOCK_SIZE],
         S alignas(16)[BLOCK_SIZE]; // wb = write-buffer
   encodeMS(dataL, dataR, M, S, BLOCK_SIZE_QUAD);
   width.multiply_block(S, BLOCK_SIZE_QUAD);
   decodeMS(M, S, dataL, dataR, BLOCK_SIZE_QUAD);
   ampL.multiply_block(dataL, BLOCK_SIZE_QUAD);
   ampR.multiply_block(dataR, BLOCK_SIZE_QUAD);

   vu[0] = max(vu[0], get_absmax(dataL, BLOCK_SIZE_QUAD));
   vu[1] = max(vu[1], get_absmax(dataR, BLOCK_SIZE_QUAD));

   for (int k = 0; k < BLOCK_SIZE; k++)
   {
      float dL = delayed[0][bufpos];
      float dR = delayed[1][bufpos];

      float la = lamax[lookahead - 2];

      la = sqrt(2.f * la); // RMS test

      la = max(1.f, la); // * outscale_inv);
      filtered_lamax = (1 - attack) * filtered_lamax + attack * la;
      filtered_lamax2 = (1 - release) * filtered_lamax2 + (release)*filtered_lamax;
      if (filtered_lamax > filtered_lamax2)
         filtered_lamax2 = filtered_lamax;

      gain = rcp(filtered_lamax2);

      delayed[0][bufpos] = dataL[k];
      delayed[1][bufpos] = dataR[k];

      lamax[bufpos] = max(fabsf(dataL[k]), fabsf(dataR[k]));
      lamax[bufpos] = lamax[bufpos] * lamax[bufpos]; // RMS

      int of = 0;
      for (int i = 0; i < (lookahead_bits); i++)
      {
         int nextof = of + (lookahead >> i);
         lamax[nextof + (bufpos >> (i + 1))] =
             max(lamax[of + (bufpos >> i)], lamax[of + ((bufpos >> i) ^ 0x1)]);
         of = nextof;
      }
      dataL[k] = (gain)*dL;
      dataR[k] = (gain)*dR;

      bufpos = (bufpos + 1) & (lookahead - 1);
   }

   postamp.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

   vu[2] = gain;

   vu[4] = max(vu[4], get_absmax(dataL, BLOCK_SIZE_QUAD));
   vu[5] = max(vu[5], get_absmax(dataR, BLOCK_SIZE_QUAD));
}

int ConditionerEffect::vu_type(int id)
{
   switch (id)
   {
   case 0:
      return vut_vu_stereo;
   case 1:
      return vut_gain_reduction;
   case 2:
      return vut_vu_stereo;
   }
   return 0;
}

int ConditionerEffect::vu_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 15;
   case 1:
      return 17;
   case 2:
      return 19;
   }
   return 0;
}
const char* ConditionerEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "EQ";
   case 1:
      return "Stereo";
   case 2:
      return "Limiter";
   case 3:
      return "Output";
   }
   return 0;
}
int ConditionerEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 7;
   case 2:
      return 13;
   case 3:
      return 27;
   }
   return 0;
}

void ConditionerEffect::suspend()
{
   init();
}

void ConditionerEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[cond_bass].set_name("Bass");
   fxdata->p[cond_bass].set_type(ct_decibel_extra_narrow);
   fxdata->p[cond_treble].set_name("Treble");
   fxdata->p[cond_treble].set_type(ct_decibel_extra_narrow);

   fxdata->p[cond_width].set_name("Width");
   fxdata->p[cond_width].set_type(ct_percent_bidirectional);
   fxdata->p[cond_balance].set_name("Balance");
   fxdata->p[cond_balance].set_type(ct_percent_bidirectional);

   fxdata->p[cond_threshold].set_name("Threshold");
   fxdata->p[cond_threshold].set_type(ct_decibel_attenuation);
   fxdata->p[cond_attack].set_name("Attack Rate");
   fxdata->p[cond_attack].set_type(ct_percent_bidirectional);
   fxdata->p[cond_release].set_name("Release Rate");
   fxdata->p[cond_release].set_type(ct_percent_bidirectional);
   fxdata->p[cond_gain].set_name("Gain");
   fxdata->p[cond_gain].set_type(ct_decibel_attenuation);

   fxdata->p[cond_bass].posy_offset = 1;
   fxdata->p[cond_treble].posy_offset = 1;

   fxdata->p[cond_width].posy_offset = 3;
   fxdata->p[cond_balance].posy_offset = 3;

   fxdata->p[cond_threshold].posy_offset = 11;
   fxdata->p[cond_attack].posy_offset = 11;
   fxdata->p[cond_release].posy_offset = 11;
   fxdata->p[cond_gain].posy_offset = 13;
}
void ConditionerEffect::init_default_values()
{}
