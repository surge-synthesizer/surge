#include "effect_defs.h"
#include "Tunings.h"
#include "Oscillator.h"
#include "DebugHelpers.h"
#include "FastMath.h"

// http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf

enum ringmodparam
{
   // Basic Control
   rm_carrierfreq = 0, // Carrier Rate
   rm_carriershape,

   rm_diode_vb,
   rm_diode_vl,

   rm_unison_detune,
   rm_unison_voices,
   
   rm_mix,
   
   rm_num_params,
};

RingModulatorEffect::RingModulatorEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   init();
}

RingModulatorEffect::~RingModulatorEffect()
{
}

void RingModulatorEffect::init()
{
   last_unison = -1;
}

void RingModulatorEffect::setvars(bool init)
{
}


void RingModulatorEffect::process(float* dataL, float* dataR)
{
   float  dphase[MAX_UNISON];
   auto mix = *f[ rm_mix ];
   auto uni = std::max( 1, *pdata_ival[rm_unison_voices] );
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

   float gscale = 0.4 +  0.6 * ( 1.f / sqrtf( uni ) );

   for( int u=0; u<uni; ++u )
   {
      // need to calc this every time since carierfreq could change
      dphase[u] = storage->note_to_pitch( *f[ rm_carrierfreq ] + fxdata->p[rm_unison_detune].get_extended(fxdata->p[rm_unison_detune].val.f * detune_offset[u]) ) *
         Tunings::MIDI_0_FREQ * samplerate_inv;
   }
   
   for( int i=0; i<BLOCK_SIZE; ++i )
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
            phase[u] -= 1;
         }
         for( int c=0; c<2; ++c )
         {
            auto vin = ( c == 0 ? dataL[i] : dataR[i] );
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
      auto outl = gscale * ( mix * resL + ( 1.f - mix ) * dataL[i] );
      auto outr = gscale * ( mix * resR + ( 1.f - mix ) * dataR[i] );

      outl = 1.5 * outl - 0.5 * outl * outl * outl;
      outr = 1.5 * outr - 0.5 * outr * outr * outr;
      
      dataL[i] = outl;
      dataR[i] = outr;


   }
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
      return "Carrier Unison";
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
      return 7;
   case 2:
      return 13;
   case 3:
      return 19;
   }
   return 0;
}

void RingModulatorEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[rm_carrierfreq].set_name( "Pitch" );
   fxdata->p[rm_carrierfreq].set_type( ct_flangerpitch );

   fxdata->p[rm_carriershape].set_name( "Shape" );
   fxdata->p[rm_carriershape].set_type( ct_sineoscmode );

   fxdata->p[rm_diode_vb].set_name( "Forward Bias Voltage" );
   fxdata->p[rm_diode_vb].set_type( ct_percent );
   fxdata->p[rm_diode_vl].set_name( "Linear Regime Voltage" );
   fxdata->p[rm_diode_vl].set_type( ct_percent );

   fxdata->p[rm_unison_detune].set_name( "Unison Detune" );
   fxdata->p[rm_unison_detune].set_type( ct_oscspread );
   fxdata->p[rm_unison_voices].set_name( "Unison Voices" );
   fxdata->p[rm_unison_voices].set_type( ct_osccount );

   fxdata->p[rm_mix].set_name( "Mix" );
   fxdata->p[rm_mix].set_type( ct_percent );

   for( int i=rm_carrierfreq; i<rm_num_params; ++i )
   {
      auto a = 1;
      if( i >= rm_diode_vb ) a += 2;
      if( i >= rm_unison_detune ) a += 2;
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
