#include "basic_dsp.h"

#include <algorithm>
#include <cmath>

void float2i15_block(float *f, short *s, int n)
{
    for (int i = 0; i < n; i++)
    {
        s[i] = (short)(int)limit_range((int)((float)f[i] * 16384.f), -16384, 16383);
    }
}

void i152float_block(short *s, float *f, int n)
{
    const float scale = 1.f / 16384.f;
    for (int i = 0; i < n; i++)
    {
        f[i] = (float)s[i] * scale;
    }
}

void i16toi15_block(short *s, short *o, int n)
{
    for (int i = 0; i < n; i++)
    {
        o[i] = s[i] >> 1;
    }
}

void hardclip_block(float *x, unsigned int nquads)
{
    const __m128 x_min = _mm_set1_ps(-1.0f);
    const __m128 x_max = _mm_set1_ps(1.0f);
    for (unsigned int i = 0; i < (nquads << 2); i += 8)
    {
        _mm_store_ps(x + i, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i), x_max), x_min));
        _mm_store_ps(x + i + 4, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i + 4), x_max), x_min));
    }
}

void hardclip_block8(float *x, unsigned int nquads)
{
    const __m128 x_min = _mm_set1_ps(-8.0f);
    const __m128 x_max = _mm_set1_ps(8.0f);
    for (unsigned int i = 0; i < (nquads << 2); i += 8)
    {
        _mm_store_ps(x + i, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i), x_max), x_min));
        _mm_store_ps(x + i + 4, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i + 4), x_max), x_min));
    }
}

void softclip_block(float *in, unsigned int nquads)
{
    // y = x - (4/27)*x^3,  x [-1.5 .. 1.5]
    const __m128 a = _mm_set1_ps(-4.f / 27.f);

    const __m128 x_min = _mm_set1_ps(-1.5f);
    const __m128 x_max = _mm_set1_ps(1.5f);

    for (unsigned int i = 0; i < nquads; i += 2)
    {
        __m128 x[2], xx[2], t[2];

        x[0] = _mm_min_ps(_mm_load_ps(in + (i << 2)), x_max);
        x[1] = _mm_min_ps(_mm_load_ps(in + ((i + 1) << 2)), x_max);

        x[0] = _mm_max_ps(x[0], x_min);
        x[1] = _mm_max_ps(x[1], x_min);

        xx[0] = _mm_mul_ps(x[0], x[0]);
        xx[1] = _mm_mul_ps(x[1], x[1]);

        t[0] = _mm_mul_ps(x[0], a);
        t[1] = _mm_mul_ps(x[1], a);

        t[0] = _mm_mul_ps(t[0], xx[0]);
        t[1] = _mm_mul_ps(t[1], xx[1]);

        t[0] = _mm_add_ps(t[0], x[0]);
        t[1] = _mm_add_ps(t[1], x[1]);

        _mm_store_ps(in + (i << 2), t[0]);
        _mm_store_ps(in + ((i + 1) << 2), t[1]);
    }
}

float get_squaremax(float *d, unsigned int nquads)
{
    __m128 mx1 = _mm_setzero_ps();
    __m128 mx2 = _mm_setzero_ps();

    for (unsigned int i = 0; i < nquads; i += 2)
    {
        mx1 = _mm_max_ps(mx1, _mm_mul_ps(((__m128 *)d)[i], ((__m128 *)d)[i]));
        mx2 = _mm_max_ps(mx2, _mm_mul_ps(((__m128 *)d)[i + 1], ((__m128 *)d)[i + 1]));
    }
    mx1 = _mm_max_ps(mx1, mx2);
    mx1 = max_ps_to_ss(mx1);
    float f;
    _mm_store_ss(&f, mx1);
    return f;
}

float get_absmax(float *d, unsigned int nquads)
{
    __m128 mx1 = _mm_setzero_ps();
    __m128 mx2 = _mm_setzero_ps();

    for (unsigned int i = 0; i < nquads; i += 2)
    {
        mx1 = _mm_max_ps(mx1, _mm_and_ps(((__m128 *)d)[i], m128_mask_absval));
        mx2 = _mm_max_ps(mx2, _mm_and_ps(((__m128 *)d)[i + 1], m128_mask_absval));
    }
    mx1 = _mm_max_ps(mx1, mx2);
    mx1 = max_ps_to_ss(mx1);
    float f;
    _mm_store_ss(&f, mx1);
    return f;
}

