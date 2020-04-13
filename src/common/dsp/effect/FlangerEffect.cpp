#include "effect_defs.h"

#define _D(x) " " << (#x) << "=" << x

enum flangparam
{
   // Basic Control
   flng_rate = 0, // How quickly the oscillations happen
   flng_depth, // How extreme the modulation of the delay is
   flng_mode, // flange, phase-inverse-flange, arepeggio, vibrato

   // Voices
   flng_voices, // how many delay lines
   
   flng_voice_zero_pitch, // tune the first max delay line to this (M = sr / f)
   flng_voice_detune, // spread in cents among the delay lines   
   flng_voice_chord, // tuning for the voices as a chord in tuning space

   // Feedback and EQ
   flng_feedback, // how much the output feeds back into the filters
   flng_fb_lf_damping, // how much low pass damping in the feedback mechanism
   flng_gain, // How much gain before we run into the final clipper
   flng_stereo_width, // how much to pan the delay lines ( 0 -> all even; 1 -> full spread)
   flng_mix, // how much we add the comb into the mix
   
   flng_num_params,
};

enum flangermode // match this with the string gneerator in Parameter.cpp pls
{
   unisontri,
   unisonsin,
   unisonsaw,
   unisonsandh,
   
   dopplertri,
   dopplersin,
   dopplersaw,
   dopplersandh,

   arptri,
   arpsin,
   arpsaw,
   arpsandh,

   n_flanger_modes
};

FlangerEffect::FlangerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   init();
}

FlangerEffect::~FlangerEffect()
{
}

void FlangerEffect::init()
{
   for( int c=0;c<2;++c )
      for( int i=0; i<COMBS_PER_CHANNEL; ++i )
      {
         lfophase[c][i] =  1.f * ( i + 0.5 * c ) / COMBS_PER_CHANNEL;
      }
   longphase = 0;

   for( int i=0; i<LFO_TABLE_SIZE; ++i )
   {
      sin_lfo_table[i] = sin( 2.0 * M_PI * i / LFO_TABLE_SIZE );
   }
}

void FlangerEffect::setvars(bool init)
{
}


