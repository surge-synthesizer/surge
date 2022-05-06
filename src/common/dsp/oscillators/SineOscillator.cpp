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

#include "SineOscillator.h"
#include "FastMath.h"
#include <algorithm>

/*
 * Sine Oscillator Optimization Strategy
 *
 * With Surge 1.9, we undertook a bunch of work to optimize the sine oscillator runtime at high
 * unison count with odd shapes. Basically at high unison we were doing large numbers of loops,
 * branches and so forth, and not using any of the advantage you could get by realizing the paralle
 * structure of unison. So we fixed that.
 *
 * There's two core fixes.
 *
 * First, and you shouldn't need to touch this, the inner unison loop of ::process is now SSEified
 * over unison. This means that we use parallel approximations of sine, we use parallel clamps and
 * feedback application, the whole nine yards. You can see it in process_block_internal.
 *
 * But that's not all. The other key trick is that the shape modes added a massive amount of
 * switching to the execution path. So we eliminated that completely. We did that with two tricks
 *
 * 1: Mode is a template applied at block level so there's no ifs inside the block
 * 2: When possible, shape generation is coded as an SSE instruction.
 *
 * So #1 just means that process_block_internal has a <mode> template as do many other operators
 * and we use template specialization. We are specializing the function
 *
 *   __m128 valueFromSineAndCosForMode<mode>(__m128 s, __m128 c, int maxc  )
 *
 * The default impleementation of this function calls
 *
 *    float valueForSineAndCosForModeAsScalar<mode>(s, c)
 *
 * on each SSE point in an unrolled SSE loop. But we also specialize it for many of the modes.
 *
 * The the original api point SineOscillator::valueFromSineAndCos (which is now only called by
 * external APIs) calls into the appropriately templated function. Since our loop doesn't use that
 * though we end up with our switch compile time inlined.
 *
 * So if you want to add a mode what does this mean? Well go update Parameter.cpp to extend the
 * max of mode of course. But then you come back here and do the following
 *
 * 1. Assume that you know how to write your new mode as a scalar function. After the
 * declaration of valueFromSineAndCosForMode as a template, specialize and put your scalar
 * implementation in there. Add your mode to th switch statement in valueFromSinAndCos
 * and into the DOCASE switch statement in process_block. Compile, run, test.
 *
 * 2. Then if that works, and you can code up your mode as an SSE function, remove that
 * specialization and instead specialize the mode in the SSE-named function.
 *
 * Should all be pretty clear.
 */

SineOscillator::SineOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage), charFilt(storage)
{
}

void SineOscillator::prepare_unison(int voices)
{
    auto us = Surge::Oscillator::UnisonSetup<float>(voices);

    out_attenuation_inv = us.attenuation_inv();
    ;
    out_attenuation = 1.0f / out_attenuation_inv;

    detune_bias = us.detuneBias();
    detune_offset = us.detuneOffset();
    for (int v = 0; v < voices; ++v)
    {
        us.panLaw(v, panL[v], panR[v]);
    }

    // normalize to be sample rate independent amount of time for 50 44.1k samples
    dplaying = 1.0 / 50.0 * 44100 / storage->samplerate;
    playingramp[0] = 1;
    for (int i = 1; i < voices; ++i)
        playingramp[i] = 0;
}

void SineOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    n_unison = limit_range(oscdata->p[sine_unison_voices].val.i, 1, MAX_UNISON);

    if (is_display)
    {
        n_unison = 1;
    }

    prepare_unison(n_unison);

    for (int i = 0; i < n_unison; i++)
    {
        phase[i] = // phase in range -PI to PI
            (oscdata->retrigger.val.b || is_display) ? 0.f : 2.0 * M_PI * storage->rand_01() - M_PI;
        lastvalue[i] = 0.f;
        driftLFO[i].init(nonzero_init_drift);
        sine[i].set_phase(phase[i]);
    }

    firstblock = (oscdata->retrigger.val.b || is_display);

    fb_val = 0.f;

    id_mode = oscdata->p[sine_shape].param_id_in_scene;
    id_fb = oscdata->p[sine_feedback].param_id_in_scene;
    id_fmlegacy = oscdata->p[sine_FMmode].param_id_in_scene;
    id_detune = oscdata->p[sine_unison_detune].param_id_in_scene;

    hp.coeff_instantize();
    lp.coeff_instantize();

    hp.coeff_HP(hp.calc_omega(oscdata->p[sine_lowcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
    lp.coeff_LP2B(lp.calc_omega(oscdata->p[sine_highcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);

    charFilt.init(storage->getPatch().character.val.i);
}

SineOscillator::~SineOscillator() {}

inline int calcquadrant(float sinx, float cosx)
{
    int sxl0 = (sinx <= 0);
    int cxl0 = (cosx <= 0);

    // quadrant
    // 1: sin > cos > so this is 0 + 0 - 0 + 1 = 1
    // 2: sin > cos < so this is 0 + 1 - 0 + 1 = 2
    // 3: sin < cos < so this is 3 + 1 - 2 + 1 = 3
    // 4: sin < cos > so this is 3 + 0 - 0 + 1 = 4
    int quadrant = 3 * sxl0 + cxl0 - 2 * sxl0 * cxl0 + 1;
    return quadrant;
}

inline int calcquadrant0(float sinx, float cosx)
{
    int sxl0 = (sinx <= 0);
    int cxl0 = (cosx <= 0);

    // quadrant
    // 1: sin > cos > so this is 0 + 0 - 0 = 0
    // 2: sin > cos < so this is 0 + 1 - 0 = 1
    // 3: sin < cos < so this is 3 + 1 - 2 = 2
    // 4: sin < cos > so this is 3 + 0 - 0 = 3
    int quadrant = 3 * sxl0 + cxl0 - 2 * sxl0 * cxl0;
    return quadrant;
}

inline __m128 calcquadrantSSE(__m128 sinx, __m128 cosx)
{
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1), m2 = _mm_set1_ps(2), m3 = _mm_set1_ps(3);
    auto slt = _mm_and_ps(_mm_cmple_ps(sinx, mz), m1);
    auto clt = _mm_and_ps(_mm_cmple_ps(cosx, mz), m1);

    // quadrant
    // 1: sin > cos > so this is 0 + 0 - 0 + 1 = 1
    // 2: sin > cos < so this is 0 + 1 - 0 + 1 = 2
    // 3: sin < cos < so this is 3 + 1 - 2 + 1 = 3
    // 4: sin < cos > so this is 3 + 0 - 0 + 1 = 4
    // int quadrant = 3 * sxl0 + cxl0 - 2 * sxl0 * cxl0 + 1;
    auto thsx = _mm_mul_ps(m3, slt);
    auto twsc = _mm_mul_ps(m2, _mm_mul_ps(slt, clt));
    auto r = _mm_add_ps(_mm_add_ps(thsx, clt), _mm_sub_ps(m1, twsc));
    return r;
}
/*
 * If you have a model you can't write as SSE and only as scalar, you can
 * specialize this
 */
template <int mode> inline float valueFromSinAndCosForModeAsScalar(float s, float c) { return 0; }

/*
 * Otherwise, specialize this as an SSE operation. We have specialized the
 * SSE version for every case to date.
 */
template <int mode>
inline __m128 valueFromSinAndCosForMode(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    float res alignas(16)[4];
    float sv alignas(16)[4], cv alignas(16)[4];
    _mm_store_ps(sv, svaluesse);
    _mm_store_ps(cv, cvaluesse);
    for (int i = 0; i < maxc; ++i)
        res[i] = valueFromSinAndCosForModeAsScalar<mode>(sv[i], cv[i]);
    return _mm_load_ps(res);
}

template <> inline __m128 valueFromSinAndCosForMode<0>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // mode zero is just sine obviously
    return svaluesse;
}

template <> inline __m128 valueFromSinAndCosForMode<1>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    /*
    switch (quadrant)
    {
    case 1:  pvalue = 1 - cosx;
    case 2: pvalue = 1 + cosx;
    case 3: pvalue = -1 - cosx;
     case 4: pvalue = -1 + cosx;
    }
     */
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto mm1 = _mm_set1_ps(-1);

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto sw = _mm_sub_ps(_mm_and_ps(h1, m1), _mm_andnot_ps(h1, m1)); // this is now 1 1 -1 -1

    auto q24 = _mm_cmplt_ps(_mm_mul_ps(svaluesse, cvaluesse), mz);
    auto pm = _mm_sub_ps(_mm_and_ps(q24, m1), _mm_andnot_ps(q24, m1)); // this is -1 1 -1 1
    return _mm_add_ps(sw, _mm_mul_ps(pm, cvaluesse));
}

template <> inline __m128 valueFromSinAndCosForMode<2>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // First half of sine.
    const auto mz = _mm_setzero_ps();
    return _mm_and_ps(svaluesse, _mm_cmpge_ps(svaluesse, mz));
}

template <> inline __m128 valueFromSinAndCosForMode<3>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // 1 -/+ cosx in first half only
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto mm1 = _mm_set1_ps(-1);
    const auto m2 = _mm_set1_ps(2);

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto q2 = _mm_and_ps(h1, _mm_cmple_ps(cvaluesse, mz));

    auto fh = _mm_and_ps(h1, m1);
    auto cx =
        _mm_mul_ps(fh, _mm_mul_ps(cvaluesse, _mm_add_ps(mm1, _mm_mul_ps(m2, _mm_and_ps(q2, m1)))));

    // return _mm_and_ps(svaluesse, _mm_cmpge_ps(svaluesse, mz));
    return _mm_add_ps(fh, cx);
}

