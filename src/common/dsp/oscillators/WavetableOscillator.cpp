/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "WavetableOscillator.h"
#include "DSPUtils.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
namespace mech = sst::basic_blocks::mechanics;

using namespace std;

const float hpf_cycle_loss = 0.99f;

WavetableOscillator::WavetableOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                         pdata *localcopy)
    : AbstractBlitOscillator(storage, oscdata, localcopy)
{
}

WavetableOscillator::~WavetableOscillator() {}

void WavetableOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    assert(storage);
    first_run = true;
    osc_out = _mm_set1_ps(0.f);
    osc_outR = _mm_set1_ps(0.f);
    bufpos = 0;

    id_shape = oscdata->p[wt_morph].param_id_in_scene;
    id_vskew = oscdata->p[wt_skewv].param_id_in_scene;
    id_clip = oscdata->p[wt_saturate].param_id_in_scene;
    id_formant = oscdata->p[wt_formant].param_id_in_scene;
    id_hskew = oscdata->p[wt_skewh].param_id_in_scene;
    id_detune = oscdata->p[wt_unison_detune].param_id_in_scene;

    float rate = 0.05;
    l_shape.setRate(rate);
    l_clip.setRate(rate);
    l_vskew.setRate(rate);
    l_hskew.setRate(rate);

    n_unison = limit_range(oscdata->p[wt_unison_voices].val.i, 1, MAX_UNISON);

    if (oscdata->wt.flags & wtf_is_sample)
    {
        sampleloop = n_unison;
        n_unison = 1;
    }

    if (is_display)
        n_unison = 1;

    prepare_unison(n_unison);

    memset(oscbuffer, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(oscbufferR, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(last_level, 0, MAX_UNISON * sizeof(float));

    pitch_last = pitch;
    pitch_t = pitch;
    update_lagvals<true>();

    // nointerp adjusts the tableid range so that it scans the whole wavetable
    // rather than wavetable from first to second to last frame
    nointerp = !oscdata->p[wt_morph].extend_range;

    float shape = l_shape.v;

    float intpart;
    shape *= ((float)oscdata->wt.n_tables - 1.f + nointerp) * 0.99999f;
    tableipol = shape;
    modff(shape, &intpart);
    tableid = limit_range((int)intpart, 0, std::max((int)oscdata->wt.n_tables - 2 + nointerp, 0));
    last_tableipol = tableipol;
    last_tableid = tableid;
    hskew = 0.f;
    last_hskew = 0.f;

    if (oscdata->wt.flags & wtf_is_sample)
    {
        tableipol = 0.f;
        tableid -= 1;
    }

    for (int i = 0; i < n_unison; i++)
    {
        {
            if (oscdata->retrigger.val.b || is_display)
            {
                oscstate[i] = 0.f;
            }
            else
            {
                float drand = storage->rand_01();
                oscstate[i] = drand;
            }

            state[i] = 0;
        }

        last_level[i] = 0.0;
        mipmap[i] = 0;
        mipmap_ofs[i] = 0;
        driftLFO[i].init(nonzero_init_drift);
    }
}

void WavetableOscillator::init_ctrltypes()
{
    oscdata->p[wt_morph].set_name("Morph");
    oscdata->p[wt_morph].set_type(ct_countedset_percent_extendable);
    oscdata->p[wt_morph].set_user_data(oscdata);
    oscdata->p[wt_skewv].set_name("Skew Vertical");
    oscdata->p[wt_skewv].set_type(ct_percent_bipolar);
    oscdata->p[wt_saturate].set_name("Saturate");
    oscdata->p[wt_saturate].set_type(ct_percent);
    oscdata->p[wt_formant].set_name("Formant");
    oscdata->p[wt_formant].set_type(ct_syncpitch);
    oscdata->p[wt_skewh].set_name("Skew Horizontal");
    oscdata->p[wt_skewh].set_type(ct_percent_bipolar);
    oscdata->p[wt_unison_detune].set_name("Unison Detune");
    oscdata->p[wt_unison_detune].set_type(ct_oscspread);
    oscdata->p[wt_unison_voices].set_name("Unison Voices");
    oscdata->p[wt_unison_voices].set_type(ct_osccount);
}

void WavetableOscillator::init_default_values()
{
    oscdata->p[wt_morph].val.f = 0.0f;
    oscdata->p[wt_morph].set_extend_range(true);
    oscdata->p[wt_skewv].val.f = 0.0f;
    oscdata->p[wt_saturate].val.f = 0.f;
    oscdata->p[wt_formant].val.f = 0.f;
    oscdata->p[wt_skewh].val.f = 0.f;
    oscdata->p[wt_unison_detune].val.f = 0.1f;
    oscdata->p[wt_unison_voices].val.i = 1;
}

float WavetableOscillator::distort_level(float x)
{
    float a = l_vskew.v * 0.5;
    float clip = l_clip.v;

    x = x - a * x * x + a;

    x = limit_range(x * (1 - clip) + clip * x * x * x, -1.f, 1.f);

    return x;
}

void WavetableOscillator::convolute(int voice, bool FM, bool stereo)
{
    float block_pos = oscstate[voice] * BLOCK_SIZE_OS_INV * pitchmult_inv;

    double detune = drift * driftLFO[voice].val();
    if (n_unison > 1)
        detune += oscdata->p[wt_unison_detune].get_extended(localcopy[id_detune].f) *
                  (detune_bias * float(voice) + detune_offset);

    const float p24 = (1 << 24);
    unsigned int ipos;

    if (FM)
        ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv * FMmul_inv));
    else
        ipos = (unsigned int)((float)p24 * (oscstate[voice] * pitchmult_inv));

    if (state[voice] == 0)
    {
        formant_last = formant_t;
        last_hskew = hskew;
        hskew = l_hskew.v;

        if (oscdata->wt.flags & wtf_is_sample)
        {
            tableid++;
            if (tableid > oscdata->wt.n_tables - 4)
            {
                if (sampleloop < 7)
                    sampleloop--;

                if (sampleloop > 0)
                {
                    tableid = 0;
                }
                else
                {
                    tableid = oscdata->wt.n_tables - 1;
                    oscstate[voice] = 100000000000.f; // rather large number
                    return;
                }
            }

            tableipol = tableid;
            last_tableipol = tableid;
        }

        int ts = oscdata->wt.size;
        float a = oscdata->wt.dt * pitchmult_inv;

        const float wtbias = 1.8f;

        mipmap[voice] = 0;

        if ((a < 0.015625 * wtbias) && (ts >= 128))
            mipmap[voice] = 6;
        else if ((a < 0.03125 * wtbias) && (ts >= 64))
            mipmap[voice] = 5;
        else if ((a < 0.0625 * wtbias) && (ts >= 32))
            mipmap[voice] = 4;
        else if ((a < 0.125 * wtbias) && (ts >= 16))
            mipmap[voice] = 3;
        else if ((a < 0.25 * wtbias) && (ts >= 8))
            mipmap[voice] = 2;
        else if ((a < 0.5 * wtbias) && (ts >= 4))
            mipmap[voice] = 1;

        mipmap_ofs[voice] = 0;
        for (int i = 0; i < mipmap[voice]; i++)
            mipmap_ofs[voice] += (ts >> i);
    }

    // generate pulse
    unsigned int delay = ((ipos >> 24) & 0x3f);

    if (FM)
        delay = FMdelay;

    unsigned int m = ((ipos >> 16) & 0xff) * (FIRipol_N << 1);
    unsigned int lipolui16 = (ipos & 0xffff);
    __m128 lipol128 = _mm_cvtsi32_ss(lipol128, lipolui16);
    lipol128 = _mm_shuffle_ps(lipol128, lipol128, _MM_SHUFFLE(0, 0, 0, 0));

    float g, gR;
    int wt_inc = (1 << mipmap[voice]);
    float dt = (oscdata->wt.dt) * wt_inc;

    // add time until next statechange
    float tempt;
    if (oscdata->p[wt_unison_detune].absolute)
    {
        // See the comment in ClassicOscillator.cpp at the absolute treatment
        tempt = storage->note_to_pitch_inv_ignoring_tuning(
            detune * storage->note_to_pitch_inv_ignoring_tuning(pitch_t) * 16 / 0.9443);
        if (tempt < 0.1)
            tempt = 0.1;
    }
    else
    {
        tempt = storage->note_to_pitch_inv_tuningctr(detune);
    }

    float t;
    float xt = ((float)state[voice] + 0.5f) * dt;
    // xt = (1 - hskew + 2*hskew*xt);
    // xt = (1 + hskew *sin(xt*2.0*M_PI));
    // 1 + a.*(1 - 2.*x + (2.*x-1).^3).*sqrt(27/4) = 1 + 4*x*a*(x-1)*(2x-1)
    const float taylorscale = sqrt((float)27.f / 4.f);
    xt = 1.f + hskew * 4.f * xt * (xt - 1.f) * (2.f * xt - 1.f) * taylorscale;

    // t = dt * tempt;
    /*while (t<0.5 && (wt_inc < wavetable_steps))
    {
            wt_inc = wt_inc << 1;
            t = dt * tempt * wt_inc;
    }	*/
    float ft = block_pos * formant_t + (1.f - block_pos) * formant_last;
    float formant = storage->note_to_pitch_tuningctr(-ft);
    dt *= formant * xt;

    int wtsize = oscdata->wt.size >> mipmap[voice];

    if (state[voice] >= (wtsize - 1))
        dt += (1 - formant);
    t = dt * tempt;

    state[voice] = state[voice] & (wtsize - 1);

    float tblip_ipol = (1 - block_pos) * last_tableipol + block_pos * tableipol;
    float newlevel;

    // in Continuous Morph mode tblip_ipol gives us position between current and next frame
    int tempTableId = floor(tblip_ipol);
    float interpolationProc = (tblip_ipol - tempTableId) * (1 - nointerp);

    // that 1 - nointerp makes sure we don't read the table off memory, keeps us bounded
    // and since it gets multiplied by lipol, in morph mode ends up being zero - no sweat!
    newlevel = distort_level(
        (oscdata->wt.TableF32WeakPointers[mipmap[voice]][tempTableId][state[voice]] *
         (1.f - interpolationProc)) +
        (oscdata->wt.TableF32WeakPointers[mipmap[voice]][tempTableId + 1 - nointerp][state[voice]] *
         interpolationProc));

    g = newlevel - last_level[voice];
    last_level[voice] = newlevel;

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

        for (int k = 0; k < FIRipol_N; k += 4)
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

        for (int k = 0; k < FIRipol_N; k += 4)
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

    rate[voice] = t;

    oscstate[voice] += rate[voice];
    oscstate[voice] = max(0.f, oscstate[voice]);
    state[voice] = (state[voice] + 1) & ((oscdata->wt.size >> mipmap[voice]) - 1);
}

