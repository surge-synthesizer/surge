#include "Reverb1Effect.h"

using namespace std;

const float db60 = powf(10.f, 0.05f * -60.f);

Reverb1Effect::Reverb1Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), band1(storage), locut(storage), hicut(storage)
{
    b = 0;
}

Reverb1Effect::~Reverb1Effect() {}

void Reverb1Effect::init()
{
    setvars(true);

    band1.coeff_peakEQ(band1.calc_omega(fxdata->p[rev1_freq1].val.f / 12.f), 2,
                       fxdata->p[rev1_gain1].val.f);
    locut.coeff_HP(locut.calc_omega(fxdata->p[rev1_lowcut].val.f / 12.f), 0.5);
    hicut.coeff_LP2B(hicut.calc_omega(fxdata->p[rev1_highcut].val.f / 12.f), 0.5);
    band1.coeff_instantize();
    locut.coeff_instantize();
    hicut.coeff_instantize();
    band1.suspend();
    locut.suspend();
    hicut.suspend();

    ringout = 10000000;
    b = 0;

    loadpreset(0);
    modphase = 0;
    update_rsize();
    mix.set_target(1.f); // Should be the smoothest
    mix.instantize();

    width.set_target(1.f); // Should be the smoothest
    width.instantize();

    for (int t = 0; t < rev_taps; t++)
    {
        float x = (float)t / (rev_taps - 1.f);
        float xbp = -1.f + 2.f * x;

        out_tap[t] = 0;
        delay_pan_L[t] = sqrt(0.5 - 0.495 * xbp);
        delay_pan_R[t] = sqrt(0.5 + 0.495 * xbp);
    }
    delay_pos = 0;
}

void Reverb1Effect::setvars(bool init) {}

void Reverb1Effect::clear_buffers()
{
    clear_block(predelay, max_rev_dly >> 2);
    clear_block(delay, (rev_taps * max_rev_dly) >> 2);
}

void Reverb1Effect::loadpreset(int id)
{
    shape = id;

    clear_buffers();

    switch (id)
    {
    case 0:
        delay_time[0] = 1339934;
        delay_time[1] = 962710;
        delay_time[2] = 1004427;
        delay_time[3] = 1103966;
        delay_time[4] = 1198575;
        delay_time[5] = 1743348;
        delay_time[6] = 1033425;
        delay_time[7] = 933313;
        delay_time[8] = 949407;
        delay_time[9] = 1402754;
        delay_time[10] = 1379894;
        delay_time[11] = 1225304;
        delay_time[12] = 1135598;
        delay_time[13] = 1402107;
        delay_time[14] = 956152;
        delay_time[15] = 1137737;
        break;
    case 1:
        delay_time[0] = 1265607;
        delay_time[1] = 844703;
        delay_time[2] = 856159;
        delay_time[3] = 1406425;
        delay_time[4] = 786608;
        delay_time[5] = 1163557;
        delay_time[6] = 1091206;
        delay_time[7] = 1129434;
        delay_time[8] = 1270379;
        delay_time[9] = 896997;
        delay_time[10] = 1415393;
        delay_time[11] = 782808;
        delay_time[12] = 868582;
        delay_time[13] = 1234463;
        delay_time[14] = 1000336;
        delay_time[15] = 968299;
        break;
    case 2:
        delay_time[0] = 1293101;
        delay_time[1] = 1334867;
        delay_time[2] = 1178781;
        delay_time[3] = 1850949;
        delay_time[4] = 1663760;
        delay_time[5] = 1982922;
        delay_time[6] = 1211021;
        delay_time[7] = 1824481;
        delay_time[8] = 1520266;
        delay_time[9] = 1351822;
        delay_time[10] = 1102711;
        delay_time[11] = 1513696;
        delay_time[12] = 1057618;
        delay_time[13] = 1671799;
        delay_time[14] = 1406360;
        delay_time[15] = 1170468;
        break;
    case 3:
        delay_time[0] = 1833435;
        delay_time[1] = 2462309;
        delay_time[2] = 2711583;
        delay_time[3] = 2219764;
        delay_time[4] = 1664194;
        delay_time[5] = 2109157;
        delay_time[6] = 1626137;
        delay_time[7] = 1434473;
        delay_time[8] = 2271242;
        delay_time[9] = 1621375;
        delay_time[10] = 1831218;
        delay_time[11] = 2640903;
        delay_time[12] = 1577737;
        delay_time[13] = 1871624;
        delay_time[14] = 2439164;
        delay_time[15] = 1427343;
        break;
    }

    for (int t = 0; t < rev_taps; t++)
    {
        // float r = storage->rand_01();
        // float rbp = storage->rand_pm1();
        // float a = 256.f * (3000.f * (1.f + rbp * rbp * *f[rev1_variation]))*(1.f + 1.f *
        // *f[rev1_roomsize]); delay_time[t] = (int)a;
        delay_time[t] = (int)((float)(2.f * *f[rev1_roomsize]) * delay_time[t]);
    }
    lastf[rev1_roomsize] = *f[rev1_roomsize];
    update_rtime();
}

