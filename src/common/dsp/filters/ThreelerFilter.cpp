#include "globals.h"
#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vembertech/basic_dsp.h"
#include "FastMath.h"

/*
** This filter is an emulation of the "Threeler" VCF by
** Ian Fritz (https://ijfritz.byethost4.com/Threeler_board_doc.pdf).
** The filter consists of three one-pole filters with nonlinear
** resonance and global feedback. The filter has four "modes":
**   1. LPF -> LPF -> LPF
**   2. LPF -> HPF -> LPF
**   3. HPF -> LPF -> HPF
**   4. HPF -> HPF -> HPF
** The filter has three output points. For modes and three output
** points makes for a total of 12 sub-types.
**
** The details of the emulation are derived in a Pyton notebook here:
** https://github.com/surge-synthesizer/surge-python/blob/main/scripts/dsp_simulations/ThreelerAnalysis.ipynb
*/

namespace
{
constexpr float res_gain = 1.5f;
constexpr float in_gain = 4.0f;
constexpr float out_gain = 1.0f / in_gain;

constexpr int nIterGlobal = 3;
constexpr int nIterStage = 1;

// each OTA is a little different :)
constexpr float ota1bp = 0.88f;
constexpr float ota1bn = 1.0f;
constexpr float ota2bp = 0.9f;
constexpr float ota2bn = 0.97f;
constexpr float ota3bp = 0.95f;
constexpr float ota3bn = 1.025f;

// for computing the resonance coefficient (k)
constexpr float Iabc = 8.0f;    // milliAmps
constexpr float Rload = 220.0f; // kOhms
const auto res_factor_db = std::log10(Iabc * Rload);

} // namespace

static float clampedFrequency(float pitch, SurgeStorage *storage)
{
    auto freq = storage->note_to_pitch_ignoring_tuning(pitch + 69) * Tunings::MIDI_0_FREQ;
    freq = limit_range((float)freq, 5.f, (float)(dsamplerate_os * 0.3f));
    return freq;
}

#define F(a) _mm_set_ps1(a)
#define M(a, b) _mm_mul_ps(a, b)
#define D(a, b) _mm_div_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define S(a, b) _mm_sub_ps(a, b)
#define N(a) S(F(0.0f), a)

/** inverse square root sigmoid */
static inline __m128 thr_sigmoid(__m128 x, float beta)
{
    __m128 vtmp = _mm_mul_ps(x, x);           // calculate in*in
    __m128 vtmp2 = _mm_add_ps(vtmp, F(beta)); // in*in+1.f
    vtmp = _mm_rsqrt_ps(vtmp2);               // 1/sqrt(in*in+1.f)
    return _mm_mul_ps(vtmp, x);               // in*1/sqrt(in*in+1)
}

static inline __m128 sech2_with_tanh(__m128 tanh_value)
{
    static const auto one = F(1.0f);
    return S(one, M(tanh_value, tanh_value));
}

namespace OnePoleLPF
{
static inline __m128 linOutput(__m128 x, __m128 z, __m128 b_coeff, __m128 a_coeff)
{
    return M(a_coeff, A(M(b_coeff, x), z));
}

static inline __m128 nonlinOutput(__m128 tanh_x, __m128 tanh_y, __m128 z, __m128 b_coeff)
{
    return A(M(b_coeff, S(tanh_x, tanh_y)), z);
}

static inline __m128 getDerivative(__m128 tanh_y, __m128 b_coeff)
{
    static const auto one = F(1.0f);
    return S(M(N(b_coeff), sech2_with_tanh(tanh_y)), one);
}

static inline __m128 getXDerivative(__m128 tanh_x, __m128 b_coeff)
{
    return M(b_coeff, sech2_with_tanh(tanh_x));
}

static inline __m128 process(__m128 tanh_x, __m128 z, __m128 estimate, __m128 b_coeff,
                             __m128 a_coeff, float beta)
{
    estimate = linOutput(tanh_x, z, b_coeff, a_coeff);
    for (int i = 0; i < nIterStage; ++i)
    {
        auto tanh_y = thr_sigmoid(estimate, beta);
        auto residue = S(nonlinOutput(tanh_x, tanh_y, z, b_coeff), estimate);
        estimate = S(estimate, D(residue, getDerivative(tanh_y, b_coeff)));
    }

    return estimate;
}
} // namespace OnePoleLPF