template <bool is_init> void WavetableOscillator::update_lagvals()
{
    l_vskew.newValue(limit_range(localcopy[id_vskew].f, -1.f, 1.f));
    l_hskew.newValue(limit_range(localcopy[id_hskew].f, -1.f, 1.f));
    float a = limit_range(localcopy[id_clip].f, 0.f, 1.f);
    l_clip.newValue(-8 * a * a * a);
    l_shape.newValue(limit_range(localcopy[id_shape].f, 0.f, 1.f));
    formant_t = max(0.f, localcopy[id_formant].f);

    float invt = min(1.0, (8.175798915 * storage->note_to_pitch_tuningctr(pitch_t)) *
                              storage->dsamplerate_os_inv);
    // TODO: Make a lookup table
    float hpf2 = min(integrator_hpf, powf(hpf_cycle_loss, 4 * invt));

    hpf_coeff.newValue(hpf2);
    integrator_mult.newValue(invt);

    li_hpf.set_target(hpf2);

    if (is_init)
    {
        hpf_coeff.instantize();
        integrator_mult.instantize();
        l_shape.instantize();
        l_vskew.instantize();
        l_hskew.instantize();
        l_clip.instantize();

        formant_last = formant_t;
    }
}

void WavetableOscillator::process_block(float pitch0, float drift, bool stereo, bool FM,
                                        float depth)
{
    pitch_last = pitch_t;
    pitch_t = min(148.f, pitch0);
    pitchmult_inv =
        max(1.0, storage->dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch_t));
    pitchmult = 1.f / pitchmult_inv; // This must be a real division, reciprocal-approximation is
                                     // not precise enough
    this->drift = drift;
    nointerp = !oscdata->p[wt_morph].extend_range;

    update_lagvals<false>();
    l_shape.process();
    l_vskew.process();
    l_hskew.process();
    l_clip.process();

    if ((oscdata->wt.n_tables == 1) ||
        (tableid >=
         oscdata->wt.n_tables)) // TableID-range may have changed in the meantime, check it!
    {
        tableipol = 0.f;
        tableid = 0;
        last_tableid = 0;
        last_tableipol = 0.f;
    }
    else if (oscdata->wt.flags & wtf_is_sample)
    {
        tableipol = tableid;
        last_tableipol = tableid;
    }
    else
    {
        last_tableipol = tableipol;
        last_tableid = tableid;

        float shape = l_shape.v;
        float intpart;
        shape *= ((float)oscdata->wt.n_tables - 1.f + nointerp) * 0.99999f;
        tableipol = shape;
        modff(shape, &intpart);
        tableid = limit_range((int)intpart, 0, (int)oscdata->wt.n_tables - 2 + nointerp);
        /*
        if (tableid > last_tableid)
        {
            if (last_tableipol != 1.f)
            {
                tableid = last_tableid;
                tableipol = 1.f;
            }
            else
                last_tableipol = 0.0f;
        }
        else if (tableid < last_tableid)
        {
            if (last_tableipol != 0.f)
            {
                tableid = last_tableid;
                tableipol = 0.f;
            }
            else
                last_tableipol = 1.0f;
        }
        */
    }

    if (FM)
    {
        for (int l = 0; l < n_unison; l++)
        {
            driftLFO[l].next();
        }

        for (int s = 0; s < BLOCK_SIZE_OS; s++)
        {
            float fmmul = limit_range(1.f + depth * master_osc[s], 0.1f, 1.9f);
            float a = pitchmult * fmmul;
            FMdelay = s;

            for (int l = 0; l < n_unison; l++)
            {
                while (oscstate[l] < a)
                {
                    FMmul_inv = mech::rcp(fmmul);
                    convolute(l, true, stereo);
                }

                oscstate[l] -= a;
            }
        }
    }
    else
    {
        float a = (float)BLOCK_SIZE_OS * pitchmult;
        for (int l = 0; l < n_unison; l++)
        {
            driftLFO[l].next();
            while (oscstate[l] < a)
                convolute(l, false, stereo);
            oscstate[l] -= a;
        }
    }

    float hpfblock alignas(16)[BLOCK_SIZE_OS];
    li_hpf.store_block(hpfblock, BLOCK_SIZE_OS_QUAD);

    for (int k = 0; k < BLOCK_SIZE_OS; k++)
    {
        __m128 hpf = _mm_load_ss(&hpfblock[k]);
        __m128 ob = _mm_load_ss(&oscbuffer[bufpos + k]);
        __m128 a = _mm_mul_ss(osc_out, hpf);
        osc_out = _mm_add_ss(a, ob);
        _mm_store_ss(&output[k], osc_out);

        if (stereo)
        {
            __m128 ob = _mm_load_ss(&oscbufferR[bufpos + k]);
            __m128 a = _mm_mul_ss(osc_outR, hpf);
            osc_outR = _mm_add_ss(a, ob);
            _mm_store_ss(&outputR[k], osc_outR);
        }
    }

    mech::clear_block<BLOCK_SIZE_OS>(&oscbuffer[bufpos]);
    if (stereo)
        mech::clear_block<BLOCK_SIZE_OS>(&oscbufferR[bufpos]);

    bufpos = (bufpos + BLOCK_SIZE_OS) & (OB_LENGTH - 1);

    // each block overlap FIRipol_N samples into the next (due to impulses not being wrapped around
    // the block edges copy the overlapping samples to the new block position

    if (!bufpos) // only needed if the new bufpos == 0
    {
        __m128 overlap[FIRipol_N >> 2], overlapR[FIRipol_N >> 2];
        const __m128 zero = _mm_setzero_ps();
        for (int k = 0; k < (FIRipol_N); k += 4)
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
}

void WavetableOscillator::handleStreamingMismatches(int streamingRevision,
                                                    int currentSynthStreamingRevision)
{
    if (streamingRevision <= 16)
    {
        oscdata->p[wt_morph].set_extend_range(true);
    }
}
