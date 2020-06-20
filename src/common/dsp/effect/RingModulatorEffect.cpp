#include "effect_defs.h"
#include "Tunings.h"
#include "Oscillator.h"
#include "DebugHelpers.h"
#include "FastMath.h"

// http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf

enum ringmodparam
{
   // Basic Control
   rm_carriershape = 0, // carrier shape
   rm_carrierfreq,
   rm_unison_detune,
   rm_unison_voices,

   rm_diode_vb,
   rm_diode_vl,
   
   rm_lowcut,
   rm_highcut,
   
   rm_mix,
   
   rm_num_params,
};

RingModulatorEffect::RingModulatorEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
   : Effect(storage, fxdata, pd),
     halfbandIN(6,true), halfbandOUT(6,true),
     lp(storage), hp(storage)
{
}

RingModulatorEffect::~RingModulatorEffect()
{
}

void RingModulatorEffect::init()
{
   setvars(true);
}

void RingModulatorEffect::setvars(bool init)
{
   if( init )
   {
      last_unison = -1;
      halfbandOUT.reset();
      halfbandIN.reset();
      
      lp.suspend();
      hp.suspend();

      hp.coeff_HP(hp.calc_omega(*f[rm_lowcut] / 12.0), 0.707);
      hp.coeff_instantize();
      
      lp.coeff_LP2B(lp.calc_omega(*f[rm_highcut] / 12.0), 0.707);
      lp.coeff_instantize();
   }
}


#define OVERSAMPLE 1

void RingModulatorEffect::process(float* dataL, float* dataR)
{
   float  dphase[MAX_UNISON];
   auto mix = *f[ rm_mix ];
   auto uni = std::max( 1, *pdata_ival[rm_unison_voices] );

   // Has unison reset? If so modify settings
   if( uni != last_unison )
   {
      last_unison = uni;
      if( uni == 1 )
      {
         detune_offset[0] = 0;
         panL[0] = 1.f;
         panR[0] = 1.f;
         phase[0] = 0.f;
      }
      else
      {
         float detune_bias = (float)2.f / (uni - 1.f);
            
         for( auto u=0; u < uni; ++u )
         {
            phase[u] = u * 1.f / ( uni );
            detune_offset[u] = -1.f + detune_bias * u;

            panL[u] = u / (uni - 1.f );
            panR[u] = (uni - 1.f - u ) / (uni - 1.f);
         }

      }
   }

   // Gain Scale based on unison
   float gscale = 0.4 +  0.6 * ( 1.f / sqrtf( uni ) );

#if OVERSAMPLE
   // Now upsample
   float dataOS alignas(16)[2][BLOCK_SIZE_OS];
   halfbandIN.process_block_U2(dataL, dataR, dataOS[0], dataOS[1] );
#else
   float *dataOS[2];
   dataOS[0] = dataL;
   dataOS[1] = dataR;
#endif

   double sri = dsamplerate_inv;
#if OVERSAMPLE
   sri = dsamplerate_os_inv;
#endif   
   for( int u=0; u<uni; ++u )
   {
      // need to calc this every time since carierfreq could change
      dphase[u] = storage->note_to_pitch( *f[ rm_carrierfreq ] + fxdata->p[rm_unison_detune].get_extended(fxdata->p[rm_unison_detune].val.f * detune_offset[u]) ) *
         Tunings::MIDI_0_FREQ * sri;
   }

   int ub = BLOCK_SIZE;
#if OVERSAMPLE
   ub = BLOCK_SIZE_OS;
#endif
   
   for( int i=0; i<ub; ++i )
   {
      float resL = 0, resR = 0;
      for( int u=0; u<uni; ++u )
      {
         // TODO efficiency of course
         auto vc = osc_sine::valueFromSinAndCos( Surge::DSP::fastsin( 2.0 * M_PI * ( phase[u] - 0.5 ) ),
                                                 Surge::DSP::fastcos( 2.0 * M_PI * ( phase[u] - 0.5 ) ),
                                                 *pdata_ival[rm_carriershape] );
         phase[u] += dphase[u];
         if( phase[u] > 1 )
         {
            phase[u] -= (int)phase[u];
         }
         for( int c=0; c<2; ++c )
         {
            auto vin = ( c == 0 ? dataOS[0][i] : dataOS[1][i] );
            auto wd  = 1.0;
            
            auto A = 0.5 * vin + vc;
            auto B = vc - 0.5 * vin;
            
            float dPA = diode_sim(A);
            float dMA = diode_sim(-A);
            float dPB = diode_sim(B);
            float dMB = diode_sim(-B);
            
            float res = dPA + dMA - dPB - dMB;
            resL += res * panL[u];
            resR += res * panR[u];
            // std::cout << "RES " << _D(res) << _D(resL) << _D(resR) << _D(panL[u]) << _D(panR[u]) << _D(u) << _D(uni) << std::endl;
         }
      }
      auto outl = gscale * ( mix * resL + ( 1.f - mix ) * dataOS[0][i] );
      auto outr = gscale * ( mix * resR + ( 1.f - mix ) * dataOS[1][i] );

      outl = 1.5 * outl - 0.5 * outl * outl * outl;
      outr = 1.5 * outr - 0.5 * outr * outr * outr;
      
      dataOS[0][i] = outl;
      dataOS[1][i] = outr;
   }

#if OVERSAMPLE   
   halfbandOUT.process_block_D2(dataOS[0], dataOS[1]);
   copy_block(dataOS[0], dataL, BLOCK_SIZE_QUAD);
   copy_block(dataOS[1], dataR, BLOCK_SIZE_QUAD);
#endif

   // Apply the filters
   hp.coeff_HP(hp.calc_omega(*f[rm_lowcut] / 12.0), 0.707);
   lp.coeff_LP2B(lp.calc_omega(*f[rm_highcut] / 12.0), 0.707);

   lp.process_block(dataL, dataR);
   hp.process_block(dataL, dataR);
}

