#include "FlangerEffect.h"
#include "Tunings.h"
#include "DebugHelpers.h"
#include <algorithm>

enum flangparam
{
   // Basic Control
   flng_mode = 0, // flange, phase-inverse-flange, arepeggio, vibrato
   flng_wave, // what's the wave shape
   flng_rate, // How quickly the oscillations happen
   flng_depth, // How extreme the modulation of the delay is
   
   // Voices
   flng_voices, // how many delay lines
   flng_voice_zero_pitch, // tune the first max delay line to this (M = sr / f)
   flng_voice_spacing, // How far apart are the combs in pitch space

   // Feedback and EQ
   flng_feedback, // how much the output feeds back into the filters
   flng_fb_lf_damping, // how much low pass damping in the feedback mechanism
   flng_stereo_width, // how much to pan the delay lines ( 0 -> all even; 1 -> full spread)
   flng_mix, // how much we add the comb into the mix
   
   flng_num_params,
};

FlangerEffect::FlangerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   haveProcessed = false;
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
         lfosandhtarget[c][i] = 0.0;
      }
   longphase[0] = 0;
   longphase[1] = 0.5;
   
   for( int i=0; i<LFO_TABLE_SIZE; ++i )
   {
      sin_lfo_table[i] = sin( 2.0 * M_PI * i / LFO_TABLE_SIZE );
      
      saw_lfo_table[i] = 0;
      
      // http://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
      double panAngle = 1.0 * i / (LFO_TABLE_SIZE - 1) * M_PI / 2.0;
      auto piby2 = M_PI / 2.0;
      auto lW = sqrt( ( piby2 - panAngle ) / piby2 * cos( panAngle ) );
      auto rW = sqrt( panAngle * sin( panAngle ) / piby2 );
   }
   haveProcessed = false;
}

void FlangerEffect::setvars(bool init)
{
}