template <> inline __m128 valueFromSinAndCosForMode<4>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Sine 2x in first half
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    return _mm_and_ps(s2x, _mm_cmpge_ps(svaluesse, mz));
}

template <> inline __m128 valueFromSinAndCosForMode<5>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // This is basically a double frequency shape 0 in the first half only
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto m2 = _mm_set1_ps(2);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    auto c2x = _mm_sub_ps(m1, _mm_mul_ps(m2, _mm_mul_ps(svaluesse, svaluesse)));

    auto v1 = valueFromSinAndCosForMode<1>(s2x, c2x, maxc);
    return _mm_and_ps(_mm_cmpge_ps(svaluesse, mz), v1);
}

template <> inline __m128 valueFromSinAndCosForMode<6>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // abs(Sine 2x in first half
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    auto s2fh = _mm_and_ps(s2x, _mm_cmpge_ps(svaluesse, mz));
    return abs_ps(s2fh);
}

template <> inline __m128 valueFromSinAndCosForMode<7>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    return abs_ps(valueFromSinAndCosForMode<5>(svaluesse, cvaluesse, maxc));
}

template <> inline __m128 valueFromSinAndCosForMode<8>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // 2 * First half of sin - 1
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);
    const auto m1 = _mm_set1_ps(1);
    auto fhs = _mm_and_ps(svaluesse, _mm_cmpge_ps(svaluesse, mz));
    return _mm_sub_ps(_mm_mul_ps(m2, fhs), m1);
}

template <> inline __m128 valueFromSinAndCosForMode<9>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // zero out quadrant 1 and 3 which are quadrants where C and S have the same sign
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);
    const auto m1 = _mm_set1_ps(1);
    auto css = _mm_mul_ps(svaluesse, cvaluesse);
    return _mm_and_ps(svaluesse, _mm_cmple_ps(css, mz));
}

template <>
inline __m128 valueFromSinAndCosForMode<10>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // zero out quadrant 2 and 3 which are quadrants where C and S have the different sign
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);
    const auto m1 = _mm_set1_ps(1);
    auto css = _mm_mul_ps(svaluesse, cvaluesse);
    return _mm_and_ps(svaluesse, _mm_cmpge_ps(css, mz));
}

template <>
inline __m128 valueFromSinAndCosForMode<11>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    auto q = valueFromSinAndCosForMode<3>(svaluesse, cvaluesse, maxc);
    const auto m2 = _mm_set1_ps(2);
    const auto m1 = _mm_set1_ps(1);
    return _mm_sub_ps(_mm_mul_ps(m2, q), m1);
}

template <>
inline __m128 valueFromSinAndCosForMode<12>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Flip sign of sin2x in quadrant 2 or 3 (when cosine is negative)
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);
    const auto m1 = _mm_set1_ps(1);
    const auto mm1 = _mm_set1_ps(-1);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));

    auto q23 = _mm_cmpge_ps(cvaluesse, mz);
    auto mul = _mm_add_ps(_mm_and_ps(q23, m1), _mm_andnot_ps(q23, mm1));
    return _mm_mul_ps(s2x, mul);
}

template <>
inline __m128 valueFromSinAndCosForMode<13>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Flip sign of sin2x in quadrant 3  (when sine is negative)
    // and zero it in quadrant 2 and 4 (when sine and cos have different signs or s2x is negative)
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);
    const auto m1 = _mm_set1_ps(1);
    const auto mm1 = _mm_set1_ps(-1);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));

    auto q13 = _mm_cmpge_ps(s2x, mz);
    auto q2 = _mm_cmple_ps(svaluesse, mz);
    auto signflip = _mm_sub_ps(m1, _mm_and_ps(q2, m2));

    return _mm_and_ps(q13, _mm_mul_ps(signflip, s2x));
}

template <>
inline __m128 valueFromSinAndCosForMode<14>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // abs of cos2x in the first half
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto m2 = _mm_set1_ps(2);

    auto c2x = _mm_sub_ps(m1, _mm_mul_ps(m2, _mm_mul_ps(svaluesse, svaluesse)));
    auto q23 = _mm_cmpge_ps(svaluesse, mz);
    return _mm_and_ps(q23, abs_ps(c2x));
}

template <>
inline __m128 valueFromSinAndCosForMode<15>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // 1 - sinx in quadrant 1, -1-sinx in quadrant 4, zero otherwise

    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto mm1 = _mm_set1_ps(-1);

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto sig = _mm_add_ps(_mm_and_ps(h1, _mm_sub_ps(m1, svaluesse)),
                          _mm_andnot_ps(h1, _mm_sub_ps(mm1, svaluesse)));

    auto q14 = _mm_cmpge_ps(cvaluesse, mz);
    return _mm_and_ps(q14, sig);
}

