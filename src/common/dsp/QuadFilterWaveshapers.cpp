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

#include "QuadFilterUnit.h"
#include "SurgeStorage.h"

__m128 CLIP(QuadFilterWaveshaperState *__restrict s, __m128 in, __m128 drive)
{
    const __m128 x_min = _mm_set1_ps(-1.0f);
    const __m128 x_max = _mm_set1_ps(1.0f);
    return _mm_max_ps(_mm_min_ps(_mm_mul_ps(in, drive), x_max), x_min);
}

__m128 DIGI_SSE2(QuadFilterWaveshaperState *__restrict s, __m128 in, __m128 drive)
{
    // v1.2: return (double)((int)((double)(x*p0inv*16.f+1.0)))*p0*0.0625f;
    const __m128 m16 = _mm_set1_ps(16.f);
    const __m128 m16inv = _mm_set1_ps(0.0625f);
    const __m128 mofs = _mm_set1_ps(0.5f);

    __m128 invdrive = _mm_rcp_ps(drive);
    __m128i a = _mm_cvtps_epi32(_mm_add_ps(mofs, _mm_mul_ps(invdrive, _mm_mul_ps(m16, in))));

    return _mm_mul_ps(drive, _mm_mul_ps(m16inv, _mm_sub_ps(_mm_cvtepi32_ps(a), mofs)));
}

__m128 TANH(QuadFilterWaveshaperState *__restrict s, __m128 in, __m128 drive)
{
    // Closer to ideal than TANH0
    // y = x * ( 27 + x * x ) / ( 27 + 9 * x * x );
    // y = clip(y)
    const __m128 m9 = _mm_set1_ps(9.f);
    const __m128 m27 = _mm_set1_ps(27.f);

    __m128 x = _mm_mul_ps(in, drive);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 denom = _mm_add_ps(m27, _mm_mul_ps(m9, xx));
    __m128 y = _mm_mul_ps(x, _mm_add_ps(m27, xx));
    y = _mm_mul_ps(y, _mm_rcp_ps(denom));

    const __m128 y_min = _mm_set1_ps(-1.0f);
    const __m128 y_max = _mm_set1_ps(1.0f);
    return _mm_max_ps(_mm_min_ps(y, y_max), y_min);
}

__m128 SINUS_SSE2(QuadFilterWaveshaperState *__restrict s, __m128 in, __m128 drive)
{
    const __m128 one = _mm_set1_ps(1.f);
    const __m128 m256 = _mm_set1_ps(256.f);
    const __m128 m512 = _mm_set1_ps(512.f);

    __m128 x = _mm_mul_ps(in, drive);
    x = _mm_add_ps(_mm_mul_ps(x, m256), m512);

    __m128i e = _mm_cvtps_epi32(x);
    __m128 a = _mm_sub_ps(x, _mm_cvtepi32_ps(e));
    e = _mm_packs_epi32(e, e);
    const __m128i UB = _mm_set1_epi16(0x3fe);
    e = _mm_max_epi16(_mm_min_epi16(e, UB), _mm_setzero_si128());

#if MAC
    // this should be very fast on C2D/C1D (and there are no macs with K8's)
    // GCC seems to optimize around the XMM -> int transfers so this is needed here
    int e4 alignas(16)[4];
    e4[0] = _mm_cvtsi128_si32(e);
    e4[1] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(1, 1, 1, 1)));
    e4[2] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(2, 2, 2, 2)));
    e4[3] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(3, 3, 3, 3)));
#else
    // on PC write to memory & back as XMM -> GPR is slow on K8
    short e4 alignas(16)[8];
    _mm_store_si128((__m128i *)&e4, e);
#endif

    __m128 ws1 = _mm_load_ss(&waveshapers[wst_sine][e4[0] & 0x3ff]);
    __m128 ws2 = _mm_load_ss(&waveshapers[wst_sine][e4[1] & 0x3ff]);
    __m128 ws3 = _mm_load_ss(&waveshapers[wst_sine][e4[2] & 0x3ff]);
    __m128 ws4 = _mm_load_ss(&waveshapers[wst_sine][e4[3] & 0x3ff]);
    __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
    ws1 = _mm_load_ss(&waveshapers[wst_sine][(e4[0] + 1) & 0x3ff]);
    ws2 = _mm_load_ss(&waveshapers[wst_sine][(e4[1] + 1) & 0x3ff]);
    ws3 = _mm_load_ss(&waveshapers[wst_sine][(e4[2] + 1) & 0x3ff]);
    ws4 = _mm_load_ss(&waveshapers[wst_sine][(e4[3] + 1) & 0x3ff]);
    __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

    x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

    return x;
}

