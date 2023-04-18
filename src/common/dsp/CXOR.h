//
// Created by Paul Walker on 4/18/23.
//

#ifndef SURGE_CXOR_H
#define SURGE_CXOR_H

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

inline void cxor43_3_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto cx = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
        dst[i] = fmin(-fmin(cx, src2[i]), fmax(src1[i], -src2[i]));
    }
}

inline void cxor43_4_block(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                           unsigned int nquads)
{
    for (auto i = 0U; i < nquads << 2; ++i)
    {
        const auto cx = fmin(fmax(src1[i], src2[i]), -fmin(src1[i], src2[i]));
        dst[i] = fmin(-fmin(cx, src2[i]), fmax(src1[i], -cx));
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
