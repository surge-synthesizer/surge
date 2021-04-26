#include "globals.h"
#include "VintageLadders.h"
#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vt_dsp/basic_dsp.h"
#include "FastMath.h"

/*
** This contains various adaptations of the models found at
**
** https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
**
** Modifications include
** 1. Modifying to make Surge compatible with state mamagenemt
** 2. SSE and so on
** 3. Model specific changes
*/

namespace VintageLadder
{
namespace Common
{
float clampedFrequency(float pitch, SurgeStorage *storage)
{
    auto freq = storage->note_to_pitch_ignoring_tuning(pitch + 69) * Tunings::MIDI_0_FREQ;
    freq = limit_range((float)freq, 5.f, (float)(dsamplerate_os * 0.3f));
    return freq;
}
} // namespace Common

namespace RK
{
/*
** Imitates a Moog resonant filter by Runge-Kutte numerical integration of
** a differential equation approximately describing the dynamics of the circuit.
**
** Useful references:
**
** Tim Stilson
** "Analyzing the Moog VCF with Considerations for Digital Implementation"
** Sections 1 and 2 are a reasonably good introduction but the
** model they use is highly idealized.
**
** Timothy E. Stinchcombe
** "Analysis of the Moog Transistor Ladder and Derivative Filters"
** Long, but a very thorough description of how the filter works including
** its nonlinearities
**
** Antti Huovilainen
** "Non-linear digital implementation of the moog ladder filter"
** Comes close to giving a differential equation for a reasonably realistic
** model of the filter
**
** The differential equations are:
**
**  y1' = k * (S(x - r * y4) - S(y1))
**  y2' = k * (S(y1) - S(y2))
**  y3' = k * (S(y2) - S(y3))
**  y4' = k * (S(y3) - S(y4))
**
** where k controls the cutoff frequency, r is feedback (<= 4 for stability), and S(x) is a
*saturation function.
**
** Although the code is modified from that location here is the originaly copyright notice:
**
** Copyright (c) 2015, Miller Puckette. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
** * Redistributions of source code must retain the above copyright notice, this
**   list of conditions and the following disclaimer.
** * Redistributions in binary form must reproduce the above copyright notice,
**   this list of conditions and the following disclaimer in the documentation
**   and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*/

enum rkm_coeffs
{
    rkm_cutoff = 0,
    rkm_reso,
    rkm_gComp,