__m128 ASYM_SSE2(QuadFilterWaveshaperState *__restrict s, __m128 in, __m128 drive)
{
    const __m128 one = _mm_set1_ps(1.f);
    const __m128 m32 = _mm_set1_ps(32.f);
    const __m128 m512 = _mm_set1_ps(512.f);
    const __m128i UB = _mm_set1_epi16(0x3fe);

    __m128 x = _mm_mul_ps(in, drive);
    x = _mm_add_ps(_mm_mul_ps(x, m32), m512);

    __m128i e = _mm_cvtps_epi32(x);
    __m128 a = _mm_sub_ps(x, _mm_cvtepi32_ps(e));
    e = _mm_packs_epi32(e, e);
    e = _mm_max_epi16(_mm_min_epi16(e, UB), _mm_setzero_si128());

#if MAC
    // this should be very fast on C2D/C1D (and there are no macs with K8's)
    int e4 alignas(16)[4];
    e4[0] = _mm_cvtsi128_si32(e);
    e4[1] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(1, 1, 1, 1)));
    e4[2] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(2, 2, 2, 2)));
    e4[3] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(3, 3, 3, 3)));

#else
    // on PC write to memory & back as XMM -> GPR is slow on K8
    short e4 alignas(16)[8];
    _mm_store_si128((__m128i *)&e4, e);
#endif

    __m128 ws1 = _mm_load_ss(&waveshapers[wst_asym][e4[0] & 0x3ff]);
    __m128 ws2 = _mm_load_ss(&waveshapers[wst_asym][e4[1] & 0x3ff]);
    __m128 ws3 = _mm_load_ss(&waveshapers[wst_asym][e4[2] & 0x3ff]);
    __m128 ws4 = _mm_load_ss(&waveshapers[wst_asym][e4[3] & 0x3ff]);
    __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
    ws1 = _mm_load_ss(&waveshapers[wst_asym][(e4[0] + 1) & 0x3ff]);
    ws2 = _mm_load_ss(&waveshapers[wst_asym][(e4[1] + 1) & 0x3ff]);
    ws3 = _mm_load_ss(&waveshapers[wst_asym][(e4[2] + 1) & 0x3ff]);
    ws4 = _mm_load_ss(&waveshapers[wst_asym][(e4[3] + 1) & 0x3ff]);
    __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

    x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

    return x;
}

template <int xRes, int xCenter, int size>
__m128 WS_LUT(QuadFilterWaveshaperState *__restrict s, const float *table, __m128 in, __m128 drive)
{
    const __m128 one = _mm_set1_ps(1.f);
    const __m128 m32 = _mm_set1_ps(xRes);
    const __m128 m512 = _mm_set1_ps(xCenter);
    const __m128i UB = _mm_set1_epi16(size - 1);

    __m128 x = _mm_mul_ps(in, drive);
    x = _mm_add_ps(_mm_mul_ps(x, m32), m512);

    __m128i e = _mm_cvtps_epi32(x);
    __m128 a = _mm_sub_ps(x, _mm_cvtepi32_ps(e));
    e = _mm_packs_epi32(e, e);
    e = _mm_max_epi16(_mm_min_epi16(e, UB), _mm_setzero_si128());

#if MAC
    // this should be very fast on C2D/C1D (and there are no macs with K8's)
    int e4 alignas(16)[4];
    e4[0] = _mm_cvtsi128_si32(e);
    e4[1] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(1, 1, 1, 1)));
    e4[2] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(2, 2, 2, 2)));
    e4[3] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(3, 3, 3, 3)));

#else
    // on PC write to memory & back as XMM -> GPR is slow on K8
    short e4 alignas(16)[8];
    _mm_store_si128((__m128i *)&e4, e);