float get_absmax_2(float *__restrict d1, float *__restrict d2, unsigned int nquads)
{
    __m128 mx1 = _mm_setzero_ps();
    __m128 mx2 = _mm_setzero_ps();
    __m128 mx3 = _mm_setzero_ps();
    __m128 mx4 = _mm_setzero_ps();

    for (unsigned int i = 0; i < nquads; i += 2)
    {
        mx1 = _mm_max_ps(mx1, _mm_and_ps(((__m128 *)d1)[i], m128_mask_absval));
        mx2 = _mm_max_ps(mx2, _mm_and_ps(((__m128 *)d1)[i + 1], m128_mask_absval));
        mx3 = _mm_max_ps(mx3, _mm_and_ps(((__m128 *)d2)[i], m128_mask_absval));
        mx4 = _mm_max_ps(mx4, _mm_and_ps(((__m128 *)d2)[i + 1], m128_mask_absval));
    }
    mx1 = _mm_max_ps(mx1, mx2);
    mx3 = _mm_max_ps(mx3, mx4);
    mx1 = _mm_max_ps(mx1, mx3);
    mx1 = max_ps_to_ss(mx1);
    float f;
    _mm_store_ss(&f, mx1);
    return f;
}

void tanh7_block(float *xb, unsigned int nquads)
{
    const __m128 a = _mm_set1_ps(-1.f / 3.f);
    const __m128 b = _mm_set1_ps(2.f / 15.f);
    const __m128 c = _mm_set1_ps(-17.f / 315.f);
    const __m128 one = _mm_set1_ps(1.f);
    const __m128 upper_bound = _mm_set1_ps(1.139f);
    const __m128 lower_bound = _mm_set1_ps(-1.139f);

    __m128 t[4], x[4], xx[4];

    for (unsigned int i = 0; i < nquads; i += 4)
    {
        x[0] = ((__m128 *)xb)[i];
        x[1] = ((__m128 *)xb)[i + 1];
        x[2] = ((__m128 *)xb)[i + 2];
        x[3] = ((__m128 *)xb)[i + 3];

        x[0] = _mm_max_ps(x[0], lower_bound);
        x[1] = _mm_max_ps(x[1], lower_bound);
        x[2] = _mm_max_ps(x[2], lower_bound);
        x[3] = _mm_max_ps(x[3], lower_bound);
        x[0] = _mm_min_ps(x[0], upper_bound);
        x[1] = _mm_min_ps(x[1], upper_bound);
        x[2] = _mm_min_ps(x[2], upper_bound);
        x[3] = _mm_min_ps(x[3], upper_bound);

        xx[0] = _mm_mul_ps(x[0], x[0]);
        xx[1] = _mm_mul_ps(x[1], x[1]);
        xx[2] = _mm_mul_ps(x[2], x[2]);
        xx[3] = _mm_mul_ps(x[3], x[3]);

        t[0] = _mm_mul_ps(xx[0], c);
        t[1] = _mm_mul_ps(xx[1], c);
        t[2] = _mm_mul_ps(xx[2], c);
        t[3] = _mm_mul_ps(xx[3], c);

        t[0] = _mm_add_ps(t[0], b);
        t[1] = _mm_add_ps(t[1], b);
        t[2] = _mm_add_ps(t[2], b);
        t[3] = _mm_add_ps(t[3], b);

        t[0] = _mm_mul_ps(t[0], xx[0]);
        t[1] = _mm_mul_ps(t[1], xx[1]);
        t[2] = _mm_mul_ps(t[2], xx[2]);
        t[3] = _mm_mul_ps(t[3], xx[3]);

        t[0] = _mm_add_ps(t[0], a);
        t[1] = _mm_add_ps(t[1], a);
        t[2] = _mm_add_ps(t[2], a);
        t[3] = _mm_add_ps(t[3], a);

        t[0] = _mm_mul_ps(t[0], xx[0]);
        t[1] = _mm_mul_ps(t[1], xx[1]);
        t[2] = _mm_mul_ps(t[2], xx[2]);
        t[3] = _mm_mul_ps(t[3], xx[3]);

        t[0] = _mm_add_ps(t[0], one);
        t[1] = _mm_add_ps(t[1], one);
        t[2] = _mm_add_ps(t[2], one);
        t[3] = _mm_add_ps(t[3], one);

        t[0] = _mm_mul_ps(t[0], x[0]);
        t[1] = _mm_mul_ps(t[1], x[1]);
        t[2] = _mm_mul_ps(t[2], x[2]);
        t[3] = _mm_mul_ps(t[3], x[3]);

        ((__m128 *)xb)[i] = t[0];
        ((__m128 *)xb)[i + 1] = t[1];
        ((__m128 *)xb)[i + 2] = t[2];
        ((__m128 *)xb)[i + 3] = t[3];
    }
}

