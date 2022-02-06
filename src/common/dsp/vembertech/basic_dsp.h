#pragma once
#include "shared.h"

template <typename T> inline T limit_range(const T &x, const T &low, const T &high)
{
    return std::clamp(x, low, high);
}

template <typename T> inline T limit01(const T &x) { return limit_range(x, (T)0, (T)1); }
template <typename T> inline T limitpm1(const T &x) { return limit_range(x, (T)-1, (T)1); }

void hardclip_block(float *x, unsigned int nquads);
void hardclip_block8(float *x, unsigned int nquads);
void softclip_block(float *in, unsigned int nquads);
void tanh7_block(float *x, unsigned int nquads);
void clear_block(float *in, unsigned int nquads);
void accumulate_block(float *src, float *dst, unsigned int nquads);
void copy_block(float *src, float *dst, unsigned int nquads); // copy block (requires aligned data)
void copy_block_US(float *src, float *dst, unsigned int nquads); // copy block (unaligned source)
void copy_block_UD(float *src, float *dst,
                   unsigned int nquads); // copy block (unaligned destination)
void copy_block_USUD(float *src, float *dst,
                     unsigned int nquads); // copy block (unaligned source + destination)
void mul_block(float *src1, float *src2, float *dst, unsigned int nquads);
void mul_block(float *src1, float scalar, float *dst, unsigned int nquads);
void add_block(float *src1, float *src2, float *dst, unsigned int nquads);
void subtract_block(float *src1, float *src2, float *dst, unsigned int nquads);
void encodeMS(float *L, float *R, float *M, float *S, unsigned int nquads);
void decodeMS(float *M, float *S, float *L, float *R, unsigned int nquads);
float get_absmax(float *d, unsigned int nquads);
float get_squaremax(float *d, unsigned int nquads);
float get_absmax_2(float *d1, float *d2, unsigned int nquads);
void float2i15_block(float *, short *, int);
void i152float_block(short *, float *, int);
void i16toi15_block(short *, short *, int);

float sine_ss(unsigned int x);
int sine(int x);

inline __m128 sum_ps_to_ss(__m128 x)
{
    /*__m128 a = _mm_add_ss(x,_mm_shuffle_ps(x,x,_MM_SHUFFLE(0,0,0,1)));
    __m128 b =
    _mm_add_ss(_mm_shuffle_ps(x,x,_MM_SHUFFLE(0,0,0,2)),_mm_shuffle_ps(x,x,_MM_SHUFFLE(0,0,0,3)));
    return _mm_add_ss(a,b);*/
    __m128 a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    return _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
}

inline __m128 max_ps_to_ss(__m128 x)
{
    __m128 a = _mm_max_ss(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 0, 0, 1)));
    __m128 b = _mm_max_ss(_mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 0, 0, 2)),
                          _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 0, 0, 3)));
    return _mm_max_ss(a, b);
}

inline __m128 hardclip_ss(__m128 x)
{
    const __m128 x_min = _mm_set_ss(-1.0f);
    const __m128 x_max = _mm_set_ss(1.0f);
    return _mm_max_ss(_mm_min_ss(x, x_max), x_min);
}

inline float rcp(float x)
{
    _mm_store_ss(&x, _mm_rcp_ss(_mm_load_ss(&x)));
    return x;
}

inline __m128d hardclip8_sd(__m128d x)
{
    const __m128d x_min = _mm_set_sd(-7.0f);
    const __m128d x_max = _mm_set_sd(8.0f);
    return _mm_max_sd(_mm_min_sd(x, x_max), x_min);
}

inline __m128 hardclip_ps(__m128 x)
{
    const __m128 x_min = _mm_set1_ps(-1.0f);
    const __m128 x_max = _mm_set1_ps(1.0f);
    return _mm_max_ps(_mm_min_ps(x, x_max), x_min);
}

inline float saturate(float f)
{
    __m128 x = _mm_load_ss(&f);
    const __m128 x_min = _mm_set_ss(-1.0f);
    const __m128 x_max = _mm_set_ss(1.0f);
    x = _mm_max_ss(_mm_min_ss(x, x_max), x_min);
    _mm_store_ss(&f, x);
    return f;
}

inline __m128 softclip_ss(__m128 in)
{
    // y = x - (4/27)*x^3,  x € [-1.5 .. 1.5]
    const __m128 a = _mm_set_ss(-4.f / 27.f);

    const __m128 x_min = _mm_set_ss(-1.5f);
    const __m128 x_max = _mm_set_ss(1.5f);

    __m128 x = _mm_max_ss(_mm_min_ss(in, x_max), x_min);
    __m128 xx = _mm_mul_ss(x, x);
    __m128 t = _mm_mul_ss(x, a);
    t = _mm_mul_ss(t, xx);
    t = _mm_add_ss(t, x);

    return t;
}

