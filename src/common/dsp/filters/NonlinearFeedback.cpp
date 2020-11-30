#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vt_dsp/basic_dsp.h"
#include "FastMath.h"

/*
** This contains an adaptation of the filter found at
** https://ccrma.stanford.edu/~jatin/ComplexNonlinearities/NLFeedback.html
** with coefficient calculation at
** https://github.com/jatinchowdhury18/audio_dspy/blob/master/audio_dspy/eq_design.py
*/

static float clampedFrequency( float pitch, SurgeStorage *storage )
{
   auto freq = storage->note_to_pitch_ignoring_tuning( pitch + 69 ) * Tunings::MIDI_0_FREQ;
   freq = limit_range( (float)freq, 5.f, (float)( dsamplerate_os * 0.3f ) );
   return freq;
}

#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )

static inline __m128 doFilter(
      const __m128 input,
      const __m128 a1,
      const __m128 a2,
      const __m128 b0,
      const __m128 b1,
      const __m128 b2,
      __m128 &z1,
      __m128 &z2)
   noexcept
{
   // out = z1 + b0 * input
   const __m128 out = A(z1, M(b0, input));
   // nonlinear feedback = tanh(out)
   const __m128 nf  = Surge::DSP::fasttanhSSEclamped(out);
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
      nlf_b2
   };

   enum dlf_state {
      nlf_z1, // 1st z-1 state for first  stage
      nlf_z2, // 2nd z-1 state for first  stage
      nlf_z3, // 1st z-1 state for second stage
      nlf_z4, // 2nd z-1 state for second stage
      nlf_z5, // 1st z-1 state for third  stage
      nlf_z6, // 2nd z-1 state for third  stage
      nlf_z7, // 1st z-1 state for fourth stage
      nlf_z8  // 2nd z-1 state for fourth stage
   };

   void makeCoefficientsLPHP( FilterCoefficientMaker *cm, float freq, float reso, int type, SurgeStorage *storage )
   {
      float C[n_cm_coeffs];

      const float q = ((reso * reso * reso) * 10.0f + 0.1f);

      const float  wc = M_PI * clampedFrequency(freq, storage) / dsamplerate_os;
      const float   c = 1.0f / Surge::DSP::fasttan(wc);
      const float phi = c * c;
      const float   K = c / q;
      
      // note we actually calculate the reciprocal of a0 because we only use a0 to normalize the
      // other coefficients, and multiplication by reciprocal is cheaper than dividing.
      const float a0r = 1.0f / (phi + K + 1.0f);

      if(type == fut_nonlinearfb_lp){ // lowpass
         C[nlf_a1] = 2.0f * (1.0f - phi) * a0r;
         C[nlf_a2] = (phi - K + 1.0f)    * a0r;
         C[nlf_b0] = a0r;
         C[nlf_b1] = 2.0f * C[nlf_b0];
         C[nlf_b2] = C[nlf_b0];
      }
      else{ // highpass
         C[nlf_a1] = 2.0f * (1.0f - phi) * a0r;
         C[nlf_a2] = (phi - K + 1.0f)    * a0r;
         C[nlf_b0] = phi                 * a0r;
         C[nlf_b1] = -2.0f * C[nlf_b0];
         C[nlf_b2] = C[nlf_b0];
      }

      cm->FromDirect(C);
   }

   void makeCoefficientsNBP( FilterCoefficientMaker *cm, float freq, float reso, int type, SurgeStorage *storage )
   {
      float C[n_cm_coeffs];

      const float q = ((reso * reso * reso) * 10.0f + 0.1f);

      const float wc = M_PI * clampedFrequency(freq, storage) / dsamplerate_os;

      const float wsin  = Surge::DSP::fastsin(wc);
      const float wcos  = Surge::DSP::fastcos(wc);
      const float alpha = wsin / (2.0f * q);

      // note we actually calculate the reciprocal of a0 because we only use a0 to normalize the
      // other coefficients, and multiplication by reciprocal is cheaper than dividing.
      const float a0r  = 1.0f / (1.0f + alpha);

      if(type == fut_nonlinearfb_n){ // notch
         C[nlf_a1] = -2.0f * wcos   * a0r;
         C[nlf_a2] = (1.0f - alpha) * a0r;
         C[nlf_b0] = a0r;
         C[nlf_b1] = -2.0f * wcos   * a0r;
         C[nlf_b2] = C[nlf_b0];
      }
      else{ // bandpass
         C[nlf_a1] = -2.0f * wcos   * a0r;
         C[nlf_a2] = (1.0f - alpha) * a0r;
         C[nlf_b0] = wsin * 0.5f    * a0r;
         C[nlf_b1] = 0.0f;
         C[nlf_b2] = -C[nlf_b0];
      }

      cm->FromDirect(C);
   }


   __m128 process( QuadFilterUnitState * __restrict f, __m128 input )
   {
      for(int i=0; i <= nlf_b2; ++i){ \
         f->C[i] = A(f->C[i], f->dC[i]); \
      }

      const int stages = f->WP[0] & 3; // lower 2 bits of subtype is the stage count

      const __m128 result1 =
         doFilter(input,
               f->C[nlf_a1],
               f->C[nlf_a2],
               f->C[nlf_b0],
               f->C[nlf_b1],
               f->C[nlf_b2],
               f->R[nlf_z1],
               f->R[nlf_z2]);

      if(stages == 0){ // if 1-stage
         // then we're done, return result1
         return result1;
      }

      // otherwise we calculate and return the second stage
      const __m128 result2 =
         doFilter(result1,
               f->C[nlf_a1],
               f->C[nlf_a2],
               f->C[nlf_b0],
               f->C[nlf_b1],
               f->C[nlf_b2],
               f->R[nlf_z3],
               f->R[nlf_z4]);
      // and so on

      if(stages == 1){
         return result2;
      }

      const __m128 result3 =
         doFilter(result2,
               f->C[nlf_a1],
               f->C[nlf_a2],
               f->C[nlf_b0],
               f->C[nlf_b1],
               f->C[nlf_b2],
               f->R[nlf_z5],
               f->R[nlf_z6]);

      if(stages == 2){
         return result3;
      }

      const __m128 result4 =
         doFilter(result3,
               f->C[nlf_a1],
               f->C[nlf_a2],
               f->C[nlf_b0],
               f->C[nlf_b1],
               f->C[nlf_b2],
               f->R[nlf_z7],
               f->R[nlf_z8]);

      return result4;
   }
}
