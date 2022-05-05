/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "TreemonsterEffect.h"
#include "DebugHelpers.h"

TreemonsterEffect::TreemonsterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), lp(storage), hp(storage)
{
    rm.set_blocksize(BLOCK_SIZE);
    width.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);
}

TreemonsterEffect::~TreemonsterEffect() {}

void TreemonsterEffect::init()
{
    setvars(true);
    bi = 0;
}

void TreemonsterEffect::setvars(bool init)
{
    if (init)
    {
        lp.suspend();
        hp.suspend();

        hp.coeff_HP(hp.calc_omega(*f[tm_hp] / 12.0), 0.707);
        hp.coeff_instantize();

        lp.coeff_LP2B(lp.calc_omega(*f[tm_lp] / 12.0), 0.707);
        lp.coeff_instantize();

        oscL.set_rate(0.f);
        oscR.set_rate(0.f);

        rm.set_target(1.f);
        width.set_target(0.f);
        mix.set_target(1.f);

        rm.instantize();
        width.instantize();
        mix.instantize();

        // envelope follower times: 5 ms attack, 500 ms release
        envA = pow(0.01, 1.0 / (5 * storage->dsamplerate * 0.001));
        envR = pow(0.01, 1.0 / (500 * storage->dsamplerate * 0.001));
        envV[0] = 0.f;
        envV[1] = 0.f;

        length[0] = 100;
        length[1] = 100;
        length_target[0] = 100;
        length_target[1] = 100;
        length_smooth[0] = 100;
        length_smooth[1] = 100;
        first_thresh[0] = true;
        first_thresh[1] = true;
        oscL.set_phase(0);
        oscR.set_phase(M_PI / 2.0);
    }
}

void TreemonsterEffect::process(float *dataL, float *dataR)
{
    float tbuf alignas(16)[2][BLOCK_SIZE];
    float envscaledSineWave alignas(16)[2][BLOCK_SIZE];

    auto thres = storage->db_to_linear(limit_range(
        *f[tm_threshold], fxdata->p[tm_threshold].val_min.f, fxdata->p[tm_threshold].val_max.f));

    // copy dry signal (dataL, dataR) to wet signal (L, R)
    copy_block(dataL, L, BLOCK_SIZE_QUAD);
    copy_block(dataR, R, BLOCK_SIZE_QUAD);

    // copy it to pitch detection buffer (tbuf) as well
    // in case filters are not activated
    copy_block(dataL, tbuf[0], BLOCK_SIZE_QUAD);
    copy_block(dataR, tbuf[1], BLOCK_SIZE_QUAD);

    // apply filters to the pitch detection buffer
    if (!fxdata->p[tm_hp].deactivated)
    {
        hp.coeff_HP(hp.calc_omega(*f[tm_hp] / 12.0), 0.707);
        hp.process_block(tbuf[0], tbuf[1]);
    }

    if (!fxdata->p[tm_lp].deactivated)
    {
        lp.coeff_LP2B(lp.calc_omega(*f[tm_lp] / 12.0), 0.707);
        lp.process_block(tbuf[0], tbuf[1]);
    }

    /*
     * We assume wavelengths below this are just noisy detection errors. This is used to
     * clamp when we have a pitch detect basically.
     */
    constexpr float smallest_wavelength = 16.0;

    float qs = clamp01(*f[tm_speed]);
    qs *= qs * qs * qs;
    float speed = 0.9999 - qs * 0.0999 / 128;
    float numberOfSteps = 32 * 48000 * storage->samplerate_inv;
    for (int i = 0; i < numberOfSteps; ++i)
    {
        length_smooth[0] = speed * length_smooth[0] + (1 - speed) * length_target[0];
        length_smooth[1] = speed * length_smooth[1] + (1 - speed) * length_target[1];
    }

    oscL.set_rate((2.0 * M_PI / std::max(2.f, length_smooth[0])) *
                  powf(2.0, *f[tm_pitch] * (1 / 12.f)));
    oscR.set_rate((2.0 * M_PI / std::max(2.f, length_smooth[1])) *
                  powf(2.0, *f[tm_pitch] * (1 / 12.f)));

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        // envelope detection
        for (int c = 0; c < 2; ++c)
        {
            auto v = (c == 0 ? dataL[k] : dataR[k]);
            auto e = envV[c];

            if (v > e)
            {
                e = envA * (e - v) + v;
            }
            else
            {
                e = envR * (e - v) + v;
            }

            envV[c] = e;
        }

        // pitch detection
        if ((lastval[0] < 0.f) && (tbuf[0][k] >= 0.f))
        {
            if (tbuf[0][k] > thres && length[0] > smallest_wavelength)
            {
                length_target[0] =
                    (length[0] > length_smooth[0] * 10 ? length_smooth[0] : length[0]);
                if (first_thresh[0])
                    length_smooth[0] = length[0];
                first_thresh[0] = false;
            }

            length[0] = 0.0; // (0.0-lastval[0]) / ( tbuf[0][k] - lastval[0]);
        }

        if ((lastval[1] < 0.f) && (tbuf[1][k] >= 0.f))
        {
            if (tbuf[1][k] > thres && length[1] > smallest_wavelength)
            {
                length_target[1] =
                    (length[1] > length_smooth[1] * 10 ? length_smooth[1] : length[1]);
                if (first_thresh[1])
                    length_smooth[1] = length[1];
                first_thresh[1] = false;
            }

            length[1] = 0.0; // (0.0-lastval[1]) / ( tbuf[1][k] - lastval[1]);
        }

        oscL.process();
        oscR.process();

        // do not apply followed envelope to sine oscillator - we need full freight sine for RM
        L[k] = oscL.r;
        R[k] = oscR.r;

        // but we need to store the scaled for mix
        envscaledSineWave[0][k] = oscL.r * envV[0];
        envscaledSineWave[1][k] = oscR.r * envV[0];

        // track positive zero crossings
        length[0] += 1.0f;
        length[1] += 1.0f;

        lastval[0] = tbuf[0][k];
        lastval[1] = tbuf[1][k];
    }

    // do dry signal * pitch tracked signal ringmod
    // store to pitch detection buffer
    mul_block(L, dataL, tbuf[0], BLOCK_SIZE_QUAD);
    mul_block(R, dataR, tbuf[1], BLOCK_SIZE_QUAD);

    // mix pure pitch tracked sine with ring modulated signal
    rm.set_target_smoothed(clamp01(*f[tm_ring_mix]));
    rm.fade_2_blocks_to(envscaledSineWave[0], tbuf[0], envscaledSineWave[1], tbuf[1], L, R,
                        BLOCK_SIZE_QUAD);

    // scale width
    width.set_target_smoothed(clamp1bp(*f[tm_width]));
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(L, R, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, L, R, BLOCK_SIZE_QUAD);

    // main dry-wet mix
    mix.set_target_smoothed(clamp01(*f[tm_mix]));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void TreemonsterEffect::suspend() { init(); }

