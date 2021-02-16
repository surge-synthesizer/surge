#include <math.h>
#include "HysteresisProcessing.h"

namespace chowdsp
{

HysteresisProcessing::HysteresisProcessing() {}

void HysteresisProcessing::reset()
{
    M_n1 = 0.0;
    H_n1 = 0.0;
    H_d_n1 = 0.0;

    coth = 0.0;
    nearZero = false;
}

void HysteresisProcessing::setSampleRate(double newSR)
{
    fs = newSR;
    T = 1.0 / fs;
    Talpha = T / 1.9;
}

void HysteresisProcessing::cook(float drive, float width, float sat)
{
    M_s = 0.5 + 1.5 * (1.0 - (double)sat);
    a = M_s / (0.01 + 6.0 * (double)drive);
    c = std::sqrt(1.0f - (double)width) - 0.01;
    k = 0.47875;
    upperLim = 20.0;

    nc = 1.0 - c;
    M_s_oa = M_s / a;
    M_s_oa_talpha = alpha * M_s_oa;
    M_s_oa_tc = c * M_s_oa;
    M_s_oa_tc_talpha = alpha * M_s_oa_tc;
    M_s_oaSq_tc_talpha = M_s_oa_tc_talpha / a;
    M_s_oaSq_tc_talphaSq = alpha * M_s_oaSq_tc_talpha;
}

} // namespace chowdsp
