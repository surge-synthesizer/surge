#include "DistortionEffect.h"
#include <vt_dsp/halfratefilter.h>
#include "QuadFilterUnit.h"

// feedback can get tricky with packed SSE

const int dist_OS_bits = 2;
const int distortion_OS = 1 << dist_OS_bits;

DistortionEffect::DistortionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), band1(storage), band2(storage), lp1(storage), lp2(storage),
      hr_a(3, false), hr_b(3, true)
{
    lp1.setBlockSize(BLOCK_SIZE * distortion_OS);
    lp2.setBlockSize(BLOCK_SIZE * distortion_OS);
    drive.set_blocksize(BLOCK_SIZE);
    outgain.set_blocksize(BLOCK_SIZE);
}

DistortionEffect::~DistortionEffect() {}

void DistortionEffect::init()
{
    setvars(true);
    band1.suspend();
    band2.suspend();
    lp1.suspend();
    lp2.suspend();
    bi = 0.f;
    L = 0.f;
    R = 0.f;
}

void DistortionEffect::setvars(bool init)
{

    if (init)
    {
        float pregain = fxdata->p[dist_preeq_gain].get_extended(fxdata->p[dist_preeq_gain].val.f);
        float postgain =
            fxdata->p[dist_posteq_gain].get_extended(fxdata->p[dist_posteq_gain].val.f);
        band1.coeff_peakEQ(band1.calc_omega(fxdata->p[dist_preeq_freq].val.f / 12.f),
                           fxdata->p[dist_preeq_bw].val.f, pregain);
        band2.coeff_peakEQ(band2.calc_omega(fxdata->p[dist_posteq_freq].val.f / 12.f),
                           fxdata->p[dist_posteq_bw].val.f, postgain);
        drive.set_target(0.f);
        outgain.set_target(0.f);
    }
    else
    {
        float pregain = fxdata->p[dist_preeq_gain].get_extended(*f[dist_preeq_gain]);
        float postgain = fxdata->p[dist_posteq_gain].get_extended(*f[dist_posteq_gain]);
        band1.coeff_peakEQ(band1.calc_omega(*f[dist_preeq_freq] / 12.f), *f[dist_preeq_bw],
                           pregain);
        band2.coeff_peakEQ(band2.calc_omega(*f[dist_posteq_freq] / 12.f), *f[dist_posteq_bw],
                           postgain);
        lp1.coeff_LP2B(lp1.calc_omega((*f[dist_preeq_highcut] / 12.0) - 2.f), 0.707);
        lp2.coeff_LP2B(lp2.calc_omega((*f[dist_posteq_highcut] / 12.0) - 2.f), 0.707);
        lp1.coeff_instantize();
        lp2.coeff_instantize();
    }
}

void DistortionEffect::process(float *dataL, float *dataR)
{
    // TODO fix denormals!
    if (bi == 0)
        setvars(false);
    bi = (bi + 1) & slowrate_m1;

    band1.process_block(dataL, dataR);
    auto dS = drive.get_target();
    auto dE = db_to_linear(fxdata->p[dist_drive].get_extended(*f[dist_drive]));
    drive.set_target_smoothed(dE);
    outgain.set_target_smoothed(db_to_linear(*f[dist_gain]));
    float fb = *f[dist_feedback];
    int ws = *pdata_ival[dist_model];
    if (ws < 0 || ws >= n_ws_types)
        ws = 0;

    float bL alignas(16)[BLOCK_SIZE << dist_OS_bits];
    float bR alignas(16)[BLOCK_SIZE << dist_OS_bits];
    assert(dist_OS_bits == 2);

    drive.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

    bool useSSEShaper = (ws + wst_soft == wst_digital || ws + wst_soft == wst_sine);

    auto wsop = GetQFPtrWaveshaper(wst_soft + ws);

    float dD = 0.f;
    float dNow = dS;
    if (useSSEShaper)
    {
        dD = (dE - dS) / (BLOCK_SIZE * dist_OS_bits);
    }

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float a = (k & 16) ? 0.00000001 : -0.00000001; // denormal thingy
        float Lin = dataL[k];
        float Rin = dataR[k];
        for (int s = 0; s < distortion_OS; s++)
        {
            L = Lin + fb * L;
            R = Rin + fb * R;

            if (!fxdata->p[dist_preeq_highcut].deactivated)
            {
                lp1.process_sample_nolag(L, R);
            }

            if (useSSEShaper)
            {
                float sb[4];
                auto dInv = 1.f / dNow;

                sb[0] = L * dInv;
                sb[1] = R * dInv;
                auto lr128 = _mm_load_ps(sb);
                auto wsres = wsop(lr128, _mm_set1_ps(dNow));
                _mm_store_ps(sb, wsres);
                L = sb[0];
                R = sb[1];

                dNow += dD;
            }
            else
            {
                L = lookup_waveshape(wst_soft + ws, L);
                R = lookup_waveshape(wst_soft + ws, R);
            }

            L += a;
            R += a; // denormal

            if (!fxdata->p[dist_posteq_highcut].deactivated)
            {
                lp2.process_sample_nolag(L, R);
            }

            bL[s + (k << dist_OS_bits)] = L;
            bR[s + (k << dist_OS_bits)] = R;
        }
    }

    hr_a.process_block_D2(bL, bR, 128);
    hr_b.process_block_D2(bL, bR, 64);

    outgain.multiply_2_blocks_to(bL, bR, dataL, dataR, BLOCK_SIZE_QUAD);

    band2.process_block(dataL, dataR);
}

