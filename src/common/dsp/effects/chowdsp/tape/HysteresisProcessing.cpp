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
#include "HysteresisProcessing.h"
#include <cmath>

HysteresisProcessing::HysteresisProcessing() {}

void HysteresisProcessing::reset()
{
#if CHOWTAPE_HYSTERESIS_USE_SIMD
    M_n1 = SIMD_MM(set1_pd)(0.0);
    H_n1 = SIMD_MM(set1_pd)(0.0);
    H_d_n1 = SIMD_MM(set1_pd)(0.0);

    hpState.coth = SIMD_MM(set1_pd)(0.0);
    hpState.nearZero = SIMD_MM(set1_pd)(0.0);
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