    n_rkcoeff,
};

static constexpr int extraOversample = 4;
static constexpr float extraOversampleInv = 0.25;

float saturation = 3, saturationInverse = 0.333333333333;

float gainCompensation = 0.666;

void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso,
                      bool applyGainCompensation, SurgeStorage *storage)
{
    // Consideration: Do we want tuning aware or not?
    float lc[n_cm_coeffs];
    auto pitch = VintageLadder::Common::clampedFrequency(freq, storage);

    lc[rkm_cutoff] = pitch * 2.0 * M_PI;
    lc[rkm_reso] =
        limit_range(reso, 0.f, 1.f) *
        4.5; // code says 0-10 is value but above 4 or so it's just out of tune self-oscillation
    lc[rkm_gComp] = 0.0;

    if (applyGainCompensation)
    {
        lc[rkm_gComp] = gainCompensation;
    }

    cm->FromDirect(lc);
}

#define F(a) _mm_set_ps1(a)
#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define S(a, b) _mm_sub_ps(a, b)

inline __m128 clip(__m128 value, __m128 saturation, __m128 saturationinverse)
{
    static const __m128 minusone = F(-1), one = F(1), onethird = F(1.f / 3.f);
    auto vtsi = M(value, saturationinverse);
    auto v2 = _mm_min_ps(one, _mm_max_ps(minusone, vtsi));
    auto v23 = M(v2, M(v2, v2));
    auto vkern = S(v2, M(onethird, v23));
    auto res = M(saturation, vkern);

    return res;
}

// void calculateDerivatives(float input, double * dstate, double * state, float cutoff, float
// resonance, float saturation, float saturationInv )
inline void calculateDerivatives(__m128 input, __m128 *dstate, __m128 *state, __m128 cutoff,
                                 __m128 resonance, __m128 saturation, __m128 saturationInv,
                                 __m128 gComp)
{
    auto satstate0 = clip(state[0], saturation, saturationInv);
    auto satstate1 = clip(state[1], saturation, saturationInv);
    auto satstate2 = clip(state[2], saturation, saturationInv);

    // dstate[0] = cutoff * (clip(input - resonance * state[3], saturation, saturationInv) -
    // satstate0); Modify dstate[0] = cutoff * (clip(input - resonance * (state[3] - gComp * input),
    // saturation, saturationInv) - satstate0);
    auto startstate =
        clip(S(input, M(resonance, S(state[3], M(gComp, input)))), saturation, saturationInv);
    dstate[0] = M(cutoff, S(startstate, satstate0));

    // dstate[1] = cutoff * (satstate0 - satstate1);
    dstate[1] = M(cutoff, S(satstate0, satstate1));

    // dstate[2] = cutoff * (satstate1 - satstate2);
    dstate[2] = M(cutoff, S(satstate1, satstate2));

    // dstate[3] = cutoff * (satstate2 - clip(state[3], saturation, saturationInv));
    dstate[3] = M(cutoff, S(satstate2, clip(state[3], saturation, saturationInv)));
}

__m128 process(QuadFilterUnitState *__restrict f, __m128 input)
{
    int i;
    __m128 deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

    __m128 *state = &(f->R[0]);

    auto stepSize = F(dsamplerate_os_inv * extraOversampleInv),
         halfStepSize = F(0.5 * dsamplerate_os_inv * extraOversampleInv);

    static const __m128 oneoversix = F(1.f / 6.f), two = F(2.f), dFac = F(extraOversampleInv),
                        sat = F(saturation), satInv = F(saturationInverse);

    __m128 outputOS[extraOversample];

    for (int osi = 0; osi < extraOversample; ++osi)
    {
        for (int j = 0; j < n_rkcoeff; ++j)
        {
            f->C[j] = A(f->C[j], M(dFac, f->dC[j]));
        }

        __m128 cutoff = f->C[rkm_cutoff];
        __m128 resonance = f->C[rkm_reso];
        __m128 gComp = f->C[rkm_gComp];

        calculateDerivatives(input, deriv1, state, cutoff, resonance, sat, satInv, gComp);
        for (i = 0; i < 4; i++)
        {
            tempState[i] = A(state[i], M(halfStepSize, deriv1[i]));
        }

        calculateDerivatives(input, deriv2, tempState, cutoff, resonance, sat, satInv, gComp);
        for (i = 0; i < 4; i++)
        {
            tempState[i] = A(state[i], M(halfStepSize, deriv2[i]));
        }

        calculateDerivatives(input, deriv3, tempState, cutoff, resonance, sat, satInv, gComp);
        for (i = 0; i < 4; i++)
        {
            tempState[i] = A(state[i], M(halfStepSize, deriv3[i]));
        }

        calculateDerivatives(input, deriv4, tempState, cutoff, resonance, sat, satInv, gComp);
        for (i = 0; i < 4; i++)
        {
            // state[i] += (1.0 / 6.0) * stepSize * (deriv1[i] + 2.0 * deriv2[i] + 2.0 * deriv3[i] +
            // deriv4[i]);
            state[i] = A(state[i],
                         M(oneoversix,
                           M(stepSize,
                             A(deriv1[i], A(M(two, deriv2[i]), A(M(two, deriv3[i]), deriv4[i]))))));
        }

        outputOS[osi] = state[3];

        // Zero stuffing
        input = _mm_setzero_ps();
    }

    /*
    ** OK this is a bit of a hack but... these are the Lanczos factors
    ** sinc( x ) sinc (x / 2), only backwards. Really we should do a proper little
    ** FIR around the whole thing, but this at least gives us a reconstruction with
    ** some aliasing supression.
    **
    ** Not entirely valid but...
    **
    ** Anyway: (2 * sin(pi * x) * sin((pi * x) / 2)) / (pi^2 * x^2), for points -1.5, -1, 0.5, and 0
    **
    */
    auto ov = _mm_setzero_ps();
    __m128 windowFactors[4];
    windowFactors[0] = F(-0.0636844);
    windowFactors[1] = _mm_setzero_ps();
    windowFactors[2] = F(0.57315917);
    windowFactors[3] = F(1);

    for (int i = 0; i < extraOversample; ++i)
    {
        ov = A(ov, M(outputOS[i], windowFactors[i]));
    }

    return M(F(1.5), ov);
}

#undef F
#undef M
#undef A
#undef S

} // namespace RK

namespace Huov
{
/*
** Huovilainen developed an improved and physically correct model of the Moog
** Ladder filter that builds upon the work done by Smith and Stilson. This model
** inserts nonlinearities inside each of the 4 one-pole sections on account of the
** smoothly saturating function of analog transistors. The base-emitter voltages of
** the transistors are considered with an experimental value of 1.22070313 which
** maintains the characteristic sound of the analog Moog. This model also permits
** self-oscillation for resonances greater than 1. The model depends on five
** hyperbolic tangent functions (tanh) for each sample, and an oversampling factor
** of two (preferably higher, if possible). Although a more faithful
** representation of the Moog ladder, these dependencies increase the processing
** time of the filter significantly. Lastly, a half-sample delay is introduced for
** phase compensation at the final stage of the filter.
**
** References: Huovilainen (2004), Huovilainen (2010), DAFX - Zolzer (ed) (2nd ed)
** Original implementation: Victor Lazzarini for CSound5
**
** Considerations for oversampling:
** http://music.columbia.edu/pipermail/music-dsp/2005-February/062778.html
** http://www.synthmaker.co.uk/dokuwiki/doku.php?id=tutorials:oversampling
*/

enum huov_coeffs
{
    h_cutoff = 0,
    h_res,
    h_fc,
    h_gComp,