void DistortionEffect::suspend() { init(); }

const char *DistortionEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pre-EQ";
    case 1:
        return "Distortion";
    case 2:
        return "Post-EQ";
    case 3:
        return "Output";
    }
    return 0;
}
int DistortionEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 19;
    case 3:
        return 29;
    }
    return 0;
}

void DistortionEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[dist_preeq_gain].set_name("Gain");
    fxdata->p[dist_preeq_gain].set_type(ct_decibel_extendable);
    fxdata->p[dist_preeq_freq].set_name("Frequency");
    fxdata->p[dist_preeq_freq].set_type(ct_freq_audible);
    fxdata->p[dist_preeq_bw].set_name("Bandwidth");
    fxdata->p[dist_preeq_bw].set_type(ct_bandwidth);
    fxdata->p[dist_preeq_highcut].set_name("High Cut");
    fxdata->p[dist_preeq_highcut].set_type(ct_freq_audible_deactivatable);

    fxdata->p[dist_drive].set_name("Drive");
    fxdata->p[dist_drive].set_type(ct_decibel_narrow_extendable);
    fxdata->p[dist_feedback].set_name("Feedback");
    fxdata->p[dist_feedback].set_type(ct_percent_bipolar);
    fxdata->p[dist_model].set_name("Model");
    fxdata->p[dist_model].set_type(ct_distortion_waveshape);

    fxdata->p[dist_posteq_gain].set_name("Gain");
    fxdata->p[dist_posteq_gain].set_type(ct_decibel_extendable);
    fxdata->p[dist_posteq_freq].set_name("Frequency");
    fxdata->p[dist_posteq_freq].set_type(ct_freq_audible);
    fxdata->p[dist_posteq_bw].set_name("Bandwidth");
    fxdata->p[dist_posteq_bw].set_type(ct_bandwidth);
    fxdata->p[dist_posteq_highcut].set_name("High Cut");
    fxdata->p[dist_posteq_highcut].set_type(ct_freq_audible_deactivatable);

    fxdata->p[dist_gain].set_name("Gain");
    fxdata->p[dist_gain].set_type(ct_decibel_narrow);

    fxdata->p[dist_preeq_gain].posy_offset = 1;
    fxdata->p[dist_preeq_freq].posy_offset = 1;
    fxdata->p[dist_preeq_bw].posy_offset = 1;
    fxdata->p[dist_preeq_highcut].posy_offset = 1;

    fxdata->p[dist_drive].posy_offset = 5;
    fxdata->p[dist_feedback].posy_offset = 5;
    fxdata->p[dist_model].posy_offset = -11;

    fxdata->p[dist_posteq_gain].posy_offset = 7;
    fxdata->p[dist_posteq_freq].posy_offset = 7;
    fxdata->p[dist_posteq_bw].posy_offset = 7;
    fxdata->p[dist_posteq_highcut].posy_offset = 7;

    fxdata->p[dist_gain].posy_offset = 9;
}
void DistortionEffect::init_default_values()
{
    fxdata->p[dist_preeq_gain].val.f = 0.f;
    fxdata->p[dist_preeq_freq].val.f = 0.f;
    fxdata->p[dist_preeq_bw].val.f = 2.f;
    fxdata->p[dist_preeq_highcut].deactivated = false;

    fxdata->p[dist_model].val.f = 0.f;

    fxdata->p[dist_posteq_gain].val.f = 0.f;
    fxdata->p[dist_posteq_freq].val.f = 0.f;
    fxdata->p[dist_posteq_bw].val.f = 2.f;
    fxdata->p[dist_posteq_highcut].deactivated = false;

    fxdata->p[dist_gain].val.f = 0.f;
}

void DistortionEffect::handleStreamingMismatches(int streamingRevision,
                                                 int currentSynthStreamingRevision)
{
    if (streamingRevision <= 11)
    {
        fxdata->p[dist_model].val.i = 0;
        fxdata->p[dist_preeq_gain].extend_range = false;
        fxdata->p[dist_posteq_gain].extend_range = false;
    }

    if (streamingRevision <= 15)
    {
        fxdata->p[dist_preeq_highcut].deactivated = false;
        fxdata->p[dist_posteq_highcut].deactivated = false;
    }
}
