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

#ifndef SURGE_SRC_COMMON_DSP_CXOR_H
#define SURGE_SRC_COMMON_DSP_CXOR_H

inline void cxor43_0_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        dst[i] = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
    }
}

inline void cxor43_1_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto v1 = fmax(src1[i], src2[i]);
        const auto cx = fmin(v1, -fmin(src1[i], src2[i]));
        dst[i] = fmin(v1, -fmin(cx, v1));
    }
}

inline void cxor43_2_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto v1 = fmax(src1[i], src2[i]);
        const auto cx = fmin(v1, -fmin(src1[i], src2[i]));
        dst[i] = fmin(src1[i], -fmin(cx, v1));
    }
}

inline void cxor43_3_legacy_block(float *__restrict src1, float *__restrict src2,
                                  float *__restrict dst, unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto cx = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
        dst[i] = fmin(-fmin(cx, src2[i]), fmax(src1[i], -src2[i]));
    }
}

inline void cxor43_4_legacy_block(float *__restrict src1, float *__restrict src2,
                                  float *__restrict dst, unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto cx = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
        dst[i] = fmin(-fmin(cx, src2[i]), fmax(src1[i], -cx));
    }
}

inline void cxor43_3_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto cx = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
        dst[i] = fmin(-fmin(cx, -src2[i]), fmax(src1[i], src2[i]));
        // minus was in the wrong place
    }
}

inline void cxor43_4_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto cx = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
        dst[i] = fmin(-fmin(cx, src2[i]), fmax(src1[i], cx));
        // wasn't supposed to have a minus sign
    }
}

inline void cxor93_0_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        auto p = src1[i] + src2[i];
        auto m = src1[i] - src2[i];
        dst[i] = fmin(fmax(p, m), -fmin(p, m));
    }
}

inline void cxor93_1_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        dst[i] = src1[i] - fmin(fmax(src2[i], fmin(src1[i], 0)), fmax(src1[i], 0));
    }
}

inline void cxor93_2_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        auto p = src2[i] + src1[i];
        auto mf = src2[i] - src1[i];
        dst[i] = fmin(src2[i], fmax(0, fmin(p, mf)));
    }
}

inline void cxor93_3_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        auto p = src2[i] + src1[i];
        auto mf = src2[i] - src1[i];
        dst[i] = fmin(fmax(src2[i], p), fmax(0, fmin(p, mf)));
    }
}

inline void cxor93_4_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        auto p = src2[i] + src1[i];
        auto mf = src2[i] - src1[i];
        dst[i] = fmax(fmin(fmax(-src1[i], src2[i]), mf), fmin(p, -p));
    }
}
#endif // SURGE_CXOR_H