template <>
inline __m128 valueFromSinAndCosForMode<16>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // 1 - sinx in quadrant 1, cosx - 1 in quadrant 4, zero otherwise

    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto sig = _mm_add_ps(_mm_and_ps(h1, _mm_sub_ps(m1, svaluesse)),
                          _mm_andnot_ps(h1, _mm_sub_ps(cvaluesse, m1)));

    auto q14 = _mm_cmpge_ps(cvaluesse, mz);
    return _mm_and_ps(q14, sig);
}

template <>
inline __m128 valueFromSinAndCosForMode<17>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // 1-sinx in 1,2; -1-sinx in 3,4
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto sw = _mm_sub_ps(_mm_and_ps(h1, m1), _mm_andnot_ps(h1, m1)); // this is now 1 1 -1 -1
    return _mm_sub_ps(sw, svaluesse);
}

template <>
inline __m128 valueFromSinAndCosForMode<18>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // sin2x in 1, cosx in 23, -sin2x in 4
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto m2 = _mm_set1_ps(2);

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto sw = _mm_sub_ps(_mm_and_ps(h1, m1), _mm_andnot_ps(h1, m1)); // this is now 1 1 -1 -1

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    auto q23 = _mm_cmple_ps(cvaluesse, mz);

    return _mm_add_ps(_mm_and_ps(q23, cvaluesse), _mm_andnot_ps(q23, _mm_mul_ps(sw, s2x)));
}

template <>
inline __m128 valueFromSinAndCosForMode<19>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // This is basically a double frequency shape 0 in the first half only
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);
    const auto m2 = _mm_set1_ps(2);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    auto c2x = _mm_sub_ps(m1, _mm_mul_ps(m2, _mm_mul_ps(svaluesse, svaluesse)));
    auto s4x = _mm_mul_ps(m2, _mm_mul_ps(s2x, c2x));

    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    auto q23 = _mm_cmpge_ps(cvaluesse, mz);
    auto fh = _mm_sub_ps(_mm_and_ps(q23, s2x), _mm_andnot_ps(q23, s4x));
    auto res = _mm_add_ps(_mm_and_ps(h1, fh), _mm_andnot_ps(h1, svaluesse));

    return res;
}

template <>
inline __m128 valueFromSinAndCosForMode<20>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Sine in quadrants 2 and 4; +/-1 in quadrants 1 and 3.
    // quadrants 1 and 3 are when cos and sin have the same sign
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto sbc = _mm_mul_ps(svaluesse, cvaluesse);
    auto q13 = _mm_cmpge_ps(sbc, mz);
    auto h12 = _mm_cmpge_ps(svaluesse, mz);
    auto mv = _mm_sub_ps(_mm_and_ps(h12, m1), _mm_andnot_ps(h12, m1));
    return _mm_add_ps(_mm_andnot_ps(q13, mv), _mm_and_ps(q13, svaluesse));
}

template <>
inline __m128 valueFromSinAndCosForMode<21>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Sine in quadrants 2 and 4; +/-1 in quadrants 1 and 3.
    // quadrants 1 and 3 are when cos and sin have the same sign
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto sbc = _mm_mul_ps(svaluesse, cvaluesse);
    auto q13 = _mm_cmpge_ps(sbc, mz);
    auto h12 = _mm_cmpge_ps(svaluesse, mz);
    auto mv = _mm_sub_ps(_mm_and_ps(h12, m1), _mm_andnot_ps(h12, m1));
    return _mm_add_ps(_mm_and_ps(q13, mv), _mm_andnot_ps(q13, svaluesse));
}

template <>
inline __m128 valueFromSinAndCosForMode<22>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // first 2 quadrants are 1-sin
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto sp = _mm_cmpge_ps(cvaluesse, mz);
    return _mm_and_ps(sp, svaluesse);
}

template <>
inline __m128 valueFromSinAndCosForMode<23>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // first 2 quadrants are 1-sin
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto sp = _mm_cmple_ps(cvaluesse, mz);
    return _mm_and_ps(sp, svaluesse);
}

template <>
inline __m128 valueFromSinAndCosForMode<24>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // first 2 quadrants are 1-sin
    const auto mz = _mm_setzero_ps();
    const auto m1 = _mm_set1_ps(1);

    auto onems = _mm_sub_ps(m1, svaluesse);
    auto sp = _mm_cmpge_ps(svaluesse, mz);
    return _mm_add_ps(_mm_and_ps(sp, onems), _mm_andnot_ps(sp, svaluesse));
}

template <>
inline __m128 valueFromSinAndCosForMode<26>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Zero out sine in quadrant 3 (which is cos and sin are both negative)
    const auto mz = _mm_setzero_ps();
    auto sl0 = _mm_cmple_ps(svaluesse, mz);
    auto cl0 = _mm_cmple_ps(cvaluesse, mz);
    return _mm_andnot_ps(_mm_and_ps(sl0, cl0), svaluesse);
}

