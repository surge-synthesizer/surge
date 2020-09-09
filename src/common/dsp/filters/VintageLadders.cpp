#include "globals.h"
#include "VintageLadders.h"
#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "vt_dsp/basic_dsp.h"
#include "FastMath.h"

/*
** This contains various adaptations of the models found at
**
** https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
**
** Modifications include
** 1. Modifying to make surge compatible with state mamagenemt
** 2. SSe and so on
** 3. Model specici changes per model
*/

namespace VintageLadder
{
   namespace Common
   {
      float clampedFrequency( float pitch, SurgeStorage *storage )
      {
         auto freq = storage->note_to_pitch_ignoring_tuning( pitch + 69 ) * Tunings::MIDI_0_FREQ;
         freq = limit_range( (float)freq, 5.f, (float)( dsamplerate_os * 0.3f ) );
         return freq;
      }
   }
   
   namespace RK
   {
      /*
      ** Imitates a Moog resonant filter by Runge-Kutte numerical integration of
      ** a differential equation approximately describing the dynamics of the circuit.
      ** 
      ** Useful references:
      ** 
      **  Tim Stilson
      ** "Analyzing the Moog VCF with Considerations for Digital Implementation"
		** Sections 1 and 2 are a reasonably good introduction but the 
		** model they use is highly idealized.
      **
      ** Timothy E. Stinchcombe
      ** "Analysis of the Moog Transistor Ladder and Derivative Filters"
		** Long, but a very thorough description of how the filter works including
      ** 		its nonlinearities
      **
      **	Antti Huovilainen
      ** "Non-linear digital implementation of the moog ladder filter"
		** Comes close to giving a differential equation for a reasonably realistic
		** model of the filter
      ** 
      ** The differential equations are:
      **
      **  y1' = k * (S(x - r * y4) - S(y1))
      **  y2' = k * (S(y1) - S(y2))
      **  y3' = k * (S(y2) - S(y3))
      **  y4' = k * (S(y3) - S(y4))
      **
      ** where k controls the cutoff frequency, r is feedback (<= 4 for stability), and S(x) is a saturation function.
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
         auto pitch = VintageLadder::Common::clampedFrequency( freq, storage );
         cm->C[rkm_cutoff] = pitch * 2.0 * M_PI;
         cm->C[rkm_reso] = reso * 5; // code says 0-10 is value but above 6 it is just self-oscillation
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
         for( int j=0; j< n_coef; ++j )
            f->C[j] = _mm_add_ps(f->C[j], f->dC[j]);
         
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
      enum huov_regoffsets { h_stage = 0, h_stageTanh = 4, h_delay = 7 };

      int extraOversample = 2;
      float extraOversampleInv = 0.5;
      
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage ) {
         auto cutoff = VintageLadder::Common::clampedFrequency( freq, storage );
         cm->C[h_cutoff] = cutoff;

         reso = limit_range( reso, 0.0f, 0.994f );
         
         double fc =  cutoff * dsamplerate_os_inv * extraOversampleInv;
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

      __m128 process( QuadFilterUnitState * __restrict f, __m128 in )
      {
         static const __m128 dFac = _mm_mul_ps( _mm_set_ps1( 0.5 ), _mm_set_ps1( extraOversampleInv ) );

         for( int j=0; j<2 * extraOversample; ++j )
         {
            auto resQuad = f->C[h_resquad];
            auto thermal = f->C[h_thermal];
            auto tune    = f->C[h_tune];

            for( int k=0; k<= h_resquad; ++k )
               f->C[k] = _mm_add_ps(f->C[k], _mm_mul_ps( dFac, f->dC[k]));
            
#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )
            
				// float input = in - resQuad * delay[5];
            auto input = _mm_sub_ps( in, _mm_mul_ps( resQuad, f->R[h_delay + 5] ) );

            // delay[0] = stage[0] = delay[0] + tune * (tanh(input * thermal) - stageTanh[0]);
            f->R[h_stage + 0] = A( f->R[h_delay + 0], M( tune, S( Surge::DSP::fasttanhSSEclamped( M( input, thermal ) ), f->R[h_stageTanh + 0] ) ) );
            f->R[h_delay + 0 ] = f->R[h_stage + 0 ];
            
            for (int k = 1; k < 4; k++) 
				{
					// input = stage[k-1];
               input = f->R[h_stage + k - 1 ];
               
					// stage[k] = delay[k] + tune * ((stageTanh[k-1] = tanh(input * thermal)) - (k != 3 ? stageTanh[k] : tanh(delay[k] * thermal)));
               f->R[h_stageTanh + k - 1 ] = Surge::DSP::fasttanhSSEclamped( M( input, thermal ) );
               f->R[h_stage + k ] = A( f->R[ h_delay + k ],
                                       M( tune, S(
                                             f->R[h_stageTanh + k - 1 ],
                                             ( k != 3 ? f->R[h_stageTanh + k ] : Surge::DSP::fasttanhSSEclamped( M( f->R[h_delay + k ], thermal ) ) )
                                             )
                                          )
                  );

					// delay[k] = stage[k];
               f->R[h_delay + k ] = f->R[h_stage + k];
				}
				// 0.5 sample delay for phase compensation
				// delay[5] = (stage[3] + delay[4]) * 0.5;
            f->R[h_delay + 5] = M( _mm_set_ps1( 0.5 ), A( f->R[h_stage +3], f->R[h_delay + 4]) );
				// delay[4] = stage[3];
				f->R[h_delay +4] = f->R[h_stage +3];

#undef M
#undef A
#undef S                                                         
         }

         return f->R[h_delay + 5];
      }
   }

   namespace Improved {
      /*
        This model is based on a reference implementation of an algorithm developed by
        Stefano D'Angelo and Vesa Valimaki, presented in a paper published at ICASSP in 2013.
        This improved model is based on a circuit analysis and compared against a reference
        Ngspice simulation. In the paper, it is noted that this particular model is
        more accurate in preserving the self-oscillating nature of the real filter.
        
        References: "An Improved Virtual Analog Model of the Moog Ladder Filter"
        Original Implementation: D'Angelo, Valimaki
      */