void FlangerEffect::process(float* dataL, float* dataR)
{
   if( ! haveProcessed )
   {
      float v0 = *f[flng_voice_zero_pitch];
      if( v0 > 0 )
         haveProcessed = true;
      vzeropitch.startValue(v0);
   }
   // So here is a flanger with everything fixed

   float rate = envelope_rate_linear(-limit_range( *f[flng_rate], -8.f, 10.f ) ) * (fxdata->p[flng_rate].temposync ? storage->temposyncratio : 1.f);

   for( int c=0; c<2; ++c )
   {
      longphase[c] += rate;
      if( longphase[c] >= COMBS_PER_CHANNEL ) longphase[c] -= COMBS_PER_CHANNEL;
   }
   
   const float oneoverFreq0 = 1.0f / Tunings::MIDI_0_FREQ;

   int mode = *pdata_ival[flng_mode];
   int mwave = *pdata_ival[flng_wave];
   
   float v0 = *f[flng_voice_zero_pitch];
   vzeropitch.newValue(v0);
   vzeropitch.process();
   v0 = vzeropitch.v;
   float averageDelayBase = 0.0;
   
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
         if( mode == arp_mix || mode == arp_solo )
         {
            // arpeggio - everyone needs to use the same phase with the voice swap
            thisphase = longphase[c] - (int)longphase[c];
         }
         switch( mwave )
         {
         case sinw: // sin
         {
            float ps = thisphase * LFO_TABLE_SIZE;
            int psi = (int)ps;
            float psf = ps - psi;
            int psn = (psi+1) & LFO_TABLE_MASK;
            lfoout = sin_lfo_table[psi] * ( 1.0 - psf ) + psf * sin_lfo_table[psn];
            lfoval[c][i].newValue(lfoout);
         }
         break;
         case triw: // triangle
            lfoout = (2.f * fabs(2.f * thisphase - 1.f) - 1.f);
            lfoval[c][i].newValue(lfoout);
            break;
         case saww: // saw - but we gotta be gentler than a pure saw. So do more like a heavily skewed triangle
         {
            float cutSawAt = 0.95;
            if( thisphase < cutSawAt )
            {
               auto usephase = thisphase / cutSawAt;
               lfoout = usephase * 2.0f - 1.f;
            }
            else
            {
               auto usephase = ( thisphase - cutSawAt ) / (1.0 - cutSawAt);
               lfoout = (1.0-usephase) * 2.f - 1.f;
            }
            lfoval[c][i].newValue(lfoout);
            break;
         }
         case sandhw: // S&H random noise. Needs smoothing over the jump like the triangle
         {
            if( lforeset )
            {
               lfosandhtarget[c][i] = 1.f * rand() / (float)RAND_MAX  - 1.f;
            }
            // FIXME - exponential creep up. We want to get there in a time related to our rate
            auto cv = lfoval[c][i].v;
            auto diff = (lfosandhtarget[c][i] - cv) * rate * 2;
            lfoval[c][i].newValue(cv + diff);
         }
         break;
         }


         auto combspace = *f[flng_voice_spacing];
         float pitch = v0 + combspace * i;
         float nv = samplerate * oneoverFreq0 * storage->note_to_pitch_inv((float)(pitch));

         // OK so biggest  tap = delaybase[c][i].v * ( 1.0 + lfoval[c][i].v * depth.v ) + 1;
         // Assume lfoval is [-1,1] and depth is known
         float maxtap = nv * ( 1.0 + limit_range( *f[flng_depth], 0.f, 2.f ) ) + 1;
         if( maxtap >= InterpDelay::DELAY_SIZE )
         {
            nv = nv * 0.999 * InterpDelay::DELAY_SIZE / maxtap;
         }
         delaybase[c][i].newValue( nv );

         averageDelayBase += delaybase[c][i].new_v;
      }
   averageDelayBase /= ( 2 * COMBS_PER_CHANNEL );
   vzeropitch.process();
   
   float dApprox = rate * samplerate / BLOCK_SIZE * averageDelayBase * *f[flng_depth];
   
   depth.newValue( limit_range( *f[flng_depth], 0.f, 2.f ) );
   mix.newValue( *f[flng_mix] );
   voices.newValue( limit_range( *f[flng_voices], 1.f, 4.f ) );
   float feedbackScale = 0.4 * sqrt( ( limit_range( dApprox, 2.f, 60.f ) + 30 ) / 100.0 );

   // Feedbackadjust based on mode
   switch( mode )
   {
   case classic:
   {
      float dv = ( voices.v - 1 );
      feedbackScale += ( 3.0 - dv ) * 0.45 / 3.0;
      break;
   }
   case doppler:
   {
      float dv = ( voices.v - 1 );
      feedbackScale += ( 3.0 - dv ) * 0.45 / 3.0;
      break;
   }
   case arp_solo:
   {
      feedbackScale += 0.2; // this is one voice doppler basically
   }
   case arp_mix:
   {
      feedbackScale += 0.3; // this is one voice classic basically and the steady signal clamps away feedback more
   }
   default:
      break;
   }

   float fbv = *f[flng_feedback];
   if( fbv > 0 )
      ringout_value = samplerate * 32.0;
   else
      ringout_value = 1024;

   if( mwave == saww || mwave == sandhw )
   {
      feedbackScale *= 0.7;
   }
   
   if( fbv < 0 ) fbv = fbv;
   else if( fbv > 1 ) fbv = fbv;
   else fbv = sqrt( fbv );

   feedback.newValue( feedbackScale * fbv ); 
   fb_lf_damping.newValue( 0.4 * *f[flng_fb_lf_damping ] );
   float combs alignas(16)[2][BLOCK_SIZE];

   // Obviously when we implement stereo spread this will be different
   for( int c=0; c<2; ++c )
   {
      for( int i=0; i<COMBS_PER_CHANNEL; ++i )
         vweights[c][i] = 0;
      
      if( mode == arp_mix || mode == arp_solo )
      {
         int ilp = (int)longphase[c];
         float flp = longphase[c] - ilp;

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
         float voices = limit_range( *f[flng_voices], 1.f, COMBS_PER_CHANNEL * 1.f );
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
         float df = limit_range( fb_lf_damping.v, 0.01f, 0.99f );
         lpaL = lpaL * ( 1.0 - df ) + fbl * df;
         fbl = fbl - lpaL;

         lpaR = lpaR * ( 1.0 - df ) + fbr * df;
         fbr = fbr - lpaR;
      }
      
      auto vl = dataL[b] - fbl;
      auto vr = dataR[b] - fbr;
      idels[0].push( vl );
      idels[1].push( vr );
      
      auto origw = 1.f;
      if( mode == doppler || mode == arp_solo )
      {
         // doppler modes
         origw = 0.f;
      }

      float outl = origw * dataL[b] + mix.v * combs[0][b];
      float outr = origw * dataR[b] + mix.v * combs[1][b];

      // Some gain heueirstics
      float gainadj = 0.0;
      switch( mode )
      {
      case classic:
         gainadj = - 1 / sqrt(7 - voices.v);
         break;
      case doppler:
         gainadj = - 1 / sqrt(8 - voices.v);
         break;
      case arp_mix:
         gainadj = -1 / sqrt(6);
         break;
      case arp_solo:
         gainadj = -1 / sqrt(7);
         break;
      }
      
      gainadj -= 0.07 * mix.v;

      outl = limit_range( (1.0f + gainadj ) * outl, -1.f, 1.f );
      outr = limit_range( (1.0f + gainadj ) * outr, -1.f, 1.f );
      
      outl = 1.5 * outl - 0.5 * outl * outl * outl;
      outr = 1.5 * outr - 0.5 * outr * outr * outr;
      
      dataL[b] = outl;
      dataR[b] = outr;
      
      depth.process();
      mix.process();
      feedback.process();
      fb_lf_damping.process();
      voices.process();
   }

   width.set_target_smoothed(db_to_linear(*f[flng_stereo_width]) / 3);

   float M alignas(16)[BLOCK_SIZE],
         S alignas(16)[BLOCK_SIZE];
   encodeMS(dataL, dataR, M, S, BLOCK_SIZE_QUAD);
   width.multiply_block(S, BLOCK_SIZE_QUAD);
   decodeMS(M, S, dataL, dataR, BLOCK_SIZE_QUAD);

}