template <>
inline __m128 valueFromSinAndCosForMode<25>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Sine 2x in first half
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    auto qv = calcquadrantSSE(svaluesse, cvaluesse);
    auto h1 = _mm_cmpge_ps(svaluesse, mz);
    return _mm_and_ps(h1, _mm_div_ps(s2x, qv));
}

template <>
inline __m128 valueFromSinAndCosForMode<27>(__m128 svaluesse, __m128 cvaluesse, int maxc)
{
    // Sine 2x in first half
    const auto mz = _mm_setzero_ps();
    const auto m2 = _mm_set1_ps(2);

    auto s2x = _mm_mul_ps(m2, _mm_mul_ps(svaluesse, cvaluesse));
    auto qv = calcquadrantSSE(svaluesse, cvaluesse);
    return _mm_div_ps(s2x, qv);
}

// This is used by the legacy apis
template <int mode> inline float singleValueFromSinAndCos(float sinx, float cosx)
{
    auto s = _mm_set1_ps(sinx);
    auto c = _mm_set1_ps(cosx);
    auto r = valueFromSinAndCosForMode<mode>(s, c, 1);
    float v;
    _mm_store_ss(&v, r);
    return v;
}

template <int mode, bool stereo, bool FM>
void SineOscillator::process_block_internal(float pitch, float drift, float fmdepth)
{
    double detune;
    double omega[MAX_UNISON];

    for (int l = 0; l < n_unison; l++)
    {
        detune = drift * driftLFO[l].next();

        if (n_unison > 1)
        {
            if (oscdata->p[sine_unison_detune].absolute)
            {
                detune += oscdata->p[sine_unison_detune].get_extended(
                              localcopy[oscdata->p[sine_unison_detune].param_id_in_scene].f) *
                          storage->note_to_pitch_inv_ignoring_tuning(std::min(148.f, pitch)) * 16 /
                          0.9443 * (detune_bias * float(l) + detune_offset);
            }
            else
            {
                detune += oscdata->p[sine_unison_detune].get_extended(localcopy[id_detune].f) *
                          (detune_bias * float(l) + detune_offset);
            }
        }

        omega[l] = std::min(M_PI, pitch_to_omega(pitch + detune));
    }

    float fv = 32.0 * M_PI * fmdepth * fmdepth * fmdepth;

    /*
    ** See issue 2619. At worst case we move phase by fv * 1. Since we
    ** need phase to be in -Pi,Pi, that means if fv / Pi > 1e5 or so
    ** we have float precision problems. So lets clamp fv.
    */
    fv = limit_range(fv, -1.0e6f, 1.0e6f);

    FMdepth.newValue(fv);
    FB.newValue(abs(fb_val));

    float p alignas(16)[MAX_UNISON];
    float sx alignas(16)[MAX_UNISON];
    float cx alignas(16)[MAX_UNISON];
    float olv alignas(16)[MAX_UNISON];
    float orv alignas(16)[MAX_UNISON];

    for (int i = 0; i < MAX_UNISON; ++i)
        p[i] = 0.0;

    auto outattensse = _mm_set1_ps(out_attenuation);
    auto fbnegmask = _mm_cmplt_ps(_mm_set1_ps(fb_val), _mm_setzero_ps());
    __m128 playramp[4], dramp[4];
    if (firstblock)
    {
        for (int i = 0; i < 4; ++i)
        {
            playramp[i] = _mm_set1_ps(0.0);
            dramp[i] = _mm_set1_ps(BLOCK_SIZE_OS_INV);
        }
        float tv alignas(16)[4];
        _mm_store_ps(tv, playramp[0]);
        tv[0] = 1.0;
        playramp[0] = _mm_load_ps(tv);

        _mm_store_ps(tv, dramp[0]);
        tv[0] = 0.0;
        dramp[0] = _mm_load_ps(tv);
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            playramp[i] = _mm_set1_ps(1.0);
            dramp[i] = _mm_setzero_ps();
        }
    }
    firstblock = false;
    for (int k = 0; k < BLOCK_SIZE_OS; k++)
    {
        float outL = 0.f, outR = 0.f;

        float fmpd = FM ? FMdepth.v * master_osc[k] : 0.f;
        auto fmpds = _mm_set1_ps(fmpd);
        auto fbv = _mm_set1_ps(FB.v);

        for (int u = 0; u < n_unison; u += 4)
        {
            float fph alignas(16)[4] = {(float)phase[u], (float)phase[u + 1], (float)phase[u + 2],
                                        (float)phase[u + 3]};
            auto ph = _mm_load_ps(&fph[0]);
            auto lv = _mm_load_ps(&lastvalue[u]);
            auto x = _mm_add_ps(_mm_add_ps(ph, lv), fmpds);

            x = Surge::DSP::clampToPiRangeSSE(x);

            auto sxl = Surge::DSP::fastsinSSE(x);
            auto cxl = Surge::DSP::fastcosSSE(x);

            auto out_local = valueFromSinAndCosForMode<mode>(sxl, cxl, std::min(n_unison - u, 4));

            auto pl = _mm_load_ps(&panL[u]);
            auto pr = _mm_load_ps(&panR[u]);

            auto ui = u >> 2;
            auto ramp = playramp[ui];
            auto olpr = _mm_mul_ps(out_local, ramp);
            playramp[ui] = _mm_add_ps(playramp[ui], dramp[ui]);

            auto l = _mm_mul_ps(_mm_mul_ps(pl, olpr), outattensse);
            auto r = _mm_mul_ps(_mm_mul_ps(pr, olpr), outattensse);
            _mm_store_ps(&olv[u], l);
            _mm_store_ps(&orv[u], r);

            auto lastv =
                _mm_mul_ps(_mm_add_ps(_mm_and_ps(fbnegmask, _mm_mul_ps(out_local, out_local)),
                                      _mm_andnot_ps(fbnegmask, out_local)),
                           fbv);
            _mm_store_ps(&lastvalue[u], lastv);
        }

        for (int u = 0; u < n_unison; ++u)
        {
            outL += olv[u];
            outR += orv[u];

            // These are doubles and need to be so keep it unrolled
            phase[u] += omega[u];
            phase[u] -= (phase[u] > M_PI) * 2.0 * M_PI;
        }

        FMdepth.process();
        FB.process();

        if (stereo)
        {
            output[k] = outL;
            outputR[k] = outR;
        }
        else
            output[k] = (outL + outR) / 2;
    }
    applyFilter();
}