void FlangerEffect::process(float* dataL, float* dataR)
{
   // So here is a flanger with everything fixed

   float rate = envelope_rate_linear(-*f[flng_rate]) * (fxdata->p[flng_rate].temposync ? storage->temposyncratio : 1.f);

   longphase += rate;
   if( longphase >= COMBS_PER_CHANNEL ) longphase -= COMBS_PER_CHANNEL;
   
   const float oneoverFreq0 = 1.0f / 8.175798915;

   int mode = *pdata_ival[flng_mode];
   int mtype = mode / 4;
   int mwave = mode % 4;

   float v0 = *f[flng_voice_zero_pitch];
   for( int c=0; c<2; ++c )
      for( int i=0; i<COMBS_PER_CHANNEL; ++i )
      {

         lfophase[c][i] += rate;
         bool lforeset = false;
         if( lfophase[c][i] > 1 )
         {
            lforeset = true;
            lfophase[c][i] -= 1;
         }
         float lfoout = lfoval[c][i].v;
         float thisphase = lfophase[c][i];
         if( mtype == 2 )
         {
            // arpeggio - everyone needs to use the same phase with the voice swap
            thisphase = longphase - (int)longphase;
         }
         switch( mwave )
         {
         case 0: // sin
         {
            float ps = thisphase * LFO_TABLE_SIZE;
            int psi = (int)ps;
            float psf = ps - psi;
            int psn = (psi+1) & LFO_TABLE_MASK;
            lfoout = sin_lfo_table[psi] * ( 1.0 - psf ) + psf * sin_lfo_table[psn];
         }
         break;
         case 1: // triangle
            lfoout = (2.f * fabs(2.f * thisphase - 1.f) - 1.f);
            break;
         case 2: // saw - but we gotta be gentler than a pure saw. So FIXME on this waveform
            lfoout = thisphase * 2.0f - 1.f;
            break;
         case 3: // S&H random noise. Needs smoothing over the jump like the triangle
            if( lforeset )
            {
               lfoout = 1.f * rand() / RAND_MAX  - 1.f;
            }
         }

         lfoval[c][i].newValue(lfoout);
         delaybase[c][i].newValue( samplerate * oneoverFreq0 * storage->note_to_pitch_inv((float)(v0 + i * 6)) );
      }
   
   depth.newValue( *f[flng_depth] );
   mix.newValue( *f[flng_mix] );
   float feedbackScale = 0.4;
   if( mtype == 1 )
   {
      feedbackScale = 0.6;
   }
   if( mtype == 2 )
   {
      feedbackScale = 0.9;
   }
   float fbv = *f[flng_feedback];
   fbv = fbv * fbv * fbv;
   feedback.newValue( feedbackScale * fbv ); 
   fb_lf_damping.newValue( 0.4 * *f[flng_fb_lf_damping ] );
   gain.newValue( *f[flng_gain] );

   float combs alignas(16)[2][BLOCK_SIZE];

   // Obviously when we implement stereo spread this will be different
   float vweights[2][COMBS_PER_CHANNEL];
   for( int c=0; c<2; ++c )
   {
      for( int i=0; i<COMBS_PER_CHANNEL; ++i )
         vweights[c][i] = 0;
      
      if( mtype == 2 )
      {
         int ilp = (int)longphase;
         float flp = longphase - ilp;
         if( ilp == COMBS_PER_CHANNEL )
            ilp = 0;
         
         if( flp > 0.9 )
         {
            float dt = (flp - 0.9) * 10; // this will be between 0,1
            float nxt = sqrt( dt );
            float prr = sqrt( 1.f - dt );
            // std::cout << _D(longphase) << _D(dt) << _D(nxt) << _D(prr) << _D(ilp) << _D(flp) << std::endl;
            vweights[c][ilp] = prr;
            if( ilp == COMBS_PER_CHANNEL - 1 )
               vweights[c][0] = nxt;
            else
               vweights[c][ilp + 1] = nxt;
            
         }
         else
         {
            vweights[c][ilp] = 1.f;
         }
      }
      else
      {
         float voices = *f[flng_voices];
         vweights[c][0] = 1.0;
         
         for( int i=0; i<voices && i < 4; ++i )
            vweights[c][i] = 1.0;
         
         int li = (int)voices;
         float fi = voices-li;
         if( li < 4 )
            vweights[c][li] = fi;
      }
   }

   for( int b=0; b<BLOCK_SIZE; ++b )
   {
      for( int c=0; c<2; ++c ) {
         combs[c][b] = 0;
         for( int i=0; i<COMBS_PER_CHANNEL; ++i )
         {
            if( vweights[c][i] > 0 )
            {
               auto tap = delaybase[c][i].v * ( 1.0 + lfoval[c][i].v * depth.v ) + 1;
               auto v = idels[c].value(tap);
               combs[c][b] += vweights[c][i] * v;
            }
            
            lfoval[c][i].process();
            delaybase[c][i].process();
         }
      }
      // softclip the feedback to avoid explosive runaways
      float fbl = 0.f;
      float fbr = 0.f;
      if( feedback.v > 0 )
      {
         fbl = limit_range( feedback.v * combs[0][b], -1.f, 1.f );
         fbr = limit_range( feedback.v * combs[1][b], -1.f, 1.f );

         fbl = 1.5 * fbl - 0.5 * fbl * fbl * fbl;
         fbr = 1.5 * fbr - 0.5 * fbr * fbr * fbr;

         // and now we have clipped, apply the damping. FIXME - move to one mul form
         lpaL = lpaL * ( 1.0 - fb_lf_damping.v ) + fbl * fb_lf_damping.v;
         fbl = fbl - lpaL;

         lpaR = lpaR * ( 1.0 - fb_lf_damping.v ) + fbr * fb_lf_damping.v;
         fbr = fbr - lpaR;
      }
      
      auto vl = dataL[b] - fbl;
      auto vr = dataR[b] - fbr;
      idels[0].push( vl );
      idels[1].push( vr );
      
      auto origw = 1.f;
      if( mtype == 1 )
      {
         // doppler modes
         origw = 0.f;
      }
         
      float outl = origw * dataL[b] + mix.v * combs[0][b];
      float outr = origw * dataR[b] + mix.v * combs[1][b];

      outl = limit_range( (1.0f + gain.v ) * outl, -1.f, 1.f );
      outr = limit_range( (1.0f + gain.v ) * outr, -1.f, 1.f );
      
      outl = 1.5 * outl - 0.5 * outl * outl * outl;
      outr = 1.5 * outr - 0.5 * outr * outr * outr;
      
      dataL[b] = outl;
      dataR[b] = outr;
      
      depth.process();
      mix.process();
      feedback.process();
      fb_lf_damping.process();
      gain.process();
   }
}