namespace OnePoleHPF
{
static inline __m128 linOutput(__m128 x_minus_x1_plus_z, __m128 a_coeff)
{
    return M(a_coeff, x_minus_x1_plus_z);
}

static inline __m128 nonlinOutput(__m128 x_minus_x1_plus_z, __m128 tanh_y, __m128 b_coeff)
{
    return A(M(N(b_coeff), tanh_y), x_minus_x1_plus_z);
}

static inline __m128 getDerivative(__m128 tanh_y, __m128 b_coeff)
{
    static const auto neg_one = F(-1.0f);
    return A(M(N(b_coeff), sech2_with_tanh(tanh_y)), neg_one);
}

static inline __m128 getXDerivative() { return F(2.0f); }

static inline __m128 process(__m128 x, __m128 x1, __m128 z, __m128 estimate, __m128 b_coeff,
                             __m128 a_coeff, float beta)
{
    auto x_minus_x1_plus_z = A(S(x, x1), z);
    estimate = linOutput(x_minus_x1_plus_z, a_coeff);
    for (int i = 0; i < nIterStage; ++i)
    {
        auto tanh_y = thr_sigmoid(estimate, beta);
        auto residue = S(nonlinOutput(x_minus_x1_plus_z, tanh_y, b_coeff), estimate);
        estimate = S(estimate, D(residue, getDerivative(tanh_y, b_coeff)));
    }

    return estimate;
}
} // namespace OnePoleHPF

namespace OnePoleLPF_FB
{
static inline __m128 linOutput(__m128 bx, __m128 z_minus_fb_plus_fb1, __m128 a_coeff)
{
    return M(a_coeff, A(bx, z_minus_fb_plus_fb1));
}

static inline __m128 nonlinOutput(__m128 tanh_x, __m128 tanh_y, __m128 z_minus_fb_plus_fb1,
                                  __m128 b_coeff)
{
    return A(M(b_coeff, S(tanh_x, tanh_y)), z_minus_fb_plus_fb1);
}

static inline __m128 getDerivative(__m128 tanh_y, __m128 b_coeff)
{
    return OnePoleLPF::getDerivative(tanh_y, b_coeff);
}

static inline __m128 getXDerivative()
{
    static const auto two = F(2.0f);
    return two;
}

static inline __m128 process(__m128 tanh_x, __m128 z, __m128 fb, __m128 fb1, __m128 estimate,
                             __m128 b_coeff, __m128 a_coeff, __m128 bx)
{
    auto z_minus_fb_plus_fb1 = A(S(z, fb), fb1);
    estimate = linOutput(bx, z_minus_fb_plus_fb1, a_coeff);
    for (int i = 0; i < nIterStage; ++i)
    {
        auto tanh_y = thr_sigmoid(estimate, ota1bn);
        auto residue = S(nonlinOutput(tanh_x, tanh_y, z_minus_fb_plus_fb1, b_coeff), estimate);
        estimate = S(estimate, D(residue, getDerivative(tanh_y, b_coeff)));
    }

    return estimate;
}
} // namespace OnePoleLPF_FB

namespace OnePoleHPF_FB
{
static inline __m128 linOutput(__m128 x_minus_x1_plus_z, __m128 tanh_fb, __m128 a_coeff,
                               __m128 b_coeff)
{
    return M(a_coeff, A(M(b_coeff, tanh_fb), x_minus_x1_plus_z));
}

static inline __m128 nonlinOutput(__m128 x_minus_x1_plus_z, __m128 tanh_y, __m128 tanh_fb,
                                  __m128 b_coeff)
{
    return A(M(b_coeff, S(tanh_fb, tanh_y)), x_minus_x1_plus_z);
}

static inline __m128 getDerivative(__m128 tanh_y, __m128 b_coeff)
{
    static const auto neg_one = F(-1.0f);
    return A(M(N(b_coeff), sech2_with_tanh(tanh_y)), neg_one);
}

static inline __m128 getFBDerivative(__m128 tanh_fb, __m128 b_coeff)
{
    return M(b_coeff, sech2_with_tanh(tanh_fb));
}

static inline __m128 process(__m128 x_minus_x1_plus_z, __m128 tanh_fb, __m128 estimate,
                             __m128 b_coeff, __m128 a_coeff)
{
    estimate = linOutput(x_minus_x1_plus_z, tanh_fb, a_coeff, b_coeff);
    for (int i = 0; i < nIterStage; ++i)
    {
        auto tanh_y = thr_sigmoid(estimate, ota1bn);
        auto residue = S(nonlinOutput(x_minus_x1_plus_z, tanh_y, tanh_fb, b_coeff), estimate);
        estimate = S(estimate, D(residue, getDerivative(tanh_y, b_coeff)));
    }

    return estimate;
}
} // namespace OnePoleHPF_FB

