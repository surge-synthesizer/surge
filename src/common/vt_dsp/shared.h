#pragma once

#include "globals.h"

inline float i2f_binary_cast(int i)
{
    float *f = (float *)&i;
    return *f;
};

const __m128 m128_mask_signbit = _mm_set1_ps(i2f_binary_cast(0x80000000));
const __m128 m128_mask_absval = _mm_set1_ps(i2f_binary_cast(0x7fffffff));
const __m128 m128_zero = _mm_set1_ps(0.0f);
const __m128 m128_half = _mm_set1_ps(0.5f);
const __m128 m128_one = _mm_set1_ps(1.0f);
const __m128 m128_two = _mm_set1_ps(2.0f);
const __m128 m128_four = _mm_set1_ps(4.0f);
const __m128 m128_1234 = _mm_set_ps(1.f, 2.f, 3.f, 4.f);
const __m128 m128_0123 = _mm_set_ps(0.f, 1.f, 2.f, 3.f);
