#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vembertech/basic_dsp.h"
#include "FastMath.h"

/*
** This contains an adaptation of the filter found at
** https://ccrma.stanford.edu/~jatin/ComplexNonlinearities/NLBiquad.html
** with coefficient calculation from
** https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
**
** A lot of code here is duplicated from NonlinearFeedback.cpp, perhaps in future they
** could be merged, but for the time being they're separate and nothing is shared.
*/

static float clampedFrequency(float pitch, SurgeStorage *storage)
{
    auto freq = storage->note_to_pitch_ignoring_tuning(pitch + 69) * Tunings::MIDI_0_FREQ;
    freq = limit_range((float)freq, 5.f, (float)(dsamplerate_os * 0.3f));
    return freq;
}

#define F(a) _mm_set_ps1(a)
#define M(a, b) _mm_mul_ps(a, b)
#define A(a, b) _mm_add_ps(a, b)
#define S(a, b) _mm_sub_ps(a, b)

enum Saturator
{
    SAT_TANH = 0,
    SAT_SOFT
};

static inline __m128 doNLFilter(const __m128 input, const __m128 a1, const __m128 a2,
                                const __m128 b0, const __m128 b1, const __m128 b2, const int sat,
                                __m128 &z1, __m128 &z2) noexcept
{
    // out = z1 + b0 * input
    const __m128 out = A(z1, M(b0, input));

    // z1 = z2 + b1 * input - a1 * out
    z1 = A(z2, S(M(b1, input), M(a1, out)));
    // z2 = b2 * input - a2 * out
    z2 = S(M(b2, input), M(a2, out));

    // now apply a nonlinearity to z1 and z2
    switch (sat)
    {
    case SAT_TANH:
        z1 = Surge::DSP::fasttanhSSEclamped(z1);
        z2 = Surge::DSP::fasttanhSSEclamped(z2);
        break;
    default:
        z1 = softclip_ps(z1); // note, this is a bit different to Jatin's softclipper
        z2 = softclip_ps(z2);
        break;
    }
    return out;
}

namespace NonlinearStatesFilter
{
enum nls_coeffs
{
    nls_a1 = 0,
    nls_a2,
    nls_b0,
    nls_b1,
    nls_b2,
    n_nls_coeff
};

enum dlf_state
{
    nls_z1, // 1st z-1 state for first  stage
    nls_z2, // 2nd z-1 state for first  stage
    nls_z3, // 1st z-1 state for second stage
    nls_z4, // 2nd z-1 state for second stage
    nls_z5, // 1st z-1 state for third  stage
    nls_z6, // 2nd z-1 state for third  stage
    nls_z7, // 1st z-1 state for fourth stage
    nls_z8, // 2nd z-1 state for fourth stage
};

void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, int type,
                      SurgeStorage *storage)
{
    float C[n_cm_coeffs];

    reso = limit_range(reso, 0.f, 1.f);

    const float q = ((reso * reso * reso) * 18.0f + 0.1f);

    const float wc = 2.0f * M_PI * clampedFrequency(freq, storage) / dsamplerate_os;

    const float wsin = Surge::DSP::fastsin(wc);
    const float wcos = Surge::DSP::fastcos(wc);
    const float alpha = wsin / (2.0f * q);

    // note we actually calculate the reciprocal of a0 because we only use a0 to normalize the
    // other coefficients, and multiplication by reciprocal is cheaper than dividing.
    const float a0r = 1.0f / (1.0f + alpha);

    C[nls_a1] = -2.0f * wcos * a0r;
    C[nls_a2] = (1.0f - alpha) * a0r;

    switch (type)
    {
    case fut_resonancewarp_lp: // lowpass
        C[nls_b1] = (1.0f - wcos) * a0r;
        C[nls_b0] = C[nls_b1] * 0.5f;
        C[nls_b2] = C[nls_b0];
        break;
    case fut_resonancewarp_hp: // highpass
        C[nls_b1] = -(1.0f + wcos) * a0r;
        C[nls_b0] = C[nls_b1] * -0.5f;
        C[nls_b2] = C[nls_b0];
        break;
    case fut_resonancewarp_n: // notch
        C[nls_b0] = a0r;
        C[nls_b1] = -2.0f * wcos * a0r;
        C[nls_b2] = C[nls_b0];
        break;
    case fut_resonancewarp_bp: // bandpass
        C[nls_b0] = wsin * 0.5f * a0r;
        C[nls_b1] = 0.0f;
        C[nls_b2] = -C[nls_b0];
        break;
    default: // allpass
        C[nls_b0] = C[nls_a2];
        C[nls_b1] = C[nls_a1];
        C[nls_b2] = 1.0f; // (1+a) / (1+a) = 1 (from normalising by a0)
        break;
    }

    cm->FromDirect(C);
}

__m128 process(QuadFilterUnitState *__restrict f, __m128 input)
{
    // lower 2 bits of subtype is the stage count
    const int stages = f->WP[0] & 3;
    // next two bits after that select the saturator
    const int sat = ((f->WP[0] >> 2) & 3);

    // n.b. stages is zero-indexed so use <=
    for (int stage = 0; stage <= stages; ++stage)
    {
        input = doNLFilter(input, f->C[nls_a1], f->C[nls_a2], f->C[nls_b0], f->C[nls_b1],
                           f->C[nls_b2], sat, f->R[nls_z1 + stage * 2], f->R[nls_z2 + stage * 2]);
    }

    for (int i = 0; i < n_nls_coeff; ++i)
    {
        f->C[i] = A(f->C[i], f->dC[i]);
    }

    return input;
}
} // namespace NonlinearStatesFilter
