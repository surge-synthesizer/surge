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

double WavetableOscillator::skewHPhaseResponse[WavetableOscillator::SAMPLES_FOR_DISPLAY] = {
    0.00, 0.00, 0.01, 0.02, 0.03, 0.03, 0.04, 0.05, 0.06, 0.07, 0.07, 0.08, 0.09, 0.10, 0.11,
    0.11, 0.12, 0.13, 0.14, 0.15, 0.15, 0.16, 0.17, 0.18, 0.19, 0.19, 0.20, 0.21, 0.22, 0.23,
    0.23, 0.24, 0.25, 0.26, 0.27, 0.27, 0.28, 0.29, 0.30, 0.31, 0.32, 0.32, 0.33, 0.34, 0.35,
    0.36, 0.37, 0.38, 0.39, 0.40, 0.41, 0.43, 0.44, 0.46, 0.49, 0.54, 0.70, 0.84, 0.92, 0.97};

WavetableOscillator::WavetableOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                         pdata *localcopy, pdata *localcopyUnmod)
    : AbstractBlitOscillator(storage, oscdata, localcopy)
{
    unmodulatedLocalcopy = localcopyUnmod;
}

WavetableOscillator::~WavetableOscillator() {}

void WavetableOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    assert(storage);
    readDeformType();

    first_run = true;
    osc_out = SIMD_MM(set1_ps)(0.f);
    osc_outR = SIMD_MM(set1_ps)(0.f);
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

    int playcount = 1;

    if (oscdata->wt.flags & wtf_is_sample)
    {
        if (oscdata->wt.flags & wtf_unison_is_loop_count)
        {
            // The voice count is a play count instead, so the oscillator runs one voice.
            // This is how samples behaved before the flag existed, which is why old patches
            // get it set on load.
            playcount = n_unison;
            n_unison = 1;
        }

        // An explicit loop flag overrides the count rather than adding to it, so a looped
        // sample ignores it entirely
        if (oscdata->wt.flags & wtf_loop_sample)
        {
            playcount = infinite_sampleloop;
        }
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

    float shape;
    float intpart;
    if (deformType == XT_134_EARLIER)
    {
        shape = oscdata->p[wt_morph].val.f;
    }
    else
    {
        shape = getMorph();
    }

    shape *= ((float)oscdata->wt.n_tables - 1.f + nointerp) * 0.99999f;
    float t_ipol = modff(shape, &intpart);
    if (deformType != XT_134_EARLIER)
        t_ipol = shape;
    int t_id = limit_range((int)intpart, 0, std::max((int)oscdata->wt.n_tables - 2 + nointerp, 0));
    // Note last_tableipol deliberately keeps the morph-derived value below even though
    // tableipol is zeroed for a sample, which is what the original scalar code did
    float t_last_ipol = t_ipol;
    last_tableid = t_id;

    selectDeform();

    hskew = 0.f;
    last_hskew = 0.f;

    if (oscdata->wt.flags & wtf_is_sample)
    {
        // Morph is the start point offset for a sample: the frame it lands on is where
        // playback begins, and the -1 is what the first advance in convolute() cancels out
        t_ipol = 0.f;
        t_id -= 1;
    }

    // Broadcast to every voice, not just the sounding ones, so nothing is ever read
    // uninitialized if the unison count changes underneath us
    for (int i = 0; i < MAX_UNISON; i++)
    {
        tableipol[i] = t_ipol;
        last_tableipol[i] = t_last_ipol;
        tableid[i] = t_id;
        sampleloop[i] = playcount;
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
    oscdata->p[wt_morph].set_type(ct_countedset_percent_extendable_wtdeform);
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
    oscdata->p[wt_unison_detune].dynamicDeactivation = &Surge::Oscillator::sampleUnisonDetuneDeact;
    oscdata->p[wt_unison_detune].dynamicName = &Surge::Oscillator::sampleUnisonDynamicName;
    oscdata->p[wt_unison_voices].set_name("Unison Voices");
    oscdata->p[wt_unison_voices].set_type(ct_osccount_or_playcount);
    oscdata->p[wt_unison_voices].dynamicName = &Surge::Oscillator::sampleUnisonDynamicName;
}

void WavetableOscillator::init_default_values()
{
    oscdata->p[wt_morph].val.f = 0.0f;
    oscdata->p[wt_morph].set_extend_range(true);
    oscdata->p[wt_morph].deform_type = FeatureDeform::XT_14;
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

void WavetableOscillator::processSamplesForDisplay(float *samples, int size, bool real)
{
    if (!real)
    {
        // saturate and skewY
        for (int i = 0; i < size; i++)
        {
            samples[i] = distort_level(samples[i]);
        }

        // formant
        if (oscdata->p[wt_formant].val.f > 0.f)
        {
            float mult = pow(2, oscdata->p[wt_formant].val.f * 0.08333333333333333);

            for (int i = 0; i < size; i++)
            {
                float pos = limit_range((float)i * mult, 0.f, (float)size - 1.f);
                int from = floor(pos);
                int to = limit_range(from + 1, 0, size - 1);

                float proc = pos - (float)from;

                samples[i] = samples[from] * (1.f - proc) + samples[to] * proc;
            }
        }

        // skewH hard coded phase response

        float skewH = pow(abs(oscdata->p[wt_skewh].val.f), 0.8);

        if (skewH != 0)
        {
            float samplesTemp[SAMPLES_FOR_DISPLAY];
            for (int i = 0; i < size; i++)
            {
                samplesTemp[i] = samples[i];
            }

            for (int i = 0; i < size; i++)
            {

                float from = i;
                float to = WavetableOscillator::skewHPhaseResponse[i] * (SAMPLES_FOR_DISPLAY - 1);

                if (oscdata->p[wt_skewh].val.f < 0)
                {
                    to = (

                        (float)(SAMPLES_FOR_DISPLAY - 1) -
                        WavetableOscillator::skewHPhaseResponse[(SAMPLES_FOR_DISPLAY - 1) - i] *
                            (SAMPLES_FOR_DISPLAY - 1));
                }

                float fromSampleF = (float)from * (1.0 - skewH) + (float)to * skewH;

                // interpolate
                int fromSample = fromSampleF;
                int toSample = limit_range(fromSample + 1, 0, size - 1);
                float percSample = fromSampleF - fromSample;

                float sample = samplesTemp[fromSample] * (1 - percSample) +
                               samplesTemp[toSample] * (percSample);

                samples[i] = sample;
            }
        }
    }
    else
    {

        // todo populate samples with process_block()
        for (int i = 0; i < size; i++)
        {
            samples[i] = 0;
        }
    }
    // saturation
};

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

        int paddingLoop = 4;
        int paddingEnd = 1;

        if (deformType == XT_134_EARLIER)
        {
            paddingLoop = 3 - nointerp;
            paddingEnd = 2 - nointerp;
        }

        if (oscdata->wt.flags & wtf_is_sample)
        {
            tableid[voice]++;
            if (tableid[voice] > (int)oscdata->wt.n_tables - paddingLoop)
            {
                if (sampleloop[voice] < infinite_sampleloop)
                    sampleloop[voice]--;

                if (sampleloop[voice] > 0)
                {
                    tableid[voice] = 0;
                }
                else
                {
                    tableid[voice] = oscdata->wt.n_tables - paddingEnd;
                    oscstate[voice] = 100000000000.f; // rather large number
                    return;
                }
            }

            if (deformType != XT_134_EARLIER)
            {
                tableipol[voice] = tableid[voice];
                last_tableipol[voice] = tableid[voice];
            }
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
    auto lipol128 = SIMD_MM(setzero_ps)();
    lipol128 = SIMD_MM(cvtsi32_ss)(lipol128, lipolui16);
    lipol128 = SIMD_MM(shuffle_ps)(lipol128, lipol128, SIMD_MM_SHUFFLE(0, 0, 0, 0));

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

    float newlevel;
    newlevel = distort_level((this->*deformSelected)(block_pos, voice));

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
        auto g128L = SIMD_MM(load_ss)(&g);
        g128L = SIMD_MM(shuffle_ps)(g128L, g128L, SIMD_MM_SHUFFLE(0, 0, 0, 0));
        auto g128R = SIMD_MM(load_ss)(&gR);
        g128R = SIMD_MM(shuffle_ps)(g128R, g128R, SIMD_MM_SHUFFLE(0, 0, 0, 0));

        for (int k = 0; k < FIRipol_N; k += 4)
        {
            float *obfL = &oscbuffer[bufpos + k + delay];
            float *obfR = &oscbufferR[bufpos + k + delay];
            auto obL = SIMD_MM(loadu_ps)(obfL);
            auto obR = SIMD_MM(loadu_ps)(obfR);
            auto st = SIMD_MM(load_ps)(&storage->sinctable[m + k]);
            auto so = SIMD_MM(load_ps)(&storage->sinctable[m + k + FIRipol_N]);
            so = SIMD_MM(mul_ps)(so, lipol128);
            st = SIMD_MM(add_ps)(st, so);
            obL = SIMD_MM(add_ps)(obL, SIMD_MM(mul_ps)(st, g128L));
            SIMD_MM(storeu_ps)(obfL, obL);
            obR = SIMD_MM(add_ps)(obR, SIMD_MM(mul_ps)(st, g128R));
            SIMD_MM(storeu_ps)(obfR, obR);
        }
    }
    else
    {
        auto g128 = SIMD_MM(load_ss)(&g);
        g128 = SIMD_MM(shuffle_ps)(g128, g128, SIMD_MM_SHUFFLE(0, 0, 0, 0));

        for (int k = 0; k < FIRipol_N; k += 4)
        {
            float *obf = &oscbuffer[bufpos + k + delay];
            auto ob = SIMD_MM(loadu_ps)(obf);
            auto st = SIMD_MM(load_ps)(&storage->sinctable[m + k]);
            auto so = SIMD_MM(load_ps)(&storage->sinctable[m + k + FIRipol_N]);
            so = SIMD_MM(mul_ps)(so, lipol128);
            st = SIMD_MM(add_ps)(st, so);
            st = SIMD_MM(mul_ps)(st, g128);
            ob = SIMD_MM(add_ps)(ob, st);
            SIMD_MM(storeu_ps)(obf, ob);
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

    if (deformType == XT_134_EARLIER)
    {
        l_shape.newValue(localcopy[id_shape].f);
    }
    else
    {
        l_shape.newValue(unmodulatedLocalcopy[id_shape].f);
    }

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

void WavetableOscillator::readDeformType()
{
    deformType = (FeatureDeform)oscdata->p[wt_morph].deform_type;
}

void WavetableOscillator::selectDeform()
{
    if (deformType == XT_134_EARLIER)
    {
        deformSelected = &WavetableOscillator::deformLegacy;
    }
    else
    {
        deformSelected = &WavetableOscillator::deformContinuous;
    }
}

float WavetableOscillator::getMorph()
{

    float shape;
    if (deformType == XT_134_EARLIER)
    {

        shape = l_shape.v;
    }
    else
    {
        shape = limit_range(l_shape.v + (localcopy[id_shape].f - unmodulatedLocalcopy[id_shape].f),
                            0.f, 1.f);
    }
    return shape;
}
/*
    Interpolation modes
*/
float WavetableOscillator::deformLegacy(float block_pos, int voice)
{
    float tblip_ipol =
        (1 - block_pos) * last_tableipol[voice] + block_pos * tableipol[voice];

    // in Continuous Morph mode tblip_ipol gives us position between current and next frame
    // when not in Continuous Morph mode, we don't interpolate so this position should be
    // zero
    float lipol = (1 - nointerp) * tblip_ipol;

    // that 1 - nointerp makes sure we don't read the table off memory, keeps us bounded
    // and since it gets multiplied by lipol, in morph mode ends up being zero - no sweat!
    return (oscdata->wt.TableF32WeakPointers[mipmap[voice]][tableid[voice]][state[voice]] *
            (1.f - lipol)) +
           (oscdata->wt
                .TableF32WeakPointers[mipmap[voice]][tableid[voice] + 1 - nointerp][state[voice]] *
            lipol);
}

float WavetableOscillator::deformContinuous(float block_pos, int voice)
{
    block_pos = nointerp ? 1 : block_pos;
    float tblip_ipol =
        (1 - block_pos) * last_tableipol[voice] + block_pos * tableipol[voice];

    int tempTableId = floor(tblip_ipol);
    int targetTableId = min((int)(tempTableId + 1), (int)(oscdata->wt.n_tables - 1));

    float interpolationProc = (tblip_ipol - tempTableId) * (1 - nointerp);

    return (oscdata->wt.TableF32WeakPointers[mipmap[voice]][tempTableId][state[voice]] *
            (1.f - interpolationProc)) +
           (oscdata->wt.TableF32WeakPointers[mipmap[voice]][targetTableId][state[voice]] *
            interpolationProc);
}

float WavetableOscillator::deformMorph(float block_pos, int voice)
{

    float frames[2] = {(last_tableipol[voice]), (tableipol[voice])};
    for (int i = 0; i < 2; i++)
    {
        int actualFrame = floor(frames[i]);
        float proc = frames[i] - floor(frames[i]);

        int d = min((int)(actualFrame + 1), (int)(oscdata->wt.n_tables - 1));
        frames[i] = oscdata->wt.TableF32WeakPointers[mipmap[voice]][actualFrame][state[voice]] *
                        (1.f - proc) +
                    oscdata->wt.TableF32WeakPointers[mipmap[voice]][d][state[voice]] * (proc);
    }

    return frames[0] * (1.f - block_pos) + frames[1] * block_pos;
}

void WavetableOscillator::process_block(float pitch0, float drift, bool stereo, bool FM,
                                        float depth)
{

#if 0
    if (fd == XT_134_EARLIER)
    {
        std::cout << "OLD WAY" << std::endl;
    }
    else
    {
        std::cout << "NEW WAY" << std::endl;
    }
#endif

    readDeformType();

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

    // TableID-range may have changed in the meantime, check it! In sample mode the voices
    // carry independent frame indices, so every sounding one has to be checked and reset,
    // not just the first: a voice left pointing past a table that shrank underneath us
    // reads out of bounds in the deform functions.
    //
    // The cast to int matters. n_tables is unsigned, so the old comparison promoted the
    // frame index and a sample sitting at the -1 that init() seeds - which is what the
    // first advance in convolute() cancels out - looked enormous and got reset to 0 before
    // it ever played. That skipped frame 0 whenever Morph put the start point there.
    bool outOfRange = (oscdata->wt.n_tables == 1);

    for (int i = 0; i < n_unison && !outOfRange; i++)
    {
        outOfRange = (tableid[i] >= (int)oscdata->wt.n_tables);
    }

    if (outOfRange)
    {
        last_tableid = 0;

        for (int i = 0; i < MAX_UNISON; i++)
        {
            tableipol[i] = 0.f;
            last_tableipol[i] = 0.f;
            tableid[i] = 0;
        }
    }
    else if (oscdata->wt.flags & wtf_is_sample)
    {
        for (int i = 0; i < n_unison; i++)
        {
            if (deformType == XT_134_EARLIER)
            {
                tableipol[i] = 0.f;
                last_tableipol[i] = 0.f;
            }
            else
            {
                tableipol[i] = tableid[i];
                last_tableipol[i] = tableid[i];
            }
        }
    }
    else
    {
        // Morph mode is identical for every voice, so work it out once on locals and
        // broadcast. Doing it this way rather than having the deform functions pick an
        // index keeps the branch out of the per-sample read in convolute().
        float t_last_ipol = tableipol[0];
        last_tableid = tableid[0];

        float shape;
        float intpart;

        shape = getMorph();

        shape *= ((float)oscdata->wt.n_tables - 1.f + nointerp) * 0.99999f;
        float t_ipol = deformType == XT_134_EARLIER ? modff(shape, &intpart) : shape;
        modff(shape, &intpart);
        int t_id = limit_range((int)intpart, 0, (int)oscdata->wt.n_tables - 2 + nointerp);

        selectDeform();

        if (deformType == XT_134_EARLIER)
        {
            if (t_id > last_tableid)
            {
                if (t_last_ipol != 1.f)
                {
                    t_id = last_tableid;
                    t_ipol = 1.f;
                }
                else
                    t_last_ipol = 0.0f;
            }
            else if (t_id < last_tableid)
            {
                if (t_last_ipol != 0.f)
                {
                    t_id = last_tableid;
                    t_ipol = 0.f;
                }
                else
                    t_last_ipol = 1.0f;
            }
        }

        for (int i = 0; i < MAX_UNISON; i++)
        {
            tableipol[i] = t_ipol;
            last_tableipol[i] = t_last_ipol;
            tableid[i] = t_id;
        }
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
        auto hpf = SIMD_MM(load_ss)(&hpfblock[k]);
        auto ob = SIMD_MM(load_ss)(&oscbuffer[bufpos + k]);
        auto a = SIMD_MM(mul_ss)(osc_out, hpf);
        osc_out = SIMD_MM(add_ss)(a, ob);
        SIMD_MM(store_ss)(&output[k], osc_out);

        if (stereo)
        {
            auto ob = SIMD_MM(load_ss)(&oscbufferR[bufpos + k]);
            auto a = SIMD_MM(mul_ss)(osc_outR, hpf);
            osc_outR = SIMD_MM(add_ss)(a, ob);
            SIMD_MM(store_ss)(&outputR[k], osc_outR);
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
        SIMD_M128 overlap[FIRipol_N >> 2], overlapR[FIRipol_N >> 2];
        const auto zero = SIMD_MM(setzero_ps)();
        for (int k = 0; k < (FIRipol_N); k += 4)
        {
            overlap[k >> 2] = SIMD_MM(load_ps)(&oscbuffer[OB_LENGTH + k]);
            SIMD_MM(store_ps)(&oscbuffer[k], overlap[k >> 2]);
            SIMD_MM(store_ps)(&oscbuffer[OB_LENGTH + k], zero);
            if (stereo)
            {
                overlapR[k >> 2] = SIMD_MM(load_ps)(&oscbufferR[OB_LENGTH + k]);
                SIMD_MM(store_ps)(&oscbufferR[k], overlapR[k >> 2]);
                SIMD_MM(store_ps)(&oscbufferR[OB_LENGTH + k], zero);
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
    if (streamingRevision <= 25)
    {
        oscdata->p[wt_morph].deform_type = (int)FeatureDeform::XT_134_EARLIER;
    }
}
