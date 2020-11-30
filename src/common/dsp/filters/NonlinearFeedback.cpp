#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vt_dsp/basic_dsp.h"
#include "FastMath.h"

/*
** This contains an adaptation of the filter found at
** https://ccrma.stanford.edu/~jatin/ComplexNonlinearities/NLFeedback.html
** with coefficient calculation from
** https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
*/

static float clampedFrequency( float pitch, SurgeStorage *storage )
{
   auto freq = storage->note_to_pitch_ignoring_tuning( pitch + 69 ) * Tunings::MIDI_0_FREQ;
   freq = limit_range( (float)freq, 5.f, (float)( dsamplerate_os * 0.3f ) );
   return freq;
}

#define F(a) _mm_set_ps1( a )
#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )

enum Saturator {
    SAT_TANH = 0,
    SAT_SOFT,
    SAT_SINE,
    SAT_ASINH
};

// sine each element of a __m128 by breaking it into floats then reassembling
static inline __m128 fastsin_ps(const __m128 in) noexcept {
   float f[4];
   _mm_storeu_ps(f, in);
   f[0] = Surge::DSP::fastsin(f[0]);
   f[1] = Surge::DSP::fastsin(f[1]);
   f[2] = Surge::DSP::fastsin(f[2]);
   f[3] = Surge::DSP::fastsin(f[3]);
   return _mm_load_ps(f);
}

// asinh each element of a __m128 by breaking it into floats then reassembling
static inline __m128 asinh_ps(const __m128 in) noexcept {
   float f[4];
   _mm_storeu_ps(f, in);
   f[0] = asinhf(f[0]);
   f[1] = asinhf(f[1]);
   f[2] = asinhf(f[2]);
   f[3] = asinhf(f[3]);
   return _mm_load_ps(f);
}

// do not change this without also recalculating windowFactors!
static constexpr int extraOversample = 4;
static constexpr float extraOversampleInv = 1.0f / (float)extraOversample;

// biquad without nonlinearity - used for upsampling
// note, b2 optimised out because it will be the same as b0 for what we use it for
static inline __m128 doFilter(
      const __m128 input,
      const __m128 a1,
      const __m128 a2,
      const __m128 b0,
      const __m128 b1,
      __m128 &z1,
      __m128 &z2)
   noexcept
{
   // out = z1 + b0 * input
   const __m128 out = A(z1, M(b0, input));
   // z1 = z2 + b1 * input - a1 * out
   z1 = A(z2, S(M(b1, input), M(a1, out)));
   // z2 = b2 * input - a2 * out
   z2 = S(M(b0, input), M(a2, out));
   return out;
}

// filter with a nonlinearity - what we actually want to hear
static inline __m128 doNLFilter(
      const __m128 input,
      const __m128 a1,
      const __m128 a2,
      const __m128 b0,
      const __m128 b1,
      const __m128 b2,
      const int sat,
      __m128 &z1,
      __m128 &z2)
   noexcept
{
   // out = z1 + b0 * input
   const __m128 out = A(z1, M(b0, input));

   // nonlinear feedback = saturator(out)
   __m128 nf;
   switch(sat){
      case SAT_TANH:
         nf = Surge::DSP::fasttanhSSEclamped(out);
         break;
      case SAT_SOFT:
         nf = softclip_ps(out); // note, this is a bit different to Jatin's softclipper
         break;
      case SAT_FASTSIN:
         nf = fastsin_ps(out);
         break;
      default: // SAT_ASINH
         nf = asinh_ps(out);
         break;
   }

   // z1 = z2 + b1 * input - a1 * nf
   z1 = A(z2, S(M(b1, input), M(a1, nf)));
   // z2 = b2 * input - a2 * nf
   z2 = S(M(b2, input), M(a2, nf));
   return out;
}

namespace NonlinearFeedbackFilter
{
   enum nlf_coeffs {
      nlf_a1 = 0,
      nlf_a2,
      nlf_b0,
      nlf_b1,
      nlf_b2,
      n_nlf_coeff
   };

   enum dlf_state {
      nlf_z1, // 1st z-1 state for first  stage
      nlf_z2, // 2nd z-1 state for first  stage
      nlf_z3, // 1st z-1 state for second stage
      nlf_z4, // 2nd z-1 state for second stage
      nlf_z5, // 1st z-1 state for third  stage
      nlf_z6, // 2nd z-1 state for third  stage
      nlf_z7, // 1st z-1 state for fourth stage
      nlf_z8, // 2nd z-1 state for fourth stage
      nlf_upz1, // 1st z-1 state for upsampling filter
      nlf_upz2, // 2nd z-2 state for upsampling filter
      // rest of the R[] state used for 3 more upsampling filter stages
   };

