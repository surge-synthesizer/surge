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

#include "GEQ11Effect.h"

GEQ11Effect::GEQ11Effect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   bands.fill(storage);

   for (auto& band : bands)
   {
      band.setBlockSize(BLOCK_SIZE * slowrate); // does not matter ATM as they're smoothed
   }

   gain.set_blocksize(BLOCK_SIZE);
}

GEQ11Effect::~GEQ11Effect()
{}

void GEQ11Effect::init()
{
   setvars(true);

   for (auto &band : bands)
   {
      band.suspend();
   }

   bi = 0;
}

void GEQ11Effect::setvars(bool init)
{
   if (init)
   {
      // Set the bands to 0dB so the EQ fades in init
      int i = 0;
      for (auto& band : bands)
      {
         band.coeff_peakEQ(band.calc_omega_from_Hz(freqs[i]), 0.5, 1.f);
         band.coeff_instantize();
         i++;
      }

      gain.set_target(1.f);
      gain.instantize();
   }
   else
   {
      int i = 0;
      for (auto& band : bands)
      {
         band.coeff_peakEQ(band.calc_omega_from_Hz(freqs[i]), 0.5, 1.f);
         i++;
      }
   }
}

void GEQ11Effect::process(float* dataL, float* dataR)
{
   if (bi == 0)
      setvars(false);
   bi = (bi + 1) & slowrate_m1;

   int i = 0;
   for (auto& band : bands)
   {
      if (! fxdata->p[i].deactivated)
      {
         band.process_block(dataL, dataR);
      }
      i++;
   }

   gain.set_target_smoothed(db_to_linear(*f[geq11_gain]));
   gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
}

void GEQ11Effect::suspend()
{
   init();
}

const char* GEQ11Effect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Bands";
   case 1:
      return "Output";
   }
   return 0;
}
int GEQ11Effect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 25;
   }
   return 0;
}

void GEQ11Effect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[geq11_gain].set_name("Gain");
   fxdata->p[geq11_gain].set_type(ct_decibel);
   fxdata->p[geq11_gain].posy_offset = 3;

   for (int i = 0; i < geq11_gain; i++)
   {
      fxdata->p[i].deactivated = false;
      fxdata->p[i].set_name(band_names[i].c_str());
      fxdata->p[i].set_type(ct_decibel_narrow_deactivatable);
      fxdata->p[i].posy_offset = 1;
   }
}

void GEQ11Effect::init_default_values()
{
   for (int i = 0; i < geq11_num_ctrls; i++)
   {
      fxdata->p[i].val.f = 0.f;
   }
}