    n_hcoeffs,
};

enum huov_regoffsets
{
    h_stage = 0,
    h_stageTanh = 4,
    h_delay = 7,
};

float gainCompensation = 0.5;

void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso,
                      bool applyGainCompensation, SurgeStorage *storage)
{
    float lC[n_cm_coeffs];
    auto cutoff = VintageLadder::Common::clampedFrequency(freq, storage);
    lC[h_cutoff] = cutoff;

    // Heuristically, at higher cutoffs the resonance becomes less stable. This is purely ear tuned
    // at 48kHz with noise input
    float co = std::max(cutoff - samplerate * 0.33333, 0.0) * 0.1 * dsamplerate_os_inv;
    float gctrim = applyGainCompensation ? 0.05 : 0.0;

    reso = limit_range(limit_range(reso, 0.0f, 0.9925f), 0.0f, 0.994f - co - gctrim);
    lC[h_res] = reso;

    double fc = cutoff * dsamplerate_os_inv;
    lC[h_fc] = fc;

    lC[h_gComp] = 0.0;

    if (applyGainCompensation)
    {
        lC[h_gComp] = gainCompensation;
    }

    cm->FromDirect(lC);
}

__m128 process(QuadFilterUnitState *__restrict f, __m128 in)
{
#define F(a) _mm_set_ps1(a)
#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define S(a, b) _mm_sub_ps(a, b)

    static const __m128 dFac = F(0.5), half = F(0.5), one = F(1.0), four = F(4.0),
                        m18730 = F(1.8730), m04955 = F(0.4995), mneg06490 = F(-0.6490),
                        m09988 = F(0.9988), mneg39364 = F(-3.9364), m18409 = F(1.8409),
                        m09968 = F(0.9968), thermal = F(1.f / 70.f), oneoverthermal = F(70),
                        neg2pi = F(-2.0 * M_PI);

    __m128 outputOS[2];

    for (int j = 0; j < 2; ++j)
    {
        auto fc = f->C[h_fc];
        auto res = f->C[h_res];

        auto fr = M(fc, half);
        auto fc2 = M(fc, fc);
        auto fc3 = M(fc, fc2);

        // double fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
        auto fcr = A(M(m18730, fc3), A(M(m04955, fc2), A(M(mneg06490, fc), m09988)));
        // auto acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
        auto acr = A(M(mneg39364, fc2), A(M(m18409, fc), m09968));

        // auto tune = (1.0 - exp(-((2 * M_PI) * f * fcr))) / thermal;
        auto tune = M(S(one, Surge::DSP::fastexpSSE(M(neg2pi, M(fr, fcr)))), oneoverthermal);
        // auto resquad = 4.0 * res * arc;
        auto resquad = M(four, M(res, acr));

        for (int k = 0; k < n_hcoeffs; ++k)
        {
            f->C[k] = _mm_add_ps(f->C[k], _mm_mul_ps(dFac, f->dC[k]));
        }

        // float input = in - resQuad * ( delay[5] - gComp * in )   // Model as an impulse stream
        auto input =
            _mm_sub_ps(in, _mm_mul_ps(resquad, S(f->R[h_delay + 5], M(f->C[h_gComp], in))));

        // delay[0] = stage[0] = delay[0] + tune * (tanh(input * thermal) - stageTanh[0]);
        f->R[h_stage + 0] =
            A(f->R[h_delay + 0],
              M(tune, S(Surge::DSP::fasttanhSSEclamped(M(input, thermal)), f->R[h_stageTanh + 0])));
        f->R[h_delay + 0] = f->R[h_stage + 0];

        for (int k = 1; k < 4; k++)
        {
            // input = stage[k-1];
            input = f->R[h_stage + k - 1];

            // stage[k] = delay[k] + tune * ((stageTanh[k-1] = tanh(input * thermal)) - (k != 3 ?
            // stageTanh[k] : tanh(delay[k] * thermal)));
            f->R[h_stageTanh + k - 1] = Surge::DSP::fasttanhSSEclamped(M(input, thermal));
            f->R[h_stage + k] =
                A(f->R[h_delay + k],
                  M(tune,
                    S(f->R[h_stageTanh + k - 1],
                      (k != 3 ? f->R[h_stageTanh + k]
                              : Surge::DSP::fasttanhSSEclamped(M(f->R[h_delay + k], thermal))))));

            // delay[k] = stage[k];
            f->R[h_delay + k] = f->R[h_stage + k];
        }

        // 0.5 sample delay for phase compensation
        // delay[5] = (stage[3] + delay[4]) * 0.5;
        f->R[h_delay + 5] = M(_mm_set_ps1(0.5), A(f->R[h_stage + 3], f->R[h_delay + 4]));

        // delay[4] = stage[3];
        f->R[h_delay + 4] = f->R[h_stage + 3];

        outputOS[j] = f->R[h_delay + 5];
    }

    return outputOS[1];
#undef M
#undef A
#undef S
#undef F
}
} // namespace Huov
} // namespace VintageLadder