#endif

    __m128 ws1 = _mm_load_ss(&table[e4[0] & size]);
    __m128 ws2 = _mm_load_ss(&table[e4[1] & size]);
    __m128 ws3 = _mm_load_ss(&table[e4[2] & size]);
    __m128 ws4 = _mm_load_ss(&table[e4[3] & size]);
    __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
    ws1 = _mm_load_ss(&table[(e4[0] + 1) & size]);
    ws2 = _mm_load_ss(&table[(e4[1] + 1) & size]);
    ws3 = _mm_load_ss(&table[(e4[2] + 1) & size]);
    ws4 = _mm_load_ss(&table[(e4[3] + 1) & size]);
    __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

    x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

    return x;
}

template <__m128 (*K)(__m128)>
__m128 CHEBY_CORE(QuadFilterWaveshaperState *__restrict s, __m128 x, __m128 drive)
{
    static const auto m1 = _mm_set1_ps(-1.0f);
    static const auto p1 = _mm_set1_ps(1.0f);

    auto bound = K(_mm_max_ps(_mm_min_ps(x, p1), m1));
    return TANH(s, bound, drive);
}

__m128 cheb2_kernel(__m128 x) // 2 x^2 - 1
{
    static const auto m1 = _mm_set1_ps(1);
    static const auto m2 = _mm_set1_ps(2);
    return _mm_sub_ps(_mm_mul_ps(m2, _mm_mul_ps(x, x)), m1);
}

__m128 cheb3_kernel(__m128 x) // 4 x^3 - 3 x
{
    static const auto m4 = _mm_set1_ps(4);
    static const auto m3 = _mm_set1_ps(3);
    auto x2 = _mm_mul_ps(x, x);
    auto v4x2m3 = _mm_sub_ps(_mm_mul_ps(m4, x2), m3);
    return _mm_mul_ps(v4x2m3, x);
}

__m128 cheb4_kernel(__m128 x) // 8 x^4 - 8 x^2 + 1
{
    static const auto m1 = _mm_set1_ps(1);
    static const auto m8 = _mm_set1_ps(8);
    auto x2 = _mm_mul_ps(x, x);
    auto x4mx2 = _mm_mul_ps(x2, _mm_sub_ps(x2, m1)); // x^2 * ( x^2 - 1)
    return _mm_add_ps(_mm_mul_ps(m8, x4mx2), m1);
}

__m128 cheb5_kernel(__m128 x) // 16 x^5 - 20 x^3 + 5 x
{
    static const auto m16 = _mm_set1_ps(16);
    static const auto mn20 = _mm_set1_ps(-20);
    static const auto m5 = _mm_set1_ps(5);

    auto x2 = _mm_mul_ps(x, x);
    auto x3 = _mm_mul_ps(x2, x);
    auto x5 = _mm_mul_ps(x3, x2);
    auto t1 = _mm_mul_ps(m16, x5);
    auto t2 = _mm_mul_ps(mn20, x3);
    auto t3 = _mm_mul_ps(m5, x);
    return _mm_add_ps(t1, _mm_add_ps(t2, t3));
}

template <__m128 F(__m128), __m128 adF(__m128), int xR, int aR>
__m128 ADAA(QuadFilterWaveshaperState *__restrict s, __m128 x)
{
    auto xPrior = s->R[xR];
    auto adPrior = s->R[aR];

    auto f = F(x);
    auto ad = adF(x);

    auto dx = _mm_sub_ps(x, xPrior);
    auto dad = _mm_sub_ps(ad, adPrior);

    const static auto tolF = 0.0001;
    const static auto tol = _mm_set1_ps(tolF), ntol = _mm_set1_ps(-tolF);
    auto ltt = _mm_and_ps(_mm_cmplt_ps(dx, tol), _mm_cmpgt_ps(dx, ntol)); // dx < tol && dx > -tol
    auto dxDiv = _mm_rcp_ps(_mm_add_ps(_mm_and_ps(ltt, tol), _mm_andnot_ps(ltt, dx)));

    auto fFromAD = _mm_mul_ps(dad, dxDiv);
    auto r = _mm_add_ps(_mm_and_ps(ltt, f), _mm_andnot_ps(ltt, fFromAD));

    s->R[xR] = x;
    s->R[aR] = ad;

    return r;
}

