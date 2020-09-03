#include "globals.h"
#include "RKMoog.h"
#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"

/*
** This code is the Rugne Kutta circuit cimulation from
**
** https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
**
** Although the code is slightly modified from that location here is the originaly copyright notice:

---
Copyright (c) 2015, Miller Puckette. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---

*/

namespace RKMoog
{
   static constexpr int rkm_cutoff = 0, rkm_reso = 1, rkm_sat = 2, rkm_satinv = 3;

   void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, int sub, SurgeStorage *storage )
   {
      // COnsideration: Do we want tuning aware or not?
      auto pitch = storage->note_to_pitch( freq + 69 ) * Tunings::MIDI_0_FREQ;
      cm->C[rkm_cutoff] = pitch * 2.0 * M_PI;
      cm->C[rkm_reso] = reso * 10; // code says 0-10 is value
      switch( sub )
      {
      case 0:
         cm->C[rkm_sat] = 1.5;
         cm->C[rkm_satinv ] = 0.66666666666666;
         break;
      case 2:
         cm->C[rkm_sat] = 8.0;
         cm->C[rkm_satinv ] = 0.125;
         break;
         break;
      case 1:
      default:
         cm->C[rkm_sat] = 3.0;
         cm->C[rkm_satinv ] = 0.3333333333;
         break;
      }
   }
   
   inline float clip(float value, float saturation, float saturationinverse)
   {
      float v2 = (value * saturationinverse > 1 ? 1 :
                  (value * saturationinverse < -1 ? -1:
                   value * saturationinverse));
      return (saturation * (v2 - (1./3.) * v2 * v2 * v2));
   }


   void calculateDerivatives(float input, double * dstate, double * state, float cutoff, float resonance, float saturation, float saturationInv )
	{
		double satstate0 = clip(state[0], saturation, saturationInv);
		double satstate1 = clip(state[1], saturation, saturationInv);
		double satstate2 = clip(state[2], saturation, saturationInv);
		
		dstate[0] = cutoff * (clip(input - resonance * state[3], saturation, saturationInv) - satstate0);
		dstate[1] = cutoff * (satstate0 - satstate1);
		dstate[2] = cutoff * (satstate1 - satstate2);
		dstate[3] = cutoff * (satstate2 - clip(state[3], saturation, saturationInv));
	}

	void rungekutteSolver(float input, double * state, float cutoff, float resonance, float sat, float satInv)
	{
		int i;
		double deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

      auto stepSize = dsamplerate_os_inv;
      
		calculateDerivatives(input, deriv1, state, cutoff, resonance, sat, satInv);
		
		for (i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5 * stepSize * deriv1[i];
		
		calculateDerivatives(input, deriv2, tempState, cutoff, resonance, sat, satInv);
		
		for (i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5 * stepSize * deriv2[i];
		
		calculateDerivatives(input, deriv3, tempState, cutoff, resonance, sat, satInv);
		
		for (i = 0; i < 4; i++)
			tempState[i] = state[i] + stepSize * deriv3[i];
		
		calculateDerivatives(input, deriv4, tempState, cutoff, resonance, sat, satInv);
		
		for (i = 0; i < 4; i++)
			state[i] += (1.0 / 6.0) * stepSize * (deriv1[i] + 2.0 * deriv2[i] + 2.0 * deriv3[i] + deriv4[i]);
	}
	
   
   __m128 process( QuadFilterUnitState * __restrict f, __m128 inm )
   {
      static constexpr int ssew = 4, n_coef=4, n_state=4;
      /*
      ** This demonstrates how to unroll SSE. At input each of the
      ** values (registers, coefficients, inputs) will be up to 4 wide
      ** and will have values which need populating if f->active[i] != 0.
      **
      ** Ideally we would code SSE code. But this is a gross, slow, probably
      ** not mergable unroll.
      */
      float in[ssew];
      _mm_store_ps( in, inm );

      float C[n_coef][ssew];
      for( int i=0; i<n_coef; ++i ) _mm_store_ps( C[i], f->C[i] );
      
      float state[n_state][ssew];
      for( int i=0; i<n_state; ++i ) _mm_store_ps( state[i], f->R[i] );

      float out[ssew];
      for( int v=0; v<ssew; ++v )
      {
         if( ! f->active[v] ) continue;

         double dstate[n_state];
         for( int i=0; i<n_state; ++i ) dstate[i] = state[i][v];
         
         rungekutteSolver( in[v], dstate, C[rkm_cutoff][v], C[rkm_reso][v], C[rkm_sat][v], C[rkm_satinv][v] );
         out[v] = dstate[ n_state - 1 ];

         for( int i=0; i<n_state; ++i ) state[i][v] = dstate[i];
      }
      
      __m128 outm = _mm_load_ps(out);
      
      for( int i=0; i<n_state; ++i ) f->R[i] = _mm_load_ps(state[i]);
      return outm;
   }
}