void clear_block(float *in, unsigned int nquads)
{
    const __m128 zero = _mm_set1_ps(0.f);

    for (unsigned int i = 0; i < nquads << 2; i += 4)
    {
        _mm_store_ps((float *)&in[i], zero);
    }
}

void accumulate_block(float *__restrict src, float *__restrict dst,
                      unsigned int nquads) // dst += src
{
    for (unsigned int i = 0; i < nquads; i += 4)
    {
        ((__m128 *)dst)[i] = _mm_add_ps(((__m128 *)dst)[i], ((__m128 *)src)[i]);
        ((__m128 *)dst)[i + 1] = _mm_add_ps(((__m128 *)dst)[i + 1], ((__m128 *)src)[i + 1]);
        ((__m128 *)dst)[i + 2] = _mm_add_ps(((__m128 *)dst)[i + 2], ((__m128 *)src)[i + 2]);
        ((__m128 *)dst)[i + 3] = _mm_add_ps(((__m128 *)dst)[i + 3], ((__m128 *)src)[i + 3]);
    }
}

void copy_block(float *__restrict src, float *__restrict dst, unsigned int nquads)
{
    float *fdst, *fsrc;
    fdst = (float *)dst;
    fsrc = (float *)src;

    for (unsigned int i = 0; i < (nquads << 2); i += (8 << 2))
    {
        _mm_store_ps(&fdst[i], _mm_load_ps(&fsrc[i]));
        _mm_store_ps(&fdst[i + 4], _mm_load_ps(&fsrc[i + 4]));
        _mm_store_ps(&fdst[i + 8], _mm_load_ps(&fsrc[i + 8]));
        _mm_store_ps(&fdst[i + 12], _mm_load_ps(&fsrc[i + 12]));
        _mm_store_ps(&fdst[i + 16], _mm_load_ps(&fsrc[i + 16]));
        _mm_store_ps(&fdst[i + 20], _mm_load_ps(&fsrc[i + 20]));
        _mm_store_ps(&fdst[i + 24], _mm_load_ps(&fsrc[i + 24]));
        _mm_store_ps(&fdst[i + 28], _mm_load_ps(&fsrc[i + 28]));
    }
}

void copy_block_US(float *__restrict src, float *__restrict dst, unsigned int nquads)
{
    float *fdst, *fsrc;
    fdst = (float *)dst;
    fsrc = (float *)src;

    for (unsigned int i = 0; i < (nquads << 2); i += (8 << 2))
    {
        _mm_store_ps(&fdst[i], _mm_loadu_ps(&fsrc[i]));
        _mm_store_ps(&fdst[i + 4], _mm_loadu_ps(&fsrc[i + 4]));
        _mm_store_ps(&fdst[i + 8], _mm_loadu_ps(&fsrc[i + 8]));
        _mm_store_ps(&fdst[i + 12], _mm_loadu_ps(&fsrc[i + 12]));
        _mm_store_ps(&fdst[i + 16], _mm_loadu_ps(&fsrc[i + 16]));
        _mm_store_ps(&fdst[i + 20], _mm_loadu_ps(&fsrc[i + 20]));
        _mm_store_ps(&fdst[i + 24], _mm_loadu_ps(&fsrc[i + 24]));
        _mm_store_ps(&fdst[i + 28], _mm_loadu_ps(&fsrc[i + 28]));
    }
}