void Reverb1Effect::update_rtime()
{
    int max_dt = 0;
    for (int t = 0; t < rev_taps; t++)
    {
        delay_fb[t] = powf(db60, delay_time[t] /
                                     (256.f * storage->samplerate * powf(2.f, *f[rev1_decaytime])));
        max_dt = max(max_dt, delay_time[t]);
    }
    lastf[rev1_decaytime] = *f[rev1_decaytime];
    float t = BLOCK_SIZE_INV *
              ((float)(max_dt >> 8) + storage->samplerate * powf(2.f, *f[rev1_decaytime]) *
                                          2.f); // * 2.f is to get the db120 time
    ringout_time = (int)t;
}

void Reverb1Effect::update_rsize()
{
    // memset(delay,0,rev_taps*max_rev_dly*sizeof(float));

    loadpreset(shape);

    // lastf[rev1_variation] = *f[rev1_variation];
    // update_rtime();
}

void Reverb1Effect::process(float *dataL, float *dataR)
{
    float wetL alignas(16)[BLOCK_SIZE], wetR alignas(16)[BLOCK_SIZE];

    if (fxdata->p[rev1_shape].val.i != shape)
        loadpreset(fxdata->p[rev1_shape].val.i);
    if ((b == 0) && (fabs(*f[rev1_roomsize] - lastf[rev1_roomsize]) > 0.001f))
        loadpreset(shape);
    //	if(fabs(*f[rev1_variation] - lastf[rev1_variation]) > 0.001f) update_rsize();
    if (fabs(*f[rev1_decaytime] - lastf[rev1_decaytime]) > 0.001f)
        update_rtime();

    // do more seldom
    if (b == 0)
    {
        band1.coeff_peakEQ(band1.calc_omega(*f[rev1_freq1] * (1.f / 12.f)), 2, *f[rev1_gain1]);
        locut.coeff_HP(locut.calc_omega(*f[rev1_lowcut] * (1.f / 12.f)), 0.5);
        hicut.coeff_LP2B(hicut.calc_omega(*f[rev1_highcut] * (1.f / 12.f)), 0.5);
    }
    b = (b + 1) & 31;

    mix.set_target_smoothed(*f[rev1_mix]);
    width.set_target_smoothed(storage->db_to_linear(*f[rev1_width]));

    int pdtime = (int)(float)storage->samplerate *
                 storage->note_to_pitch_ignoring_tuning(12 * *f[rev1_predelay]) *
                 (fxdata->p[rev1_predelay].temposync ? storage->temposyncratio_inv : 1.f);

    const __m128 one4 = _mm_set1_ps(1.f);
    float dv = *(f[rev1_damping]);

    dv = limit_range(dv, 0.01f, 0.99f); // this is a simple one-pole damper, w * y[n] + ( 1-w )
                                        // y[n-1] so to be stable has to stay in range
    __m128 damp4 = _mm_load1_ps(&dv);
    __m128 damp4m1 = _mm_sub_ps(one4, damp4);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        for (int t = 0; t < rev_taps; t += 4)
        {
            int dp = (delay_pos - (delay_time[t] >> 8));
            // float newa = delay[t + ((dp & (max_rev_dly-1))<<rev_tap_bits)];
            __m128 newa = _mm_load_ss(&delay[t + ((dp & (max_rev_dly - 1)) << rev_tap_bits)]);
            dp = (delay_pos - (delay_time[t + 1] >> 8));
            __m128 newb = _mm_load_ss(&delay[t + 1 + ((dp & (max_rev_dly - 1)) << rev_tap_bits)]);
            dp = (delay_pos - (delay_time[t + 2] >> 8));
            newa = _mm_unpacklo_ps(newa, newb); // a,b,0,0
            __m128 newc = _mm_load_ss(&delay[t + 2 + ((dp & (max_rev_dly - 1)) << rev_tap_bits)]);
            dp = (delay_pos - (delay_time[t + 3] >> 8));
            __m128 newd = _mm_load_ss(&delay[t + 3 + ((dp & (max_rev_dly - 1)) << rev_tap_bits)]);
            newc = _mm_unpacklo_ps(newc, newd);      // c,d,0,0
            __m128 new4 = _mm_movelh_ps(newa, newc); // a,b,c,d

            __m128 out_tap4 = _mm_load_ps(&out_tap[t]);
            out_tap4 = _mm_add_ps(_mm_mul_ps(out_tap4, damp4), _mm_mul_ps(new4, damp4m1));
            _mm_store_ps(&out_tap[t], out_tap4);
            // out_tap[t] = *f[rev1_damping]*out_tap[t] + (1- *f[rev1_damping])*newa;
        }

        __m128 fb = _mm_add_ps(_mm_add_ps(_mm_load_ps(out_tap), _mm_load_ps(out_tap + 4)),
                               _mm_add_ps(_mm_load_ps(out_tap + 8), _mm_load_ps(out_tap + 12)));
        fb = sum_ps_to_ss(fb);
        /*for(int t=0; t<rev_taps; t+=4)
        {
                fb += out_tap[t] + out_tap[t+1] + out_tap[t+2] + out_tap[t+3];
        }*/

        const __m128 ca = _mm_set_ss(((float)(-(2.f) / rev_taps)));
        // fb =  ca * fb + predelay[(delay_pos - pdtime)&(max_rev_dly-1)];
        fb = _mm_add_ss(_mm_mul_ss(ca, fb),
                        _mm_load_ss(&predelay[(delay_pos - pdtime) & (max_rev_dly - 1)]));

        delay_pos = (delay_pos + 1) & (max_rev_dly - 1);

        predelay[delay_pos] = 0.5f * (dataL[k] + dataR[k]);
        //__m128 fb4 = _mm_load1_ps(&fb);
        __m128 fb4 = _mm_shuffle_ps(fb, fb, 0);

        __m128 L = _mm_setzero_ps(), R = _mm_setzero_ps();
        for (int t = 0; t < rev_taps; t += 4)
        {
            __m128 ot = _mm_load_ps(&out_tap[t]);
            __m128 dfb = _mm_load_ps(&delay_fb[t]);
            __m128 a = _mm_mul_ps(dfb, _mm_add_ps(fb4, ot));
            _mm_store_ps(&delay[(delay_pos << rev_tap_bits) + t], a);
            L = _mm_add_ps(L, _mm_mul_ps(ot, _mm_load_ps(&delay_pan_L[t])));
            R = _mm_add_ps(R, _mm_mul_ps(ot, _mm_load_ps(&delay_pan_R[t])));
        }
        L = sum_ps_to_ss(L);
        R = sum_ps_to_ss(R);
        _mm_store_ss(&wetL[k], L);
        _mm_store_ss(&wetR[k], R);
    }

    if (!fxdata->p[rev1_lowcut].deactivated)
    {
        locut.process_block(wetL, wetR);
    }

    band1.process_block(wetL, wetR);

    if (!fxdata->p[rev1_highcut].deactivated)
    {
        hicut.process_block(wetL, wetR);
    }

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(wetL, wetR, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, wetL, wetR, BLOCK_SIZE_QUAD);

    mix.fade_2_blocks_to(dataL, wetL, dataR, wetR, dataL, dataR, BLOCK_SIZE_QUAD);
}

