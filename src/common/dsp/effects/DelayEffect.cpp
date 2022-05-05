#include "DelayEffect.h"

using namespace std;

DelayEffect::DelayEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), timeL(0.0001), timeR(0.0001), lp(storage), hp(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
    pan.set_blocksize(BLOCK_SIZE);
    feedback.set_blocksize(BLOCK_SIZE);
    crossfeed.set_blocksize(BLOCK_SIZE);
}

DelayEffect::~DelayEffect() {}

void DelayEffect::init()
{
    memset(buffer[0], 0, (max_delay_length + FIRipol_N) * sizeof(float));
    memset(buffer[1], 0, (max_delay_length + FIRipol_N) * sizeof(float));
    wpos = 0;
    lfophase = 0.0;
    ringout_time = 100000;
    envf = 0.f;
    LFOval = 0.f;
    LFOdirection = true;
    lp.suspend();
    hp.suspend();
    // See issue #1444 and the fix for this stuff
    inithadtempo = (storage->temposyncratio_inv != 0);
    setvars(true);
    inithadtempo = (storage->temposyncratio_inv != 0);
}

void DelayEffect::setvars(bool init)
{
    if (!inithadtempo && storage->temposyncratio_inv != 0)
    {
        init = true;
        inithadtempo = true;
    }

    float fb = amp_to_linear(*f[dly_feedback]);
    float cf = amp_to_linear(*f[dly_crossfeed]);

    feedback.set_target_smoothed(fb);
    crossfeed.set_target_smoothed(cf);

    float lforate = storage->envelope_rate_linear(-*f[dly_mod_rate]) *
                    (fxdata->p[dly_mod_rate].temposync ? storage->temposyncratio : 1.f);
    lfophase += lforate;

    if (lfophase > 0.5)
    {
        lfophase -= 1;
        LFOdirection = !LFOdirection;
    }

    float lfo_increment =
        (0.00000000001f + powf(2, *f[dly_mod_depth] * (1.f / 12.f)) - 1.f) * BLOCK_SIZE;
    // small bias to avoid denormals

    const float ca = 0.99f;
    if (LFOdirection)
        LFOval = ca * LFOval + lfo_increment;
    else
        LFOval = ca * LFOval - lfo_increment;

    auto isLinked = fxdata->p[dly_time_right].deactivated ? dly_time_left : dly_time_right;

    if (init)
    {
        timeL.newValue(
            storage->samplerate *
                ((fxdata->p[dly_time_left].temposync ? storage->temposyncratio_inv : 1.f) *
                 storage->note_to_pitch_ignoring_tuning(12 * fxdata->p[dly_time_left].val.f)) +
            LFOval - FIRoffset);
        timeR.newValue(
            storage->samplerate *
                ((fxdata->p[isLinked].temposync ? storage->temposyncratio_inv : 1.f) *
                 storage->note_to_pitch_ignoring_tuning(12 * fxdata->p[isLinked].val.f)) -
            LFOval - FIRoffset);
    }
    else
    {
        timeL.newValue(
            storage->samplerate *
                ((fxdata->p[dly_time_left].temposync ? storage->temposyncratio_inv : 1.f) *
                 storage->note_to_pitch_ignoring_tuning(12 * *f[dly_time_left])) +
            LFOval - FIRoffset);
        timeR.newValue(storage->samplerate *
                           ((fxdata->p[isLinked].temposync ? storage->temposyncratio_inv : 1.f) *
                            storage->note_to_pitch_ignoring_tuning(12 * *f[isLinked])) -
                       LFOval - FIRoffset);
    }

    const float db96 = powf(10.f, 0.05f * -96.f);
    float maxfb = max(db96, fb + cf);

    if (maxfb < 1.f)
    {
        float f = BLOCK_SIZE_INV * max(timeL.v, timeR.v) * (1.f + log(db96) / log(maxfb));
        ringout_time = (int)f;
    }
    else
    {
        ringout_time = -1;
        ringout = 0;
    }

    mix.set_target_smoothed(*f[dly_mix]);
    width.set_target_smoothed(storage->db_to_linear(*f[dly_width]));
    pan.set_target_smoothed(clamp1bp(*f[dly_input_channel]));

    lp.coeff_LP2B(lp.calc_omega(*f[dly_highcut] / 12.0), 0.707);
    hp.coeff_HP(hp.calc_omega(*f[dly_lowcut] / 12.0), 0.707);

    if (init)
    {
        timeL.instantize();
        timeR.instantize();
        feedback.instantize();
        crossfeed.instantize();
        mix.instantize();
        width.instantize();
        pan.instantize();
        lp.coeff_instantize();
        hp.coeff_instantize();
    }
}

void DelayEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    int k;
    float tbufferL alignas(16)[BLOCK_SIZE], wbL alignas(16)[BLOCK_SIZE]; // wb = write-buffer
    float tbufferR alignas(16)[BLOCK_SIZE], wbR alignas(16)[BLOCK_SIZE];

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        timeL.process();
        timeR.process();

        int i_dtimeL = max(BLOCK_SIZE, min((int)timeL.v, max_delay_length - FIRipol_N - 1));
        int i_dtimeR = max(BLOCK_SIZE, min((int)timeR.v, max_delay_length - FIRipol_N - 1));

        int rpL = ((wpos - i_dtimeL + k) - FIRipol_N) & (max_delay_length - 1);
        int rpR = ((wpos - i_dtimeR + k) - FIRipol_N) & (max_delay_length - 1);

        int sincL = FIRipol_N * limit_range((int)(FIRipol_M * (float(i_dtimeL + 1) - timeL.v)), 0,
                                            FIRipol_M - 1);
        int sincR = FIRipol_N * limit_range((int)(FIRipol_M * (float(i_dtimeR + 1) - timeR.v)), 0,
                                            FIRipol_M - 1);

        __m128 L, R;
        L = _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sincL]), _mm_loadu_ps(&buffer[0][rpL]));
        L = _mm_add_ps(L, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sincL + 4]),
                                     _mm_loadu_ps(&buffer[0][rpL + 4])));
        L = _mm_add_ps(L, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sincL + 8]),
                                     _mm_loadu_ps(&buffer[0][rpL + 8])));
        L = sum_ps_to_ss(L);
        R = _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sincR]), _mm_loadu_ps(&buffer[1][rpR]));
        R = _mm_add_ps(R, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sincR + 4]),
                                     _mm_loadu_ps(&buffer[1][rpR + 4])));
        R = _mm_add_ps(R, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sincR + 8]),
                                     _mm_loadu_ps(&buffer[1][rpR + 8])));
        R = sum_ps_to_ss(R);
        _mm_store_ss(&tbufferL[k], L);
        _mm_store_ss(&tbufferR[k], R);
    }

    switch (fxdata->p[dly_feedback].deform_type)
    {
    case dly_clipping_soft:
        softclip_block(tbufferL, BLOCK_SIZE_QUAD);
        softclip_block(tbufferR, BLOCK_SIZE_QUAD);
        break;
    case dly_clipping_tanh:
        tanh7_block(tbufferL, BLOCK_SIZE_QUAD);
        tanh7_block(tbufferR, BLOCK_SIZE_QUAD);
        break;
    case dly_clipping_hard:
        hardclip_block(tbufferL, BLOCK_SIZE_QUAD);
        hardclip_block(tbufferR, BLOCK_SIZE_QUAD);
        break;
    case dly_clipping_hard18:
        hardclip_block8(tbufferL, BLOCK_SIZE_QUAD);
        hardclip_block8(tbufferR, BLOCK_SIZE_QUAD);
        break;
    case dly_clipping_off:
    default:
        break;
    }

    if (!fxdata->p[dly_highcut].deactivated)
    {
        lp.process_block(tbufferL, tbufferR);
    }
    if (!fxdata->p[dly_lowcut].deactivated)
    {
        hp.process_block(tbufferL, tbufferR);
    }

    pan.trixpan_blocks(dataL, dataR, wbL, wbR, BLOCK_SIZE_QUAD);

    feedback.MAC_2_blocks_to(tbufferL, tbufferR, wbL, wbR, BLOCK_SIZE_QUAD);
    crossfeed.MAC_2_blocks_to(tbufferL, tbufferR, wbR, wbL, BLOCK_SIZE_QUAD);

    if (wpos + BLOCK_SIZE >= max_delay_length)
    {
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            buffer[0][(wpos + k) & (max_delay_length - 1)] = wbL[k];
            buffer[1][(wpos + k) & (max_delay_length - 1)] = wbR[k];
        }
    }
    else
    {
        copy_block(wbL, &buffer[0][wpos], BLOCK_SIZE_QUAD);
        copy_block(wbR, &buffer[1][wpos], BLOCK_SIZE_QUAD);
    }

    if (wpos == 0)
    {
        for (k = 0; k < FIRipol_N; k++)
        {
            buffer[0][k + max_delay_length] =
                buffer[0][k]; // copy buffer so FIR-core doesn't have to wrap
            buffer[1][k + max_delay_length] = buffer[1][k];
        }
    }

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(tbufferL, tbufferR, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, tbufferL, tbufferR, BLOCK_SIZE_QUAD);

    mix.fade_2_blocks_to(dataL, tbufferL, dataR, tbufferR, dataL, dataR, BLOCK_SIZE_QUAD);

    wpos += BLOCK_SIZE;
    wpos = wpos & (max_delay_length - 1);
}

