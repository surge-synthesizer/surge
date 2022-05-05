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

#include "SampleAndHoldOscillator.h"
#include "DSPUtils.h"

using namespace std;

// const float integrator_hpf = 0.99999999f;
// const float integrator_hpf = 0.9992144f;		// 44.1 kHz
// const float integrator_hpf = 0.9964f;		// 44.1 kHz
// const float integrator_hpf = 0.9982f;		// 44.1 kHz	 Magic Moog freq
const float integrator_hpf = 0.999f;
// 290 samples to fall 50% (British) (is probably a 2-pole HPF)
// 202 samples (American)
// const float integrator_hpf = 0.999f;
// pow(ln(0.5)/(samplerate/50hz)
const float hpf_cycle_loss = 0.995f;

SampleAndHoldOscillator::SampleAndHoldOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                                 pdata *localcopy)
    : AbstractBlitOscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{
}

SampleAndHoldOscillator::~SampleAndHoldOscillator() {}

void SampleAndHoldOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    assert(storage);
    first_run = true;
    osc_out = _mm_set1_ps(0.f);
    osc_outR = _mm_set1_ps(0.f);
    bufpos = 0;
    dc = 0;

    // init here
    id_shape = oscdata->p[shn_correlation].param_id_in_scene;
    id_pw = oscdata->p[shn_width].param_id_in_scene;
    id_smooth = oscdata->p[shn_lowcut].param_id_in_scene;
    id_sub = oscdata->p[shn_highcut].param_id_in_scene;
    id_sync = oscdata->p[shn_sync].param_id_in_scene;
    id_detune = oscdata->p[shn_unison_detune].param_id_in_scene;

    float rate = 0.05;
    l_pw.setRate(rate);

    l_shape.setRate(rate);
    l_sub.setRate(rate);
    l_sync.setRate(rate);

    n_unison = limit_range(oscdata->p[shn_unison_voices].val.i, 1, MAX_UNISON);
    if (is_display)
    {
        n_unison = 1;

        auto gen = std::minstd_rand(2);
        std::uniform_real_distribution<float> distro(-1.f, 1.f);
        urng = std::bind(distro, gen);
    }
    else
    {
        std::uniform_real_distribution<float> distro(-1.f, 1.f);
#ifdef STORAGE_USES_INDEPENDENT_RNG
        urng = std::bind(distro, storage->rngGen.g);
#else
        std::minstd_rand gen(std::rand());
        urng = std::bind(distro, gen);
#endif
    }
    prepare_unison(n_unison);

    memset(oscbuffer, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(oscbufferR, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(last_level, 0, MAX_UNISON * sizeof(float));
    memset(last_level2, 0, MAX_UNISON * sizeof(float));
    memset(elapsed_time, 0, MAX_UNISON * sizeof(float));

    this->pitch = pitch;
    update_lagvals<true>();

    int i;
    for (i = 0; i < n_unison; i++)
    {
        if (oscdata->retrigger.val.b || is_display)
        {
            oscstate[i] = 0;
            syncstate[i] = 0;
        }
        else
        {
            double drand = (double)storage->rand_01();
            double detune = oscdata->p[shn_unison_detune].get_extended(localcopy[id_detune].f) *
                            (detune_bias * float(i) + detune_offset);
            double st = drand * storage->note_to_pitch_tuningctr(detune) * 0.5;
            drand = (double)storage->rand_01();
            double ot = drand * storage->note_to_pitch_tuningctr(detune);
            oscstate[i] = st;
            syncstate[i] = st;
        }
        state[i] = 0;
        last_level[i] = 0.0;
        pwidth[i] = limit_range(l_pw.v, 0.001, 0.999);
        driftLFO[i].init(nonzero_init_drift);
    }

    hp.coeff_instantize();
    lp.coeff_instantize();

    hp.coeff_HP(hp.calc_omega(oscdata->p[shn_lowcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
    lp.coeff_LP2B(lp.calc_omega(oscdata->p[shn_highcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
}

void SampleAndHoldOscillator::init_ctrltypes()
{
    oscdata->p[shn_correlation].set_name("Correlation");
    oscdata->p[shn_correlation].set_type(ct_percent_bipolar);
    oscdata->p[shn_width].set_name("Width");
    oscdata->p[shn_width].set_type(ct_percent);
    oscdata->p[shn_width].val_default.f = 0.5f;
    oscdata->p[shn_lowcut].set_name("Low Cut");
    oscdata->p[shn_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    oscdata->p[shn_highcut].set_name("High Cut");
    oscdata->p[shn_highcut].set_type(ct_freq_audible_deactivatable_lp);
    oscdata->p[shn_sync].set_name("Sync");
    oscdata->p[shn_sync].set_type(ct_syncpitch);
    oscdata->p[shn_unison_detune].set_name("Unison Detune");
    oscdata->p[shn_unison_detune].set_type(ct_oscspread);
    oscdata->p[shn_unison_voices].set_name("Unison Voices");
    oscdata->p[shn_unison_voices].set_type(ct_osccount);
}
void SampleAndHoldOscillator::init_default_values()
{
    oscdata->p[shn_correlation].val.f = 0.f;

    oscdata->p[shn_width].val.f = 0.5f;

    // low cut at the bottom
    oscdata->p[shn_lowcut].val.f = oscdata->p[shn_lowcut].val_min.f;
    oscdata->p[shn_lowcut].deactivated = true;

    // high cut at the top
    oscdata->p[shn_highcut].val.f = oscdata->p[shn_highcut].val_max.f;
    oscdata->p[shn_highcut].deactivated = true;

    oscdata->p[shn_sync].val.f = 0.f;

    oscdata->p[shn_unison_detune].val.f = 0.1f;
    oscdata->p[shn_unison_voices].val.i = 1;
}

void SampleAndHoldOscillator::convolute(int voice, bool FM, bool stereo)
{
    float detune = drift * driftLFO[voice].val();
    if (n_unison > 1)
        detune += oscdata->p[shn_unison_detune].get_extended(localcopy[id_detune].f) *
                  (detune_bias * float(voice) + detune_offset);

    float sub = l_sub.v;

    const float p24 = (1 << 24);
    float invertcorrelation = 1.f;
    unsigned int ipos;

    if (syncstate[voice] < oscstate[voice])
    {
        if (FM)
            ipos = (unsigned int)(p24 * (syncstate[voice] * pitchmult_inv * FMmul_inv));
        else
            ipos = (unsigned int)(p24 * (syncstate[voice] * pitchmult_inv));

        float t;

        if (!oscdata->p[shn_unison_detune].absolute)
            t = storage->note_to_pitch_inv_tuningctr(detune) * 2;
        else
            t = storage->note_to_pitch_inv_ignoring_tuning(
                    detune * storage->note_to_pitch_inv_ignoring_tuning(pitch) * 16 / 0.9443) *
                2;

        if (state[voice] == 1)
            invertcorrelation = -1.f;

        state[voice] = 0;
        oscstate[voice] = syncstate[voice];
        syncstate[voice] += t;
        syncstate[voice] = max(0.f, syncstate[voice]);
    }
    else
    {
        if (FM)
            ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv * FMmul_inv));
        else
            ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv));
    }

    unsigned int delay;

    if (FM)
        delay = FMdelay;
    else
        delay = ((ipos >> 24) & 0x3f);

    unsigned int m = ((ipos >> 16) & 0xff) * (FIRipol_N << 1);
    unsigned int lipolui16 = (ipos & 0xffff);
    __m128 lipol128 = _mm_cvtsi32_ss(lipol128, lipolui16);
    lipol128 = _mm_shuffle_ps(lipol128, lipol128, _MM_SHUFFLE(0, 0, 0, 0));

    int k;
    const float s = 0.99952f;
    // add time until next statechange
    float t;
    if (oscdata->p[shn_unison_detune].absolute)
    {
        // see the comment in ClassicOscillator in the absolute branch
        t = storage->note_to_pitch_inv_ignoring_tuning(
            detune * storage->note_to_pitch_inv_ignoring_tuning(pitch) * 16 / 0.9443 + l_sync.v);

        if (t < 0.1)
            t = 0.1;
    }
    else
        t = storage->note_to_pitch_inv_tuningctr(detune + l_sync.v);

    float g, gR;

    float wf = l_shape.v * 0.8 * invertcorrelation;
    float wfabs = fabs(wf);
    float smooth = l_smooth.v;
    float rand11 = urng();
    float randt = rand11 * (1 - wfabs) - wf * last_level[voice];

    randt = randt * rcp(1.0f - wfabs);
    randt = min(0.5f, max(-0.5f, randt));

    if (state[voice] == 0)
        pwidth[voice] = l_pw.v;

    g = randt - last_level[voice];
    last_level[voice] = randt;

    g *= out_attenuation;
    if (stereo)
    {
        gR = g * panR[voice];
        g *= panL[voice];
    }

    if (stereo)
    {
        __m128 g128L = _mm_load_ss(&g);
        g128L = _mm_shuffle_ps(g128L, g128L, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 g128R = _mm_load_ss(&gR);
        g128R = _mm_shuffle_ps(g128R, g128R, _MM_SHUFFLE(0, 0, 0, 0));

        for (k = 0; k < FIRipol_N; k += 4)
        {
            float *obfL = &oscbuffer[bufpos + k + delay];
            float *obfR = &oscbufferR[bufpos + k + delay];
            __m128 obL = _mm_loadu_ps(obfL);
            __m128 obR = _mm_loadu_ps(obfR);
            __m128 st = _mm_load_ps(&storage->sinctable[m + k]);
            __m128 so = _mm_load_ps(&storage->sinctable[m + k + FIRipol_N]);
            so = _mm_mul_ps(so, lipol128);
            st = _mm_add_ps(st, so);
            obL = _mm_add_ps(obL, _mm_mul_ps(st, g128L));
            _mm_storeu_ps(obfL, obL);
            obR = _mm_add_ps(obR, _mm_mul_ps(st, g128R));
            _mm_storeu_ps(obfR, obR);
        }
    }
    else
    {
        __m128 g128 = _mm_load_ss(&g);
        g128 = _mm_shuffle_ps(g128, g128, _MM_SHUFFLE(0, 0, 0, 0));

        for (k = 0; k < FIRipol_N; k += 4)
        {
            float *obf = &oscbuffer[bufpos + k + delay];
            __m128 ob = _mm_loadu_ps(obf);
            __m128 st = _mm_load_ps(&storage->sinctable[m + k]);
            __m128 so = _mm_load_ps(&storage->sinctable[m + k + FIRipol_N]);
            so = _mm_mul_ps(so, lipol128);
            st = _mm_add_ps(st, so);
            st = _mm_mul_ps(st, g128);
            ob = _mm_add_ps(ob, st);
            _mm_storeu_ps(obf, ob);
        }
    }

    if (state[voice] & 1)
        rate[voice] = t * (1.0 - pwidth[voice]);
    else
        rate[voice] = t * pwidth[voice];

    oscstate[voice] += rate[voice];
    oscstate[voice] = max(0.f, oscstate[voice]);
    state[voice] = (state[voice] + 1) & 1;
}

void SampleAndHoldOscillator::applyFilter()
{
    if (!oscdata->p[shn_lowcut].deactivated)
    {
        auto par = &(oscdata->p[shn_lowcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        hp.coeff_HP(hp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    if (!oscdata->p[shn_highcut].deactivated)
    {
        auto par = &(oscdata->p[shn_highcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        lp.coeff_LP2B(lp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    for (int k = 0; k < BLOCK_SIZE_OS; k += BLOCK_SIZE)
    {
        if (!oscdata->p[shn_lowcut].deactivated)
            hp.process_block(&(output[k]), &(outputR[k]));
        if (!oscdata->p[shn_highcut].deactivated)
            lp.process_block(&(output[k]), &(outputR[k]));
    }
}

template <bool is_init> void SampleAndHoldOscillator::update_lagvals()
{
    l_sync.newValue(max(0.f, localcopy[id_sync].f));
    l_pw.newValue(limit_range(localcopy[id_pw].f, 0.001f, 0.999f));
    l_shape.newValue(localcopy[id_shape].f);
    l_smooth.newValue(localcopy[id_smooth].f);
    l_sub.newValue(localcopy[id_sub].f);

    auto pp = storage->note_to_pitch_tuningctr(pitch + l_sync.v);
    float invt = 4.f * min(1.0, (8.175798915 * pp * storage->dsamplerate_os_inv));
    // TODO: Make a lookup table
    float hpf2 = min(integrator_hpf, powf(hpf_cycle_loss, invt));

    li_hpf.set_target(hpf2);

    if (is_init)
    {
        l_pw.instantize();
        l_shape.instantize();
        l_smooth.instantize();
        l_sub.instantize();
        l_sync.instantize();
    }
}

void SampleAndHoldOscillator::process_block(float pitch0, float drift, bool stereo, bool FM,
                                            float depth)
{
    this->pitch = min(148.f, pitch0);
    this->drift = drift;
    pitchmult_inv =
        max(1.0, storage->dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch));
    pitchmult = 1.f / pitchmult_inv;
    // This must be a real division, reciprocal-approximation is not precise enough
    int k, l;

    update_lagvals<false>();

    l_pw.process();
    l_shape.process();
    l_smooth.process();
    l_sub.process();
    l_sync.process();

    if (FM)
    {
        for (l = 0; l < n_unison; l++)
        {
            driftLFO[l].next();
        }

        for (int s = 0; s < BLOCK_SIZE_OS; s++)
        {
            float fmmul = limit_range(1.f + depth * master_osc[s], 0.1f, 1.9f);
            float a = pitchmult * fmmul;

            FMdelay = s;

            for (l = 0; l < n_unison; l++)
            {
                while (oscstate[l] < a)
                {
                    FMmul_inv = rcp(fmmul);
                    convolute(l, true, stereo);
                }

                oscstate[l] -= a;
                if (l_sync.v > 0)
                    syncstate[l] -= a;
            }
        }
    }
    else
    {
        float a = (float)BLOCK_SIZE_OS * pitchmult;

        for (l = 0; l < n_unison; l++)
        {
            driftLFO[l].next();

            while ((syncstate[l] < a) || (oscstate[l] < a))
            {
                convolute(l, false, stereo);
            }

            oscstate[l] -= a;
            if (l_sync.v > 0)
                syncstate[l] -= a;
        }
    }

    float hpfblock alignas(16)[BLOCK_SIZE_OS];
    li_hpf.store_block(hpfblock, BLOCK_SIZE_OS_QUAD);

    __m128 mdc = _mm_load_ss(&dc);
    __m128 oa = _mm_load_ss(&out_attenuation);
    oa = _mm_mul_ss(oa, _mm_load_ss(&pitchmult));

    for (k = 0; k < BLOCK_SIZE_OS; k++)
    {
        __m128 hpf = _mm_load_ss(&hpfblock[k]);
        __m128 ob = _mm_load_ss(&oscbuffer[bufpos + k]);
        __m128 a = _mm_mul_ss(osc_out, hpf);
        ob = _mm_sub_ss(ob, _mm_mul_ss(mdc, oa));
        osc_out = _mm_add_ss(a, ob);

        _mm_store_ss(&output[k], osc_out);

        if (stereo)
        {
            ob = _mm_load_ss(&oscbufferR[bufpos + k]);
            a = _mm_mul_ss(osc_outR, hpf);
            ob = _mm_sub_ss(ob, _mm_mul_ss(mdc, oa));
            osc_outR = _mm_add_ss(a, ob);
            _mm_store_ss(&outputR[k], osc_outR);
        }
    }
    _mm_store_ss(&dc, mdc);

    clear_block(&oscbuffer[bufpos], BLOCK_SIZE_OS_QUAD);
    if (stereo)
        clear_block(&oscbufferR[bufpos], BLOCK_SIZE_OS_QUAD);
    clear_block(&dcbuffer[bufpos], BLOCK_SIZE_OS_QUAD);

    bufpos = (bufpos + BLOCK_SIZE_OS) & (OB_LENGTH - 1);

    // each block overlap FIRipol_N samples into the next (due to impulses not being wrapped around
    // the block edges copy the overlapping samples to the new block position

    if (!bufpos) // only needed if the new bufpos == 0
    {
        __m128 overlap[FIRipol_N >> 2], overlapR[FIRipol_N >> 2];
        const __m128 zero = _mm_setzero_ps();
        for (k = 0; k < (FIRipol_N); k += 4)
        {
            overlap[k >> 2] = _mm_load_ps(&oscbuffer[OB_LENGTH + k]);
            _mm_store_ps(&oscbuffer[k], overlap[k >> 2]);
            _mm_store_ps(&oscbuffer[OB_LENGTH + k], zero);
            if (stereo)
            {
                overlapR[k >> 2] = _mm_load_ps(&oscbufferR[OB_LENGTH + k]);
                _mm_store_ps(&oscbufferR[k], overlapR[k >> 2]);
                _mm_store_ps(&oscbufferR[OB_LENGTH + k], zero);
            }
        }
    }

    applyFilter();
}

void SampleAndHoldOscillator::handleStreamingMismatches(int streamingRevision,
                                                        int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        oscdata->p[shn_lowcut].val.f = oscdata->p[shn_lowcut].val_min.f; // low cut at the bottom
        oscdata->p[shn_lowcut].deactivated = true;
        oscdata->p[shn_highcut].val.f = oscdata->p[shn_highcut].val_max.f; // high cut at the top
        oscdata->p[shn_sync].deactivated = true;
    }
}