      enum imp_coeffs { i_cutoff = 0, i_reso, i_x, i_g, i_drive, n_icoeff };
      enum imp_regoffsets { h_V = 0, h_dV = 4, h_tV = 8 };
      static constexpr float VT = 0.312;

      int extraOversample = 2;
      float extraOversampleInv = 0.5;
      
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage )
      {
         auto cutoff = VintageLadder::Common::clampedFrequency( freq, storage );
         cm->C[i_cutoff] = cutoff;
         cm->C[i_reso ] = reso * 4;
         cm->C[i_x] = M_PI * cutoff * dsamplerate_os_inv * extraOversampleInv;
         cm->C[i_g] = 4.0 * M_PI * VT * cutoff * ( 1.0 - cm->C[i_x] ) / ( 1.0 + cm->C[i_x] );
         cm->C[i_drive] = 1.0;
      }

      __m128 process( QuadFilterUnitState * __restrict f, __m128 in )
      {
         static const __m128 dFac = _mm_set_ps1( extraOversampleInv );

#define F(a) _mm_set_ps1( a )         
#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )

         __m128 dV0, dV1, dV2, dV3;
         
         __m128 halfsri = M( F(0.5), M( F( dsamplerate_os_inv ), F( extraOversampleInv ) ) );
         static const __m128 oneo2v2 = _mm_div_ps( F(1.0), M( F(2.0), F(VT) ) );

         for( int i=0; i<extraOversample; ++i )
         {
            for( int j=0; j< n_icoeff; ++j )
               f->C[j] = A(f->C[j], M(dFac,f->dC[j]));

            __m128 drive = f->C[i_drive];
            __m128 resonance = f->C[i_reso];
            __m128 g = f->C[i_g];



            // dV0 = -g * (tanh((drive * in + resonance * V[3]) / (2.0 * VT)) + tV[0]);
            dV0 = M( M( F(-1.f) , g ), A( Surge::DSP::fasttanhSSEclamped( M( A( M( drive, in ), M( resonance, f->R[h_V + 3] ) ), oneo2v2 ) ), f->R[h_tV + 0] ) );

            //std::cout << _D(drive[0]) << _D(resonance[0]) << _D(g[0]) << _D(dV0[0]) << std::endl;

            // V[0] += (dV0 + dV[0]) * 0.5 * sri;
            f->R[h_V + 0  ] = A( f->R[h_V + 0], M( A( f->R[h_dV + 0], dV0 ), halfsri ) );
            f->R[h_dV + 0 ] = dV0;
            // tV[0] = tanh(V[0] / (2.0 * VT));
            f->R[h_tV + 0 ] = Surge::DSP::fasttanhSSEclamped( M( f->R[h_V + 0], oneo2v2 ) );

            
            // dV1 = g * (tV[0] - tV[1]);
            dV1 = M( g, S( f->R[h_tV + 0], f->R[h_tV + 1 ] ) );
            // V[1] += (dV1 + dV[1]) * 0.5 * sri;
            f->R[h_V + 1 ] = A( f->R[h_V + 1 ], M( A( f->R[h_dV + 1], dV1 ), halfsri ) );
            f->R[h_dV + 1 ] = dV1;
            // tV[1] = tanh(V[1] / (2.0 * VT));
            f->R[h_tV + 1 ] = Surge::DSP::fasttanhSSEclamped( M( f->R[h_V + 1], oneo2v2 ) );
            
            // dV2 = g * (tV[1] - tV[2]);
            dV2 = M( g, S( f->R[h_tV + 1], f->R[h_tV + 2 ] ) );
            // V[2] += (dV2 + dV[2]) * 0.5 * sri;
            f->R[h_V + 2 ] = A( f->R[h_V + 2 ], M( A( f->R[h_dV + 2], dV2 ), halfsri ) );
            f->R[h_dV + 2 ] = dV2;
            // tV[2] = tanh(V[2] / (2.0 * VT));
            f->R[h_tV + 2 ] = Surge::DSP::fasttanhSSEclamped( M( f->R[h_V + 2], oneo2v2 ) );

            
            // dV3 = g * (tV[2] - tV[3]);
            dV3 = M( g, S( f->R[h_tV + 2], f->R[h_tV + 3 ] ) );
            // V[3] += (dV3 + dV[3]) * 0.5 * sri;
            f->R[h_V + 3 ] = A( f->R[h_V + 3 ], M( A( f->R[h_dV + 3], dV3 ), halfsri ) );
            f->R[h_dV + 3 ] = dV3;
            // tV[2] = tanh(V[2] / (2.0 * VT));
            f->R[h_tV + 3 ] = Surge::DSP::fasttanhSSEclamped( M( f->R[h_V + 3], oneo2v2 ) );
         }

#undef F
#undef M
#undef A
#undef S         
         
         return f->R[h_V + 3];
      }
   }
}