void DelayEffect::suspend() { init(); }

const char *DelayEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Delay Time";
    case 2:
        return "Feedback/EQ";
    case 3:
        return "Modulation";
    case 4:
        return "Output";
    }
    return 0;
}
int DelayEffect::group_label_ypos(int id)
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
    case 4:
        return 27;
    }
    return 0;
}

void DelayEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[dly_time_left].set_name("Left");
    fxdata->p[dly_time_left].set_type(ct_envtime);
    fxdata->p[dly_time_right].set_name("Right");
    fxdata->p[dly_time_right].set_type(ct_envtime_linkable_delay);
    fxdata->p[dly_feedback].set_name("Feedback");
    fxdata->p[dly_feedback].set_type(ct_dly_fb_clippingmodes);
    fxdata->p[dly_feedback].deform_type = 1;
    fxdata->p[dly_crossfeed].set_name("Crossfeed");
    fxdata->p[dly_crossfeed].set_type(ct_percent);
    fxdata->p[dly_lowcut].set_name("Low Cut");
    fxdata->p[dly_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[dly_highcut].set_name("High Cut");
    fxdata->p[dly_highcut].set_type(ct_freq_audible_deactivatable_lp);
    fxdata->p[dly_mod_rate].set_name("Rate");
    fxdata->p[dly_mod_rate].set_type(ct_lforate);
    fxdata->p[dly_mod_depth].set_name("Depth");
    fxdata->p[dly_mod_depth].set_type(ct_detuning);
    fxdata->p[dly_input_channel].set_name("Channel");
    fxdata->p[dly_input_channel].set_type(ct_percent_bipolar_stereo);

    fxdata->p[dly_mix].set_name("Mix");
    fxdata->p[dly_mix].set_type(ct_percent);
    fxdata->p[dly_width].set_name("Width");
    fxdata->p[dly_width].set_type(ct_decibel_narrow);

    fxdata->p[dly_time_left].posy_offset = 5;
    fxdata->p[dly_time_right].posy_offset = 5;

    fxdata->p[dly_feedback].posy_offset = 7;
    fxdata->p[dly_crossfeed].posy_offset = 7;
    fxdata->p[dly_lowcut].posy_offset = 7;
    fxdata->p[dly_highcut].posy_offset = 7;

    fxdata->p[dly_mod_rate].posy_offset = 9;
    fxdata->p[dly_mod_depth].posy_offset = 9;
    fxdata->p[dly_input_channel].posy_offset = -15;

    fxdata->p[dly_mix].posy_offset = 9;
    fxdata->p[dly_width].posy_offset = 5;
}

void DelayEffect::init_default_values()
{
    fxdata->p[dly_time_left].val.f = -2.f;
    fxdata->p[dly_time_right].val.f = -2.f;
    fxdata->p[dly_time_right].deactivated = false;
    fxdata->p[dly_feedback].val.f = 0.0f;
    fxdata->p[dly_feedback].deform_type = 1;
    fxdata->p[dly_crossfeed].val.f = 0.0f;

    fxdata->p[dly_lowcut].val.f = -24.f;
    fxdata->p[dly_lowcut].deactivated = false;

    fxdata->p[dly_highcut].val.f = 30.f;
    fxdata->p[dly_highcut].deactivated = false;

    fxdata->p[dly_mod_rate].val.f = -2.f;
    fxdata->p[dly_mod_depth].val.f = 0.f;
    fxdata->p[dly_input_channel].val.f = 0.f;
    fxdata->p[dly_mix].val.f = 1.f;
    fxdata->p[dly_width].val.f = 0.f;
}

void DelayEffect::handleStreamingMismatches(int streamingRevision,
                                            int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[dly_lowcut].deactivated = false;
        fxdata->p[dly_highcut].deactivated = false;
        fxdata->p[dly_time_right].deactivated = false;
    }

    if (streamingRevision <= 17)
    {
        fxdata->p[dly_feedback].deform_type = 1;
    }
}