void RingModulatorEffect::suspend()
{
   init();
}

const char* RingModulatorEffect::group_label(int id)
{
   switch( id )
   {
   case 0:
      return "Carrier";
   case 1:
      return "Diode";
   case 2:
      return "EQ";
   case 3:
      return "Output";
   }
   return 0;
}

int RingModulatorEffect::group_label_ypos(int id)
{
   switch( id )
   {
   case 0:
      return 1;
   case 1:
      return 11;
   case 2:
      return 17;
   case 3:
      return 23;
   }
   return 0;
}

void RingModulatorEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[rm_carriershape].set_name( "Shape" );
   fxdata->p[rm_carriershape].set_type( ct_sineoscmode );
   fxdata->p[rm_carrierfreq].set_name( "Pitch" );
   fxdata->p[rm_carrierfreq].set_type( ct_flangerpitch );
   fxdata->p[rm_unison_detune].set_name( "Unison Detune" );
   fxdata->p[rm_unison_detune].set_type( ct_oscspread );
   fxdata->p[rm_unison_voices].set_name( "Unison Voices" );
   fxdata->p[rm_unison_voices].set_type( ct_osccount );

   fxdata->p[rm_diode_vb].set_name( "Forward Bias" );
   fxdata->p[rm_diode_vb].set_type( ct_percent );
   fxdata->p[rm_diode_vl].set_name( "Linear Region" );
   fxdata->p[rm_diode_vl].set_type( ct_percent );

   fxdata->p[rm_lowcut].set_name( "Low Cut" );
   fxdata->p[rm_lowcut].set_type( ct_freq_audible );
   fxdata->p[rm_highcut].set_name( "High Cut" );
   fxdata->p[rm_highcut].set_type( ct_freq_audible );

   fxdata->p[rm_mix].set_name( "Mix" );
   fxdata->p[rm_mix].set_type( ct_percent );

   for( int i = rm_carriershape; i < rm_num_params; ++i )
   {
      auto a = 1;
      if( i >= rm_diode_vb ) a += 2;
      if( i >= rm_lowcut ) a += 2;
      if( i >= rm_mix ) a += 2;
      fxdata->p[i].posy_offset = a;
   }


}

void RingModulatorEffect::init_default_values()
{
   fxdata->p[rm_carrierfreq].val.f = 60;
   fxdata->p[rm_carriershape].val.i = 0;
   fxdata->p[rm_diode_vb].val.f = 0.3;
   fxdata->p[rm_diode_vl].val.f = 0.7;
   fxdata->p[rm_unison_detune].val.f = 0.2;
   fxdata->p[rm_unison_voices].val.i = 1;

   // FIX THIS
   fxdata->p[rm_lowcut].val.f = fxdata->p[rm_lowcut].val_min.f;
   fxdata->p[rm_highcut].val.f = fxdata->p[rm_highcut].val_max.f;
   fxdata->p[rm_mix].val.f = 1.0;
}

float RingModulatorEffect::diode_sim(float v)
{
   auto vb = *(f[rm_diode_vb]);
   auto vl = *(f[rm_diode_vl]);
   auto h = 1.f;
   vl = std::max( vl, vb + 0.02f );
   if( v < vb )
   {
      return 0;
   }
   if( v < vl )
   {
      auto vvb = v - vb;
      return h * vvb * vvb / ( 2.f * vl - 2.f * vb );
   }
   auto vlvb = vl - vb;
   return h * v - h * vl + h * vlvb * vlvb / ( 2.f * vl - 2.f * vb );
}
