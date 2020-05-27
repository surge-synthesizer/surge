#include "effect_defs.h"
#include "Tunings.h"

// http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf

enum ringmodparam
{
   // Basic Control
   rm_carrierfreq = 0, // Carrier Rate
   rm_carriershape,

   rm_diode_vb,
   rm_diode_vl,
   
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
   phase = 0.f;
}

void RingModulatorEffect::setvars(bool init)
{
}


void RingModulatorEffect::process(float* dataL, float* dataR)
{
   // TODO lag this
   auto dphase = storage->note_to_pitch( *f[ rm_carrierfreq ] ) * Tunings::MIDI_0_FREQ * samplerate_inv;
   auto mix = *f[ rm_mix ];

   for( int i=0; i<BLOCK_SIZE; ++i )
   {
      // TODO efficiency of course
      auto vc = sin( 2.0 * M_PI * phase );
      phase += dphase;
      if( phase > 1 )
      {
         phase -= 1;
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
         if( c == 0 )
         {
            dataL[i] = mix * res + ( 1.f - mix ) * vin;
         }
         else
         {
            dataR[i] = mix * res + ( 1.f - mix ) * vin;
         }
         
      }
      
   }
   
}

void RingModulatorEffect::suspend()
{
   init();
}

const char* RingModulatorEffect::group_label(int id)
{
   return 0;
}

int RingModulatorEffect::group_label_ypos(int id)
{
   return 0;
}

void RingModulatorEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[rm_carrierfreq].set_name( "Carrier Frequency" );
   fxdata->p[rm_carrierfreq].set_type( ct_flangerpitch );

   fxdata->p[rm_carriershape].set_name( "Carrier Shape (UNIMPL)" );
   fxdata->p[rm_carriershape].set_type( ct_flangerwave );

   fxdata->p[rm_diode_vb].set_name( "Diode Crossing Voltage" );
   fxdata->p[rm_diode_vb].set_type( ct_percent );
   fxdata->p[rm_diode_vl].set_name( "Diode Linear Voltage" );
   fxdata->p[rm_diode_vl].set_type( ct_percent );

   fxdata->p[rm_mix].set_name( "Mix" );
   fxdata->p[rm_mix].set_type( ct_percent );

   for( int i=rm_carrierfreq; i<rm_num_params; ++i )
   {
      auto a = 1;
      //if( i >= flng_voices ) a += 2;
      //if( i >= flng_feedback ) a += 2;
      //if( i >= flng_stereo_width ) a += 2;
      fxdata->p[i].posy_offset = a;
   }


}

void RingModulatorEffect::init_default_values()
{
   fxdata->p[rm_carrierfreq].val.f = 60;
   fxdata->p[rm_carriershape].val.i = 0;
   fxdata->p[rm_diode_vb].val.f = 0.3;
   fxdata->p[rm_diode_vl].val.f = 0.7;
   fxdata->p[rm_mix].val.f = 1.0;
}

float RingModulatorEffect::diode_sim(float v)
{
   auto vb = *(f[rm_diode_vb]);
   auto vl = *(f[rm_diode_vl]);
   auto h = 1.f;
   vl = std::max( vl, vb + 0.1f );
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