const char *TreemonsterEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pitch Detection";
    case 1:
        return "Oscillator";
    case 2:
        return "Output";
    }
    return 0;
}
int TreemonsterEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 17;
    }
    return 0;
}

void TreemonsterEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[tm_threshold].set_name("Threshold");
    fxdata->p[tm_threshold].set_type(ct_decibel_attenuation_large);
    fxdata->p[tm_threshold].val_default.f = -24.f;
    fxdata->p[tm_threshold].posy_offset = 1;
    fxdata->p[tm_speed].set_name("Speed");
    fxdata->p[tm_speed].set_type(ct_percent);
    fxdata->p[tm_speed].val_default.f = 0.5f;
    fxdata->p[tm_speed].posy_offset = 1;
    fxdata->p[tm_hp].set_name("Low Cut");
    fxdata->p[tm_hp].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[tm_hp].posy_offset = 1;
    fxdata->p[tm_lp].set_name("High Cut");
    fxdata->p[tm_lp].set_type(ct_freq_audible_deactivatable_lp);
    fxdata->p[tm_lp].posy_offset = 1;

    fxdata->p[tm_pitch].set_name("Pitch");
    fxdata->p[tm_pitch].set_type(ct_pitch);
    fxdata->p[tm_pitch].posy_offset = 3;
    fxdata->p[tm_ring_mix].set_name("Ring Modulation");
    fxdata->p[tm_ring_mix].set_type(ct_percent);
    fxdata->p[tm_ring_mix].val_default.f = 0.5f;
    fxdata->p[tm_ring_mix].posy_offset = 3;

    fxdata->p[tm_width].set_name("Width");
    fxdata->p[tm_width].set_type(ct_percent_bipolar);
    fxdata->p[tm_width].posy_offset = 5;
    fxdata->p[tm_mix].set_name("Mix");
    fxdata->p[tm_mix].set_type(ct_percent);
    fxdata->p[tm_mix].posy_offset = 5;
    fxdata->p[tm_mix].val_default.f = 1.f;
}

void TreemonsterEffect::init_default_values()
{
    fxdata->p[tm_threshold].val.f = -24.f;
    fxdata->p[tm_speed].val.f = 0.5f;

    fxdata->p[tm_hp].val.f = fxdata->p[tm_hp].val_min.f;
    fxdata->p[tm_hp].deactivated = false;

    fxdata->p[tm_lp].val.f = fxdata->p[tm_lp].val_max.f;
    fxdata->p[tm_lp].deactivated = false;

    fxdata->p[tm_pitch].val.f = 0;
    fxdata->p[tm_ring_mix].val.f = 0.5f;

    fxdata->p[tm_width].val.f = 1.f;
    fxdata->p[tm_mix].val.f = 1.f;
}
