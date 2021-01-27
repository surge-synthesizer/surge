#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vt_dsp/basic_dsp.h"
#include "FastMath.h"

/*
** This contains an adaptation of the filter from
** https://github.com/TheWaveWarden/odin2/blob/master/Source/audio/Filters/Korg35Filter.cpp
*/

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

// note that things that were NOPs in the Odin code have been removed.
// m_gamma remains 1.0 so xn * m_gamma == xn; that's a NOP
// m_feedback remains 0, that's a NOP
// m_epsilon remains 0, that's a NOP
// m_a_0 remains 1 so that's also a NOP
// so we only need to compute:
// (xn - z) * alpha + za
static inline __m128 doLpf(const __m128 &G, const __m128 &input, __m128 &z) noexcept
{
    const __m128 v = M(S(input, z), G);
    const __m128 result = A(v, z);
    z = A(v, result);
    return result;
}
static inline __m128 doHpf(const __m128 &G, const __m128 &input, __m128 &z) noexcept
{
    return S(input, doLpf(G, input, z));
}

namespace K35Filter
{
enum k35_coeffs
{
    k35_G = 0,                // aka alpha
    k35_lb,                   // LPF beta
    k35_hb,                   // HPF beta
    k35_k,                    // k (m_k_modded)
    k35_alpha,                // aka m_alpha
    k35_saturation,           // amount of saturation to apply (scaling before tanh)
    k35_saturation_blend,     // above but clamped to 0..1, used to blend tanh version when <1
    k35_saturation_blend_inv, // above but inverted, used to blend non-tanh version when <1
};

enum k35_state
{
    k35_lz, // LPF1 z-1 storage
    k35_hz, // HPF1 z-1 storage
    k35_2z, // xPF2 z-1 storage
};

void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, bool is_lowpass,
                      float saturation, SurgeStorage *storage)
{
    float C[n_cm_coeffs];

    const float wd = clampedFrequency(freq, storage) * 2.0f * M_PI;
    const float wa = (2.0f * dsamplerate_os) * Surge::DSP::fasttan(wd * dsamplerate_os_inv * 0.5);
    const float g = wa * dsamplerate_os_inv * 0.5f;
    const float gp1 = (1.0f + g); // g plus 1
    const float G = g / gp1;

    const float k = reso * 1.96f;
    // clamp to [0.01..1.96]
    const float mk = (k > 1.96f) ? 1.96f : ((k < 0.01f) ? 0.01f : k);

    C[k35_G] = G;

    if (is_lowpass)
    {
        C[k35_lb] = (mk - mk * G) / gp1;
        C[k35_hb] = -1.0f / gp1;
    }
    else
    {
        C[k35_lb] = 1.0f / gp1;
        C[k35_hb] = -G / gp1;
    }

    C[k35_k] = mk;

    C[k35_alpha] = 1.0f / (1.0f - mk * G + mk * G * G);

    C[k35_saturation] = saturation;
    C[k35_saturation_blend] = fminf(saturation, 1.0f);
    C[k35_saturation_blend_inv] = 1.0f - C[k35_saturation_blend];

    cm->FromDirect(C);
}

#define process_coeffs()                                                                           \
    for (int i = 0; i < n_cm_coeffs; ++i)                                                          \
    {                                                                                              \
        f->C[i] = A(f->C[i], f->dC[i]);                                                            \
    }

__m128 process_lp(QuadFilterUnitState *__restrict f, __m128 input)
{
    process_coeffs();

    const __m128 y1 = doLpf(f->C[k35_G], input, f->R[k35_lz]);
    // (lpf beta * lpf2 feedback) + (hpf beta * hpf1 feedback)
    const __m128 s35 = A(M(f->C[k35_lb], f->R[k35_2z]), M(f->C[k35_hb], f->R[k35_hz]));
    // alpha * (y1 + s35)
    const __m128 u_clean = M(f->C[k35_alpha], A(y1, s35));
    const __m128 u_driven = Surge::DSP::fasttanhSSEclamped(M(u_clean, f->C[k35_saturation]));
    const __m128 u =
        A(M(u_clean, f->C[k35_saturation_blend_inv]), M(u_driven, f->C[k35_saturation_blend]));

    // mk * lpf2(u)
    const __m128 y = M(f->C[k35_k], doLpf(f->C[k35_G], u, f->R[k35_2z]));
    doHpf(f->C[k35_G], y, f->R[k35_hz]);

    const __m128 result = D(y, f->C[k35_k]);

    return result;
}

__m128 process_hp(QuadFilterUnitState *__restrict f, __m128 input)
{
    process_coeffs();

    const __m128 y1 = doHpf(f->C[k35_G], input, f->R[k35_hz]);
    // (lpf beta * lpf2 feedback) + (hpf beta * hpf1 feedback)
    const __m128 s35 = A(M(f->C[k35_hb], f->R[k35_2z]), M(f->C[k35_lb], f->R[k35_lz]));
    // alpha * (y1 + s35)
    const __m128 u = M(f->C[k35_alpha], A(y1, s35));

    // mk * lpf2(u)
    const __m128 y_clean = M(f->C[k35_k], u);
    const __m128 y_driven = Surge::DSP::fasttanhSSEclamped(M(y_clean, f->C[k35_saturation]));
    const __m128 y =
        A(M(y_clean, f->C[k35_saturation_blend_inv]), M(y_driven, f->C[k35_saturation_blend]));

    doLpf(f->C[k35_G], doHpf(f->C[k35_G], y, f->R[k35_2z]), f->R[k35_lz]);

    const __m128 result = D(y, f->C[k35_k]);

    return result;
}
#undef F
#undef M
#undef D
#undef A
#undef S
} // namespace K35Filter
