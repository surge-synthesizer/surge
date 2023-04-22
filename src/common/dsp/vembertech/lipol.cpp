/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#include "lipol.h"
#include "globals.h"

const __m128 m128_one = _mm_set1_ps(1.f);
const __m128 m128_zero = _mm_setzero_ps();
const __m128 m128_two = _mm_set1_ps(2.f);
const __m128 m128_four = _mm_set1_ps(4.f);

lipol_ps::lipol_ps()
{
    target = _mm_setzero_ps();
    currentval = _mm_setzero_ps();
    coef = _mm_set1_ps(0.25f);
    coef_m1 = _mm_sub_ss(m128_one, coef);
    m128_lipolstarter = _mm_set_ps(1.f, 0.75f, 0.5f, 0.25f);
    set_blocksize(BLOCK_SIZE_OS);
}

void lipol_ps::set_blocksize(int bs)
{
    lipol_BLOCK_SIZE = _mm_cvt_si2ss(m128_zero, bs);
    m128_bs4_inv = _mm_div_ss(m128_four, lipol_BLOCK_SIZE);
}

void lipol_ps::multiply_block(float *src, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    unsigned int n = nquads << 2;
    for (unsigned int i = 0; (i < n); i += 8) // nquads must be multiple of 4
    {
        __m128 a = _mm_mul_ps(_mm_load_ps(src + i), y1);
        _mm_store_ps(src + i, a);
        y1 = _mm_add_ps(y1, dy);
        __m128 b = _mm_mul_ps(_mm_load_ps(src + i + 4), y2);
        _mm_store_ps(src + i + 4, b);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::multiply_block_sat1(float *src, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    const __m128 satv = _mm_set1_ps(1.0f);

    for (unsigned int i = 0; (i < nquads << 2); i += 8) // nquads must be multiple of 4
    {
        _mm_store_ps(src + i, _mm_mul_ps(_mm_load_ps(src + i), y1));
        y1 = _mm_min_ps(satv, _mm_add_ps(y1, dy));
        _mm_store_ps(src + i + 4, _mm_mul_ps(_mm_load_ps(src + i + 4), y2));
        y2 = _mm_min_ps(satv, _mm_add_ps(y2, dy));
    }
}

void lipol_ps::store_block(float *dst, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads << 2; i += 8) // nquads must be multiple of 4
    {
        _mm_store_ps(dst + i, y1);
        y1 = _mm_add_ps(y1, dy);
        _mm_store_ps(dst + i + 4, y2);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::add_block(float *src, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        ((__m128 *)src)[i] = _mm_add_ps(((__m128 *)src)[i], y1);
        y1 = _mm_add_ps(y1, dy);
        ((__m128 *)src)[i + 1] = _mm_add_ps(((__m128 *)src)[i + 1], y2);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::subtract_block(float *src, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        ((__m128 *)src)[i] = _mm_sub_ps(((__m128 *)src)[i], y1);
        y1 = _mm_add_ps(y1, dy);
        ((__m128 *)src)[i + 1] = _mm_sub_ps(((__m128 *)src)[i + 1], y2);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::multiply_2_blocks(float *__restrict src1, float *__restrict src2,
                                 unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        ((__m128 *)src1)[i] = _mm_mul_ps(((__m128 *)src1)[i], y1);
        ((__m128 *)src2)[i] = _mm_mul_ps(((__m128 *)src2)[i], y1);
        y1 = _mm_add_ps(y1, dy);
        ((__m128 *)src1)[i + 1] = _mm_mul_ps(((__m128 *)src1)[i + 1], y2);
        ((__m128 *)src2)[i + 1] = _mm_mul_ps(((__m128 *)src2)[i + 1], y2);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::MAC_block_to(float *__restrict src, float *__restrict dst, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        ((__m128 *)dst)[i] = _mm_add_ps(((__m128 *)dst)[i], _mm_mul_ps(((__m128 *)src)[i], y1));
        y1 = _mm_add_ps(y1, dy);
        ((__m128 *)dst)[i + 1] =
            _mm_add_ps(((__m128 *)dst)[i + 1], _mm_mul_ps(((__m128 *)src)[i + 1], y2));
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::MAC_2_blocks_to(float *__restrict src1, float *__restrict src2,
                               float *__restrict dst1, float *__restrict dst2, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        ((__m128 *)dst1)[i] = _mm_add_ps(((__m128 *)dst1)[i], _mm_mul_ps(((__m128 *)src1)[i], y1));
        ((__m128 *)dst2)[i] = _mm_add_ps(((__m128 *)dst2)[i], _mm_mul_ps(((__m128 *)src2)[i], y1));
        y1 = _mm_add_ps(y1, dy);
        ((__m128 *)dst1)[i + 1] =
            _mm_add_ps(((__m128 *)dst1)[i + 1], _mm_mul_ps(((__m128 *)src1)[i + 1], y2));
        ((__m128 *)dst2)[i + 1] =
            _mm_add_ps(((__m128 *)dst2)[i + 1], _mm_mul_ps(((__m128 *)src2)[i + 1], y2));
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::multiply_block_to(float *__restrict src, float *__restrict dst, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        __m128 a = _mm_mul_ps(((__m128 *)src)[i], y1);
        ((__m128 *)dst)[i] = a;
        y1 = _mm_add_ps(y1, dy);

        __m128 b = _mm_mul_ps(((__m128 *)src)[i + 1], y2);
        ((__m128 *)dst)[i + 1] = b;
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::multiply_2_blocks_to(float *__restrict src1, float *__restrict src2,
                                    float *__restrict dst1, float *__restrict dst2,
                                    unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        ((__m128 *)dst1)[i] = _mm_mul_ps(((__m128 *)src1)[i], y1);
        ((__m128 *)dst2)[i] = _mm_mul_ps(((__m128 *)src2)[i], y1);
        y1 = _mm_add_ps(y1, dy);
        ((__m128 *)dst1)[i + 1] = _mm_mul_ps(((__m128 *)src1)[i + 1], y2);
        ((__m128 *)dst2)[i + 1] = _mm_mul_ps(((__m128 *)src2)[i + 1], y2);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::trixpan_blocks(float *__restrict L, float *__restrict R, float *__restrict dL,
                              float *__restrict dR, unsigned int nquads)
{
    __m128 y, dy;
    initblock(y, dy);

    for (unsigned int i = 0; i < nquads; i++)
    {
        __m128 a = _mm_max_ps(m128_zero, y);
        __m128 b = _mm_min_ps(m128_zero, y);
        __m128 tL = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(m128_one, a), ((__m128 *)L)[i]),
                               _mm_mul_ps(b, ((__m128 *)R)[i])); // L = (1-a)*L - b*R
        __m128 tR =
            _mm_add_ps(_mm_mul_ps(a, ((__m128 *)L)[i]),
                       _mm_mul_ps(_mm_add_ps(m128_one, b), ((__m128 *)R)[i])); // R = a*L + (1+b)*R
        ((__m128 *)dL)[i] = tL;
        ((__m128 *)dR)[i] = tR;
        y = _mm_add_ps(y, dy);
    }
}

void lipol_ps::fade_block_to(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                             unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        __m128 a = _mm_mul_ps(((__m128 *)src1)[i], _mm_sub_ps(m128_one, y1));
        __m128 b = _mm_mul_ps(((__m128 *)src2)[i], y1);
        ((__m128 *)dst)[i] = _mm_add_ps(a, b);
        y1 = _mm_add_ps(y1, dy);

        a = _mm_mul_ps(((__m128 *)src1)[i + 1], _mm_sub_ps(m128_one, y2));
        b = _mm_mul_ps(((__m128 *)src2)[i + 1], y2);
        ((__m128 *)dst)[i + 1] = _mm_add_ps(a, b);
        y2 = _mm_add_ps(y2, dy);
    }
}

void lipol_ps::fade_2_blocks_to(float *__restrict src11, float *__restrict src12,
                                float *__restrict src21, float *__restrict src22,
                                float *__restrict dst1, float *__restrict dst2, unsigned int nquads)
{
    __m128 y1, y2, dy;
    initblock(y1, dy);
    y2 = _mm_add_ps(y1, dy);
    dy = _mm_mul_ps(dy, m128_two);

    for (unsigned int i = 0; i < nquads; i += 2) // nquads must be multiple of 4
    {
        __m128 a = _mm_mul_ps(((__m128 *)src11)[i], _mm_sub_ps(m128_one, y1));
        __m128 b = _mm_mul_ps(((__m128 *)src12)[i], y1);
        ((__m128 *)dst1)[i] = _mm_add_ps(a, b);
        a = _mm_mul_ps(((__m128 *)src21)[i], _mm_sub_ps(m128_one, y1));
        b = _mm_mul_ps(((__m128 *)src22)[i], y1);
        ((__m128 *)dst2)[i] = _mm_add_ps(a, b);
        y1 = _mm_add_ps(y1, dy);

        a = _mm_mul_ps(((__m128 *)src11)[i + 1], _mm_sub_ps(m128_one, y2));
        b = _mm_mul_ps(((__m128 *)src12)[i + 1], y2);
        ((__m128 *)dst1)[i + 1] = _mm_add_ps(a, b);
        a = _mm_mul_ps(((__m128 *)src21)[i + 1], _mm_sub_ps(m128_one, y2));
        b = _mm_mul_ps(((__m128 *)src22)[i + 1], y2);
        ((__m128 *)dst2)[i + 1] = _mm_add_ps(a, b);
        y2 = _mm_add_ps(y2, dy);
    }
}
