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

#include "SineOscillator.h"
#include "sst/basic-blocks/dsp/FastMath.h"
#include <algorithm>

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
namespace mech = sst::basic_blocks::mechanics;

/*
 * Sine Oscillator Optimization Strategy
 *
 * With Surge 1.9, we undertook a bunch of work to optimize the sine oscillator runtime at high
 * unison count with odd shapes. Basically at high unison we were doing large numbers of loops,
 * branches and so forth, and not using any of the advantage you could get by realizing the
 * parallel structure of unison. So we fixed that.
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
 *   SIMD_M128 valueFromSineAndCosForMode<mode>(SIMD_M128 s, SIMD_M128 c, int maxc  )
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
    limit_range(oscdata->p[sine_unison_voices].val.i, 1, MAX_UNISON);

    if (is_display)
    {
        n_unison = 1;
    }

    prepare_unison(n_unison);

    for (int i = 0; i < n_unison; i++)
    {
        phase[i] = // phase in range -PI to PI
            (oscdata->retrigger.val.b || is_display) ? 0.f : 2.0 * M_PI * storage->rand_01() - M_PI;
        lastvalue[0][i] = 0.f;
        lastvalue[1][i] = 0.f;
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

inline SIMD_M128 calcquadrantSSE(SIMD_M128 sinx, SIMD_M128 cosx)
{
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1), m2 = SIMD_MM(set1_ps)(2), m3 = SIMD_MM(set1_ps)(3);
    auto slt = SIMD_MM(and_ps)(SIMD_MM(cmple_ps)(sinx, mz), m1);
    auto clt = SIMD_MM(and_ps)(SIMD_MM(cmple_ps)(cosx, mz), m1);

    // quadrant
    // 1: sin > cos > so this is 0 + 0 - 0 + 1 = 1
    // 2: sin > cos < so this is 0 + 1 - 0 + 1 = 2
    // 3: sin < cos < so this is 3 + 1 - 2 + 1 = 3
    // 4: sin < cos > so this is 3 + 0 - 0 + 1 = 4
    // int quadrant = 3 * sxl0 + cxl0 - 2 * sxl0 * cxl0 + 1;
    auto thsx = SIMD_MM(mul_ps)(m3, slt);
    auto twsc = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(slt, clt));
    auto r = SIMD_MM(add_ps)(SIMD_MM(add_ps)(thsx, clt), SIMD_MM(sub_ps)(m1, twsc));
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
inline SIMD_M128 valueFromSinAndCosForMode(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    float res alignas(16)[4];
    float sv alignas(16)[4], cv alignas(16)[4];
    SIMD_MM(store_ps)(sv, svaluesse);
    SIMD_MM(store_ps)(cv, cvaluesse);
    for (int i = 0; i < maxc; ++i)
        res[i] = valueFromSinAndCosForModeAsScalar<mode>(sv[i], cv[i]);
    return SIMD_MM(load_ps)(res);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<0>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // mode zero is just sine obviously
    return svaluesse;
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<1>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
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
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto mm1 = SIMD_MM(set1_ps)(-1);

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto sw = SIMD_MM(sub_ps)(SIMD_MM(and_ps)(h1, m1),
                              SIMD_MM(andnot_ps)(h1, m1)); // this is now 1 1 -1 -1

    auto q24 = SIMD_MM(cmplt_ps)(SIMD_MM(mul_ps)(svaluesse, cvaluesse), mz);
    auto pm =
        SIMD_MM(sub_ps)(SIMD_MM(and_ps)(q24, m1), SIMD_MM(andnot_ps)(q24, m1)); // this is -1 1 -1 1
    return SIMD_MM(add_ps)(sw, SIMD_MM(mul_ps)(pm, cvaluesse));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<2>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // First half of sine.
    const auto mz = SIMD_MM(setzero_ps)();
    return SIMD_MM(and_ps)(svaluesse, SIMD_MM(cmpge_ps)(svaluesse, mz));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<3>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // 1 -/+ cosx in first half only
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto mm1 = SIMD_MM(set1_ps)(-1);
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto q2 = SIMD_MM(and_ps)(h1, SIMD_MM(cmple_ps)(cvaluesse, mz));

    auto fh = SIMD_MM(and_ps)(h1, m1);
    auto cx = SIMD_MM(mul_ps)(
        fh, SIMD_MM(mul_ps)(cvaluesse,
                            SIMD_MM(add_ps)(mm1, SIMD_MM(mul_ps)(m2, SIMD_MM(and_ps)(q2, m1)))));

    // return SIMD_MM(and_ps)(svaluesse, SIMD_MM(cmpge_ps)(svaluesse, mz));
    return SIMD_MM(add_ps)(fh, cx);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<4>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Sine 2x in first half
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    return SIMD_MM(and_ps)(s2x, SIMD_MM(cmpge_ps)(svaluesse, mz));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<5>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // This is basically a double frequency shape 0 in the first half only
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    auto c2x = SIMD_MM(sub_ps)(m1, SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, svaluesse)));

    auto v1 = valueFromSinAndCosForMode<1>(s2x, c2x, maxc);
    return SIMD_MM(and_ps)(SIMD_MM(cmpge_ps)(svaluesse, mz), v1);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<6>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // abs(Sine 2x in first half
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    auto s2fh = SIMD_MM(and_ps)(s2x, SIMD_MM(cmpge_ps)(svaluesse, mz));
    return mech::abs_ps(s2fh);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<7>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    return mech::abs_ps(valueFromSinAndCosForMode<5>(svaluesse, cvaluesse, maxc));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<8>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // 2 * First half of sin - 1
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);
    const auto m1 = SIMD_MM(set1_ps)(1);
    auto fhs = SIMD_MM(and_ps)(svaluesse, SIMD_MM(cmpge_ps)(svaluesse, mz));
    return SIMD_MM(sub_ps)(SIMD_MM(mul_ps)(m2, fhs), m1);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<9>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // zero out quadrant 1 and 3 which are quadrants where C and S have the same sign
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);
    const auto m1 = SIMD_MM(set1_ps)(1);
    auto css = SIMD_MM(mul_ps)(svaluesse, cvaluesse);
    return SIMD_MM(and_ps)(svaluesse, SIMD_MM(cmple_ps)(css, mz));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<10>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // zero out quadrant 2 and 3 which are quadrants where C and S have the different sign
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);
    const auto m1 = SIMD_MM(set1_ps)(1);
    auto css = SIMD_MM(mul_ps)(svaluesse, cvaluesse);
    return SIMD_MM(and_ps)(svaluesse, SIMD_MM(cmpge_ps)(css, mz));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<11>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    auto q = valueFromSinAndCosForMode<3>(svaluesse, cvaluesse, maxc);
    const auto m2 = SIMD_MM(set1_ps)(2);
    const auto m1 = SIMD_MM(set1_ps)(1);
    return SIMD_MM(sub_ps)(SIMD_MM(mul_ps)(m2, q), m1);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<12>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Flip sign of sin2x in quadrant 2 or 3 (when cosine is negative)
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto mm1 = SIMD_MM(set1_ps)(-1);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));

    auto q23 = SIMD_MM(cmpge_ps)(cvaluesse, mz);
    auto mul = SIMD_MM(add_ps)(SIMD_MM(and_ps)(q23, m1), SIMD_MM(andnot_ps)(q23, mm1));
    return SIMD_MM(mul_ps)(s2x, mul);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<13>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Flip sign of sin2x in quadrant 3  (when sine is negative)
    // and zero it in quadrant 2 and 4 (when sine and cos have different signs or s2x is negative)
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto mm1 = SIMD_MM(set1_ps)(-1);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));

    auto q13 = SIMD_MM(cmpge_ps)(s2x, mz);
    auto q2 = SIMD_MM(cmple_ps)(svaluesse, mz);
    auto signflip = SIMD_MM(sub_ps)(m1, SIMD_MM(and_ps)(q2, m2));

    return SIMD_MM(and_ps)(q13, SIMD_MM(mul_ps)(signflip, s2x));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<14>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // abs of cos2x in the first half
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto c2x = SIMD_MM(sub_ps)(m1, SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, svaluesse)));
    auto q23 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    return SIMD_MM(and_ps)(q23, mech::abs_ps(c2x));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<15>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // 1 - sinx in quadrant 1, -1-sinx in quadrant 4, zero otherwise

    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto mm1 = SIMD_MM(set1_ps)(-1);

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto sig = SIMD_MM(add_ps)(SIMD_MM(and_ps)(h1, SIMD_MM(sub_ps)(m1, svaluesse)),
                               SIMD_MM(andnot_ps)(h1, SIMD_MM(sub_ps)(mm1, svaluesse)));

    auto q14 = SIMD_MM(cmpge_ps)(cvaluesse, mz);
    return SIMD_MM(and_ps)(q14, sig);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<16>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // 1 - sinx in quadrant 1, cosx - 1 in quadrant 4, zero otherwise

    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto sig = SIMD_MM(add_ps)(SIMD_MM(and_ps)(h1, SIMD_MM(sub_ps)(m1, svaluesse)),
                               SIMD_MM(andnot_ps)(h1, SIMD_MM(sub_ps)(cvaluesse, m1)));

    auto q14 = SIMD_MM(cmpge_ps)(cvaluesse, mz);
    return SIMD_MM(and_ps)(q14, sig);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<17>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // 1-sinx in 1,2; -1-sinx in 3,4
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto sw = SIMD_MM(sub_ps)(SIMD_MM(and_ps)(h1, m1),
                              SIMD_MM(andnot_ps)(h1, m1)); // this is now 1 1 -1 -1
    return SIMD_MM(sub_ps)(sw, svaluesse);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<18>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // sin2x in 1, cosx in 23, -sin2x in 4
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto sw = SIMD_MM(sub_ps)(SIMD_MM(and_ps)(h1, m1),
                              SIMD_MM(andnot_ps)(h1, m1)); // this is now 1 1 -1 -1

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    auto q23 = SIMD_MM(cmple_ps)(cvaluesse, mz);

    return SIMD_MM(add_ps)(SIMD_MM(and_ps)(q23, cvaluesse),
                           SIMD_MM(andnot_ps)(q23, SIMD_MM(mul_ps)(sw, s2x)));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<19>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // This is basically a double frequency shape 0 in the first half only
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    auto c2x = SIMD_MM(sub_ps)(m1, SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, svaluesse)));
    auto s4x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(s2x, c2x));

    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto q23 = SIMD_MM(cmpge_ps)(cvaluesse, mz);
    auto fh = SIMD_MM(sub_ps)(SIMD_MM(and_ps)(q23, s2x), SIMD_MM(andnot_ps)(q23, s4x));
    auto res = SIMD_MM(add_ps)(SIMD_MM(and_ps)(h1, fh), SIMD_MM(andnot_ps)(h1, svaluesse));

    return res;
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<20>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Sine in quadrants 2 and 4; +/-1 in quadrants 1 and 3.
    // quadrants 1 and 3 are when cos and sin have the same sign
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto sbc = SIMD_MM(mul_ps)(svaluesse, cvaluesse);
    auto q13 = SIMD_MM(cmpge_ps)(sbc, mz);
    auto h12 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto mv = SIMD_MM(sub_ps)(SIMD_MM(and_ps)(h12, m1), SIMD_MM(andnot_ps)(h12, m1));
    return SIMD_MM(add_ps)(SIMD_MM(andnot_ps)(q13, mv), SIMD_MM(and_ps)(q13, svaluesse));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<21>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Sine in quadrants 2 and 4; +/-1 in quadrants 1 and 3.
    // quadrants 1 and 3 are when cos and sin have the same sign
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto sbc = SIMD_MM(mul_ps)(svaluesse, cvaluesse);
    auto q13 = SIMD_MM(cmpge_ps)(sbc, mz);
    auto h12 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    auto mv = SIMD_MM(sub_ps)(SIMD_MM(and_ps)(h12, m1), SIMD_MM(andnot_ps)(h12, m1));
    return SIMD_MM(add_ps)(SIMD_MM(and_ps)(q13, mv), SIMD_MM(andnot_ps)(q13, svaluesse));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<22>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // first 2 quadrants are 1-sin
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto sp = SIMD_MM(cmpge_ps)(cvaluesse, mz);
    return SIMD_MM(and_ps)(sp, svaluesse);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<23>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // first 2 quadrants are 1-sin
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto sp = SIMD_MM(cmple_ps)(cvaluesse, mz);
    return SIMD_MM(and_ps)(sp, svaluesse);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<24>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // first 2 quadrants are 1-sin
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m1 = SIMD_MM(set1_ps)(1);

    auto onems = SIMD_MM(sub_ps)(m1, svaluesse);
    auto sp = SIMD_MM(cmpge_ps)(svaluesse, mz);
    return SIMD_MM(add_ps)(SIMD_MM(and_ps)(sp, onems), SIMD_MM(andnot_ps)(sp, svaluesse));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<26>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Zero out sine in quadrant 3 (which is cos and sin are both negative)
    const auto mz = SIMD_MM(setzero_ps)();
    auto sl0 = SIMD_MM(cmple_ps)(svaluesse, mz);
    auto cl0 = SIMD_MM(cmple_ps)(cvaluesse, mz);
    return SIMD_MM(andnot_ps)(SIMD_MM(and_ps)(sl0, cl0), svaluesse);
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<25>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Sine 2x in first half
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    auto qv = calcquadrantSSE(svaluesse, cvaluesse);
    auto h1 = SIMD_MM(cmpge_ps)(svaluesse, mz);
    return SIMD_MM(and_ps)(h1, SIMD_MM(div_ps)(s2x, qv));
}

template <>
inline SIMD_M128 valueFromSinAndCosForMode<27>(SIMD_M128 svaluesse, SIMD_M128 cvaluesse, int maxc)
{
    // Sine 2x in first half
    const auto mz = SIMD_MM(setzero_ps)();
    const auto m2 = SIMD_MM(set1_ps)(2);

    auto s2x = SIMD_MM(mul_ps)(m2, SIMD_MM(mul_ps)(svaluesse, cvaluesse));
    auto qv = calcquadrantSSE(svaluesse, cvaluesse);
    return SIMD_MM(div_ps)(s2x, qv);
}

// This is used by the legacy apis
template <int mode> inline float singleValueFromSinAndCos(float sinx, float cosx)
{
    auto s = SIMD_MM(set1_ps)(sinx);
    auto c = SIMD_MM(set1_ps)(cosx);
    auto r = valueFromSinAndCosForMode<mode>(s, c, 1);
    float v;
    SIMD_MM(store_ss)(&v, r);
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
    FB.newValue(fb_val);

    float p alignas(16)[MAX_UNISON];
    float olv alignas(16)[MAX_UNISON];
    float orv alignas(16)[MAX_UNISON];

    for (int i = 0; i < MAX_UNISON; ++i)
        p[i] = 0.0;

    auto outattensse = SIMD_MM(set1_ps)(out_attenuation);
    SIMD_M128 playramp[4], dramp[4];
    if (firstblock)
    {
        for (int i = 0; i < 4; ++i)
        {
            playramp[i] = SIMD_MM(set1_ps)(0.0);
            dramp[i] = SIMD_MM(set1_ps)(BLOCK_SIZE_OS_INV);
        }
        float tv alignas(16)[4];
        SIMD_MM(store_ps)(tv, playramp[0]);
        tv[0] = 1.0;
        playramp[0] = SIMD_MM(load_ps)(tv);

        SIMD_MM(store_ps)(tv, dramp[0]);
        tv[0] = 0.0;
        dramp[0] = SIMD_MM(load_ps)(tv);
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            playramp[i] = SIMD_MM(set1_ps)(1.0);
            dramp[i] = SIMD_MM(setzero_ps)();
        }
    }
    firstblock = false;

    auto fb_mode = oscdata->p[sine_feedback].deform_type;

    auto fb0weight = SIMD_MM(setzero_ps)();
    auto fb1weight = SIMD_MM(set1_ps)(1.f);

    if (fb_mode == 1)
    {
        fb0weight = SIMD_MM(set1_ps)(0.5f);
        fb1weight = SIMD_MM(set1_ps)(0.5f);
    }

    for (int k = 0; k < BLOCK_SIZE_OS; k++)
    {
        float outL = 0.f, outR = 0.f;

        float fmpd = FM ? FMdepth.v * master_osc[k] : 0.f;
        auto fmpds = SIMD_MM(set1_ps)(fmpd);
        auto fbv = SIMD_MM(set1_ps)(std::fabs(FB.v));
        auto fbnegmask = SIMD_MM(cmplt_ps)(SIMD_MM(set1_ps)(FB.v), SIMD_MM(setzero_ps)());

        for (int u = 0; u < n_unison; u += 4)
        {
            float fph alignas(16)[4] = {(float)phase[u], (float)phase[u + 1], (float)phase[u + 2],
                                        (float)phase[u + 3]};
            auto ph = SIMD_MM(load_ps)(&fph[0]);
            auto lv0 = SIMD_MM(load_ps)(&lastvalue[0][u]);
            auto lv1 = SIMD_MM(load_ps)(&lastvalue[1][u]);

            auto lv =
                SIMD_MM(add_ps)(SIMD_MM(mul_ps)(lv0, fb0weight), SIMD_MM(mul_ps)(lv1, fb1weight));
            auto fba =
                SIMD_MM(mul_ps)(SIMD_MM(add_ps)(SIMD_MM(and_ps)(fbnegmask, SIMD_MM(mul_ps)(lv, lv)),
                                                SIMD_MM(andnot_ps)(fbnegmask, lv)),
                                fbv);
            auto x = SIMD_MM(add_ps)(SIMD_MM(add_ps)(ph, fba), fmpds);

            x = sst::basic_blocks::dsp::clampToPiRangeSSE(x);

            auto sxl = sst::basic_blocks::dsp::fastsinSSE(x);
            auto cxl = sst::basic_blocks::dsp::fastcosSSE(x);

            auto out_local = valueFromSinAndCosForMode<mode>(sxl, cxl, std::min(n_unison - u, 4));

            auto pl = SIMD_MM(load_ps)(&panL[u]);
            auto pr = SIMD_MM(load_ps)(&panR[u]);

            auto ui = u >> 2;
            auto ramp = playramp[ui];
            auto olpr = SIMD_MM(mul_ps)(out_local, ramp);
            playramp[ui] = SIMD_MM(add_ps)(playramp[ui], dramp[ui]);

            auto l = SIMD_MM(mul_ps)(SIMD_MM(mul_ps)(pl, olpr), outattensse);
            auto r = SIMD_MM(mul_ps)(SIMD_MM(mul_ps)(pr, olpr), outattensse);
            SIMD_MM(store_ps)(&olv[u], l);
            SIMD_MM(store_ps)(&orv[u], r);

            SIMD_MM(store_ps)(&lastvalue[0][u], lv1);
            SIMD_MM(store_ps)(&lastvalue[1][u], out_local);
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
                float out_local =
                    singleValueFromSinAndCos<mode>(sst::basic_blocks::dsp::fastsin(phase[u]),
                                                   sst::basic_blocks::dsp::fastcos(phase[u]));

                outL += (panL[u] * out_local) * out_attenuation * playingramp[u];
                outR += (panR[u] * out_local) * out_attenuation * playingramp[u];

                if (playingramp[u] < 1)
                    playingramp[u] += dplaying;
                if (playingramp[u] > 1)
                    playingramp[u] = 1;

                phase[u] += omega[u] + master_osc[k] * FMdepth.v;
                phase[u] = sst::basic_blocks::dsp::clampToPiRange(phase[u]);
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