void copy_block_UD(float *__restrict src, float *__restrict dst, unsigned int nquads)
{
    float *fdst, *fsrc;
    fdst = (float *)dst;
    fsrc = (float *)src;

    for (unsigned int i = 0; i < (nquads << 2); i += (8 << 2))
    {
        _mm_storeu_ps(&fdst[i], _mm_load_ps(&fsrc[i]));
        _mm_storeu_ps(&fdst[i + 4], _mm_load_ps(&fsrc[i + 4]));
        _mm_storeu_ps(&fdst[i + 8], _mm_load_ps(&fsrc[i + 8]));
        _mm_storeu_ps(&fdst[i + 12], _mm_load_ps(&fsrc[i + 12]));
        _mm_storeu_ps(&fdst[i + 16], _mm_load_ps(&fsrc[i + 16]));
        _mm_storeu_ps(&fdst[i + 20], _mm_load_ps(&fsrc[i + 20]));
        _mm_storeu_ps(&fdst[i + 24], _mm_load_ps(&fsrc[i + 24]));
        _mm_storeu_ps(&fdst[i + 28], _mm_load_ps(&fsrc[i + 28]));
    }
}
void copy_block_USUD(float *__restrict src, float *__restrict dst, unsigned int nquads)
{
    float *fdst, *fsrc;
    fdst = (float *)dst;
    fsrc = (float *)src;

    for (unsigned int i = 0; i < (nquads << 2); i += (8 << 2))
    {
        _mm_storeu_ps(&fdst[i], _mm_loadu_ps(&fsrc[i]));
        _mm_storeu_ps(&fdst[i + 4], _mm_loadu_ps(&fsrc[i + 4]));
        _mm_storeu_ps(&fdst[i + 8], _mm_loadu_ps(&fsrc[i + 8]));
        _mm_storeu_ps(&fdst[i + 12], _mm_loadu_ps(&fsrc[i + 12]));
        _mm_storeu_ps(&fdst[i + 16], _mm_loadu_ps(&fsrc[i + 16]));
        _mm_storeu_ps(&fdst[i + 20], _mm_loadu_ps(&fsrc[i + 20]));
        _mm_storeu_ps(&fdst[i + 24], _mm_loadu_ps(&fsrc[i + 24]));
        _mm_storeu_ps(&fdst[i + 28], _mm_loadu_ps(&fsrc[i + 28]));
    }
}

void mul_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
               unsigned int nquads)
{
    for (unsigned int i = 0; i < nquads; i += 4)
    {
        ((__m128 *)dst)[i] = _mm_mul_ps(((__m128 *)src1)[i], ((__m128 *)src2)[i]);
        ((__m128 *)dst)[i + 1] = _mm_mul_ps(((__m128 *)src1)[i + 1], ((__m128 *)src2)[i + 1]);
        ((__m128 *)dst)[i + 2] = _mm_mul_ps(((__m128 *)src1)[i + 2], ((__m128 *)src2)[i + 2]);
        ((__m128 *)dst)[i + 3] = _mm_mul_ps(((__m128 *)src1)[i + 3], ((__m128 *)src2)[i + 3]);
    }
}

void mul_block(float *__restrict src1, float scalar, float *__restrict dst, unsigned int nquads)
{
    auto scalar_mm = _mm_set1_ps(scalar);
    for (unsigned int i = 0; i < nquads; i += 4)
    {
        ((__m128 *)dst)[i] = _mm_mul_ps(((__m128 *)src1)[i], scalar_mm);
        ((__m128 *)dst)[i + 1] = _mm_mul_ps(((__m128 *)src1)[i + 1], scalar_mm);
        ((__m128 *)dst)[i + 2] = _mm_mul_ps(((__m128 *)src1)[i + 2], scalar_mm);
        ((__m128 *)dst)[i + 3] = _mm_mul_ps(((__m128 *)src1)[i + 3], scalar_mm);
    }
}

