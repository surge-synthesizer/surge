#pragma once

inline __m128 tanh_taylor3_ss(__m128 in)
{
    const __m128 a = _mm_set_ss(-1.f / 3.f);
    const __m128 b = _mm_set_ss(2.f / 15.f);

    const __m128 x_min = _mm_set_ss(-3.f);
    const __m128 x_max = _mm_set_ss(3.f);

    __m128 x = _mm_max_ss(_mm_min_ss(in, x_max), x_min);
    //__m128 x = in;
    __m128 xx = _mm_mul_ss(x, x);
    __m128 apath = _mm_mul_ss(xx, a);
    __m128 bpath = _mm_mul_ss(xx, b);
    bpath = _mm_mul_ss(bpath, xx);
    apath = _mm_add_ss(apath, m128_one);
    apath = _mm_add_ss(apath, bpath);
    apath = _mm_mul_ss(apath, x);
    return apath;
}

inline __m128 tanh_ss(__m128 in)
{
    __m128 x = _mm_and_ps(in, m128_mask_absval);
    __m128 sign = _mm_and_ps(in, m128_mask_signbit);

    const __m128 a = _mm_set_ss(2.f / 3.f);
    const __m128 one = _mm_set_ss(1.f);

    // float denom = 1 + x*(1 + x*(1 + a*x));
    // xx = x*x; 	t1 = 1+x;	t2 = 1+a*x;		denom = t1 + t2*xx

    __m128 t2 = _mm_mul_ss(x, a);
    __m128 xx = _mm_mul_ss(x, x);
    t2 = _mm_add_ss(t2, one);
    __m128 t1 = _mm_add_ss(x, one);
    t2 = _mm_mul_ss(t2, xx);
    t1 = _mm_add_ss(t1, t2);

    __m128 t3 = _mm_rcp_ss(t1);
    t2 = _mm_sub_ss(one, t3);
    t1 = _mm_xor_ps(sign, t2);
    return t1;
}

inline __m128 tanh_ps(__m128 in)
{
    __m128 x = _mm_and_ps(in, m128_mask_absval);
    __m128 sign = _mm_and_ps(in, m128_mask_signbit);

    const __m128 a = _mm_set1_ps(2.f / 3.f);

    __m128 t2 = _mm_mul_ps(x, a);
    __m128 xx = _mm_mul_ps(x, x);
    t2 = _mm_add_ps(t2, m128_one);
    __m128 t1 = _mm_add_ps(x, m128_one);
    t2 = _mm_mul_ps(t2, xx);
    t1 = _mm_add_ps(t1, xx);

    // return ((in>0.f)?1.f:-1.f)*(1.f - 1.f/denom);
    __m128 t3 = _mm_rcp_ps(t1);
    t2 = _mm_sub_ps(m128_one, t3);
    t1 = _mm_xor_ps(sign, t2);
    return t1;
}

/*	inline double tanh_fast3(double in)
        {
                double x = fabs((double)in);
                const double a = 2/3;
                double xx = x*x;
                double d1 = 1+x;
                double d2 = 1+x*a;
                double denom = d1 + d2*xx;
                return ((in>0)?1:-1)*(1 - 1/denom);
        }*/
