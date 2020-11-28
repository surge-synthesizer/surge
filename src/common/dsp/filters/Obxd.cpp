#include "Obxd.h"
  
/*
  ==============================================================================
  This file is derived from part of part of Obxd synthesizer.
  Copyright © 2013-2014 Filatov Vadim (justdat_@_e1.ru)
  
  This file may be licensed under the terms of of the
  GNU General Public License Version 2 (the ``GPL'').
  Software distributed under the License is distributed
  on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
  express or implied. See the GPL for the specific language
  governing rights and limitations.
  You should have received a copy of the GPL along with this
  program. If not, go to http://www.gnu.org/licenses/gpl.html
  or write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
  ==============================================================================
*/

#include <math.h>
#include "QuadFilterUnit.h"
#include "SurgeStorage.h"
#include "DebugHelpers.h"
#include "FilterCoefficientMaker.h"

namespace ObxdFilter {


   enum Obxd12dBCoeff
   {
      g12,
      R12,
      multimode,
      bandpass,
      self_osc_push,
      n_obxd12_coeff
    };

   enum Obxd24dBCoeff
   {
      g24,
      R24,
      rcor24,
      rcor24inv,
      pole_mix, // mm
      pole_mix_inv_int, // mmch
      pole_mix_scaled,  // mmt
      n_obxd24_coeff
    };

   enum Params {
        s1,
        s2,
        s3,
        s4,
     };

   static constexpr int ssew = 4;
   
   const __m128 zero = _mm_set1_ps(0.0f);
   const __m128 nine_two_zero = _mm_set1_ps(0.00920833f);
   const __m128 zero_zero_five = _mm_set1_ps(0.05f);
   const __m128 eight_seven_six = _mm_set1_ps(0.0876f);
   const __m128 one_zero_three = _mm_set1_ps(0.0103592f);
   const __m128 one_eight_five = _mm_set1_ps(0.185f);
   const __m128 zero_four_five = _mm_set1_ps(0.45f);
   const __m128 zero_five = _mm_set1_ps(0.5f);
   const __m128 one = _mm_set1_ps(1.0f);
   const __m128 one_three_five = _mm_set1_ps(1.035f);
   const __m128 two = _mm_set1_ps(2.0f);
   const __m128 three = _mm_set1_ps(3.0f);
   const __m128 gainAdjustment2Pole = _mm_set1_ps(0.74);
   const __m128 gainAdjustment4Pole = _mm_set1_ps(0.6);

   void makeCoefficients(FilterCoefficientMaker* cm, Poles p, float freq, float reso, int sub, SurgeStorage* storage)
   {
      float lC[n_cm_coeffs];
      float rcrate = sqrt((44000 * dsamplerate_os_inv));
      float rcor = (500.0 / 44000) * rcrate;
      float cutoff = fmin(storage->note_to_pitch(freq + 69) * Tunings::MIDI_0_FREQ, 22000.0) * dsamplerate_os_inv * M_PI;

      if (p == TWO_POLE)
      {
         lC[g12] = tanf(cutoff);
         lC[R12] = 1.0 - reso;
         lC[bandpass] = 0.f;

         switch (sub)
         {
         case 0: // lowpass
         case 4: // lowpass self-oscillation push
            lC[multimode] = 0.f;
            break;
         case 1: // bandpass
         case 5: // bandpass self-oscillation push
            lC[multimode] = 0.5;
            lC[bandpass] = 1.f;
            break;
         case 2: // highpass
         case 6: // highpass self-oscillation push
            lC[multimode] = 1.0;
            break;
         case 3: // notch
         case 7: // notch self-oscillation push
            lC[multimode] = 0.5;
            break;
         }

         lC[self_osc_push] = (sub > 3);
      }
      else
      {
         lC[rcor24] = (970.0 / 44000) * rcrate;
         lC[rcor24inv] = 1.0 / lC[rcor24];
         lC[g24] = tanf(cutoff);
         lC[R24] = 3.5 * reso;
         lC[pole_mix] = 1.f - (sub / 3.f);
         lC[pole_mix_inv_int] = (int)(3.f - sub);
         lC[pole_mix_scaled] = (lC[pole_mix] * 3) - lC[pole_mix_inv_int];
      }

      cm->FromDirect(lC);
   }