void encodeMS(float *__restrict L, float *__restrict R, float *__restrict M, float *__restrict S,
              unsigned int nquads)
{
    const __m128 half = _mm_set1_ps(0.5f);
#define L ((__m128 *)L)
#define R ((__m128 *)R)
#define M ((__m128 *)M)
#define S ((__m128 *)S)

    for (unsigned int i = 0; i < nquads; i += 4)
    {
        M[i] = _mm_mul_ps(_mm_add_ps(L[i], R[i]), half);
        S[i] = _mm_mul_ps(_mm_sub_ps(L[i], R[i]), half);
        M[i + 1] = _mm_mul_ps(_mm_add_ps(L[i + 1], R[i + 1]), half);
        S[i + 1] = _mm_mul_ps(_mm_sub_ps(L[i + 1], R[i + 1]), half);
        M[i + 2] = _mm_mul_ps(_mm_add_ps(L[i + 2], R[i + 2]), half);
        S[i + 2] = _mm_mul_ps(_mm_sub_ps(L[i + 2], R[i + 2]), half);
        M[i + 3] = _mm_mul_ps(_mm_add_ps(L[i + 3], R[i + 3]), half);
        S[i + 3] = _mm_mul_ps(_mm_sub_ps(L[i + 3], R[i + 3]), half);
    }
#undef L
#undef R
#undef M
#undef S
}
void decodeMS(float *__restrict M, float *__restrict S, float *__restrict L, float *__restrict R,
              unsigned int nquads)
{
#define L ((__m128 *)L)
#define R ((__m128 *)R)
#define M ((__m128 *)M)
#define S ((__m128 *)S)
    for (unsigned int i = 0; i < nquads; i += 4)
    {
        L[i] = _mm_add_ps(M[i], S[i]);
        R[i] = _mm_sub_ps(M[i], S[i]);
        L[i + 1] = _mm_add_ps(M[i + 1], S[i + 1]);
        R[i + 1] = _mm_sub_ps(M[i + 1], S[i + 1]);
        L[i + 2] = _mm_add_ps(M[i + 2], S[i + 2]);
        R[i + 2] = _mm_sub_ps(M[i + 2], S[i + 2]);
        L[i + 3] = _mm_add_ps(M[i + 3], S[i + 3]);
        R[i + 3] = _mm_sub_ps(M[i + 3], S[i + 3]);
    }
#undef L
#undef R
#undef M
#undef S
}

void add_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
               unsigned int nquads)
{
    for (unsigned int i = 0; i < nquads; i += 4)
    {
        ((__m128 *)dst)[i] = _mm_add_ps(((__m128 *)src1)[i], ((__m128 *)src2)[i]);
        ((__m128 *)dst)[i + 1] = _mm_add_ps(((__m128 *)src1)[i + 1], ((__m128 *)src2)[i + 1]);
        ((__m128 *)dst)[i + 2] = _mm_add_ps(((__m128 *)src1)[i + 2], ((__m128 *)src2)[i + 2]);
        ((__m128 *)dst)[i + 3] = _mm_add_ps(((__m128 *)src1)[i + 3], ((__m128 *)src2)[i + 3]);
    }
}

void subtract_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                    unsigned int nquads)
{
    for (unsigned int i = 0; i < nquads; i += 4)
    {
        ((__m128 *)dst)[i] = _mm_sub_ps(((__m128 *)src1)[i], ((__m128 *)src2)[i]);
        ((__m128 *)dst)[i + 1] = _mm_sub_ps(((__m128 *)src1)[i + 1], ((__m128 *)src2)[i + 1]);
        ((__m128 *)dst)[i + 2] = _mm_sub_ps(((__m128 *)src1)[i + 2], ((__m128 *)src2)[i + 2]);
        ((__m128 *)dst)[i + 3] = _mm_sub_ps(((__m128 *)src1)[i + 3], ((__m128 *)src2)[i + 3]);
    }
}