float FlangerEffect::InterpDelay::value( float delayBy )
{
   // so if delayBy is 19.2
   int itap = (int) delayBy; // this is 19
   float fractap = delayBy - itap; // this is .2
   int k0 = ( k + DELAY_SIZE - itap - 1 ) & DELAY_SIZE_MASK; // this is 20 back
   int k1 = ( k + DELAY_SIZE - itap     ) & DELAY_SIZE_MASK; // this is 19 back
   // std::cout << "KDATUM" << k << "," << delayBy << "," << itap << "," << k0 << "," << k1 << "," << fractap << std::endl;
   // so the result is
   float result = line[k0] * fractap + line[k1] * ( 1.0 - fractap ); // FIXME move to the one mul form
   // std::cout << "id::value " << _D(delayBy) << _D(itap) << _D(fractap) << _D(k) << _D(k0) << _D(k1) << _D(result) << _D(line[k0]) << _D(line[k1]) << std::endl;

   return result;
}

void FlangerEffect::suspend()
{
   init();
}

const char* FlangerEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Modulation";
   case 1:
      return "Combs";
   case 2:
      return "Feedback";
   case 3:
      return "Output";
   }
   return 0;
}
int FlangerEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 9;
   case 2:
      return 19;
   case 3:
      return 27;
   }
   return 0;
}

void FlangerEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[flng_rate].set_name( "Rate" );
   fxdata->p[flng_rate].set_type( ct_lforate );

   fxdata->p[flng_depth].set_name( "Depth" );
   fxdata->p[flng_depth].set_type( ct_percent );

   fxdata->p[flng_mode].set_name( "Mode" );
   fxdata->p[flng_mode].set_type( ct_flangermode );

   fxdata->p[flng_voices].set_name( "Count" );
   fxdata->p[flng_voices].set_type( ct_flangervoices );

   fxdata->p[flng_voice_zero_pitch].set_name( "Base Pitch" );
   fxdata->p[flng_voice_zero_pitch].set_type( ct_flangerpitch );

   fxdata->p[flng_voice_detune].set_name( "Detune (unimpl)" );
   fxdata->p[flng_voice_detune].set_type( ct_percent );

   fxdata->p[flng_voice_chord].set_name( "Chord (unimpl)" );
   fxdata->p[flng_voice_chord].set_type( ct_percent );

   fxdata->p[flng_feedback].set_name( "Feedback" );
   fxdata->p[flng_feedback].set_type( ct_percent );

   fxdata->p[flng_fb_lf_damping].set_name( "LF Damp" );
   fxdata->p[flng_fb_lf_damping].set_type( ct_percent );

   fxdata->p[flng_gain].set_name( "Saturation" );
   fxdata->p[flng_gain].set_type( ct_percent_bidirectional );

   fxdata->p[flng_stereo_width].set_name( "Width (unimpl)" );
   fxdata->p[flng_stereo_width].set_type( ct_percent );

   fxdata->p[flng_mix].set_name( "Mix" );
   fxdata->p[flng_mix].set_type( ct_percent_bidirectional );


   for( int i=flng_rate; i<flng_num_params; ++i )
   {
      auto a = 1;
      if( i >= flng_voices ) a += 2;
      if( i >= flng_feedback ) a += 2;
      if( i >= flng_stereo_width ) a += 2;
      fxdata->p[i].posy_offset = a;
   }

}

void FlangerEffect::init_default_values()
{
   fxdata->p[flng_rate].val.f = -2.f;
   fxdata->p[flng_depth].val.f = 1.f;
   fxdata->p[flng_mix].val.f = 0.8f;

   //flng_mode, // flange, phase-inverse-flange, arepeggio, vibrato

   // Voices

   fxdata->p[flng_voices].val.f = 4.0f;
   fxdata->p[flng_voice_zero_pitch].val.f = 60.f;
   //flng_voice_detune, // spread in cents among the delay lines   
   //flng_voice_dephase, // how much the modulation phase varies 0 is in phase; 1 is max spread
   //flng_voice_chord, // tuning for the voices as a chord in tuning space

   //flng_feedback, // how much the output feeds back into the filters
   fxdata->p[flng_feedback].val.f = 0.f;
   fxdata->p[flng_fb_lf_damping].val.f = 0.1f;
   //flng_stereo_width, // how much to pan the delay lines ( 0 -> all even; 1 -> full spread)

}
