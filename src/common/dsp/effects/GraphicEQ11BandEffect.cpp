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

#include "GraphicEQ11BandEffect.h"

GraphicEQ11BandEffect::GraphicEQ11BandEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), band1(storage), band2(storage), band3(storage), band4(storage),
      band5(storage), band6(storage), band7(storage), band8(storage), band9(storage),
      band10(storage), band11(storage)
{
    band1.setBlockSize(BLOCK_SIZE * slowrate); // does not matter ATM as they're smoothed
    band2.setBlockSize(BLOCK_SIZE * slowrate);
    band3.setBlockSize(BLOCK_SIZE * slowrate);
    band4.setBlockSize(BLOCK_SIZE * slowrate);
    band5.setBlockSize(BLOCK_SIZE * slowrate);
    band6.setBlockSize(BLOCK_SIZE * slowrate);
    band7.setBlockSize(BLOCK_SIZE * slowrate);
    band8.setBlockSize(BLOCK_SIZE * slowrate);
    band9.setBlockSize(BLOCK_SIZE * slowrate);
    band10.setBlockSize(BLOCK_SIZE * slowrate);
    band11.setBlockSize(BLOCK_SIZE * slowrate);

    gain.set_blocksize(BLOCK_SIZE);
}

GraphicEQ11BandEffect::~GraphicEQ11BandEffect() {}

void GraphicEQ11BandEffect::init()
{
    setvars(true);
    band1.suspend();
    band2.suspend();
    band3.suspend();
    band4.suspend();
    band5.suspend();
    band6.suspend();
    band7.suspend();
    band8.suspend();
    band9.suspend();
    band10.suspend();
    band11.suspend();
    bi = 0;
}

void GraphicEQ11BandEffect::setvars(bool init)
{
    if (init)
    {
        // Set the bands to 0dB so the EQ fades in init
        band1.coeff_peakEQ(band1.calc_omega_from_Hz(30.f), 0.5, 1.f);
        band2.coeff_peakEQ(band2.calc_omega_from_Hz(60.f), 0.5, 1.f);
        band3.coeff_peakEQ(band3.calc_omega_from_Hz(120.f), 0.5, 1.f);
        band4.coeff_peakEQ(band4.calc_omega_from_Hz(250.f), 0.5, 1.f);
        band5.coeff_peakEQ(band5.calc_omega_from_Hz(500.f), 0.5, 1.f);
        band6.coeff_peakEQ(band6.calc_omega_from_Hz(1000.f), 0.5, 1.f);
        band7.coeff_peakEQ(band7.calc_omega_from_Hz(2000.f), 0.5, 1.f);
        band8.coeff_peakEQ(band8.calc_omega_from_Hz(4000.f), 0.5, 1.f);
        band9.coeff_peakEQ(band9.calc_omega_from_Hz(8000.f), 0.5, 1.f);
        band10.coeff_peakEQ(band10.calc_omega_from_Hz(12000.f), 0.5, 1.f);
        band11.coeff_peakEQ(band11.calc_omega_from_Hz(16000.f), 0.5, 1.f);

        band1.coeff_instantize();
        band2.coeff_instantize();
        band3.coeff_instantize();
        band4.coeff_instantize();
        band5.coeff_instantize();
        band6.coeff_instantize();
        band7.coeff_instantize();
        band8.coeff_instantize();
        band9.coeff_instantize();
        band10.coeff_instantize();
        band11.coeff_instantize();

        gain.set_target(1.f);

        gain.instantize();
    }
    else
    {
        band1.coeff_peakEQ(band1.calc_omega_from_Hz(30.f), 0.5, *f[geq11_30]);
        band2.coeff_peakEQ(band2.calc_omega_from_Hz(60.f), 0.5, *f[geq11_60]);
        band3.coeff_peakEQ(band3.calc_omega_from_Hz(120.f), 0.5, *f[geq11_120]);
        band4.coeff_peakEQ(band4.calc_omega_from_Hz(250.f), 0.5, *f[geq11_250]);
        band5.coeff_peakEQ(band5.calc_omega_from_Hz(500.f), 0.5, *f[geq11_500]);
        band6.coeff_peakEQ(band6.calc_omega_from_Hz(1000.f), 0.5, *f[geq11_1k]);
        band7.coeff_peakEQ(band7.calc_omega_from_Hz(2000.f), 0.5, *f[geq11_2k]);
        band8.coeff_peakEQ(band8.calc_omega_from_Hz(4000.f), 0.5, *f[geq11_4k]);
        band9.coeff_peakEQ(band9.calc_omega_from_Hz(8000.f), 0.5, *f[geq11_8k]);
        band10.coeff_peakEQ(band10.calc_omega_from_Hz(12000.f), 0.5, *f[geq11_12k]);
        band11.coeff_peakEQ(band11.calc_omega_from_Hz(16000.f), 0.5, *f[geq11_16k]);
    }
}

void GraphicEQ11BandEffect::process(float *dataL, float *dataR)
{
    if (bi == 0)
        setvars(false);
    bi = (bi + 1) & slowrate_m1;

    if (!fxdata->p[geq11_30].deactivated)
        band1.process_block(dataL, dataR);
    if (!fxdata->p[geq11_60].deactivated)
        band2.process_block(dataL, dataR);
    if (!fxdata->p[geq11_120].deactivated)
        band3.process_block(dataL, dataR);
    if (!fxdata->p[geq11_250].deactivated)
        band4.process_block(dataL, dataR);
    if (!fxdata->p[geq11_500].deactivated)
        band5.process_block(dataL, dataR);
    if (!fxdata->p[geq11_1k].deactivated)
        band6.process_block(dataL, dataR);
    if (!fxdata->p[geq11_2k].deactivated)
        band7.process_block(dataL, dataR);
    if (!fxdata->p[geq11_4k].deactivated)
        band8.process_block(dataL, dataR);
    if (!fxdata->p[geq11_8k].deactivated)
        band9.process_block(dataL, dataR);
    if (!fxdata->p[geq11_12k].deactivated)
        band10.process_block(dataL, dataR);
    if (!fxdata->p[geq11_16k].deactivated)
        band11.process_block(dataL, dataR);

    gain.set_target_smoothed(storage->db_to_linear(*f[geq11_gain]));
    gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
}

void GraphicEQ11BandEffect::suspend() { init(); }

const char *GraphicEQ11BandEffect::group_label(int id)
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
int GraphicEQ11BandEffect::group_label_ypos(int id)
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

void GraphicEQ11BandEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[geq11_gain].set_name("Gain");
    fxdata->p[geq11_gain].set_type(ct_decibel);
    fxdata->p[geq11_gain].posy_offset = 3;

    for (int i = 0; i < geq11_gain; i++)
    {
        fxdata->p[i].set_name(band_names[i].c_str());
        fxdata->p[i].set_type(ct_decibel_narrow_deactivatable);
        fxdata->p[i].posy_offset = 1;
    }
}

void GraphicEQ11BandEffect::init_default_values()
{
    for (int i = 0; i < geq11_gain; i++)
    {
        fxdata->p[i].deactivated = false;
    }

    for (int i = 0; i < geq11_num_ctrls; i++)
    {
        fxdata->p[i].val.f = 0.f;
    }
}
