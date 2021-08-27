#include "HysteresisProcessing.h"
#include <cmath>

HysteresisProcessing::HysteresisProcessing() {}

void HysteresisProcessing::reset()
{
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    M_n1 = _mm_set1_pd(0.0);
    H_n1 = _mm_set1_pd(0.0);
    H_d_n1 = _mm_set1_pd(0.0);

    hpState.coth = _mm_set1_pd(0.0);
    hpState.nearZero = _mm_set1_pd(0.0);
#else
    M_n1 = 0.0;
    H_n1 = 0.0;
    H_d_n1 = 0.0;

    hpState.coth = 0.0;
    hpState.nearZero = false;
#endif
}

void HysteresisProcessing::setSampleRate(double newSR)
{
    fs = newSR;
    T = 1.0 / fs;
    Talpha = T / 1.9;
}

void HysteresisProcessing::cook(float drive, float width, float sat, bool v1)
{
    hpState.M_s = 0.5 + 1.5 * (1.0 - (double)sat);
    hpState.a = hpState.M_s / (0.01 + 6.0 * (double)drive);
    hpState.c = std::sqrt(1.0f - (double)width) - 0.01;
    hpState.k = 0.47875;
    upperLim = 20.0;

    if (v1)
    {
        hpState.k = 27.0e3;
        hpState.c = 1.7e-1;
        hpState.M_s *= 50000.0;
        hpState.a = hpState.M_s / (0.01 + 40.0 * (double)drive);
        upperLim = 100000.0;
    }

    hpState.nc = 1.0 - hpState.c;
    hpState.M_s_oa = hpState.M_s / hpState.a;
    hpState.M_s_oa_talpha = hpState.alpha * hpState.M_s_oa;
    hpState.M_s_oa_tc = hpState.c * hpState.M_s_oa;
    hpState.M_s_oa_tc_talpha = hpState.alpha * hpState.M_s_oa_tc;
    hpState.M_s_oaSq_tc_talpha = hpState.M_s_oa_tc_talpha / hpState.a;
    hpState.M_s_oaSq_tc_talphaSq = hpState.alpha * hpState.M_s_oaSq_tc_talpha;
}
