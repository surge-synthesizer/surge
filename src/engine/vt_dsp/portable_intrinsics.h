#pragma once

#if LINUX && !ARM_NEON
#include <immintrin.h>
#endif

#define vFloat __m128

#define vZero _mm_setzero_ps()

#define vAdd _mm_add_ps
#define vSub _mm_sub_ps
#define vMul _mm_mul_ps
#define vMAdd(a, b, c) _mm_add_ps(_mm_mul_ps(a, b), c)
#define vNMSub(a, b, c) _mm_sub_ps(c, _mm_mul_ps(a, b))
#define vNeg(a) vSub(vZero, a)

#define vAnd _mm_and_ps
#define vOr _mm_or_ps

#define vCmpGE _mm_cmpge_ps

#define vMax _mm_max_ps
#define vMin _mm_min_ps

#define vLoad _mm_load_ps

inline vFloat vLoad1(float f) { return _mm_load1_ps(&f); }

inline vFloat vSqrtFast(vFloat v) { return _mm_rcp_ps(_mm_rsqrt_ps(v)); }

inline float vSum(vFloat x)
{
    __m128 a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    a = _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
    float f;
    _mm_store_ss(&f, a);

    return f;
}
