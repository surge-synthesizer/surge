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

#include "BBDEnsembleEffect.h"

BBDEnsembleEffect::BBDEnsembleEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    width.set_blocksize(BLOCK_SIZE);
    gain.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);
}

BBDEnsembleEffect::~BBDEnsembleEffect() {}

void BBDEnsembleEffect::init()
{
    setvars(true);
    bi = 0;
}

void BBDEnsembleEffect::setvars(bool init)
{
    if (init)
    {
        width.set_target(0.f);
        gain.set_target(0.f);
        mix.set_target(1.f);

        width.instantize();
        gain.instantize();
        mix.instantize();
    }
    else
    {
    }
}

void BBDEnsembleEffect::process(float *dataL, float *dataR)
{
    if (bi == 0)
        setvars(false);
    bi = (bi + 1) & slowrate_m1;

    copy_block(dataL, L, BLOCK_SIZE_QUAD);
    copy_block(dataR, R, BLOCK_SIZE_QUAD);

    width.set_target_smoothed(clamp1bp(*f[ens_width]));

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(L, R, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, L, R, BLOCK_SIZE_QUAD);

    gain.set_target_smoothed(db_to_linear(*f[ens_gain]));
    gain.multiply_2_blocks(L, R, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(limit_range(*f[ens_mix], -1.f, 1.f));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void BBDEnsembleEffect::suspend() { init(); }

const char *BBDEnsembleEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "BBD";
    case 2:
        return "Modulation";
    case 3:
        return "Output";
    }
    return 0;
}

int BBDEnsembleEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 11;
    case 3:
        return 21;
    }
    return 0;
}

void BBDEnsembleEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[ens_input_gain].set_name("Gain");
    fxdata->p[ens_input_gain].set_type(ct_decibel);
    fxdata->p[ens_input_gain].posy_offset = 1;

    fxdata->p[ens_bbd_stages].set_name("Stages");
    fxdata->p[ens_bbd_stages].set_type(ct_decibel);
    fxdata->p[ens_bbd_stages].posy_offset = 3;
    fxdata->p[ens_bbd_aa_cutoff].set_name("AA Cutoff");
    fxdata->p[ens_bbd_aa_cutoff].set_type(ct_freq_audible);
    fxdata->p[ens_bbd_aa_cutoff].posy_offset = 3;

    fxdata->p[ens_lfo_freq1].set_name("Frequency 1");
    fxdata->p[ens_lfo_freq1].set_type(ct_lforate);
    fxdata->p[ens_lfo_freq1].posy_offset = 5;
    fxdata->p[ens_lfo_depth1].set_name("Depth 1");
    fxdata->p[ens_lfo_depth1].set_type(ct_percent);
    fxdata->p[ens_lfo_depth1].posy_offset = 5;
    fxdata->p[ens_lfo_freq2].set_name("Frequency 2");
    fxdata->p[ens_lfo_freq2].set_type(ct_lforate);
    fxdata->p[ens_lfo_freq2].posy_offset = 5;
    fxdata->p[ens_lfo_depth2].set_name("Depth 2");
    fxdata->p[ens_lfo_depth2].set_type(ct_percent);
    fxdata->p[ens_lfo_depth2].posy_offset = 5;

    fxdata->p[ens_width].set_name("Width");
    fxdata->p[ens_width].set_type(ct_percent_bidirectional);
    fxdata->p[ens_width].val_default.f = 1.f;
    fxdata->p[ens_width].posy_offset = 7;
    fxdata->p[ens_gain].set_name("Gain");
    fxdata->p[ens_gain].set_type(ct_decibel);
    fxdata->p[ens_gain].posy_offset = 7;
    fxdata->p[ens_mix].set_name("Mix");
    fxdata->p[ens_mix].set_type(ct_percent);
    fxdata->p[ens_mix].val_default.f = 1.f;
    fxdata->p[ens_mix].posy_offset = 7;
}

void BBDEnsembleEffect::init_default_values()
{
    fxdata->p[ens_input_gain].val.f = 0.f;

    fxdata->p[ens_width].val.f = 1.f;
    fxdata->p[ens_gain].val.f = 0.f;
    fxdata->p[ens_mix].val.f = 1.f;
}