__m128 posrect_kernel(__m128 x) // x > 0 ? x : 0
{
    auto gz = _mm_cmpge_ps(x, _mm_setzero_ps());
    return _mm_and_ps(gz, x);
}

__m128 posrect_ad_kernel(__m128 x) // x > 0 ? x^2/2 : 0
{
    static const auto p5 = _mm_set1_ps(0.5f);
    auto x2o2 = _mm_mul_ps(p5, _mm_mul_ps(x, x));
    auto gz = _mm_cmpge_ps(x, _mm_setzero_ps());
    return _mm_and_ps(gz, x2o2);
}

template <int R1, int R2>
__m128 ADAA_POS_WAVE(QuadFilterWaveshaperState *__restrict s, __m128 x, __m128 drive)
{
    x = CLIP(s, x, drive);
    return ADAA<posrect_kernel, posrect_ad_kernel, R1, R2>(s, x);
}

__m128 negrect_kernel(__m128 x) // x < 0 ? x : 0
{
    auto gz = _mm_cmple_ps(x, _mm_setzero_ps());
    return _mm_and_ps(gz, x);
}

__m128 negrect_ad_kernel(__m128 x) // x < 0 ? x^2/2 : 0
{
    static const auto p5 = _mm_set1_ps(0.5f);
    auto x2o2 = _mm_mul_ps(p5, _mm_mul_ps(x, x));
    auto gz = _mm_cmple_ps(x, _mm_setzero_ps());
    return _mm_and_ps(gz, x2o2);
}

template <int R1, int R2>
__m128 ADAA_NEG_WAVE(QuadFilterWaveshaperState *__restrict s, __m128 x, __m128 drive)
{
    x = CLIP(s, x, drive);
    return ADAA<negrect_kernel, negrect_ad_kernel, R1, R2>(s, x);
}

__m128 fw_kernel(__m128 x) // x > 0 ? x : -x
{
    auto gz = _mm_cmpge_ps(x, _mm_setzero_ps());
    return _mm_sub_ps(_mm_and_ps(gz, x), _mm_andnot_ps(gz, x));
}

__m128 fw_ad_kernel(__m128 x) // x > 0 ? x^2/2 : -x^2/2
{
    static const auto p5 = _mm_set1_ps(0.5f);
    auto x2o2 = _mm_mul_ps(p5, _mm_mul_ps(x, x));
    auto gz = _mm_cmpge_ps(x, _mm_setzero_ps());
    return _mm_sub_ps(_mm_and_ps(gz, x2o2), _mm_andnot_ps(gz, x2o2));
}

__m128 ADAA_FULL_WAVE(QuadFilterWaveshaperState *__restrict s, __m128 x, __m128 drive)
{
    x = CLIP(s, x, drive);
    return ADAA<fw_kernel, fw_ad_kernel, 0, 1>(s, x);
}

WaveshaperQFPtr GetQFPtrWaveshaper(int type)
{
    switch (type)
    {
    case wst_soft:
        return TANH;
    case wst_hard:
        return CLIP;
    case wst_asym:
        // return ASYM_SSE2;
        return [](QuadFilterWaveshaperState *__restrict s, auto a, auto b) {
            return WS_LUT<32, 512, 0x3ff>(s, waveshapers[wst_asym], a, b);
        };
    case wst_sine:
        return SINUS_SSE2;
    case wst_digital:
        return DIGI_SSE2;
    case wst_cheby2:
        return CHEBY_CORE<cheb2_kernel>;
    case wst_cheby3:
        return CHEBY_CORE<cheb3_kernel>;
    case wst_cheby4:
        return CHEBY_CORE<cheb4_kernel>;
    case wst_cheby5:
        return CHEBY_CORE<cheb5_kernel>;
    case wst_fwrectify:
        return ADAA_FULL_WAVE;
    case wst_poswav:
        return ADAA_POS_WAVE<0, 1>;
    case wst_negwav:
        return ADAA_NEG_WAVE<0, 1>;
    }
    return 0;
}

void initializeWaveshaperRegister(int type, float R[n_waveshaper_registers])
{
    switch (type)
    {
    default:
    {
        for (int i = 0; i < n_waveshaper_registers; ++i)
            R[i] = 0.f;
    }
    break;
    }
    return;
}
