#include "globals.h"
#include "K35.h"
#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vt_dsp/basic_dsp.h"
#include "FastMath.h"

/*
** This contains various adaptations of the models found at
** https://github.com/TheWaveWarden/odin2/blob/master/Source/audio/Filters/Korg35Filter.cpp
*/

static float clampedFrequency( float pitch, SurgeStorage *storage )
{
   auto freq = storage->note_to_pitch_ignoring_tuning( pitch + 69 ) * Tunings::MIDI_0_FREQ;
   freq = limit_range( (float)freq, 5.f, (float)( dsamplerate_os * 0.3f ) );
   return freq;
}

// like in FastMath.h this is from
// https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/maths/juce_FastMathApproximations.h
// but it's kept here because we only need the double version here
static inline double fastdtan(double x) noexcept
{
   auto x2 = x * x;
   auto numerator = x * (-135135 + x2 * (17325 + x2 * (-378 + x2)));
   auto denominator = -135135 + x2 * (62370 + x2 * (-3150 + 28 * x2));
   return numerator / denominator;
}

#define F(a) _mm_set_ps1( a )         
#define M(a,b) _mm_mul_ps( a, b )
#define D(a,b) _mm_div_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )

// note that things that were NOPs in the Odin code have been removed.
// m_gamma remains 1.0 so xn * m_gamma == xn; that's a NOP
// m_feedback remains 0, that's a NOP
// m_epsilon remains 0, that's a NOP
// m_a_0 remains 1 so that's also a NOP
// so we only need to compute:
// (xn - z) * alpha + za
static inline __m128 doLpf(const __m128 &G, const __m128 &input, __m128 &z) noexcept {
   const __m128 v = M(S(input, z), G);
   const __m128 result = A(v, z);
   z = A(v, result);
   return result;
}
static inline __m128 doHpf(const __m128 &G, const __m128 &input, __m128 &z) noexcept {
   return S(input, doLpf(G, input, z));
}

namespace K35Filter
{
   enum k35_coeffs { 
      k35_G = 0, // aka alpha
      k35_lb,    // LPF beta
      k35_hb,    // HPF beta
      k35_k,     // k (m_k_modded)
      k35_alpha, // aka m_alpha
      k35_saturation, // amount of saturation to apply (scaling before tanh)
      k35_saturation_blend, // above but clamped to 0..1, used to blend tanh version when <1
      k35_saturation_blend_inv, // above but inverted, used to blend non-tanh version when <1
   };

   enum k35_state {
      k35_lz, // LPF1 z-1 storage
      k35_hz, // HPF1 z-1 storage
      k35_2z, // xPF2 z-1 storage
   };

   void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, bool is_lowpass, float saturation, SurgeStorage *storage )
   {
      const double wd = clampedFrequency( freq, storage ) * 2.0 * M_PI;
      const double wa = (2.0 * dsamplerate_os) * fastdtan(wd * dsamplerate_os_inv * 0.5);
      const double g  = wa * dsamplerate_os_inv * 0.5;
      const double gp1= (1.0 + g); // g plus 1
      const double G  = g / gp1;

      const double k = reso * 2.0;
      // clamp to [0.01..1.96]
      const double mk = (k > 1.96) ? 1.96 : ((k < 0.01) ? 0.01 : k);

      cm->C[k35_G] = G;

      if(is_lowpass) {
         cm->C[k35_lb] = (mk - mk * G) / gp1;
         cm->C[k35_hb] = -1.0 / gp1;
      }
      else {
         cm->C[k35_lb] =  1.0 / gp1;
         cm->C[k35_hb] = -G / gp1;
      }

      cm->C[k35_k] = mk;

      cm->C[k35_alpha] = 1.0 / (1.0 - mk * G + mk * G * G);

      cm->C[k35_saturation] = saturation;
      cm->C[k35_saturation_blend] = fminf(saturation, 1.0);
      cm->C[k35_saturation_blend_inv] = 1.0 - cm->C[k35_saturation_blend];
   }

   __m128 process_lp( QuadFilterUnitState * __restrict f, __m128 input )
   {
      const __m128 y1 = doLpf(f->C[k35_G], input, f->R[k35_lz]);
      // (lpf beta * lpf2 feedback) + (hpf beta * hpf1 feedback)
      const __m128 s35 = A(M(f->C[k35_lb], f->R[k35_2z]), M(f->C[k35_hb], f->R[k35_hz]));
      // alpha * (y1 + s35)
      const __m128 u_clean = M(f->C[k35_alpha], A(y1, s35));
      const __m128 u_driven = Surge::DSP::fasttanhSSEclamped(M(u_clean, f->C[k35_saturation]));
      const __m128 u = A(M(u_clean, f->C[k35_saturation_blend_inv]), M(u_driven, f->C[k35_saturation_blend]));

      // mk * lpf2(u)
      const __m128 y = M(f->C[k35_k], doLpf(f->C[k35_G], u, f->R[k35_2z]));
      doHpf(f->C[k35_G], y, f->R[k35_hz]);

      const __m128 result = D(y, f->C[k35_k]);

      return result;
   }

   __m128 process_hp( QuadFilterUnitState * __restrict f, __m128 input )
   {
      const __m128 y1 = doHpf(f->C[k35_G], input, f->R[k35_hz]);
      // (lpf beta * lpf2 feedback) + (hpf beta * hpf1 feedback)
      const __m128 s35 = A(M(f->C[k35_hb], f->R[k35_2z]), M(f->C[k35_lb], f->R[k35_lz]));
      // alpha * (y1 + s35)
      const __m128 u = M(f->C[k35_alpha], A(y1, s35));

      // mk * lpf2(u)
      const __m128 y_clean = M(f->C[k35_k], u);
      const __m128 y_driven = Surge::DSP::fasttanhSSEclamped(M(y_clean, f->C[k35_saturation]));
      const __m128 y = A(M(y_clean, f->C[k35_saturation_blend_inv]), M(y_driven, f->C[k35_saturation_blend]));

      doLpf(f->C[k35_G], doHpf(f->C[k35_G], y, f->R[k35_2z]), f->R[k35_lz]);

      const __m128 result = D(y, f->C[k35_k]);

      return result;
   }
#undef overdrive
#undef F
#undef M
#undef D
#undef A
#undef S
}