void Reverb1Effect::suspend() { init(); }

const char *Reverb1Effect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pre-Delay";
    case 1:
        return "Reverb";
    case 2:
        return "EQ";
    case 3:
        return "Output";
    }
    return 0;
}
int Reverb1Effect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 15;
    case 3:
        return 25;
    }
    return 0;
}

void Reverb1Effect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rev1_predelay].set_name("Pre-Delay");
    fxdata->p[rev1_predelay].set_type(ct_envtime);
    fxdata->p[rev1_predelay].modulateable = false;

    fxdata->p[rev1_shape].set_name("Room Shape");
    fxdata->p[rev1_shape].set_type(ct_reverbshape);
    fxdata->p[rev1_shape].modulateable = false;
    fxdata->p[rev1_roomsize].set_name("Size");
    fxdata->p[rev1_roomsize].set_type(ct_percent);
    fxdata->p[rev1_roomsize].modulateable = false;
    fxdata->p[rev1_decaytime].set_name("Decay Time");
    fxdata->p[rev1_decaytime].set_type(ct_reverbtime);
    fxdata->p[rev1_damping].set_name("HF Damping");
    fxdata->p[rev1_damping].set_type(ct_percent);

    fxdata->p[rev1_lowcut].set_name("Low Cut");
    fxdata->p[rev1_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[rev1_freq1].set_name("Peak Freq");
    fxdata->p[rev1_freq1].set_type(ct_freq_audible);
    fxdata->p[rev1_gain1].set_name("Peak Gain");
    fxdata->p[rev1_gain1].set_type(ct_decibel);
    fxdata->p[rev1_highcut].set_name("High Cut");
    fxdata->p[rev1_highcut].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[rev1_mix].set_name("Mix");
    fxdata->p[rev1_mix].set_type(ct_percent);
    fxdata->p[rev1_width].set_name("Width");
    fxdata->p[rev1_width].set_type(ct_decibel_narrow);

    // fxdata->p[rev1_variation].set_name("Irregularity");
    // fxdata->p[rev1_variation].set_type(ct_percent);

    fxdata->p[rev1_predelay].posy_offset = 1;

    fxdata->p[rev1_shape].posy_offset = 3;
    fxdata->p[rev1_decaytime].posy_offset = 3;
    fxdata->p[rev1_roomsize].posy_offset = 3;
    fxdata->p[rev1_damping].posy_offset = 3;

    fxdata->p[rev1_lowcut].posy_offset = 5;
    fxdata->p[rev1_freq1].posy_offset = 5;
    fxdata->p[rev1_gain1].posy_offset = 5;
    fxdata->p[rev1_highcut].posy_offset = 5;

    fxdata->p[rev1_mix].posy_offset = 9;
    fxdata->p[rev1_width].posy_offset = 5;
}

void Reverb1Effect::init_default_values()
{
    fxdata->p[rev1_predelay].val.f = -4.f;
    fxdata->p[rev1_shape].val.i = 0;
    fxdata->p[rev1_decaytime].val.f = 1.f;
    fxdata->p[rev1_roomsize].val.f = 0.5f;
    fxdata->p[rev1_damping].val.f = 0.2f;
    fxdata->p[rev1_freq1].val.f = 0.0f;
    fxdata->p[rev1_gain1].val.f = 0.0f;

    fxdata->p[rev1_lowcut].val.f = -24.0f;
    fxdata->p[rev1_lowcut].deactivated = false;

    fxdata->p[rev1_highcut].val.f = 72.0f;
    fxdata->p[rev1_highcut].deactivated = false;

    fxdata->p[rev1_mix].val.f = 1.0f;
    fxdata->p[rev1_width].val.f = 0.0f;

    // fxdata->p[rev1_variation].val.f = 0.f;
}

void Reverb1Effect::handleStreamingMismatches(int streamingRevision,
                                              int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[rev1_lowcut].deactivated = false;
        fxdata->p[rev1_highcut].deactivated = false;
    }
}