namespace ResWaveshaper
{
constexpr float alpha = 1.0168177f;
const float log_alpha = std::log(alpha);
constexpr float beta = 9.03240196f;
const float beta_exp = beta * log_alpha;
constexpr float c = 0.222161f;
constexpr float bias = 8.2f;

constexpr float max_val = 7.5f;
constexpr float mult = 10.0f;
constexpr float one = 0.99f;
constexpr float oneOverMult = one / mult;
const float betaExpOverMult = beta_exp / mult;

static inline __m128 sign_ps(__m128 x)
{
    static const __m128 zero = _mm_setzero_ps();
    static const __m128 one = _mm_set1_ps(1.0f);
    static const __m128 neg_one = _mm_set1_ps(-1.0f);

    __m128 positive = _mm_and_ps(_mm_cmpgt_ps(x, zero), one);
    __m128 negative = _mm_and_ps(_mm_cmplt_ps(x, zero), neg_one);

    return _mm_or_ps(positive, negative);
}

static inline __m128 res_func_ps(__m128 x)
{
    x = M(F(mult), x);

    auto x_abs = abs_ps(x);
    auto x_less_than = _mm_cmplt_ps(x_abs, F(max_val));

    auto y = A(N(Surge::DSP::fastexpSSE(M(F(beta_exp), N(abs_ps(A(x, F(c))))))), F(bias));
    y = M(sign_ps(x), M(y, F(oneOverMult)));

    return _mm_or_ps(_mm_and_ps(x_less_than, M(x, F(oneOverMult))), _mm_andnot_ps(x_less_than, y));
}

static inline __m128 res_deriv_ps(__m128 x)
{
    x = M(F(mult), x);

    auto x_abs = abs_ps(x);
    auto x_less_than = _mm_cmplt_ps(x_abs, F(max_val));

    auto y = A(Surge::DSP::fastexpSSE(M(F(beta_exp), N(abs_ps(A(x, F(c)))))), F(betaExpOverMult));

    return _mm_or_ps(_mm_and_ps(x_less_than, F(one)), _mm_andnot_ps(x_less_than, y));
}
} // namespace ResWaveshaper

namespace ThreelerFilter
{
enum thr_coeffs
{
    thr_b0 = 0, // b-coefficient for 1st stage
    thr_a0,     // a-coefficient for 1st stage
    thr_b1,     // b-coefficient for 2nd stage
    thr_a1,     // a-coefficient for 2nd stage
    thr_b2,     // b-coefficient for 3rd stage
    thr_a2,     // a-coefficient for 3rd stage
    thr_k,      // resonance coefficient for the filter
    n_thr_coeff
};

enum thr_state
{
    thr_z0,  // output state for 1st stage
    thr_x0,  // input state for 1st stage
    thr_z1,  // output state for 2nd stage
    thr_x1,  // input state for 2nd stage
    thr_z2,  // output state for 3rd stage
    thr_x2,  // input state for 3rd stage
    thr_fb,  // state for global feedback
    thr_fb1, // state for global feedback, delayed
};

void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, int subtype,
                      SurgeStorage *storage)
{
    float C[n_cm_coeffs];

    constexpr float capVal = 220e-12f;
    const float T = 1.0 / dsamplerate_os;
    const float wc = 2.0f * M_PI * clampedFrequency(freq, storage) * T;
    const float g = (capVal / T) * (std::exp(wc) - 1.0f);

    // coeffs are slightly perturbed for each filter stage
    // stage 1
    C[thr_b0] = 0.998f * T * g / capVal;
    C[thr_a0] = 1.0f / (1.0f + C[thr_b0]);

    // stage 2
    C[thr_b1] = 1.0012f * T * g / capVal;
    C[thr_a1] = 1.0f / (1.0f + C[thr_b1]);

    // stage 3
    C[thr_b2] = T * g / capVal;
    C[thr_a2] = 1.0f / (1.0f + C[thr_b2]);

    // resonance
    reso = limit_range(reso, 0.f, 1.f);
    C[thr_k] = -(std::pow(10.0f, res_factor_db * reso) + 1.0f);

    cm->FromDirect(C);
}

