

#include "obxd.hpp"
  
/*
  ==============================================================================
  This file is part of Obxd synthesizer.
  Copyright © 2013-2014 Filatov Vadim
   
  Contact author via email :
  justdat_@_e1.ru
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
#include "globals.h"
#include "QuadFilterUnit.h"
#include "SurgeStorage.h"
#include "DebugHelpers.h"
#include "FilterCoefficientMaker.h"
#include "globals.h"


class Filter
{
private:
   //float s1,s2,s3,s4;
   //float R;
   //float R24;
   //float rcor,rcorInv;
   //float rcor24,rcor24Inv;

   //24 db multimode
   //float mmt;
   //int mmch;
public:
   //float SampleRate;
   //float sampleRateInv;
   //bool bandPassSw;
   //float mm;
   //bool selfOscPush;
   Filter()
   {
      //selfOscPush = false;
      //bandPassSw = false;
      //mm=0;
      //s1=s2=s3=s4=0;
      //SampleRate = 44000;
      //sampleRateInv = 1 / SampleRate;
      //rcor =500.0 / 44000;
      //rcorInv = 1 / rcor;
      //rcor24 = 970.0 / 44000;
      //rcor24Inv = 1 / rcor24;
      //R=1;
      //R24=0;
   }
};
   enum Params {
      s1,
      s2,
      s3,
      s4,
      R,
      R24,
      g
   };
enum ObxdCoeff{
   mm,
   bandPass,
   selfOscPush,
   mmch,
   mmt
};

   void setMultimode(float m)
   {
   //   mm = m;
   //   mmch = (int)(mm * 3);
   //   mmt = mm*3-mmch;
   }

   
static constexpr int ssew = 4;
   inline void setResonance(QuadFilterUnitState * __restrict f, __m128 res)
   {const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
      const __m128 three_five = _mm_load_ps(new float[]{3.5f, 3.5f, 3.5f, 3.5f});
      f->R[R] = _mm_sub_ps(one, res);
      f->R[R24] =  _mm_mul_ps(three_five, res);
   }
   
   inline __m128 diodePairResistanceApprox(__m128 x)
   {
      const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
      const __m128 one_zero_three = _mm_load_ps(new float[]{0.0103592f,0.0103592f,0.0103592f,0.0103592f});
      const __m128 nine_two_zero = _mm_load_ps(new float[]{0.00920833f,0.00920833f,0.00920833f,0.00920833f});
      const __m128 one_eight_five = _mm_load_ps(new float[]{0.185f,0.185f,0.185f,0.185f});
      const __m128 zero_zero_five = _mm_load_ps(new float[]{0.05f,0.05f,0.05f,0.05f});
      return _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(one_zero_three, x), _mm_mul_ps(nine_two_zero, x) + one_eight_five), x),zero_zero_five), x), one);
      //Taylor approx of slightly mismatched diode pair
   }
   
   //resolve 0-delay feedback
   inline __m128 NR(__m128 sample, QuadFilterUnitState * __restrict f)
   {
      const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
      const __m128 two = _mm_load_ps(new float[]{2.0f, 2.0f, 2.0f, 2.0f});
      const __m128 one_three_five = _mm_load_ps(new float[]{1.035f, 1.035f, 1.035f, 1.035f});
      const __m128 eight_seven_six = _mm_load_ps(new float[]{0.0876f, 0.0876f, 0.0876f, 0.0876f});
      //calculating feedback non-linear transconducance and compensated for R (-1)
      //Boosting non-linearity
      __m128 tCfb;
      float selfosc_arr[ssew];
      _mm_store_ps( selfosc_arr, f->C[selfOscPush]);
      if (!selfosc_arr[0])
         tCfb = _mm_sub_ps(diodePairResistanceApprox(_mm_mul_ps(f->R[s1], eight_seven_six)), one);
      else
         tCfb = _mm_sub_ps(diodePairResistanceApprox(_mm_mul_ps(f->R[s1], eight_seven_six)), one_three_five);
      //float tCfb = 0;
      //disable non-linearity == digital filter

      //resolve linear feedback
      //float y = ((sample - 2*(s1*(R+tCfb)) - g*s1  - s2)/(1+ g*(2*(R+tCfb)+ g)));
                           __m128 y = _mm_div_ps(_mm_sub_ps(_mm_sub_ps(_mm_sub_ps(sample, _mm_mul_ps(two, _mm_mul_ps(f->R[s1], _mm_add_ps(f->R[R], tCfb)))), _mm_mul_ps(f->R[g], f->R[s1])), f->R[s2]), _mm_add_ps(_mm_add_ps(one, _mm_mul_ps(f->R[g], _mm_mul_ps(two, _mm_add_ps(f->R[R], tCfb)))), f->R[g]));

      //float y = ((sample - 2*(s1*(R+tCfb)) - g2*s1  - s2)/(1+ g1*(2*(R+tCfb)+ g2)));

      return y;
   }

void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, int sub, SurgeStorage *storage)
{
   auto pitch = storage->note_to_pitch( freq + 69 ) * Tunings::MIDI_0_FREQ;
 
}



   __m128 process(QuadFilterUnitState * __restrict f,__m128 sample)
   {
      const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
      float g_arr[ssew];
      _mm_store_ps( g_arr, f->R[g]);
        
      float gpw_arr[ssew];
      for (int i = 0; i < ssew; i++)
      {
         gpw_arr[i] = tanf(g_arr[i] * dsamplerate_inv * M_PI);
      }
      
      __m128 gpw = _mm_load_ps(gpw_arr);
      
      f->R[g] = gpw;
      //float v = ((sample- R * s1*2 - g2*s1 - s2)/(1+ R*g1*2 + g1*g2));
     __m128 v = NR(sample, f);
      //float y1 = v * g + s1;
     __m128 y1 = _mm_add_ps(_mm_mul_ps(v, f->R[g]), f->R[s1]);
      //s1 = v * g + y1;
      f->R[s1] = _mm_add_ps(_mm_mul_ps(v, f->R[g]), y1);
      //float y2 = y1 * g + s2;
     __m128 y2 = _mm_add_ps(_mm_mul_ps(y1, f->R[g]), f->R[s2]);
      //s2 = y1 * g + y2;
      f->R[s2] = _mm_add_ps(_mm_mul_ps(y1, f->R[g]), y2);
    
     __m128 mc;
      float bp_arr[ssew];
      _mm_store_ps(bp_arr, f->C[bandPass]);
      if (!bp_arr[0])
      {
        mc = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[mm]), y2), _mm_mul_ps(f->C[mm], v));
      }
      else
      {
         float mm_arr[ssew];
          _mm_store_ps(mm_arr, f->C[mm]);
         float mc_arr[ssew];
         float y1_arr[ssew];
         _mm_store_ps( y1_arr, y1 );
         float y2_arr[ssew];
         _mm_store_ps( y2_arr, y2 );
         float v_arr[ssew];
         _mm_store_ps( v_arr, v );
         for (int i = 0; i < ssew; i++)
         {
            //mc =2 * ( mm < 0.5 ? ((0.5 - mm) * y2 + (mm) * y1) : ((1-mm) * y1 + (mm-0.5) * v));
            mc_arr[i] = 2.0 * (mm_arr[i] < 0.5 ? ((0.5 - mm_arr[i]) * y2_arr[i] + (mm_arr[i] * y1_arr[i]))
                                                      : ((1.0 - mm_arr[i]) * y1_arr[i] + (mm_arr[i] - 0.5) * v_arr[i]));
         }
        mc =  _mm_load_ps(mc_arr);
      }

      return mc;
   }
   
   inline __m128 NR24(__m128 sample, __m128 g, __m128 lpc, QuadFilterUnitState * __restrict f)
   {
      const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
            
      //float ml = 1 / (1+g);
     __m128 ml = _mm_div_ps(one, _mm_add_ps(one, g));
      //float S = (lpc * (lpc * (lpc * f->R[s1] + f->R[s2]) + f->R[s3]) + f->R[s4]) * ml;
     __m128 S = _mm_add_ps(_mm_mul_ps(lpc, _mm_add_ps(_mm_mul_ps(lpc, _mm_add_ps(_mm_mul_ps(lpc, f->R[s1]), f->R[s2])), f->R[s3])), _mm_mul_ps(f->R[s4], ml));
      
      // float G = lpc * lpc * lpc * lpc;
     __m128 G = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(lpc, lpc), lpc), lpc);
      //float y = (sample - f->R[R24] * S) / (1 + f->R[R24] * G);
     __m128 y = _mm_div_ps(_mm_sub_ps(sample, _mm_mul_ps(f->R[R24], S)), _mm_add_ps(one, _mm_mul_ps(f->R[R24], G)));
      return y;
   }
   
   inline static __m128 tptpc(__m128& state,__m128 inp, __m128 cutoff)

   {
      const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
      __m128 v = _mm_div_ps(_mm_mul_ps(_mm_sub_ps(inp, state), cutoff), _mm_add_ps(one, cutoff));
      __m128 res = _mm_add_ps(v,  state);
      state = _mm_add_ps(res, v);
      return res;
   }

   inline __m128 Apply4Pole(__m128 sample, QuadFilterUnitState * __restrict f )
   {
      
      float SampleRate = 1/dsamplerate_inv;
      float rcrate =sqrt((44000/SampleRate));
      float rcor = (500.0 / 44000)*rcrate;
      __m128 rcors = _mm_load_ps(new float[]{rcor, rcor, rcor, rcor});
      float rcor24 = (970.0 / 44000)*rcrate;
      __m128 rcor24s = _mm_load_ps(new float[]{rcor24, rcor24, rcor24, rcor24});
      float rcorInv = 1 / rcor;
      const __m128 rcorInvs = _mm_load_ps(new float[]{rcorInv, rcorInv, rcorInv, rcorInv});
      float rcor24Inv = 1 / rcor24;
      const __m128 rcor24Invs = _mm_load_ps(new float[]{rcor24Inv, rcor24Inv, rcor24Inv, rcor24Inv});
       const __m128 one = _mm_load_ps(new float[]{1.0f, 1.0f, 1.0f, 1.0f});
      const __m128 zero_four_five = _mm_load_ps(new float[]{0.45f, 0.45f, 0.45f, 0.45f});
      const __m128 zero = _mm_load_ps(new float[]{0.0f, 0.0f, 0.0f, 0.0f});
      float g_arr[ssew];
      _mm_store_ps( g_arr, f->R[g] );
      float g1_arr[ssew];
      for (int i = 0; i < ssew; i++)
      {
         g1_arr[i] = (float)tan(g_arr[i] * dsamplerate_inv * M_PI);
      }
      
      __m128 g1 = _mm_load_ps(g1_arr);
      f->R[g] = g1;

      //float lpc = g / (1 + g);
     __m128 lpc = _mm_div_ps(f->R[g], _mm_add_ps(one,  f->R[g]));
      //float y0 = NR24(sample,g,lpc);
     __m128 y0 = NR24(sample, f->R[g], lpc, f);
      
      //first low pass in cascade
      //double v = (y0 - f->R[s1]) * lpc;
     __m128 v = _mm_sub_ps(y0, _mm_mul_ps(f->R[s1], lpc));
      
      //double res = v + f->R[s1];
     __m128 res = _mm_add_ps(v, f->R[s1]);
      
      //f->R[s1] = res + v;
      f->R[s1] = _mm_add_ps(res, v);
      
      //damping
      //f->R[s1] =atan(s1*rcor24)*rcor24Inv;
      __m128 s1_rcor24 = _mm_mul_ps(f->R[s1], rcor24s);
      float s1_rcor24_arr[ssew];
      for (int i = 0; i < ssew; i++)
      {
         s1_rcor24_arr[i] = atan(s1_rcor24_arr[i]);
      }
      s1_rcor24 = _mm_load_ps(s1_rcor24_arr);
      f->R[s1] = _mm_mul_ps(s1_rcor24, rcor24Invs);

      //float y1 = res;
     __m128 y1 = res;
      
      //float y2 = tptpc(f->R[s2],y1,g);
     __m128 y2 = tptpc(f->R[s2], y1, f->R[g]);
      
      //float y3 = tptpc(f->R[s3],y2,g);
     __m128 y3 = tptpc(f->R[s3], y2, f->R[g]);
      
      //float y4 = tptpc(f->R[s4],y3,g);
     __m128 y4 = tptpc(f->R[s4], y3, f->R[g]);
      
     __m128 mc;
      float mmch_arr[ssew];
      _mm_store_ps(mmch_arr, f->C[mmch]);
   
      switch ((int)(mmch_arr[0]))
      {
      case 0:
         //mc = ((1 - mmt) * y4 + (mmt) * y3);
         mc = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[mmt]), y4), _mm_add_ps(f->C[mmt], y3));
         break;
      case 1:
         //mc = ((1 - mmt) * y3 + (mmt) * y2);
         mc = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[mmt]), y3), _mm_mul_ps(f->C[mmt], y2));
         break;
      case 2:
         //mc = ((1 - mmt) * y2 + (mmt) * y1);
         mc = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, f->C[mmt]), y2), _mm_mul_ps(f->C[mmt], y1));
         break;
      case 3:
         mc = y1;
         break;
      default:
         mc = zero;
      }
      //half volume comp
      return _mm_mul_ps(mc, _mm_add_ps(one, _mm_mul_ps(f->R[R24], zero_four_five)));
   }