// Snabba sin(x) substitut
// work in progress
inline float sine_float_nowrap(float x)
{
    // http://www.devmaster.net/forums/showthread.php?t=5784
    const float B = 4.f / M_PI;
    const float C = -4.f / (M_PI * M_PI);

    float y = B * x + C * x * ::abs(x);

    // EXTRA_PRECISION
    //  const float Q = 0.775;
    const float P = 0.225;

    return P * (y * fabs(y) - y) + y; // Q * y + P * y * abs(y)
}

inline __m128 sine_ps_nowrap(__m128 x)
{
    const __m128 B = _mm_set1_ps(4.f / M_PI);
    const __m128 C = _mm_set1_ps(-4.f / (M_PI * M_PI));

    // todo wrap x [0..1] ?

    // y = B * x + C * x * abs(x);
    __m128 y =
        _mm_add_ps(_mm_mul_ps(B, x), _mm_mul_ps(_mm_mul_ps(C, x), _mm_and_ps(m128_mask_absval, x)));

    // EXTRA_PRECISION
    //  const float Q = 0.775;
    const __m128 P = _mm_set1_ps(0.225f);
    return _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_and_ps(m128_mask_absval, y), y), y), P),
                      y);
}

inline __m128 sine_xpi_ps_SSE2(__m128 x) // sin(x*pi)
{
    const __m128 B = _mm_set1_ps(4.f);
    const __m128 premul = _mm_set1_ps(16777216.f);
    const __m128 postmul = _mm_set1_ps(1.f / 16777216.f);
    const __m128i mask = _mm_set1_epi32(0x01ffffff);
    const __m128i offset = _mm_set1_epi32(0x01000000);

    // wrap x
    x = _mm_cvtepi32_ps(
        _mm_sub_epi32(_mm_and_si128(_mm_add_epi32(offset, _mm_cvttps_epi32(x)), mask), offset));

    __m128 y = _mm_mul_ps(B, _mm_sub_ps(x, _mm_mul_ps(x, _mm_and_ps(m128_mask_absval, x))));

    const __m128 P = _mm_set1_ps(0.225f);
    return _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_and_ps(m128_mask_absval, y), y), y), P),
                      y);
}

float sine_ss(unsigned int x) // using 24-bit range as [0..2PI] input
{
    return 0.f;
    /*x = x & 0x00FFFFFF;	// wrap

    float fx;
    _mm_cvtsi32_ss

    const float B = 4.f/M_PI;
    const float C = -4.f/(M_PI*M_PI);

    float y = B * x + C * x * abs(x);

    //EXTRA_PRECISION
    //  const float Q = 0.775;
    const float P = 0.225;

    return P * (y * abs(y) - y) + y;   // Q * y + P * y * abs(y)   */
}
#if !_M_X64 && !defined(ARM_NEON)
__m64 sine(__m64 x)
{
    __m64 xabs = _mm_xor_si64(x, _mm_srai_pi16(x, 15));
    __m64 y = _mm_subs_pi16(_mm_srai_pi16(x, 1), _mm_mulhi_pi16(x, xabs));
    y = _mm_slli_pi16(y, 2);
    y = _mm_adds_pi16(y, y);
    const __m64 Q = _mm_set1_pi16(0x6333);
    const __m64 P = _mm_set1_pi16(0x1CCD);

    __m64 yabs = _mm_xor_si64(y, _mm_srai_pi16(y, 15));
    __m64 y1 = _mm_mulhi_pi16(Q, y);
    __m64 y2 = _mm_mulhi_pi16(P, _mm_slli_pi16(_mm_mulhi_pi16(y, yabs), 1));

    y = _mm_add_pi16(y1, y2);
    return _mm_adds_pi16(y, y);
}
#endif

int sine(int x) // 16-bit sine
{
    x = ((x + 0x8000) & 0xffff) - 0x8000;
    int y = ((x << 2) - ((abs(x >> 1) * (x >> 1)) >> 11));
    const int Q = (0.775f * 65536.f);
    const int P = (0.225f * 32768.f);
    y = ((Q * y) >> 16) + (((((y >> 2) * abs(y >> 2)) >> 11) * P) >> 15);
    return y;
}
