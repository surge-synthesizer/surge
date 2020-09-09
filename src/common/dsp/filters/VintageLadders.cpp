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


      enum rkm_coeffs { rkm_cutoff = 0, rkm_reso = 1, n_rkcoeff };

      int extraOversample = 3;
      float extraOversampleInv = 0.333333;

      float saturation = 3, saturationInverse = 0.333333333333;
      
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage )
      {
         // COnsideration: Do we want tuning aware or not?
         auto pitch = VintageLadder::Common::clampedFrequency( freq, storage );
         cm->C[rkm_cutoff] = pitch * 2.0 * M_PI;
         cm->C[rkm_reso] = limit_range( reso, 0.f, 1.f ) * 4.5; // code says 0-10 is value but above 4 it is just out of tune self-oscillation
      }

#define F(a) _mm_set_ps1( a )         
#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )
      
      inline __m128 clip(__m128 value, __m128 saturation, __m128 saturationinverse)
      {
         static const __m128 minusone = F(-1), one = F(1), onethird = F(1.f / 3.f);
         auto vtsi = M( value, saturationinverse );
         auto v2 = _mm_min_ps( one, _mm_max_ps( minusone, vtsi ) );

         auto v23 = M(v2, M( v2, v2 ) );
         auto vkern = S( v2, M( onethird, v23 ) );
         auto res = M( saturation, vkern );

         return res;
      }
      
      
      //void calculateDerivatives(float input, double * dstate, double * state, float cutoff, float resonance, float saturation, float saturationInv )
      inline void calculateDerivatives( __m128 input, __m128 *dstate, __m128 *state, __m128 cutoff, __m128 resonance, __m128 saturation, __m128 saturationInv )
      {
         auto satstate0 = clip( state[0], saturation, saturationInv );
         auto satstate1 = clip( state[1], saturation, saturationInv );
         auto satstate2 = clip( state[2], saturation, saturationInv );

         // dstate[0] = cutoff * (clip(input - resonance * state[3], saturation, saturationInv) - satstate0);
         auto startstate = clip( S( input, M( resonance, state[3] ) ), saturation, saturationInv );
         dstate[0] = M( cutoff, S( startstate, satstate0 ) );
         
         // dstate[1] = cutoff * (satstate0 - satstate1);
         dstate[1] = M( cutoff, S( satstate0, satstate1 ) );
                        
         // dstate[2] = cutoff * (satstate1 - satstate2);
         dstate[2] = M( cutoff, S( satstate1, satstate2 ) );
         
         // dstate[3] = cutoff * (satstate2 - clip(state[3], saturation, saturationInv));
         dstate[3] = M( cutoff, S( satstate2, clip( state[3], saturation, saturationInv)));
      }
      
      __m128 process( QuadFilterUnitState * __restrict f, __m128 input )
      {
         int i;
         __m128 deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

         __m128 *state = &( f->R[0] );

         auto stepSize = F( dsamplerate_os_inv * extraOversampleInv ),
            halfStepSize = F( 0.5 * dsamplerate_os_inv * extraOversampleInv );

         static const __m128 oneoversix = F( 1.0 / 6.0 ), two = F(2.0), dFac = F(extraOversampleInv),
            sat = F(saturation), satInv=F(saturationInverse);
         
         for( int i=0; i<extraOversample; ++i )
         {
            for( int j=0; j< n_rkcoeff; ++j )
               f->C[j] = A(f->C[j], M(dFac,f->dC[j]));

            __m128 cutoff = f->C[rkm_cutoff];
            __m128 resonance = f->C[rkm_reso];

            calculateDerivatives( input, deriv1, state, cutoff, resonance, sat, satInv);
            for (i = 0; i < 4; i++)
               tempState[i] = A( state[i], M( halfStepSize, deriv1[i] ) );
            
            calculateDerivatives(input, deriv2, tempState, cutoff, resonance, sat, satInv);
            for (i = 0; i < 4; i++)
               tempState[i] = A( state[i], M( halfStepSize, deriv2[i] ) );

            calculateDerivatives(input, deriv3, tempState, cutoff, resonance, sat, satInv);
            for (i = 0; i < 4; i++)
               tempState[i] = A( state[i], M( halfStepSize, deriv3[i] ) );
            
            calculateDerivatives(input, deriv4, tempState, cutoff, resonance, sat, satInv);
            for (i = 0; i < 4; i++)
               // state[i] +=(1.0 / 6.0) * stepSize * (deriv1[i] + 2.0 * deriv2[i] + 2.0 * deriv3[i] + deriv4[i]);
               state[i] = A(state[i], M( oneoversix, M( stepSize, A( deriv1[i], A( M( two, deriv2[i] ), A( M( two, deriv3[i] ), deriv4[i] ) ) ) ) ) );
            
            input = _mm_setzero_ps();
         }
         
         return M( state[3], F(extraOversample) );
      }