inline __m128 softclip_ps(__m128 in)
{
    // y = x - (4/27)*x^3,  x € [-1.5 .. 1.5]
    const __m128 a = _mm_set1_ps(-4.f / 27.f);

    const __m128 x_min = _mm_set1_ps(-1.5f);
    const __m128 x_max = _mm_set1_ps(1.5f);

    __m128 x = _mm_max_ps(_mm_min_ps(in, x_max), x_min);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 t = _mm_mul_ps(x, a);
    t = _mm_mul_ps(t, xx);
    t = _mm_add_ps(t, x);

    return t;
}

inline __m128 tanh7_ps(__m128 x)
{
    const __m128 a = _mm_set1_ps(-1.f / 3.f);
    const __m128 b = _mm_set1_ps(2.f / 15.f);
    const __m128 c = _mm_set1_ps(-17.f / 315.f);
    const __m128 one = _mm_set1_ps(1.f);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 y = _mm_add_ps(one, _mm_mul_ps(a, xx));
    __m128 x4 = _mm_mul_ps(xx, xx);
    y = _mm_add_ps(y, _mm_mul_ps(b, x4));
    x4 = _mm_mul_ps(x4, xx);
    y = _mm_add_ps(y, _mm_mul_ps(c, x4));
    return _mm_mul_ps(y, x);
}

inline __m128 tanh7_ss(__m128 x)
{
    const __m128 a = _mm_set1_ps(-1.f / 3.f);
    const __m128 b = _mm_set1_ps(2.f / 15.f);
    const __m128 c = _mm_set1_ps(-17.f / 315.f);
    const __m128 one = _mm_set1_ps(1.f);
    __m128 xx = _mm_mul_ss(x, x);
    __m128 y = _mm_add_ss(one, _mm_mul_ss(a, xx));
    __m128 x4 = _mm_mul_ss(xx, xx);
    y = _mm_add_ss(y, _mm_mul_ss(b, x4));
    x4 = _mm_mul_ss(x4, xx);
    y = _mm_add_ss(y, _mm_mul_ss(c, x4));
    return _mm_mul_ss(y, x);
}

inline __m128 abs_ps(__m128 x) { return _mm_and_ps(x, m128_mask_absval); }

inline __m128 softclip8_ps(__m128 in)
{
    const __m128 a = _mm_set1_ps(-0.00028935185185f);

    const __m128 x_min = _mm_set1_ps(-12.f);
    const __m128 x_max = _mm_set1_ps(12.f);

    __m128 x = _mm_max_ps(_mm_min_ps(in, x_max), x_min);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 t = _mm_mul_ps(x, a);
    t = _mm_mul_ps(t, xx);
    t = _mm_add_ps(t, x);
    return t;
}

inline double tanh7_double(double x)
{
    const double a = -1 / 3, b = 2 / 15, c = -17 / 315;
    // return tanh(x);
    double xs = x * x;
    double y = 1 + xs * a + xs * xs * b + xs * xs * xs * c;
    // double y = 1 + xs*(a + xs*(b + xs*c));
    // t = xs*c;
    // t += b
    // t *= xs;
    // t += a;
    // t *= xs;
    // t += 1;
    // t *= x;

    return y * x;
}

inline double softclip_double(double in)
{
    const double c0 = (4.0 / 27.0);
    double x = (in > -1.5f) ? in : -1.5;
    x = (x < 1.5f) ? x : 1.5;
    double ax = c0 * x;
    double xx = x * x;
    return x - ax * xx;
}

inline double softclip4_double(double in)
{
    double x = (in > -6.0) ? in : -6.0;
    x = (x < 6.0) ? x : 6.0;
    double ax = 0.0023148148148 * x; // 1.0 / 27*16
    double xx = x * x;
    return x - ax * xx;
}

inline double softclip8_double(double in)
{
    double x = (in > -12.0) ? in : -12.0;
    x = (x < 12.0) ? x : 12.0;
    double ax = 0.00028935185185 * x; // 1.0 / 27*128
    double xx = x * x;
    return x - ax * xx;
}

inline double softclip2_double(double in)
{
    double x = (in > -3.0) ? in : -3.0;
    x = (x < 3.0) ? x : 3.0;
    double ax = 0.185185185185185185 * x; // 1.0 / 27*2
    double xx = x * x;
    return x - ax * xx;
}

inline float megapanL(float pos) // valid range -2 .. 2 (> +- 1 is inverted phase)
{
    if (pos > 2.f)
        pos = 2.f;
    if (pos < -2.f)
        pos = -2.f;
    return (1 - 0.75f * pos - 0.25f * pos * pos);
}

inline float megapanR(float pos)
{
    if (pos > 2.f)
        pos = 2.f;
    if (pos < -2.f)
        pos = -2.f;
    return (1 + 0.75f * pos - 0.25f * pos * pos);
}
