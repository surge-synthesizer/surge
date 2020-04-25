#include "effect_defs.h"
#include "Tunings.h"

#define _D(x) " " << (#x) << "=" << x

enum flangparam
{
   // Basic Control
   flng_rate = 0, // How quickly the oscillations happen
   flng_depth, // How extreme the modulation of the delay is
   flng_mode, // flange, phase-inverse-flange, arepeggio, vibrato
   flng_wave, // what's the wave shape
   
   // Voices
   flng_voices, // how many delay lines
   
   flng_voice_zero_pitch, // tune the first max delay line to this (M = sr / f)
   flng_voice_detune, // spread in cents among the delay lines   
   flng_voice_chord, // tuning for the voices as a chord in tuning space

   // Feedback and EQ
   flng_feedback, // how much the output feeds back into the filters
   flng_fb_lf_damping, // how much low pass damping in the feedback mechanism
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
         lfosandhtarget[c][i] = 0.0;
      }
   longphase = 0;
   
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
   
   const float oneoverFreq0 = 1.0f / Tunings::MIDI_0_FREQ;

   int mode = *pdata_ival[flng_mode];
   int mwave = *pdata_ival[flng_wave];
   
   /*
   ** flanger tuning. Ahh a complicated topic.
   **
   ** Our modes map as follows
   **
   ** - Classic. Mix the flange tone; tune the comb filter frequency
   ** - Classic Tuned. Mix the flange tone; Tune the comb filter velocity
   ** - Doppler. Don't mix
   ** - Doppler tuned. Don't mix; tune felocity
   ** - Arp Tuned Mix - tune velocity, rotate voices, mix
   ** - Arp Tuned Bare - tune velocity, rotate voices, don't mix
   ** 
   ** in classic and arpeggio the comb filter delay matches the frequency of the voice
   ** 
   ** Now we know the doppler equation is:
   **
   **   f_observed = f * ( 1 + dv / c )
   **
   ** We also know our dv/c is == max of rate, but the max of rate is the rate since we use
   ** sin or slope 1 triangle as our oscillators. So f_observed = f * ( 1 + dv/c ). Great.
   ** So our frequency shift f_observed - f = f * ( 1 + r ) - f = fr.
   **
   ** So since our delay is tuned to inverse of frequency, in tuned mode, we also want
   ** to add the inverse of the rate to it.
   **
   ** and that should mean we end up tuned. We have to special case for r=0 of course.
   */

   int tuneToDelay = true;
   if( mode == classic_tuned || mode == doppler_tuned || mode == arp_tuned || mode == arp_tuned_bare )
      tuneToDelay = false;
   
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
         if( mode == arp_tuned_bare || mode == arp_tuned )
         {
            // arpeggio - everyone needs to use the same phase with the voice swap
            thisphase = longphase - (int)longphase;
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
         case saww: // saw - but we gotta be gentler than a pure saw. So FIXME on this waveform
            lfoout = thisphase * 2.0f - 1.f;
            lfoval[c][i].newValue(lfoout);
            break;
         case sandhw: // S&H random noise. Needs smoothing over the jump like the triangle
         {
            if( lforeset )
            {
               lfosandhtarget[c][i] = 1.f * rand() / RAND_MAX  - 1.f;
            }
            // FIXME - exponential creep up. We want to get there in a time related to our rate
            auto cv = lfoval[c][i].v;
            auto diff = (lfosandhtarget[c][i] - cv) * rate * 2;
            lfoval[c][i].newValue(cv + diff);
         }
         break;
         }


         auto ichord = *pdata_ival[flng_voice_chord];
         float pitch = v0;
         switch( ichord )
         {
         case 0: // unisons
            pitch = v0;
            break;
         case 1: // ocraves
            pitch = v0 + i * storage->currentScale.count;
            break;
         case 2: // m2nds up to 5ths
         case 3:
         case 4:
         case 5:
         case 6:
         case 7:
         case 8:
         {
            auto step = ichord - 1;
            pitch = v0 + step * i;
         }
            break;
         case 9:
            pitch = v0 + (i == 0 ? 0 : ( i == 1 ? 4 : ( i == 2 ?  7 : 12 ) ) );
            break;
         case 10:
            pitch = v0 + ( i == 0 ? 0 : ( i == 1 ? 3 : ( i == 2 ?  7 : 12 ) ) );
            break;
         case 11:
            pitch = v0 + ( i == 0 ? 0 : ( i == 1 ? 4 : ( i == 2 ?  7 : 10 ) ) );
            break;
         case 12:
            pitch = v0 + ( i == 0 ? 0 : ( i == 1 ? 4 : ( i == 2 ?  7 : 11 ) ) );
            break;
         default:
            pitch = v0;
            break;
         };
         pitch += *f[flng_voice_detune] * i;

         // Tuning goes here
         if( tuneToDelay )
         {
            delaybase[c][i].newValue( samplerate * oneoverFreq0 * storage->note_to_pitch_inv((float)(pitch)) );
         }
         else
         {

            auto pratio = storage->note_to_pitch(pitch) / storage->note_to_pitch( v0 );
            float d0 = samplerate * oneoverFreq0 * storage->note_to_pitch_inv((float)(v0));
            float r = std::max( rate * samplerate / BLOCK_SIZE, 0.05f );

            switch( mwave )
            {
            case sinw:
               r *= 2.0 * M_PI;
               break;
            case triw:
               r *= 2.0;
               break;
            default:
               break;
            }
            delaybase[c][i].newValue( d0 * pratio / r );
         }
      }
   
   depth.newValue( *f[flng_depth] );
   mix.newValue( *f[flng_mix] );
   float feedbackScale = 0.4;
   if( mode == doppler_tuned || mode == doppler )
   {
      feedbackScale = 0.4;
   }
   if( mode == arp_tuned || mode == arp_tuned_bare )
   {
      feedbackScale = 0.9;
   }
   float fbv = *f[flng_feedback];
   if( fbv > 0 )
      ringout_value = samplerate * 32.0;
   else
      ringout_value = 1024;
   
   fbv = fbv * fbv * fbv;
   feedback.newValue( feedbackScale * fbv ); 
   fb_lf_damping.newValue( 0.4 * *f[flng_fb_lf_damping ] );
   float combs alignas(16)[2][BLOCK_SIZE];

   // Obviously when we implement stereo spread this will be different
   float vweights[2][COMBS_PER_CHANNEL];
   for( int c=0; c<2; ++c )
   {
      for( int i=0; i<COMBS_PER_CHANNEL; ++i )
         vweights[c][i] = 0;
      
      if( mode == arp_tuned_bare || mode == arp_tuned )
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
      if( mode == doppler || mode == doppler_tuned || mode == arp_tuned_bare )
      {
         // doppler modes
         origw = 0.f;
      }

      float outl = origw * dataL[b] + mix.v * combs[0][b];
      float outr = origw * dataR[b] + mix.v * combs[1][b];

      // Some gain heueirstics
      float gainadj = 0.0;
      if( mode == classic || mode == classic_tuned )
         gainadj = - 1 / sqrt(6 - voices.v);
      if( mode == doppler || mode == doppler_tuned )
         gainadj = - 1 / sqrt(8 - voices.v);
      gainadj -= 0.07 * mix.v;

      if( *f[flng_stereo_width] > 8 )
         gainadj -= (*f[flng_stereo_width] - 8 ) / 36.f;
      
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
      return 11;
   case 2:
      return 21;
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

   fxdata->p[flng_wave].set_name( "Waveform" );
   fxdata->p[flng_wave].set_type( ct_flangerwave );

   fxdata->p[flng_voices].set_name( "Count" );
   fxdata->p[flng_voices].set_type( ct_flangervoices );

   fxdata->p[flng_voice_zero_pitch].set_name( "Base Pitch" );
   fxdata->p[flng_voice_zero_pitch].set_type( ct_flangerpitch );

   fxdata->p[flng_voice_detune].set_name( "Detune" );
   fxdata->p[flng_voice_detune].set_type( ct_percent_bidirectional );

   fxdata->p[flng_voice_chord].set_name( "Chord" );
   fxdata->p[flng_voice_chord].set_type( ct_flangerchord );

   fxdata->p[flng_feedback].set_name( "Feedback" );
   fxdata->p[flng_feedback].set_type( ct_percent );

   fxdata->p[flng_fb_lf_damping].set_name( "LF Damp" );
   fxdata->p[flng_fb_lf_damping].set_type( ct_percent );

   fxdata->p[flng_stereo_width].set_name( "Width" );
   fxdata->p[flng_stereo_width].set_type( ct_decibel_narrow); 

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
   fxdata->p[flng_voice_detune].val.f = 0;
   fxdata->p[flng_voice_chord].val.i = 0;
   
   //flng_feedback, // how much the output feeds back into the filters
   fxdata->p[flng_feedback].val.f = 0.f;
   fxdata->p[flng_fb_lf_damping].val.f = 0.1f;
   fxdata->p[flng_stereo_width].val.f = 1.0f;

}