   inline __m128 diodePairResistanceApprox(__m128 x)
   {
      // return (((((0.0103592f * x) + 0.00920833f) * x + 0.185f) * x + 0.05f) * x + 1.0f);
      return _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(one_zero_three, x), nine_two_zero), x), one_eight_five), x), zero_zero_five), x), one);
      // Taylor approximation of a slightly mismatched diode pair
   }
   
   // resolve 0-delay feedback
   inline __m128 NewtonRaphson12dB(__m128 sample, QuadFilterUnitState * __restrict f)
   {
      // calculating feedback non-linear transconducance and compensated for R (-1)
      // boosting non-linearity
      __m128 tCfb;
      __m128 selfOscEnabledMask = _mm_cmpeq_ps(f->C[self_osc_push], one);
      __m128 selfOscOffVal = _mm_sub_ps(diodePairResistanceApprox(_mm_mul_ps(f->R[s1], eight_seven_six)), one);
      __m128 selfOscOnVal = _mm_sub_ps(diodePairResistanceApprox(_mm_mul_ps(f->R[s1], eight_seven_six)), one_three_five);
      tCfb = _mm_add_ps(_mm_and_ps(selfOscEnabledMask, selfOscOnVal), _mm_andnot_ps(selfOscEnabledMask, selfOscOffVal));

      // resolve linear feedback
      // float y = ((sample - 2*(s1*(R+tCfb)) - g*s1  - s2)/(1+ g*(2*(R+tCfb)+ g)));
      __m128 y = _mm_div_ps(_mm_sub_ps(_mm_sub_ps(_mm_sub_ps(sample, _mm_mul_ps(two, _mm_mul_ps(f->R[s1], _mm_add_ps(f->C[R12], tCfb)))), _mm_mul_ps(f->C[g12], f->R[s1])), f->R[s2]), _mm_add_ps(one, _mm_mul_ps(f->C[g12], _mm_add_ps(_mm_mul_ps(two, _mm_add_ps(f->C[R12], tCfb)), f->C[g12]))));

      return y;
   }

   __m128 process_2_pole(QuadFilterUnitState * __restrict f, __m128 sample)
   {
      for (int i = 0; i < n_obxd12_coeff; i++)
      {
         f->C[i] = _mm_add_ps(f->C[i], f->dC[i]);
      }

      // float v = ((sample- R * s1*2 - g2*s1 - s2)/(1+ R*g1*2 + g1*g2));
      __m128 v = NewtonRaphson12dB(sample, f);
      // float y1 = v * g + s1;
      __m128 y1 = _mm_add_ps(_mm_mul_ps(v, f->C[g12]), f->R[s1]);
      // s1 = v * g + y1;
      f->R[s1] = _mm_add_ps(_mm_mul_ps(v, f->C[g12]), y1);
      // float y2 = y1 * g + s2;
      __m128 y2 = _mm_add_ps(_mm_mul_ps(y1, f->C[g12]), f->R[s2]);
      // s2 = y1 * g + y2;
      f->R[s2] = _mm_add_ps(_mm_mul_ps(y1, f->C[g12]), y2);

      __m128 mc;
      __m128 mask_bp = _mm_cmpeq_ps(f->C[bandpass], zero);
      __m128 bp_false = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[multimode]), y2), _mm_mul_ps(f->C[multimode], v));
      __m128 mask = _mm_cmplt_ps(f->C[multimode], zero_five);
      __m128 val1 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(zero_five, f->C[multimode]), y2), _mm_mul_ps(f->C[multimode], y1));
      __m128 val2 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[multimode]), y1), _mm_mul_ps(_mm_sub_ps(f->C[multimode], zero_five), v));
      __m128 bp_true = _mm_add_ps(_mm_and_ps(mask, val1), _mm_andnot_ps(mask, val2));
      mc =_mm_add_ps(_mm_and_ps(mask_bp, bp_false), _mm_andnot_ps(mask_bp, bp_true));
      return _mm_mul_ps(mc, gainAdjustment2Pole);
   }
  
   inline __m128 NewtonRaphsonR24dB(__m128 sample, __m128 lpc, QuadFilterUnitState * __restrict f)
   {
      // float ml = 1 / (1+g24);
     __m128 ml = _mm_div_ps(one, _mm_add_ps(one, f->C[g24]));
      // float S = (lpc * (lpc * (lpc * f->R[s1] + f->R[s2]) + f->R[s3]) + f->R[s4]) * ml;
     __m128 S = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(lpc, _mm_add_ps(_mm_mul_ps(lpc, _mm_add_ps(_mm_mul_ps(lpc, f->R[s1]), f->R[s2])), f->R[s3])), f->R[s4]), ml);
      // float G = lpc * lpc * lpc * lpc;
     __m128 G = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(lpc, lpc), lpc), lpc);
      // float y = (sample - f->C[R24] * S) / (1 + f->C[R24] * G);
     __m128 y = _mm_div_ps(_mm_sub_ps(sample, _mm_mul_ps(f->C[R24], S)), _mm_add_ps(one, _mm_mul_ps(f->C[R24], G)));

      return y;
   }
   
   inline static __m128 tptpc(__m128& state,__m128 inp, __m128 cutoff)
   {
      __m128 v = _mm_div_ps(_mm_mul_ps(_mm_sub_ps(inp, state), cutoff), _mm_add_ps(one, cutoff));
      __m128 res = _mm_add_ps(v,  state);
      state = _mm_add_ps(res, v);
      return res;
   }

   __m128 process_4_pole(QuadFilterUnitState * __restrict f, __m128 sample)
   {
      for (int i = 0; i < n_obxd24_coeff; i++)
      {
         f->C[i] = _mm_add_ps(f->C[i], f->dC[i]);
      }

      // float lpc = f->C[g] / (1 + f->C[g]);
     __m128 lpc = _mm_div_ps(f->C[g24], _mm_add_ps(one, f->C[g24]));
      
      // float y0 = NewtonRaphsonR24dB(sample,f->C[g],lpc);
     __m128 y0 = NewtonRaphsonR24dB(sample, lpc, f);
      
      // first lowpass in cascade
      // double v = (y0 - f->R[s1]) * lpc;
     __m128 v = _mm_mul_ps(_mm_sub_ps(y0, f->R[s1]), lpc);
      
      // double res = v + f->R[s1];
     __m128 res = _mm_add_ps(v, f->R[s1]);
      
      // f->R[s1] = res + v;
      f->R[s1] = _mm_add_ps(res, v);
      
      // damping
      // f->R[s1] =atan(s1*rcor24)*rcor24inv;
      __m128 s1_rcor24 = _mm_mul_ps(f->R[s1], f->C[rcor24]);

      float s1_rcor24_arr[ssew];
      _mm_store_ps(s1_rcor24_arr, s1_rcor24);
      
      for (int i = 0; i < ssew; i++)
      {
         if( f->active[i] )
            s1_rcor24_arr[i] = atan(s1_rcor24_arr[i]);
         else
            s1_rcor24_arr[i] = 0.f;
      }
      
      s1_rcor24 = _mm_load_ps(s1_rcor24_arr);
      f->R[s1] = _mm_mul_ps(s1_rcor24, f->C[rcor24inv]);

      // float y1 = res;
     __m128 y1 = res;
      
      // float y2 = tptpc(f->R[s2],y1,f->C[g24]);
     __m128 y2 = tptpc(f->R[s2], y1, f->C[g24]);
      
      // float y3 = tptpc(f->R[s3],y2,f->C[g24]);
     __m128 y3 = tptpc(f->R[s3], y2, f->C[g24]);
      
      // float y4 = tptpc(f->R[s4],y3,f->C[g24]);
     __m128 y4 = tptpc(f->R[s4], y3, f->C[g24]);
      
     __m128 mc;
      
      __m128 zero_val = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[pole_mix_scaled]), y4), _mm_add_ps(f->C[pole_mix_scaled], y3));
      __m128 zero_mask = _mm_cmpeq_ps(f->C[pole_mix_inv_int], zero);
      __m128 one_mask = _mm_cmpeq_ps(f->C[pole_mix_inv_int], one);
      __m128 one_val = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[pole_mix_scaled]), y3), _mm_mul_ps(f->C[pole_mix_scaled], y2));
      __m128 two_mask = _mm_cmpeq_ps(f->C[pole_mix_inv_int], two);
      __m128 two_val =_mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[pole_mix_scaled]), y2), _mm_mul_ps(f->C[pole_mix_scaled], y1));
      __m128 three_mask = _mm_cmpeq_ps(f->C[pole_mix_inv_int], three);
      __m128 three_val = y1;
      mc =_mm_add_ps(_mm_and_ps(zero_mask, zero_val), _mm_and_ps(one_mask, one_val));
      mc =_mm_add_ps(mc, _mm_add_ps(_mm_and_ps(two_mask, two_val), _mm_and_ps(three_mask, three_val)));
      
      // half volume compensation
      auto out = _mm_mul_ps(mc, _mm_add_ps(one, _mm_mul_ps(f->C[R24], zero_four_five)));
      return _mm_mul_ps( out, gainAdjustment4Pole );
   }
}
