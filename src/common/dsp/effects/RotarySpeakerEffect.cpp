#include "RotarySpeakerEffect.h"

using namespace std;

RotarySpeakerEffect::RotarySpeakerEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), xover(storage), lowbass(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
    width.set_blocksize(BLOCK_SIZE);
}

RotarySpeakerEffect::~RotarySpeakerEffect() {}

void RotarySpeakerEffect::init()
{
    memset(buffer, 0, max_delay_length * sizeof(float));

    wpos = 0;

    xover.suspend();
    lowbass.suspend();

    xover.coeff_LP2B(xover.calc_omega(0.862496f), 0.707); // 800 Hz
    lowbass.coeff_LP2B(xover.calc_omega(-1.14f), 0.707);  // 200 Hz

    setvars(true);
}

void RotarySpeakerEffect::setvars(bool init)
{
    drive.newValue(*f[rot_drive]);
    width.set_target_smoothed(storage->db_to_linear(*f[rot_width]));
    mix.set_target_smoothed(*f[rot_mix]);

    if (init)
    {
        drive.instantize();
        width.instantize();
        mix.instantize();

        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
            wsState.R[i] = _mm_setzero_ps();
    }
}

void RotarySpeakerEffect::suspend()
{
    memset(buffer, 0, max_delay_length * sizeof(float));
    xover.suspend();
    lowbass.suspend();
    wpos = 0;
}

void RotarySpeakerEffect::init_default_values()
{
    fxdata->p[rot_horn_rate].val.f = 1.f;
    fxdata->p[rot_rotor_rate].val.f = 0.7;
    fxdata->p[rot_drive].val.f = 0.f;
    fxdata->p[rot_waveshape].val.i = 0;
    fxdata->p[rot_doppler].val.f = 0.25f;
    fxdata->p[rot_tremolo].val.f = 0.5f;
    fxdata->p[rot_width].val.f = 1.0f;
    fxdata->p[rot_mix].val.f = 1.0f;
}

const char *RotarySpeakerEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Speaker";
    case 1:
        return "Amp";
    case 2:
        return "Modulation";
    case 3:
        return "Output";
    }
    return 0;
}
int RotarySpeakerEffect::group_label_ypos(int id)
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
        return 19;
    }
    return 0;
}

void RotarySpeakerEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rot_horn_rate].set_name("Horn Rate");
    fxdata->p[rot_horn_rate].set_type(ct_lforate);
    fxdata->p[rot_rotor_rate].set_name("Rotor Rate");
    fxdata->p[rot_rotor_rate].set_type(ct_percent200);
    fxdata->p[rot_drive].set_name("Drive");
    fxdata->p[rot_drive].set_type(ct_rotarydrive);
    fxdata->p[rot_waveshape].set_name("Model");
    fxdata->p[rot_waveshape].set_type(ct_distortion_waveshape);
    fxdata->p[rot_doppler].set_name("Doppler");
    fxdata->p[rot_doppler].set_type(ct_percent);
    fxdata->p[rot_tremolo].set_name("Tremolo");
    fxdata->p[rot_tremolo].set_type(ct_percent);
    fxdata->p[rot_width].set_name("Width");
    fxdata->p[rot_width].set_type(ct_decibel_narrow);
    fxdata->p[rot_mix].set_name("Mix");
    fxdata->p[rot_mix].set_type(ct_percent);

    fxdata->p[rot_rotor_rate].val_default.f = 0.7;

    fxdata->p[rot_horn_rate].posy_offset = 1;
    fxdata->p[rot_rotor_rate].posy_offset = -3;

    fxdata->p[rot_drive].posy_offset = 1;
    fxdata->p[rot_waveshape].posy_offset = -3;

    fxdata->p[rot_doppler].posy_offset = 11;
    fxdata->p[rot_tremolo].posy_offset = 11;

    fxdata->p[rot_width].posy_offset = 7;
    fxdata->p[rot_mix].posy_offset = 7;
}

void RotarySpeakerEffect::process_only_control()
{
    float frate =
        *f[rot_horn_rate] * (fxdata->p[rot_horn_rate].temposync ? storage->temposyncratio : 1.f);

    lfo.set_rate(2 * M_PI * powf(2, frate) * storage->dsamplerate_inv * BLOCK_SIZE);
    lf_lfo.set_rate(*f[rot_rotor_rate] * 2 * M_PI * powf(2, frate) * storage->dsamplerate_inv *
                    BLOCK_SIZE);

    lfo.process();
    lf_lfo.process();
}

void RotarySpeakerEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    float frate =
        *f[rot_horn_rate] * (fxdata->p[rot_horn_rate].temposync ? storage->temposyncratio : 1.f);

    /*
     ** lf_lfo.process drives the sub-frequency and processes inside the iteration over samples>
     ** lfo.process drives the speaker and processes outside the iteration
     ** therefore lf_lfo processes BLOCK_SIZE more times
     ** hence the lack of BLOCK_SIZE here
     */
    lfo.set_rate(2 * M_PI * powf(2, frate) * storage->dsamplerate_inv * BLOCK_SIZE);
    lf_lfo.set_rate(*f[rot_rotor_rate] * 2 * M_PI * powf(2, frate) * storage->dsamplerate_inv);

    float precalc0 = (-2 - (float)lfo.i);
    float precalc1 = (-1 - (float)lfo.r);
    float precalc2 = (+1 - (float)lfo.r);
    float lenL = sqrt(precalc0 * precalc0 + precalc1 * precalc1);
    float lenR = sqrt(precalc0 * precalc0 + precalc2 * precalc2);

    float delay = storage->samplerate * 0.0018f * *f[rot_doppler];

    dL.newValue(delay * lenL);
    dR.newValue(delay * lenR);

    float dotp_L = (precalc1 * (float)lfo.r + precalc0 * (float)lfo.i) / lenL;
    float dotp_R = (precalc2 * (float)lfo.r + precalc0 * (float)lfo.i) / lenR;

    float a = *f[rot_tremolo] * 0.6f;

    hornamp[0].newValue((1.f - a) + a * dotp_L);
    hornamp[1].newValue((1.f - a) + a * dotp_R);

    lfo.process();

    float upper alignas(16)[BLOCK_SIZE];
    float lower alignas(16)[BLOCK_SIZE];
    float lower_sub alignas(16)[BLOCK_SIZE];
    float tbufferL alignas(16)[BLOCK_SIZE];
    float tbufferR alignas(16)[BLOCK_SIZE];
    float wbL alignas(16)[BLOCK_SIZE];
    float wbR alignas(16)[BLOCK_SIZE];

    int k;

    drive.newValue(*f[rot_drive]);

    int wsi = *pdata_ival[rot_waveshape];

    if (wsi < 0 || wsi >= n_fxws)
    {
        wsi = 0;
    }

    auto ws = FXWaveShapers[wsi];

    /*
    ** This is a set of completely empirical scaling settings to offset gain being too crazy
    ** in the drive cycle. There's no science really, just us playing with it and listening
    */
    float gain_tweak, compensate, drive_factor, gain_comp_factor;
    float compensateStartsAt = 0.18;
    bool square_drive_comp = false;

    switch (ws)
    {
    case sst::waveshapers::WaveshaperType::wst_hard:
    {
        gain_tweak = 1.4;
        compensate = 3.f;
    }
    case sst::waveshapers::WaveshaperType::wst_asym:
    {
        gain_tweak = 1.15;
        compensate = 9.f;
        compensateStartsAt = 0.05;
        break;
    }
    case sst::waveshapers::WaveshaperType::wst_sine:
    {
        gain_tweak = 4.4;
        compensate = 10.f;
        compensateStartsAt = 0.f;
        square_drive_comp = true;
        break;
    }
    case sst::waveshapers::WaveshaperType::wst_digital:
    {
        gain_tweak = 1.f;
        compensate = 4.f;
        compensateStartsAt = 0.f;
        break;
    }
    case sst::waveshapers::WaveshaperType::wst_fwrectify:
    case sst::waveshapers::WaveshaperType::wst_fuzzsoft:
    {
        gain_tweak = 1.f;
        compensate = 2.f;
        compensateStartsAt = 0.f;
        break;
    }
    default:
    {
        gain_tweak = 1.f;
        compensate = 4.f;
        break;
    }
    }

    if (!fxdata->p[rot_drive].deactivated)
    {
        drive_factor = 1.f + (drive.v * drive.v * 15.f);

        if (drive.v < compensateStartsAt)
            gain_comp_factor = 1.0;
        else if (square_drive_comp)
            gain_comp_factor = 1.f + (((drive.v * drive.v) - compensateStartsAt) * compensate);
        else
            gain_comp_factor = 1.f + ((drive.v - compensateStartsAt) * compensate);
    }

    // FX waveshapers have value at wst_soft for 0; so don't add wst_soft here (like we did in 1.9)
    bool useSSEShaper = (ws >= sst::waveshapers::WaveshaperType::wst_sine);
    auto wsop = sst::waveshapers::GetQuadWaveshaper(ws);

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        float input;

        if (!fxdata->p[rot_drive].deactivated)
        {
            drive_factor = 1.f + (drive.v * drive.v * 15.f);
            if (useSSEShaper)
            {
                auto inp = _mm_set1_ps(0.5 * (dataL[k] + dataR[k]));
                auto wsres = wsop(&wsState, inp, _mm_set1_ps(drive_factor));
                float r[4];
                _mm_store_ps(r, wsres);
                input = r[0] * gain_tweak;
            }
            else
            {
                input = storage->lookup_waveshape(ws, 0.5f * (dataL[k] + dataR[k]) * drive_factor) *
                        gain_tweak;
            }
            input /= gain_comp_factor;

            drive.process();
        }
        else
        {
            input = 0.5f * (dataL[k] + dataR[k]);
        }

        upper[k] = input;
        lower[k] = input;
    }

    xover.process_block(lower);

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        // feed delay input
        int wp = (wpos + k) & (max_delay_length - 1);
        lower_sub[k] = lower[k];
        upper[k] -= lower[k];
        buffer[wp] = upper[k];

        int i_dtimeL = max(BLOCK_SIZE, min((int)dL.v, max_delay_length - FIRipol_N - 1));
        int i_dtimeR = max(BLOCK_SIZE, min((int)dR.v, max_delay_length - FIRipol_N - 1));

        int rpL = (wpos - i_dtimeL + k);
        int rpR = (wpos - i_dtimeR + k);

        int sincL = FIRipol_N *
                    limit_range((int)(FIRipol_M * (float(i_dtimeL + 1) - dL.v)), 0, FIRipol_M - 1);
        int sincR = FIRipol_N *
                    limit_range((int)(FIRipol_M * (float(i_dtimeR + 1) - dR.v)), 0, FIRipol_M - 1);

        // get delay output
        tbufferL[k] = 0;
        tbufferR[k] = 0;
        for (int i = 0; i < FIRipol_N; i++)
        {
            tbufferL[k] += buffer[(rpL - i) & (max_delay_length - 1)] *
                           storage->sinctable1X[sincL + FIRipol_N - i];
            tbufferR[k] += buffer[(rpR - i) & (max_delay_length - 1)] *
                           storage->sinctable1X[sincR + FIRipol_N - i];
        }
        dL.process();
        dR.process();
    }

    lowbass.process_block(lower_sub);

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        lower[k] -= lower_sub[k];

        float bass = lower_sub[k] + lower[k] * (lf_lfo.r * 0.6f + 0.3f);

        wbL[k] = hornamp[0].v * tbufferL[k] + bass;
        wbR[k] = hornamp[1].v * tbufferR[k] + bass;

        lf_lfo.process();
        hornamp[0].process();
        hornamp[1].process();
    }

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(wbL, wbR, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, wbL, wbR, BLOCK_SIZE_QUAD);

    mix.fade_2_blocks_to(dataL, wbL, dataR, wbR, dataL, dataR, BLOCK_SIZE_QUAD);

    wpos += BLOCK_SIZE;
    wpos = wpos & (max_delay_length - 1);
}

void RotarySpeakerEffect::handleStreamingMismatches(int streamingRevision,
                                                    int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        fxdata->p[rot_rotor_rate].val.f = 0.7;
        fxdata->p[rot_drive].val.f = 0.f;
        fxdata->p[rot_drive].deactivated = true;
        fxdata->p[rot_waveshape].val.i = 0;
        fxdata->p[rot_width].val.f = 1.f;
        fxdata->p[rot_mix].val.f = 1.f;
    }
}