void SineOscillator::applyFilter()
{
    if (!oscdata->p[sine_lowcut].deactivated)
    {
        auto par = &(oscdata->p[sine_lowcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        hp.coeff_HP(hp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    if (!oscdata->p[sine_highcut].deactivated)
    {
        auto par = &(oscdata->p[sine_highcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        lp.coeff_LP2B(lp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    for (int k = 0; k < BLOCK_SIZE_OS; k += BLOCK_SIZE)
    {
        if (!oscdata->p[sine_lowcut].deactivated)
            hp.process_block(&(output[k]), &(outputR[k]));
        if (!oscdata->p[sine_highcut].deactivated)
            lp.process_block(&(output[k]), &(outputR[k]));
    }
}

template <int mode>
void SineOscillator::process_block_legacy(float pitch, float drift, bool stereo, bool FM,
                                          float fmdepth)
{
    double detune;
    double omega[MAX_UNISON];

    if (FM)
    {
        for (int l = 0; l < n_unison; l++)
        {
            detune = drift * driftLFO[l].next();

            if (n_unison > 1)
            {
                if (oscdata->p[sine_unison_detune].absolute)
                {
                    detune += oscdata->p[sine_unison_detune].get_extended(
                                  localcopy[oscdata->p[sine_unison_detune].param_id_in_scene].f) *
                              storage->note_to_pitch_inv_ignoring_tuning(std::min(148.f, pitch)) *
                              16 / 0.9443 * (detune_bias * float(l) + detune_offset);
                }
                else
                {
                    detune += oscdata->p[sine_unison_detune].get_extended(localcopy[id_detune].f) *
                              (detune_bias * float(l) + detune_offset);
                }
            }

            omega[l] = std::min(M_PI, (double)pitch_to_omega(pitch + detune));
        }

        FMdepth.newValue(fmdepth);

        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            float outL = 0.f, outR = 0.f;

            for (int u = 0; u < n_unison; u++)
            {
                float out_local = singleValueFromSinAndCos<mode>(Surge::DSP::fastsin(phase[u]),
                                                                 Surge::DSP::fastcos(phase[u]));

                outL += (panL[u] * out_local) * out_attenuation * playingramp[u];
                outR += (panR[u] * out_local) * out_attenuation * playingramp[u];

                if (playingramp[u] < 1)
                    playingramp[u] += dplaying;
                if (playingramp[u] > 1)
                    playingramp[u] = 1;

                phase[u] += omega[u] + master_osc[k] * FMdepth.v;
                phase[u] = Surge::DSP::clampToPiRange(phase[u]);
            }

            FMdepth.process();

            if (stereo)
            {
                output[k] = outL;
                outputR[k] = outR;
            }
            else
                output[k] = (outL + outR) / 2;
        }
    }
    else
    {
        for (int l = 0; l < n_unison; l++)
        {
            detune = drift * driftLFO[l].next();

            if (n_unison > 1)
                detune += oscdata->p[sine_unison_detune].get_extended(localcopy[id_detune].f) *
                          (detune_bias * float(l) + detune_offset);

            omega[l] = std::min(M_PI, (double)pitch_to_omega(pitch + detune));
            sine[l].set_rate(omega[l]);
        }

        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            float outL = 0.f, outR = 0.f;

            for (int u = 0; u < n_unison; u++)
            {
                sine[u].process();

                float sinx = sine[u].r;
                float cosx = sine[u].i;

                float out_local = singleValueFromSinAndCos<mode>(sinx, cosx);

                outL += (panL[u] * out_local) * out_attenuation * playingramp[u];
                outR += (panR[u] * out_local) * out_attenuation * playingramp[u];

                if (playingramp[u] < 1)
                    playingramp[u] += dplaying;
                if (playingramp[u] > 1)
                    playingramp[u] = 1;
            }

            if (stereo)
            {
                output[k] = outL;
                outputR[k] = outR;
            }
            else
                output[k] = (outL + outR) / 2;
        }
    }
}

void SineOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
    auto mode = localcopy[id_mode].i;

    if (localcopy[id_fmlegacy].i == 0)
    {
#define DOCASE(x)                                                                                  \
    case x:                                                                                        \
        process_block_legacy<x>(pitch, drift, stereo, FM, fmdepth);                                \
        break;

        switch (mode)
        {
            DOCASE(0)
            DOCASE(1)
            DOCASE(2)
            DOCASE(3)
            DOCASE(4)
            DOCASE(5)
            DOCASE(6)
            DOCASE(7)
            DOCASE(8)
            DOCASE(9)
            DOCASE(10)

            DOCASE(11)
            DOCASE(12)
            DOCASE(13)
            DOCASE(14)
            DOCASE(15)
            DOCASE(16)
            DOCASE(17)
            DOCASE(18)
            DOCASE(19)
            DOCASE(20)
            DOCASE(21)
            DOCASE(22)
            DOCASE(23)
            DOCASE(24)
            DOCASE(25)
            DOCASE(26)
            DOCASE(27)
        }
#undef DOCASE
        applyFilter();

        if (charFilt.doFilter)
        {
            if (stereo)
            {
                charFilt.process_block_stereo(output, outputR, BLOCK_SIZE_OS);
            }
            else
            {
                charFilt.process_block(output, BLOCK_SIZE_OS);
            }
        }

        return;
    }

    fb_val = oscdata->p[sine_feedback].get_extended(localcopy[id_fb].f);

#define DOCASE(x)                                                                                  \
    case x:                                                                                        \
        if (stereo)                                                                                \
            if (FM)                                                                                \
                process_block_internal<x, true, true>(pitch, drift, fmdepth);                      \
            else                                                                                   \
                process_block_internal<x, true, false>(pitch, drift, fmdepth);                     \
        else if (FM)                                                                               \
            process_block_internal<x, false, true>(pitch, drift, fmdepth);                         \
        else                                                                                       \
            process_block_internal<x, false, false>(pitch, drift, fmdepth);                        \
        break;

    switch (mode)
    {
        DOCASE(0)
        DOCASE(1)
        DOCASE(2)
        DOCASE(3)
        DOCASE(4)
        DOCASE(5)
        DOCASE(6)
        DOCASE(7)
        DOCASE(8)
        DOCASE(9)
        DOCASE(10)

        DOCASE(11)
        DOCASE(12)
        DOCASE(13)
        DOCASE(14)
        DOCASE(15)
        DOCASE(16)
        DOCASE(17)
        DOCASE(18)
        DOCASE(19)
        DOCASE(20)
        DOCASE(21)
        DOCASE(22)
        DOCASE(23)
        DOCASE(24)
        DOCASE(25)
        DOCASE(26)
        DOCASE(27)
    }
#undef DOCASE

    if (charFilt.doFilter)
    {
        if (stereo)
        {
            charFilt.process_block_stereo(output, outputR, BLOCK_SIZE_OS);
        }
        else
        {
            charFilt.process_block(output, BLOCK_SIZE_OS);
        }
    }
}

void SineOscillator::init_ctrltypes()
{
    oscdata->p[sine_shape].set_name("Shape");
    oscdata->p[sine_shape].set_type(ct_sineoscmode);

    oscdata->p[sine_feedback].set_name("Feedback");
    oscdata->p[sine_feedback].set_type(ct_osc_feedback_negative);

    oscdata->p[sine_FMmode].set_name("Behavior");
    oscdata->p[sine_FMmode].set_type(ct_sinefmlegacy);

    oscdata->p[sine_lowcut].set_name("Low Cut");
    oscdata->p[sine_lowcut].set_type(ct_freq_audible_deactivatable_hp);

    oscdata->p[sine_highcut].set_name("High Cut");
    oscdata->p[sine_highcut].set_type(ct_freq_audible_deactivatable_lp);

    oscdata->p[sine_unison_detune].set_name("Unison Detune");
    oscdata->p[sine_unison_detune].set_type(ct_oscspread);

    oscdata->p[sine_unison_voices].set_name("Unison Voices");
    oscdata->p[sine_unison_voices].set_type(ct_osccount);
}

void SineOscillator::init_default_values()
{
    oscdata->p[sine_shape].val.i = 0;
    oscdata->p[sine_feedback].val.f = 0;
    oscdata->p[sine_FMmode].val.i = 1;

    // low cut at the bottom
    oscdata->p[sine_lowcut].val.f = oscdata->p[sine_lowcut].val_min.f;
    oscdata->p[sine_lowcut].deactivated = true;

    // high cut at the top
    oscdata->p[sine_highcut].val.f = oscdata->p[sine_highcut].val_max.f;
    oscdata->p[sine_highcut].deactivated = true;

    oscdata->p[sine_unison_detune].val.f = 0.1;
    oscdata->p[sine_unison_voices].val.i = 1;
}

void SineOscillator::handleStreamingMismatches(int streamingRevision,
                                               int currentSynthStreamingRevision)
{
    if (streamingRevision <= 9)
    {
        oscdata->p[sine_shape].val.i = oscdata->p[sine_shape].val_min.i;
    }

    if (streamingRevision <= 10)
    {
        oscdata->p[sine_feedback].val.f = 0;
        oscdata->p[sine_FMmode].val.i = 0;
    }

    if (streamingRevision <= 12)
    {
        // low cut at the bottom
        oscdata->p[sine_lowcut].val.f = oscdata->p[sine_lowcut].val_min.f;
        oscdata->p[sine_lowcut].deactivated = true;

        // high cut at the top
        oscdata->p[sine_highcut].val.f = oscdata->p[sine_highcut].val_max.f;
        oscdata->p[sine_highcut].deactivated = true;

        oscdata->p[sine_feedback].set_type(ct_osc_feedback);

        int wave_remap[] = {0, 8, 9, 10, 1, 11, 4, 12, 13, 2, 3, 5, 6, 7, 14, 15, 16, 17, 18, 19};

        // range checking for garbage data
        if (oscdata->p[sine_shape].val.i < 0 ||
            (oscdata->p[sine_shape].val.i >= (sizeof wave_remap) / sizeof *wave_remap))
            oscdata->p[sine_shape].val.i = oscdata->p[sine_shape].val_min.i;
        else
        {
            // make sure old patches still point to the correct waveforms
            oscdata->p[sine_shape].val.i = wave_remap[oscdata->p[sine_shape].val.i];
        }
    }

    if (streamingRevision <= 15)
    {
        oscdata->retrigger.val.b = true;
        charFilt.doFilter = false;
    }
}

float SineOscillator::valueFromSinAndCos(float sinx, float cosx, int wfMode)
{
    // As you port to SSE, put them here
    switch (wfMode)
    {
    case 0:
        return sinx;
    case 1:
        return singleValueFromSinAndCos<1>(sinx, cosx);
    case 2:
        return singleValueFromSinAndCos<2>(sinx, cosx);
    case 3:
        return singleValueFromSinAndCos<3>(sinx, cosx);
    case 4:
        return singleValueFromSinAndCos<4>(sinx, cosx);
    case 5:
        return singleValueFromSinAndCos<5>(sinx, cosx);
    case 6:
        return singleValueFromSinAndCos<6>(sinx, cosx);
    case 7:
        return singleValueFromSinAndCos<7>(sinx, cosx);
    case 8:
        return singleValueFromSinAndCos<8>(sinx, cosx);
    case 9:
        return singleValueFromSinAndCos<9>(sinx, cosx);
    case 10:
        return singleValueFromSinAndCos<10>(sinx, cosx);
    case 11:
        return singleValueFromSinAndCos<11>(sinx, cosx);
    case 12:
        return singleValueFromSinAndCos<12>(sinx, cosx);
    case 13:
        return singleValueFromSinAndCos<13>(sinx, cosx);
    case 14:
        return singleValueFromSinAndCos<14>(sinx, cosx);
    case 15:
        return singleValueFromSinAndCos<15>(sinx, cosx);
    case 16:
        return singleValueFromSinAndCos<16>(sinx, cosx);
    case 17:
        return singleValueFromSinAndCos<17>(sinx, cosx);
    case 18:
        return singleValueFromSinAndCos<18>(sinx, cosx);
    case 19:
        return singleValueFromSinAndCos<19>(sinx, cosx);
    case 20:
        return singleValueFromSinAndCos<20>(sinx, cosx);
    case 21:
        return singleValueFromSinAndCos<21>(sinx, cosx);
    case 22:
        return singleValueFromSinAndCos<22>(sinx, cosx);
    case 23:
        return singleValueFromSinAndCos<23>(sinx, cosx);
    case 24:
        return singleValueFromSinAndCos<24>(sinx, cosx);
    case 25:
        return singleValueFromSinAndCos<25>(sinx, cosx);
    case 26:
        return singleValueFromSinAndCos<26>(sinx, cosx);
    case 27:
        return singleValueFromSinAndCos<27>(sinx, cosx);
    }

    return 0;
}