float FlangerEffect::InterpDelay::value( float delayBy )
{
   // so if delayBy is 19.2
   int itap = (int) std::min( delayBy, (float)(DELAY_SIZE-2) ); // this is 19
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
      return 11;
   case 2:
      return 19;
   case 3:
      return 25;
   }
   return 0;
}

void FlangerEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[flng_mode].set_name( "Mode" );
   fxdata->p[flng_mode].set_type( ct_flangermode );

   fxdata->p[flng_wave].set_name( "Waveform" );
   fxdata->p[flng_wave].set_type( ct_flangerwave );

   fxdata->p[flng_rate].set_name( "Rate" );
   fxdata->p[flng_rate].set_type( ct_lforate );

   fxdata->p[flng_depth].set_name( "Depth" );
   fxdata->p[flng_depth].set_type( ct_percent );

   fxdata->p[flng_voices].set_name( "Count" );
   fxdata->p[flng_voices].set_type( ct_flangervoices );

   fxdata->p[flng_voice_zero_pitch].set_name( "Base Pitch" );
   fxdata->p[flng_voice_zero_pitch].set_type( ct_flangerpitch );

   fxdata->p[flng_voice_spacing].set_name( "Spacing" );
   fxdata->p[flng_voice_spacing].set_type( ct_flangerspacing );

   fxdata->p[flng_feedback].set_name( "Feedback" );
   fxdata->p[flng_feedback].set_type( ct_percent );

   fxdata->p[flng_fb_lf_damping].set_name( "LF Damping" );
   fxdata->p[flng_fb_lf_damping].set_type( ct_percent );

   fxdata->p[flng_stereo_width].set_name( "Width" );
   fxdata->p[flng_stereo_width].set_type( ct_decibel_narrow); 

   fxdata->p[flng_mix].set_name( "Mix" );
   fxdata->p[flng_mix].set_type( ct_percent_bidirectional );


   for( int i=flng_mode; i<flng_num_params; ++i )
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

   fxdata->p[flng_voices].val.f = 4.f;
   fxdata->p[flng_voice_zero_pitch].val.f = 60.f;
   fxdata->p[flng_voice_spacing].val.f = 0.f;
   
   fxdata->p[flng_feedback].val.f = 0.f;
   fxdata->p[flng_fb_lf_damping].val.f = 0.1f;
   
   fxdata->p[flng_stereo_width].val.f = 0.f;
   fxdata->p[flng_mix].val.f = 0.8f;

}
