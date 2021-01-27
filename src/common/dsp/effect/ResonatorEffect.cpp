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

#include "ResonatorEffect.h"

ResonatorEffect::ResonatorEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   gain.set_blocksize(BLOCK_SIZE);
   mix.set_blocksize(BLOCK_SIZE);
}

ResonatorEffect::~ResonatorEffect()
{}

void ResonatorEffect::init()
{
   setvars(true);
   bi = 0;
}

void ResonatorEffect::setvars(bool init)
{
   if (init)
   {
      gain.set_target(1.f);
      mix.set_target(1.f);

      gain.instantize();
      mix.instantize();
   }
   else
   {

   }
}

void ResonatorEffect::process(float* dataL, float* dataR)
{
   if (bi == 0)
      setvars(false);
   bi = (bi + 1) & slowrate_m1;

   copy_block(dataL, L, BLOCK_SIZE_QUAD);
   copy_block(dataR, R, BLOCK_SIZE_QUAD);

   gain.set_target_smoothed(db_to_linear(*f[resonator_gain]));
   gain.multiply_2_blocks(L, R, BLOCK_SIZE_QUAD);

   mix.set_target_smoothed(limit_range(*f[resonator_mix], -1.f, 1.f));
   mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void ResonatorEffect::suspend()
{
   init();
}

const char* ResonatorEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Low Band";
   case 1:
      return "Mid Band";
   case 2:
      return "High Band";
   case 3:
      return "Output";
   }
   return 0;
}
int ResonatorEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 9;
   case 2:
      return 17;
   case 3:
      return 25;
   }
   return 0;
}

void ResonatorEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[resonator_freq1].set_name("Frequency 1");
   fxdata->p[resonator_freq1].set_type(ct_freq_reson_band1);
   fxdata->p[resonator_freq1].posy_offset = 1;
   fxdata->p[resonator_res1].set_name("Resonance 1");
   fxdata->p[resonator_res1].set_type(ct_percent);
   fxdata->p[resonator_res1].posy_offset = 1;
   fxdata->p[resonator_res1].val_default.f = 0.5f;
   fxdata->p[resonator_gain1].set_name("Gain 1");
   fxdata->p[resonator_gain1].set_type(ct_amplitude);
   fxdata->p[resonator_gain1].posy_offset = 1;
   fxdata->p[resonator_gain1].val_default.f = 0.5f;

   fxdata->p[resonator_freq2].set_name("Frequency 2");
   fxdata->p[resonator_freq2].set_type(ct_freq_reson_band2);
   fxdata->p[resonator_freq2].posy_offset = 3;
   fxdata->p[resonator_res2].set_name("Resonance 2");
   fxdata->p[resonator_res2].set_type(ct_percent);
   fxdata->p[resonator_res2].posy_offset = 3;
   fxdata->p[resonator_res2].val_default.f = 0.5f;
   fxdata->p[resonator_gain2].set_name("Gain 2");
   fxdata->p[resonator_gain2].set_type(ct_amplitude);
   fxdata->p[resonator_gain2].posy_offset = 3;
   fxdata->p[resonator_gain2].val_default.f = 0.5f;

   fxdata->p[resonator_freq3].set_name("Frequency 3");
   fxdata->p[resonator_freq3].set_type(ct_freq_reson_band3);
   fxdata->p[resonator_freq3].posy_offset = 5;
   fxdata->p[resonator_res3].set_name("Resonance 3");
   fxdata->p[resonator_res3].set_type(ct_percent);
   fxdata->p[resonator_res3].posy_offset = 5;
   fxdata->p[resonator_res3].val_default.f = 0.5f;
   fxdata->p[resonator_gain3].set_name("Gain 3");
   fxdata->p[resonator_gain3].set_type(ct_amplitude);
   fxdata->p[resonator_gain3].posy_offset = 5;
   fxdata->p[resonator_gain3].val_default.f = 0.5f;

   fxdata->p[resonator_mode].set_name("Mode");
   fxdata->p[resonator_mode].set_type(ct_reson_mode);
   fxdata->p[resonator_mode].posy_offset = 7;
   fxdata->p[resonator_gain].set_name("Gain");
   fxdata->p[resonator_gain].set_type(ct_decibel);
   fxdata->p[resonator_gain].posy_offset = 7;
   fxdata->p[resonator_mix].set_name("Mix");
   fxdata->p[resonator_mix].set_type(ct_percent);
   fxdata->p[resonator_mix].posy_offset = 7;
   fxdata->p[resonator_mix].val_default.f = 1.f;
}

void ResonatorEffect::init_default_values()
{
   fxdata->p[resonator_freq1].val.f = -25.65f;  // 100 Hz
   fxdata->p[resonator_res1].val.f = 0.5f;
   fxdata->p[resonator_gain1].val.f = 0.5f;

   fxdata->p[resonator_freq2].val.f = 8.038216f; // 700 Hz
   fxdata->p[resonator_res2].val.f = 0.5f;
   fxdata->p[resonator_gain2].val.f = 0.5f;

   fxdata->p[resonator_freq3].val.f = 35.90135f; // 3500 Hz
   fxdata->p[resonator_res3].val.f = 0.5f;
   fxdata->p[resonator_gain3].val.f = 0.5f;

   fxdata->p[resonator_mode].val.i = 1;
   fxdata->p[resonator_gain].val.f = 0.f;
   fxdata->p[resonator_mix].val.f = 1.f;
}