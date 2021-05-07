#include "DSPUtils.h"
#include "SurgeStorage.h"

float correlated_noise(float lastval, float correlation)
{
    float wf = correlation * 0.9;
    float wfabs = fabs(wf);
    float rand11 = (((float)rand() / (float)RAND_MAX) * 2.f - 1.f);
    float randt = rand11 * (1 - wfabs) - wf * lastval;
    return randt;
}

float correlated_noise_mk2(float &lastval, float correlation)
{
    float wf = correlation * 0.9;
    float wfabs = fabs(wf);
    float m = 1.f / sqrt(1.f - wfabs);
    float rand11 = (((float)rand() / (float)RAND_MAX) * 2.f - 1.f);
    lastval = rand11 * (1 - wfabs) - wf * lastval;
    return lastval * m;
}

float drift_noise(float &lastval)
{
    const float filter = 0.00001f;
    const float m = 1.f / sqrt(filter);
    //__m128 mvec = _mm_rsqrt_ss(_mm_load_ss(&filter));
    //_mm_store_ss(&m,mvec);

    float rand11 = (((float)rand() / (float)RAND_MAX) * 2.f - 1.f);
    lastval = lastval * (1.f - filter) + rand11 * filter;
    return lastval * m;
}

float correlated_noise_o2(float lastval, float &lastval2, float correlation)
{
    float wf = correlation * 0.9;
    float wfabs = fabs(wf);
    float rand11 = (((float)rand() / (float)RAND_MAX) * 2.f - 1.f);
    float randt = rand11 * (1 - wfabs) - wf * lastval2;
    lastval2 = randt;
    randt = lastval2 * (1 - wfabs) - wf * lastval;
    return randt;
}

float correlated_noise_o2mk2(float &lastval, float &lastval2, float correlation)
{
    float wf = correlation;
    float wfabs = fabs(wf) * 0.8f;
    // wfabs = 1.f - (1.f-wfabs)*(1.f-wfabs);
    wfabs = (2.f * wfabs - wfabs * wfabs);
    if (wf > 0.f)
        wf = wfabs;
    else
        wf = -wfabs;
#if MAC
    float m = 1.f / sqrt(1.f - wfabs);
#else
    float m = 1.f - wfabs;
    // float m = 1.f/sqrt(1.f-wfabs);
    __m128 m1 = _mm_rsqrt_ss(_mm_load_ss(&m));
    _mm_store_ss(&m, m1);
    // if (wf>0.f) m *= 1 + wf*8;
#endif
    float rand11 = (((float)rand() / (float)RAND_MAX) * 2.f - 1.f);
    lastval2 = rand11 * (1 - wfabs) - wf * lastval2;
    lastval = lastval2 * (1 - wfabs) - wf * lastval;
    return lastval * m;
}

float correlated_noise_o2mk2_suppliedrng(float &lastval, float &lastval2, float correlation,
                                         std::function<float()> &urng)
{
    float wf = correlation;
    float wfabs = fabs(wf) * 0.8f;
    // wfabs = 1.f - (1.f-wfabs)*(1.f-wfabs);
    wfabs = (2.f * wfabs - wfabs * wfabs);
    if (wf > 0.f)
        wf = wfabs;
    else
        wf = -wfabs;
#if MAC
    float m = 1.f / sqrt(1.f - wfabs);
#else
    float m = 1.f - wfabs;
    // float m = 1.f/sqrt(1.f-wfabs);
    __m128 m1 = _mm_rsqrt_ss(_mm_load_ss(&m));
    _mm_store_ss(&m, m1);
    // if (wf>0.f) m *= 1 + wf*8;
#endif
    float rand11 = urng();
    lastval2 = rand11 * (1 - wfabs) - wf * lastval2;
    lastval = lastval2 * (1 - wfabs) - wf * lastval;
    return lastval * m;
}

float correlated_noise_o2mk2_storagerng(float &lastval, float &lastval2, float correlation,
                                        SurgeStorage *s)
{
    float wf = correlation;
    float wfabs = fabs(wf) * 0.8f;
    // wfabs = 1.f - (1.f-wfabs)*(1.f-wfabs);
    wfabs = (2.f * wfabs - wfabs * wfabs);
    if (wf > 0.f)
        wf = wfabs;
    else
        wf = -wfabs;
#if MAC
    float m = 1.f / sqrt(1.f - wfabs);
#else
    float m = 1.f - wfabs;
    // float m = 1.f/sqrt(1.f-wfabs);
    __m128 m1 = _mm_rsqrt_ss(_mm_load_ss(&m));
    _mm_store_ss(&m, m1);
    // if (wf>0.f) m *= 1 + wf*8;
#endif
    float rand11 = s->rand_pm1();
    lastval2 = rand11 * (1 - wfabs) - wf * lastval2;
    lastval = lastval2 * (1 - wfabs) - wf * lastval;
    return lastval * m;
}
