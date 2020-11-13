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

#include "Eq3BandEffect.h"

/* eq3band */

Eq3BandEffect::Eq3BandEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), band1(storage), band2(storage), band3(storage)
{
   band1.setBlockSize(BLOCK_SIZE * slowrate); // does not matter ATM as they're smoothed
   band2.setBlockSize(BLOCK_SIZE * slowrate);
   band3.setBlockSize(BLOCK_SIZE * slowrate);

   gain.set_blocksize(BLOCK_SIZE);
   mix.set_blocksize(BLOCK_SIZE);
}

Eq3BandEffect::~Eq3BandEffect()
{}

void Eq3BandEffect::init()
{
   setvars(true);
   band1.suspend();
   band2.suspend();
   band3.suspend();
   bi = 0;
}

void Eq3BandEffect::setvars(bool init)
{
   if (init)
   {
      // Set the bands to 0dB so the EQ fades in init
      band1.coeff_peakEQ(band1.calc_omega(fxdata->p[eq3_freq1].val.f * (1.f / 12.f)), fxdata->p[eq3_bw1].val.f, 1.f);
      band2.coeff_peakEQ(band2.calc_omega(fxdata->p[eq3_freq2].val.f * (1.f / 12.f)), fxdata->p[eq3_bw2].val.f, 1.f);
      band3.coeff_peakEQ(band3.calc_omega(fxdata->p[eq3_freq3].val.f * (1.f / 12.f)), fxdata->p[eq3_bw3].val.f, 1.f);

      band1.coeff_instantize();
      band2.coeff_instantize();
      band3.coeff_instantize();

      gain.set_target(1.f);
      mix.set_target(1.f);

      gain.instantize();
      mix.instantize();
   }
   else
   {
      band1.coeff_peakEQ(band1.calc_omega(*f[eq3_freq1] * (1.f / 12.f)), *f[eq3_bw1], *f[eq3_gain1]);
      band2.coeff_peakEQ(band2.calc_omega(*f[eq3_freq2] * (1.f / 12.f)), *f[eq3_bw2], *f[eq3_gain2]);
      band3.coeff_peakEQ(band3.calc_omega(*f[eq3_freq3] * (1.f / 12.f)), *f[eq3_bw3], *f[eq3_gain3]);
   }
}

void Eq3BandEffect::process(float* dataL, float* dataR)
{
   if (bi == 0)
      setvars(false);
   bi = (bi + 1) & slowrate_m1;

   copy_block(dataL, L, BLOCK_SIZE_QUAD);
   copy_block(dataR, R, BLOCK_SIZE_QUAD);

   if( ! fxdata->p[eq3_gain1].deactivated )
      band1.process_block(L, R);
   if( ! fxdata->p[eq3_gain2].deactivated )
      band2.process_block(L, R);
   if( ! fxdata->p[eq3_gain3].deactivated )
      band3.process_block(L, R);

   gain.set_target_smoothed(db_to_linear(*f[eq3_gain]));
   gain.multiply_2_blocks(L, R, BLOCK_SIZE_QUAD);

   mix.set_target_smoothed(limit_range(*f[eq3_mix], -1.f, 1.f));
   mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void Eq3BandEffect::suspend()
{
   init();
}

const char* Eq3BandEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Band 1";
   case 1:
      return "Band 2";
   case 2:
      return "Band 3";
   case 3:
      return "Output";
   }
   return 0;
}
int Eq3BandEffect::group_label_ypos(int id)
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

void Eq3BandEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[eq3_gain1].set_name("Gain 1");
   fxdata->p[eq3_gain1].set_type(ct_decibel_deactivatable);
   fxdata->p[eq3_gain1].deactivated = false;
   fxdata->p[eq3_freq1].set_name("Frequency 1");
   fxdata->p[eq3_freq1].set_type(ct_freq_audible);
   fxdata->p[eq3_bw1].set_name("Bandwidth 1");
   fxdata->p[eq3_bw1].set_type(ct_bandwidth);

   fxdata->p[eq3_gain2].set_name("Gain 2");
   fxdata->p[eq3_gain2].set_type(ct_decibel_deactivatable);
   fxdata->p[eq3_gain2].deactivated = false;
   fxdata->p[eq3_freq2].set_name("Frequency 2");
   fxdata->p[eq3_freq2].set_type(ct_freq_audible);
   fxdata->p[eq3_bw2].set_name("Bandwidth 2");
   fxdata->p[eq3_bw2].set_type(ct_bandwidth);

   fxdata->p[eq3_gain3].set_name("Gain 3");
   fxdata->p[eq3_gain3].set_type(ct_decibel_deactivatable);
   fxdata->p[eq3_gain3].deactivated = false;
   fxdata->p[eq3_freq3].set_name("Frequency 3");
   fxdata->p[eq3_freq3].set_type(ct_freq_audible);
   fxdata->p[eq3_bw3].set_name("Bandwidth 3");
   fxdata->p[eq3_bw3].set_type(ct_bandwidth);

   fxdata->p[eq3_gain].set_name("Gain");
   fxdata->p[eq3_gain].set_type(ct_decibel);
   fxdata->p[eq3_mix].set_name("Mix");
   fxdata->p[eq3_mix].set_type(ct_percent_bidirectional);
   fxdata->p[eq3_mix].val_default.f = 1.f;

   for (int i = 0; i < eq3_num_ctrls; i++)
      fxdata->p[i].posy_offset = 1 + 2 * (i / 3);
}
void Eq3BandEffect::init_default_values()
{
   fxdata->p[eq3_gain1].val.f = 0.f;
   fxdata->p[eq3_freq1].val.f = -2.5f * 12.f;
   fxdata->p[eq3_bw1].val.f = 2.f;

   fxdata->p[eq3_gain2].val.f = 0.f;
   fxdata->p[eq3_freq2].val.f = 0.5f * 12.f;
   fxdata->p[eq3_bw2].val.f = 2.f;

   fxdata->p[eq3_gain3].val.f = 0.f;
   fxdata->p[eq3_freq3].val.f = 4.5f * 12.f;
   fxdata->p[eq3_bw3].val.f = 2.f;

   fxdata->p[eq3_gain].val.f = 0.f;
   fxdata->p[eq3_mix].val.f = 1.f;
}

void Eq3BandEffect::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
   if (streamingRevision <= 12)
   {
       fxdata->p[eq3_mix].val.f = 1.f;
   }
}


