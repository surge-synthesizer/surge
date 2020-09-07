#include "globals.h"
#include "VintageLadders.h"
#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"

namespace VintageLadder
{
   namespace RK
   {

      /*
      ** This code is based on the Rugne Kutta circuit cimulation from
      **
      ** https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
      **
      ** Modifications include
      ** 1. List these
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


      enum rkm_coeffs { rkm_cutoff = 0, rkm_reso = 1, rkm_sat = 2, rkm_satinv = 3 };
      
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage )
      {
         // COnsideration: Do we want tuning aware or not?
         auto pitch = storage->note_to_pitch( freq + 69 ) * Tunings::MIDI_0_FREQ;
         cm->C[rkm_cutoff] = pitch * 2.0 * M_PI;
         cm->C[rkm_reso] = reso * 10; // code says 0-10 is value
         cm->C[rkm_sat] = 3.0;
         cm->C[rkm_satinv ] = 0.3333333333;
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

      enum huov_coeffs { h_cutoff = 0, h_res, h_thermal, h_tune, h_acr, h_resquad };
      enum huov_regoffsets { h_stage = 0, h_steageTanh = 4, h_delay = 7 };
      
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage ) {
         auto cutoff = storage->note_to_pitch( freq + 69 ) * Tunings::MIDI_0_FREQ;

         cm->C[h_cutoff] = cutoff;
         
         double fc =  cutoff * dsamplerate_inv;
         double f  =  fc * 0.5; // oversampled 
         double fc2 = fc * fc;
         double fc3 = fc * fc * fc;
         
         double fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
         auto acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
         cm->C[h_acr] = acr;
         double thermal = 0.000025;
         cm->C[h_thermal] = thermal;
         auto tune = (1.0 - exp(-((2 * M_PI) * f * fcr))) / thermal;
         cm->C[h_tune] = tune;

         cm->C[h_res] = reso;
         cm->C[h_resquad] = 4.0 * reso * acr;
      }

      double processCore( double in, double coeff[6], double stage[4], double stageTanh[3], double delay[6] )
      {
         auto resQuad = coeff[h_resquad];
         auto thermal = coeff[h_thermal];
         auto tune = coeff[h_tune];

			for (int j = 0; j < 2; j++) 
			{
				float input = in - resQuad * delay[5];
				delay[0] = stage[0] = delay[0] + tune * (tanh(input * thermal) - stageTanh[0]);
				for (int k = 1; k < 4; k++) 
				{
					input = stage[k-1];
					stage[k] = delay[k] + tune * ((stageTanh[k-1] = tanh(input * thermal)) - (k != 3 ? stageTanh[k] : tanh(delay[k] * thermal)));
					delay[k] = stage[k];
				}
				// 0.5 sample delay for phase compensation
				delay[5] = (stage[3] + delay[4]) * 0.5;
				delay[4] = stage[3];
			}
			return delay[5];
      }
      
      __m128 process( QuadFilterUnitState * __restrict f, __m128 inm ) {
         static constexpr int ssew = 4, n_coef=6, n_state=13;

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
            
            double stage[4], stageTanh[3], delay[6];
            for( int i=0; i<4; ++i ) stage[i] = state[i][v];
            for( int i=0; i<3; ++i ) stageTanh[i] = state[i+4][v];
            for( int i=0; i<6; ++i ) delay[i] = state[i+7][v];

            double coeff[n_coef];
            for( int i=0; i<n_coef; ++i ) coeff[i] = C[i][v];
            
            out[v] = processCore( in[v], coeff, stage, stageTanh, delay );
            
            for( int i=0; i<4; ++i ) state[i][v] = stage[i];
            for( int i=0; i<3; ++i ) state[i+4][v] = stageTanh[i];
            for( int i=0; i<6; ++i ) state[i+7][v] = delay[i];

            
         }
         
         __m128 outm = _mm_load_ps(out);
         
         for( int i=0; i<n_state; ++i ) f->R[i] = _mm_load_ps(state[i]);
         return outm;

      }

   }
}