__m128 process(QuadFilterUnitState *__restrict f, __m128 in)
{
    // input gain
    in = M(F(in_gain), in);

    // lower 2 bits of subtype is the filter mode
    const int mode = f->WP[0] & 3;
    // next two bits after that select the saturator
    const int out_stage = ((f->WP[0] >> 2) & 3);

    // stuff for stage 1
    auto z0 = f->R[thr_z0];
    auto x0 = f->R[thr_x0];
    auto estimate0 = z0;
    const auto b0 = f->C[thr_b0];
    const auto a0 = f->C[thr_a0];

    // stuff for stage 3
    auto z1 = f->R[thr_z1];
    auto x1 = f->R[thr_x1];
    auto estimate1 = z1;
    const auto b1 = f->C[thr_b1];
    const auto a1 = f->C[thr_a1];

    // stuff for stage 3
    auto z2 = f->R[thr_z2];
    auto x2 = f->R[thr_x2];
    auto res_out = x2;
    auto estimate2 = z2;
    const auto b2 = f->C[thr_b2];
    const auto a2 = f->C[thr_a2];

    const auto k_ps = f->C[thr_k];

    // define local variables
    __m128 tanh_x0, tanh_x1, tanh_x2, tanh_fb, f0_deriv, f1_deriv, f2_deriv, bx, hpf_in;
    switch (mode)
    {
    case 0: // lowpass
    case 1:
        tanh_x0 = thr_sigmoid(in, ota1bp);
        bx = M(b0, tanh_x0);
        break;
    case 2: // highpass
    case 3:
        hpf_in = A(S(in, x0), z0);
        break;
    };
    auto estimate = f->R[thr_fb];

    // global feedback iteration
    for (int i = 0; i < nIterGlobal; ++i)
    {
        // filter stage 1 (with feedback input)
        switch (mode)
        {
        case 0: // lowpass
        case 1:
            estimate0 =
                OnePoleLPF_FB::process(tanh_x0, z0, estimate, f->R[thr_fb1], estimate0, b0, a0, bx);
            f0_deriv = OnePoleLPF_FB::getXDerivative();
            break;
        case 2: // highpass
        case 3:
            tanh_fb = thr_sigmoid(estimate, ota1bp);
            estimate0 = OnePoleHPF_FB::process(hpf_in, tanh_fb, estimate0, b0, a0);
            f0_deriv = OnePoleHPF_FB::getFBDerivative(tanh_fb, b0);
            break;
        };

        // filter stage 2
        switch (mode)
        {
        case 0: // lowpass
        case 2:
            tanh_x1 = thr_sigmoid(estimate0, ota2bp);
            estimate1 = OnePoleLPF::process(tanh_x1, z1, estimate1, b1, a1, ota2bn);
            f1_deriv = OnePoleLPF::getXDerivative(tanh_x1, b1);
            break;
        case 1: // highpass
        case 3:
            estimate1 = OnePoleHPF::process(estimate0, x1, z1, estimate1, b1, a1, ota2bn);
            f1_deriv = OnePoleHPF::getXDerivative();
            break;
        };

        // resonance stage
        auto k_times_f1_out = M(k_ps, estimate1);
        res_out = M(F(1.0f / res_gain), ResWaveshaper::res_func_ps(M(F(res_gain), k_times_f1_out)));
        auto res_deriv = ResWaveshaper::res_deriv_ps(k_times_f1_out);

        // filter stage 3
        switch (mode)
        {
        case 0: // lowpass
        case 1:
            tanh_x2 = thr_sigmoid(res_out, ota3bp);
            estimate2 = OnePoleLPF::process(tanh_x2, z2, estimate2, b2, a2, ota3bn);
            f2_deriv = OnePoleLPF::getXDerivative(tanh_x2, b2);
            break;
        case 2:
        case 3:
            estimate2 = OnePoleHPF::process(res_out, x2, z2, estimate2, b2, a2, ota3bn);
            f2_deriv = OnePoleHPF::getXDerivative();
            break;
        };

        auto num = S(estimate, estimate2);
        auto den = S(F(1.0f), M(k_ps, M(res_deriv, M(f0_deriv, M(f1_deriv, f2_deriv)))));
        estimate = S(estimate, D(num, den));
    }

    // update filter state
    f->R[thr_z0] = estimate0;
    f->R[thr_x0] = in;
    f->R[thr_z1] = estimate1;
    f->R[thr_x1] = estimate0;
    f->R[thr_z2] = estimate2;
    f->R[thr_x2] = res_out;
    f->R[thr_fb1] = f->R[thr_fb];
    f->R[thr_fb] = estimate;

    // update coefficients
    for (int i = 0; i < n_thr_coeff; ++i)
        f->C[i] = A(f->C[i], f->dC[i]);

    // return output for whichever stage we need
    switch (out_stage)
    {
    case 0: // first stage
        return M(F(out_gain), estimate0);
    case 1: // second stage
        return M(F(out_gain), estimate1);
    case 2: // third (final) stage
    default:
        return M(F(out_gain), estimate);
    };
}
} // namespace ThreelerFilter
