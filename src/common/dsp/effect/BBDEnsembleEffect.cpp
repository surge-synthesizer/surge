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
#include "DebugHelpers.h"

enum EnsembleStages
{
    ens_sinc = 0,
    ens_128,
    ens_256,
    ens_512,
    ens_1024,
    ens_2048
};

std::string ensemble_stage_name(int i)
{
    switch ((EnsembleStages)i)
    {
    case ens_sinc:
        return "Sinc (clean) Delay";
    case ens_128:
        return "128 Stage BBD";
    case ens_256:
        return "256 Stage BBD";
    case ens_512:
        return "512 Stage BBD";
    case ens_1024:
        return "1024 State BBD";
    case ens_2048:
        return "2048 Stage BBD";
    }
    return "Error";
}

int ensemble_stage_count() { return 6; }

namespace
{
constexpr float delay1Ms = 0.6f;
constexpr float delay2Ms = 0.2f;
constexpr float delay0Ms = 5.0f;
} // namespace

BBDEnsembleEffect::BBDEnsembleEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    input.set_blocksize(BLOCK_SIZE);
    width.set_blocksize(BLOCK_SIZE);
    gain.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);
}

BBDEnsembleEffect::~BBDEnsembleEffect() {}

void BBDEnsembleEffect::init()
{
    setvars(true);

    auto init_bbds = [=](auto &delL1, auto &delL2, auto &delR1, auto &delR2) {
        for (auto *del : {&delL1, &delL2, &delR1, &delR2})
        {
            del->prepare(samplerate);
            del->setWaveshapeParams(0.5f);
            del->setFilterFreq(10000.0f);
            del->setDelayTime(delay0Ms * 0.001f);
        }
    };

    init_bbds(del_128L1, del_128L2, del_128R1, del_128R2);
    init_bbds(del_256L1, del_256L2, del_256R1, del_256R2);
    init_bbds(del_512L1, del_512L2, del_512R1, del_512R2);
    init_bbds(del_1024L1, del_1024L2, del_1024R1, del_1024R2);
    init_bbds(del_2048L1, del_2048L2, del_2048R1, del_2048R2);
}

void BBDEnsembleEffect::setvars(bool init)
{
    if (init)
    {
        input.set_target(0.f);
        width.set_target(0.f);
        gain.set_target(0.f);
        mix.set_target(1.f);

        input.instantize();
        width.instantize();
        gain.instantize();
        mix.instantize();
    }
    else
    {
    }
}

void BBDEnsembleEffect::process_sinc_delays(float *dataL, float *dataR)
{
    float del1 = delay1Ms * 0.001 * samplerate;
    float del2 = delay2Ms * 0.001 * samplerate;
    float del0 = delay0Ms * 0.001 * samplerate;

    for (int s = 0; s < BLOCK_SIZE; ++s)
    {
        // soft-clip input
        dataL[s] = lookup_waveshape(wst_soft, dataL[s]);
        dataR[s] = lookup_waveshape(wst_soft, dataR[s]);

        delL.write(dataL[s]);
        delR.write(dataR[s]);

        // OK so look at the diagram in #3743
        float t1 = del1 * modlfos[0][0].value() + del2 * modlfos[1][0].value() + del0;
        float t2 = del1 * modlfos[0][1].value() + del2 * modlfos[1][1].value() + del0;
        float t3 = del1 * modlfos[0][2].value() + del2 * modlfos[1][2].value() + del0;

        float ltap1 = t1;
        float ltap2 = t2;
        float rtap1 = t2;
        float rtap2 = t3;

        L[s] = delL.read(ltap1) + delL.read(ltap2);
        R[s] = delR.read(rtap1) + delR.read(rtap2);

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                modlfos[j][i].post_process();
            }
        }
    }
}

void BBDEnsembleEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    // limit between 0.005 and 40 Hz with modulation
    double rate1 = envelope_rate_linear(-limit_range(*f[ens_lfo_freq1], -7.64386f, 5.322f));
    double rate2 = envelope_rate_linear(-limit_range(*f[ens_lfo_freq2], -7.64386f, 5.322f));
    float roff = 0;
    constexpr float onethird = 1.0 / 3.0;

    for (int i = 0; i < 3; ++i)
    {
        modlfos[0][i].pre_process(Surge::ModControl::mod_sine, rate1, *f[ens_lfo_depth1], roff);
        modlfos[1][i].pre_process(Surge::ModControl::mod_sine, rate2, *f[ens_lfo_depth2], roff);

        roff += onethird;
    }

    // input gain
    input.set_target_smoothed(db_to_linear(limit_range(*f[ens_input_gain], -48.f, 48.f)));
    input.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

    auto process_bbd_delays = [=](float *dataL, float *dataR, auto &delL1, auto &delL2, auto &delR1,
                                  auto &delR2) {
        const auto aa_cutoff = 2 * 3.14159265358979323846 * 440 *
                               storage->note_to_pitch_ignoring_tuning(*f[ens_bbd_aa_cutoff]);
        for (auto *del : {&delL1, &delL2, &delR1, &delR2})
        {
            del->setFilterFreq(aa_cutoff);
            del->setWaveshapeParams(*f[ens_bbd_nonlin]);
        }

        float del1 = delay1Ms * 0.001;
        float del2 = delay2Ms * 0.001;
        float del0 = delay0Ms * 0.001;

        for (int s = 0; s < BLOCK_SIZE; ++s)
        {
            // soft-clip input
            dataL[s] = lookup_waveshape(wst_soft, dataL[s]);
            dataR[s] = lookup_waveshape(wst_soft, dataR[s]);

            // OK so look at the diagram in #3743
            float t1 = del1 * modlfos[0][0].value() + del2 * modlfos[1][0].value() + del0;
            float t2 = del1 * modlfos[0][1].value() + del2 * modlfos[1][1].value() + del0;
            float t3 = del1 * modlfos[0][2].value() + del2 * modlfos[1][2].value() + del0;

            delL1.setDelayTime(t1);
            delL2.setDelayTime(t2);
            delR1.setDelayTime(t2);
            delR2.setDelayTime(t3);

            L[s] = delL1.process(dataL[s]) + delL2.process(dataL[s]);
            R[s] = delR1.process(dataR[s]) + delR2.process(dataR[s]);

            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 2; ++j)
                {
                    modlfos[j][i].post_process();
                }
            }
        }

        mul_block(L, db_to_linear(-8.0f), L, BLOCK_SIZE_QUAD);
        mul_block(R, db_to_linear(-8.0f), R, BLOCK_SIZE_QUAD);
    };

    auto bbd_stages = (EnsembleStages)*pdata_ival[ens_bbd_stages];
    switch (bbd_stages)
    {
    case ens_sinc:
        process_sinc_delays(dataL, dataR);
        break;
    case ens_128:
        process_bbd_delays(dataL, dataR, del_128L1, del_128L2, del_128R1, del_128R2);
        break;
    case ens_256:
        process_bbd_delays(dataL, dataR, del_256L1, del_256L2, del_256R1, del_256R2);
        break;
    case ens_512:
        process_bbd_delays(dataL, dataR, del_512L1, del_512L2, del_512R1, del_512R2);
        break;
    case ens_1024:
        process_bbd_delays(dataL, dataR, del_1024L1, del_1024L2, del_1024R1, del_1024R2);
        break;
    case ens_2048:
        process_bbd_delays(dataL, dataR, del_2048L1, del_2048L2, del_2048R1, del_2048R2);
        break;
    }

    // scale width
    width.set_target_smoothed(clamp1bp(*f[ens_width]));

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
        return 13;
    case 3:
        return 23;
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
    fxdata->p[ens_bbd_stages].set_type(ct_ensemble_stages);
    fxdata->p[ens_bbd_stages].posy_offset = 3;
    fxdata->p[ens_bbd_aa_cutoff].set_name("AA Cutoff");
    fxdata->p[ens_bbd_aa_cutoff].set_type(ct_freq_audible);
    fxdata->p[ens_bbd_aa_cutoff].val_default.f = 3.65f * 12.f;
    fxdata->p[ens_bbd_aa_cutoff].posy_offset = 3;
    fxdata->p[ens_bbd_nonlin].set_name("Nonlinearity");
    fxdata->p[ens_bbd_nonlin].set_type(ct_percent);
    fxdata->p[ens_bbd_nonlin].val_default.f = 0.5f;
    fxdata->p[ens_bbd_nonlin].posy_offset = 3;

    fxdata->p[ens_lfo_freq1].set_name("Frequency 1");
    fxdata->p[ens_lfo_freq1].set_type(ct_bbd_lforate);
    fxdata->p[ens_lfo_freq1].posy_offset = 5;
    fxdata->p[ens_lfo_depth1].set_name("Depth 1");
    fxdata->p[ens_lfo_depth1].set_type(ct_percent);
    fxdata->p[ens_lfo_depth1].posy_offset = 5;
    fxdata->p[ens_lfo_freq2].set_name("Frequency 2");
    fxdata->p[ens_lfo_freq2].set_type(ct_bbd_lforate);
    fxdata->p[ens_lfo_freq2].posy_offset = 5;
    fxdata->p[ens_lfo_depth2].set_name("Depth 2");
    fxdata->p[ens_lfo_depth2].set_type(ct_percent);
    fxdata->p[ens_lfo_depth2].posy_offset = 5;

    fxdata->p[ens_width].set_name("Width");
    fxdata->p[ens_width].set_type(ct_percent_bipolar);
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

    fxdata->p[ens_bbd_stages].val.i = 2;
    fxdata->p[ens_bbd_aa_cutoff].val.f = 3.65f * 12.f;
    fxdata->p[ens_bbd_nonlin].val.f = 0.5f;

    fxdata->p[ens_width].val.f = 1.f;
    fxdata->p[ens_gain].val.f = 0.f;
    fxdata->p[ens_mix].val.f = 1.f;

    // we want LFO 1 at .18 Hz and LFO 2 at 5.52 Hz
    // these go as 2^x so...
    fxdata->p[ens_lfo_freq1].val.f = log2(0.18);
    fxdata->p[ens_lfo_freq2].val.f = log2(5.52);

    fxdata->p[ens_lfo_depth1].val.f = 1.f;
    fxdata->p[ens_lfo_depth2].val.f = 0.6f;
}