#undef F
#undef M
#undef A
#undef S
      
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

      enum huov_coeffs { h_cutoff = 0, h_res, h_fc, n_hcoeffs };
      enum huov_regoffsets { h_stage = 0, h_stageTanh = 4, h_delay = 7 };

      int extraOversample = 4;
      float extraOversampleInv = 0.25;
      
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage ) {
         auto cutoff = VintageLadder::Common::clampedFrequency( freq, storage );
         cm->C[h_cutoff] = cutoff;

         // Heueristically at higher cutoffs the resonance becomes less stable. This is purely ear tuned at 49khz with noise input
         float co = std::max( cutoff - samplerate * 0.33333, 0.0 ) * 0.1 * dsamplerate_os_inv;

         reso = limit_range( reso, 0.0f, 0.994f - co );
         cm->C[h_res] = reso;
         
         double fc =  cutoff * dsamplerate_os_inv * extraOversampleInv;
         cm->C[h_fc] = fc;
      }

      __m128 process( QuadFilterUnitState * __restrict f, __m128 in )
      {
#define F(a) _mm_set_ps1( a )         
#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )
         
         static const __m128 dFac = M( F( 0.5 ), F( extraOversampleInv ) ),
            half = F(0.5),
            one = F(1.0),
            four = F(4.0),
            m18730 = F(1.8730),
            m04955 = F(0.4995),
            mneg06490 = F(-0.6490),
            m09988 = F(0.9988),
            mneg39364 = F(-3.9364 ),
            m18409 = F(1.8409),
            m09968 = F(0.9968 ),
            thermal = F(0.000025),
            oneoverthermal = F(40000),
            neg2pi = F( -2.0 * M_PI );


         for( int j=0; j<2 * extraOversample; ++j )
         {

            auto fc = f->C[h_fc];
            auto res = f->C[h_res];

            auto fr = M(fc, half);
            auto fc2 = M( fc, fc );
            auto fc3 = M( fc, fc2 );
            
            // double fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
            auto fcr = A( M( m18730, fc3 ), A( M( m04955, fc2 ), A( M( mneg06490, fc ), m09988 ) ) );
            //auto acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
            auto acr = A( M( mneg39364, fc2 ), A( M( m18409, fc ), m09968 ) );
            
            // auto tune = (1.0 - exp(-((2 * M_PI) * f * fcr))) / thermal;
            auto tune = M( S( one, Surge::DSP::fastexpSSE( M( neg2pi, M( fr, fcr ) ) ) ), oneoverthermal );
            // auto resquad = 4.0 * res * arc;
            auto resquad = M( four, M( res, acr ) );

            for( int k=0; k<= n_hcoeffs; ++k )
               f->C[k] = _mm_add_ps(f->C[k], _mm_mul_ps( dFac, f->dC[k]));

            
				// float input = in - resQuad * delay[5]. Model as an impulse stream
            auto input = _mm_sub_ps( (j == 0 ? in : _mm_setzero_ps()) , _mm_mul_ps( resquad, f->R[h_delay + 5] ) );

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

         }

         return _mm_mul_ps( F(2 * extraOversample ), f->R[h_delay + 5] );
#undef M
#undef A
#undef S
#undef F                            
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

      enum imp_coeffs { i_cutoff = 0, i_reso, i_x, i_drive, n_icoeff };
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
         static const __m128 fourpivt = M( F(4.0), M( F(M_PI), F(VT) ) );
         static const __m128 one = F(1.0);

         for( int i=0; i<extraOversample; ++i )
         {
            for( int j=0; j< n_icoeff; ++j )
               f->C[j] = A(f->C[j], M(dFac,f->dC[j]));

            __m128 drive = f->C[i_drive];
            __m128 resonance = f->C[i_reso];

            /*
            ** Rather than caching this in a register calculate it in each loop since it is not a linear function of coefficients
            ** but all the other coefficients are, so linear interpolation will work fine on them
            */
            // cm->C[i_g] = 4.0 * M_PI * VT * cutoff * ( 1.0 - cm->C[i_x] ) / ( 1.0 + cm->C[i_x] );
            __m128 g = M( fourpivt, M( f->C[i_cutoff ], _mm_div_ps( S( one, f->C[i_x] ), A( one, f->C[i_x] ) ) ) );



            // dV0 = -g * (tanh((drive * in + resonance * V[3]) / (2.0 * VT)) + tV[0]);
            dV0 = M( M( F(-1.f) , g ), A( Surge::DSP::fasttanhSSEclamped( M( A( M( drive, in ), M( resonance, f->R[h_V + 3] ) ), oneo2v2 ) ), f->R[h_tV + 0] ) );
            // This forces a 'zero stuffing' oversample
            in = _mm_setzero_ps();
            
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

                  
         return M( F(extraOversample), f->R[h_V + 3] );

#undef F
#undef M
#undef A
#undef S         
      }
   }

   namespace Simplified {
      /*
        The simplified nonlinear Moog filter is based on the full Huovilainen model,
        with five nonlinear (tanh) functions (4 first-order sections and a feedback).
        Like the original, this model needs an oversampling factor of at least two when
        these nonlinear functions are used to reduce possible aliasing. This model
        maintains the ability to self oscillate when the feedback gain is >= 1.0.
        
        References: DAFX - Zolzer (ed) (2nd ed)
        Original implementation: Valimaki, Bilbao, Smith, Abel, Pakarinen, Berners (DAFX)
        This is a transliteration into C++ of the original matlab source (moogvcf.m)
      */

      enum simp_coeffs { s_cutoff = 0, s_reso, n_scoeffs };
      enum simp_regoffsets { h_stage = 0, h_stageZ1 = 4, h_stagetanh = 8 };

      int extraOversample = 4;
      float extraOversampleInv = 0.25;

      float gainCompensation = 0.5;
         
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage )
      {
         auto cutoff = VintageLadder::Common::clampedFrequency( freq, storage );
         cm->C[s_cutoff] = cutoff;
         cm->C[s_reso ] = reso * 5;
      }

      __m128 process( QuadFilterUnitState * __restrict f, __m128 in )
      {
#define F(a) _mm_set_ps1( a )         
#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
#define S(a,b) _mm_sub_ps( a, b )

         static const __m128 twopi = F( 2.0 * M_PI ),
            oneover13 = F( 1.0 / 1.3 ),
            p3over13 = F( 0.3 / 1.3 ),
            piover13 = F( M_PI / 1.3 ),
            dFac = F(extraOversampleInv),
            gComp = F(gainCompensation);

         __m128 *stage = &(f->R[h_stage]), *stageZ1  = &(f->R[h_stageZ1]), *stageTanh = &(f->R[h_stagetanh]);

         __m128 fs2i = M( F(dsamplerate_os_inv), F(extraOversampleInv ) );
         
         for( int i=0; i<extraOversample; ++i )
         {
            for( int j=0; j< n_scoeffs; ++j )
               f->C[j] = A(f->C[j], M(dFac,f->dC[j]));

            auto cutoff = f->C[s_cutoff];
            auto resonance = f->C[s_reso];

            //g = (2 * MOOG_PI) * cutoff / fs2; // feedback coefficient at fs*2 because of doublesampling
            //g *= MOOG_PI / 1.3; // correction factor that allows _cutoff to be supplied Hertz
            auto g = M( M( twopi, M( cutoff, fs2i ) ), piover13 );
            auto h = M( oneover13, g );
            auto h0 = M( p3over13, g );
            
            for (int stageIdx = 0; stageIdx < 4; ++stageIdx)
            {
               if (stageIdx)
               {
                  auto input = stage[stageIdx-1];
                  stageTanh[stageIdx-1] = Surge::DSP::fasttanhSSEclamped(input);
                  stage[stageIdx] = (h * stageZ1[stageIdx] + h0 * stageTanh[stageIdx-1]) + (1.0 - g) * (stageIdx != 3 ? stageTanh[stageIdx] : Surge::DSP::fasttanhSSEclamped(stageZ1[stageIdx]));
               }
               else
               {
                  __m128 input;
                  if( i == 0 )
                  {
                     // input = samples[s] - ((4.0 * resonance) * (output - gainCompensation * samples[s]));
                     input = S(in, M( M( F(4.0), resonance ), S( stage[3], M( gComp, in ) ) ) );
                  }
                  else
                  {
                     input = _mm_setzero_ps();
                  }
                  // stage[stageIdx] = (h * tanh(input) + h0 * stageZ1[stageIdx]) + (1.0 - g) * stageTanh[stageIdx];
                  auto p1 = M( h, Surge::DSP::fasttanhSSEclamped( input ) );
                  auto p2 = M( h0, stageZ1[stageIdx] );
                  auto p3 = M( S( F(1.0), g ), stageTanh[stageIdx] );
                  stage[stageIdx] = A( p1, A( p2, p3 ) );
               }
               
               stageZ1[stageIdx] = stage[stageIdx];
            }
         }
         return M( stage[3], F( extraOversample ) );
      }

      
#undef F
#undef M
#undef A
#undef S         
   }

}