   void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, int type, bool oversample, SurgeStorage *storage )
   {
      float C[n_cm_coeffs];

      const float q = ((reso * reso * reso) * 18.0f + 0.1f);

      const float oversampling = oversample ? extraOversample : 1.0f;

      const float wc = 2.0f * M_PI * clampedFrequency(freq, storage) / (dsamplerate_os * oversampling);

      const float wsin  = Surge::DSP::fastsin(wc);
      const float wcos  = Surge::DSP::fastcos(wc);
      const float alpha = wsin / (2.0f * q);

      // note we actually calculate the reciprocal of a0 because we only use a0 to normalize the
      // other coefficients, and multiplication by reciprocal is cheaper than dividing.
      const float a0r  = 1.0f / (1.0f + alpha);

      C[nlf_a1] = -2.0f * wcos    * a0r;
      C[nlf_a2] = (1.0f - alpha)  * a0r;

      switch(type){
         case fut_nonlinearfb_lp: // lowpass
         case fut_nonlinearfb_lp_os:
            C[nlf_b1] =  (1.0f - wcos) * a0r;
            C[nlf_b0] = C[nlf_b1] *  0.5f;
            C[nlf_b2] = C[nlf_b0];
            break;
         case fut_nonlinearfb_hp: // highpass
         case fut_nonlinearfb_hp_os:
            C[nlf_b1] = -(1.0f + wcos) * a0r;
            C[nlf_b0] = C[nlf_b1] * -0.5f;
            C[nlf_b2] = C[nlf_b0];
            break;
         case fut_nonlinearfb_n: // notch
         case fut_nonlinearfb_n_os:
            C[nlf_b0] = a0r;
            C[nlf_b1] = -2.0f * wcos   * a0r;
            C[nlf_b2] = C[nlf_b0];
            break;
         default: // bandpass
            C[nlf_b0] = wsin * 0.5f    * a0r;
            C[nlf_b1] = 0.0f;
            C[nlf_b2] = -C[nlf_b0];
            break;
      }

      cm->FromDirect(C);
   }

   __m128 process( QuadFilterUnitState * __restrict f, __m128 input )
   {
      // lower 2 bits of subtype is the stage count
      const int stages = f->WP[0] & 3;
      // next two bits after that select the saturator
      const int sat = ((f->WP[0] >> 2) & 3);

      // n.b. stages is zero-indexed so use <=
      for(int stage = 0; stage <= stages; ++stage){
         input =
            doNLFilter(input,
                  f->C[nlf_a1],
                  f->C[nlf_a2],
                  f->C[nlf_b0],
                  f->C[nlf_b1],
                  f->C[nlf_b2],
                  sat,
                  f->R[nlf_z1 + stage*2],
                  f->R[nlf_z2 + stage*2]);
      }

      for(int i=0; i < n_nlf_coeff; ++i){
         f->C[i] = A(f->C[i], f->dC[i]);
      }

      return input;
   }

   __m128 process_oversampled( QuadFilterUnitState * __restrict f, __m128 input )
   {
      // lower 2 bits of subtype is the stage count
      const int stages = f->WP[0] & 3;
      // next two bits after that select the saturator
      const int sat = ((f->WP[0] >> 2) & 3);

      // oversampling method copied from VintageLadders.cpp, look there for better explanation

      // prescale f->dC so it's for each oversampled sample
      static const __m128 dFac = F(extraOversampleInv);
      for(int i=0; i < n_nlf_coeff; ++i){
         f->dC[i] = M(f->dC[i], dFac);
      }

      // do a crappy chain of 4 butterworth biquads for post-upsampling filter.
      // calculated with python code:
      // import math
      // w0 = math.tau * 1/8  # 1/8 of the 4x sample rate is 1/2 of the original sample rate
      // Q = 1/math.sqrt(2)   # Butterworth Q
      // alpha = math.sin(w0)/(2*Q)
      // b1 = (1 - math.cos(w0))
      // b0 = b1 / 2
      // a0 = 1 + alpha
      // a1 = -2 * math.cos(w0) * (1/a0)
      // a2 = (1-alpha) * (1/a0)
      // print(a1, a2, b0, b1)
      static const __m128 upsample_a1 = F(-0.9428090415820634f);
      static const __m128 upsample_a2 = F( 0.3333333333333333f);
      static const __m128 upsample_b0 = F( 0.1464466094067262f);
      static const __m128 upsample_b1 = F( 0.2928932188134524f);

      __m128 outputOS[extraOversample];
      for(int osi = 0; osi < extraOversample; ++osi){
         for(int stage = 0; stage < 4; ++stage){
            input =
               doFilter(input,
                     upsample_a1,
                     upsample_a2,
                     upsample_b0,
                     upsample_b1,
                     f->R[nlf_upz1 + stage*2],
                     f->R[nlf_upz2 + stage*2]);
         }

         outputOS[osi] = input;

         // Zero stuffing
         input = _mm_setzero_ps();
      }

      for(int osi = 0; osi < extraOversample; ++osi){
         for(int i=0; i < n_nlf_coeff; ++i){
            f->C[i] = A(f->C[i], f->dC[i]);
         }

         // n.b. stages is zero-indexed so use <=
         for(int stage = 0; stage <= stages; ++stage){
            outputOS[osi] =
               doNLFilter(outputOS[osi],
                     f->C[nlf_a1],
                     f->C[nlf_a2],
                     f->C[nlf_b0],
                     f->C[nlf_b1],
                     f->C[nlf_b2],
                     sat,
                     f->R[nlf_z1 + stage*2],
                     f->R[nlf_z2 + stage*2]);
         }
      }

      static const __m128 windowFactors[extraOversample] = {
         F(-0.0636844f),
         _mm_setzero_ps(),
         F(0.57315917f),
         F(1.0f)
      };

      __m128 ov = _mm_setzero_ps();
      for(int i=0; i < extraOversample; ++i ){
         ov = A(ov, M(outputOS[i], windowFactors[i]));
      }

      return ov;
   }
}
