/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "MSEGModulationHelper.h"
#include <cmath>
#include <iostream>
#include "DebugHelpers.h"
#include "basic_dsp.h" // for limit_range
#include "FastMath.h"

namespace Surge {
namespace MSEG {

void rebuildCache( MSEGStorage *ms )
{
   if (ms->loop_start > ms->n_activeSegments - 1)
      ms->loop_start = -1;
   if (ms->loop_end > ms->n_activeSegments - 1)
      ms->loop_end = -1;

   float totald = 0;
   for (int i = 0; i < ms->n_activeSegments; ++i)
   {
      ms->segmentStart[i] = totald;
      totald += ms->segments[i].duration;
      ms->segmentEnd[i] = totald;

      int nextseg = i + 1;

      if (nextseg >= ms->n_activeSegments)
      {
         if (ms->endpointMode == MSEGStorage::EndpointMode::LOCKED)
            ms->segments[i].nv1 = ms->segments[0].v0;
      }
      else
      {
         ms->segments[i].nv1 = ms->segments[nextseg].v0;
      }
      if (ms->segments[i].nv1 != ms->segments[i].v0)
      {
         ms->segments[i].dragcpratio = (ms->segments[i].cpv - ms->segments[i].v0) /
                                       (ms->segments[i].nv1 - ms->segments[i].v0);
      }
   }

   ms->totalDuration = totald;

   for (int i = 0; i < ms->n_activeSegments; ++i)
   {
      constrainControlPointAt(ms, i);
   }

   ms->durationToLoopEnd = ms->totalDuration;
   ms->durationLoopStartToLoopEnd = ms->totalDuration;

   if (ms->n_activeSegments > 0)
   {
      if (ms->loop_end >= 0)
         ms->durationToLoopEnd = ms->segmentEnd[ms->loop_end];
      ms->durationLoopStartToLoopEnd =
          ms->segmentEnd[(ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1)] -
          ms->segmentStart[(ms->loop_start >= 0 ? ms->loop_start : 0)];
   }
}

float valueAt(int ip, float fup, float df, MSEGStorage *ms, EvaluatorState *es, bool forceOneShot )
{
   if( ms->n_activeSegments <= 0 ) return df;

   // This still has some problems but lets try this for now
   double up = (double)ip + fup;

   // If a oneshot is done, it is done
   if( up >= ms->totalDuration && ( ms->loopMode == MSEGStorage::LoopMode::ONESHOT || forceOneShot ) )
      return ms->segments[ms->n_activeSegments - 1].nv1;


   df = limit_range( df, -1.f, 1.f );

   float timeAlongSegment = 0;

   if( es->loopState == EvaluatorState::PLAYING && es->released )
   {
      es->releaseStartPhase = up;
      es->releaseStartValue = es->lastOutput;
      es->loopState = EvaluatorState::RELEASING;
   }

   int idx = -1;
   if( es->loopState == EvaluatorState::PLAYING || ms->loopMode != MSEGStorage::LoopMode::GATED_LOOP )
   {
      idx = timeToSegment(ms, up, forceOneShot || ms->loopMode == MSEGStorage::LoopMode::ONESHOT, timeAlongSegment);
      if( idx < 0 || idx >= ms->n_activeSegments ) return 0;
   }
   else
   {
      if (ms->loop_end == -1 || ms->loop_end >= ms->n_activeSegments)
         return es->releaseStartValue;

      if( es->releaseStartPhase == up )
      {
         // We just released. We know what to do but we might be off by epsiolon so...
         idx = ms->loop_end + 1;
         timeAlongSegment = 0;
      }
      else
      {
         double adjustedPhase = up - es->releaseStartPhase + ms->segmentEnd[ms->loop_end];

         // so now find the index
         idx = -1;
         for (int ai = 0; ai < ms->n_activeSegments && idx < 0; ai++)
            if (ms->segmentStart[ai] <= adjustedPhase && ms->segmentEnd[ai] > adjustedPhase)
               idx = ai;
         if (idx < 0)
         {
            return ms->segments[ms->n_activeSegments - 1].nv1; // We are past the end
         }
         timeAlongSegment = adjustedPhase - ms->segmentStart[idx];
      }
   }

   auto r = ms->segments[idx];
   // std::cout << up << " " << idx << std::endl;
   bool segInit = false;
   if( idx != es->lastEval)
   {
      segInit = true;
      es->lastEval = idx;
   }

   if( ! ms->segments[idx].useDeform )
      df = 0;

   if (ms->segments[idx].invertDeform)
      df = -df;

   if( ms->segments[idx].duration <= MSEGStorage::minimumDuration )
      return ( ms->segments[idx].v0 + ms->segments[idx].nv1 ) * 0.5;

   float res = r.v0;

   // we use local copies of these values so we can adjust them in the gated release phase
   float lv0 = r.v0;
   float lv1 = r.nv1;
   float lcpv = r.cpv;

   // So are we in the gated release segment?
   if( es->loopState == EvaluatorState::RELEASING && ms->loopMode == MSEGStorage::GATED_LOOP && idx == ms->loop_end + 1 )
   {
      // Move the point at which we start
      lv0 = es->releaseStartValue;
      float cpratio = 0.5;
      if( r.nv1 != r.v0 )
         cpratio = ( r.cpv - r.v0 ) / ( r.nv1 - r.v0 );
      lcpv = cpratio * ( r.nv1 - lv0 ) + lv0;
   }
   switch( r.type )
   {
   case MSEGStorage::segment::HOLD:
   {
      es->lastOutput = lv0;
      return lv0;
      break;
   }
   case MSEGStorage::segment::LINEAR:
   {
      if( lv0 == lv1 ) return lv0;
      float frac = timeAlongSegment / r.duration;

      // So we want to handle control points
      auto cpd = r.cpv;

      /*
       * Alright so we have a functional form (e^ax-1)/(e^a-1) = y;
       * We also know that since we have vertical only motion here x = 1/2 and y is where we want
       * to hit ( specifically since we are generating a 0,1 line and cpv is -1,1 then
       * here we get y = 0.5 * cpv + 0.5.
       *
       * Fine so lets show our work. I'm going to use X and V for now
       *
       * (e^aX-1)/(e^a-1) = V  @ x=1/2
       * introduce Q = e^a/2
       * (Q - 1) / ( Q^2 - 1 ) = V
       * Q - 1 = V Q^2 - V
       * V Q^2 - Q + ( 1-V ) = 0
       *
       * OK cool we know how to solve that (for V != 0)
       *
       * Q = (1 +/- sqrt( 1 - 4 * V * (1-V) )) / 2 V
       *
       * and since Q = e^a/2
       *
       * a = 2 * log(Q)
       *
       */

      float V = 0.5 * r.cpv + 0.5;
      float amul = 1;
      if( V < 0.5 )
      {
         amul = -1;
         V = 1 - V;
      }
      float disc = ( 1 - 4 * V * ( 1-V) );
      float a = 0;
      if( fabs(V) > 1e-3 )
      {
         float Q = limit_range( ( 1 - sqrt( disc ) ) / ( 2 * V ), 0.00001f, 1000000.f );
         a = amul * 2 * log( Q );
      }

      // OK so frac is the 0,1 line point
      auto cpline = frac;
      if( fabs(a) > 1e-3 )
         cpline = ( exp( a * frac ) - 1 ) / ( exp( a ) - 1 );

      // This is the equivalent of LFOModulationSource.cpp::bend3
      float dfa = -0.5f * limit_range( df, -3.f, 3.f );

      float x = (2 * cpline - 1);
      x = x - dfa * x * x + dfa;
      x = x - dfa * x * x + dfa; // do twice because bend3 does it twice

      cpline = 0.5 * ( x + 1 );

      // cpline will still be 0,1 so now we need to transform it
      res = cpline * ( lv1 - lv0 ) + lv0;
      break;
   }
   case MSEGStorage::segment::BROWNIAN:
   {
      static constexpr int validx = 0, lasttime = 1, outidx = 2;
      if( segInit )
      {
         es->msegState[validx] = lv0;
         es->msegState[outidx] = lv0;
         es->msegState[lasttime] = 0;
      }
      float targetTime = timeAlongSegment /r.duration;
      if( targetTime >= 1 )
      {
         res = lv1;
      }
      else if( targetTime <= 0 )
      {
         res = lv0;
      }
      else if( targetTime <= es->msegState[lasttime] )
      {
         res = es->msegState[outidx];
      }
      else
      {
         while( es->msegState[lasttime] < targetTime && es->msegState[lasttime] < 1 )
         {
            auto ncd = r.cpduration; // 0-1
            // OK so the closer this is to 1 the more spiky we should be, which is basically
            // increasing the dt.

            // The slowest is 5 steps (max dt is 0.2)
            float sdt = 0.2f * ( 1 - ncd ) * ( 1 - ncd );
            float dt = std::min( std::max( sdt, 0.0001f ), 1 - es->msegState[lasttime] );

            if( es->msegState[lasttime] < 1 )
            {
               float lincoef  = ( lv1 - es->msegState[validx] ) / ( 1 - es->msegState[lasttime] );
               float randcoef = 0.1 * lcpv; // CPV is on -1,1
               /*
               * Modification to standard bridge. We want to move away from 1,-1 so lets
               * explicitly bounce away from them if random will push us over
               */
               auto uniformRand = ( ( 2.f * rand() ) / (float)(RAND_MAX) - 1 );
               auto linstp = es->msegState[validx] + lincoef * dt;
               float randup = randcoef;
               float randdn = randcoef;
               if( linstp + randup > 1 ) randup = 1 - linstp;
               else if( linstp - randdn < -1 ) randdn = linstp + 1;
               auto randadj = ( uniformRand > 0 ? randup * uniformRand : randdn * uniformRand );

               es->msegState[validx] += limit_range( lincoef * dt + randadj, -1.f, 1.f );
               es->msegState[lasttime] += dt;
            }
            else
               es->msegState[validx] = lv1;
         }
         if( lcpv < 0 )
         {
            // Snap to between 1 and 24 values.
            int a = floor( - lcpv * 24 ) + 1;
            es->msegState[outidx] = floor( es->msegState[validx] * a ) * 1.f / a;
         }
         else
         {
            es->msegState[outidx] = es->msegState[validx];
         }
         es->msegState[outidx] = limit_range(es->msegState[outidx],-1.f, 1.f );
         // so basically softclip it
         res  = es->msegState[outidx];
      }
      
      break;
   }
   case MSEGStorage::segment::QUAD_BEZIER:
   {
      /*
      ** First let's correct the control point so that the
      ** user specified one is through the curve. This means
      ** that the line from the center of the segment connecting
      ** the endpoints to the control point pushes out 2x
      */

      float cpv = lcpv;
      float cpt = r.cpduration * r.duration;

      // If we are positioned exactly at the midpoint our calculation below to find time will fail
      // so walk off a smidge
      if( fabs( cpt - r.duration * 0.5 ) < 1e-5 )
         cpt += 1e-4;
      
      // here's the midpoint along the connecting curve
      float tp = r.duration/2;
      float vp = (lv1-lv0)/2 + lv0;

      // The distance from tp,vp to cpt,cpv
      float dt = (cpt-tp), dy = (cpv-vp);

      cpv = vp + 2 * dy; 
      cpt = tp + 2 * dt;
      
      // B = (1-t)^2 P0 + 2 t (1-t) P1 + t^2 P2
      float ttarget = timeAlongSegment;
      float px0 = 0, px1 = cpt, px2 = r.duration,
         py0 = lv0, py1 = cpv, py2 = lv1;



      /*
      ** OK so we want to find the bezier t corresponding to phase ttarget
      **
      ** (1-t)^2 px0 + 2 * (1-t) t px1 + t * t px2 = ttarget;
      **
      ** Luckily we know px0 = 0. So
      **
      ** 2 * ( 1-t ) * t * px1 + t * t * px2 - ttarget = 0;
      ** 2 * t * px1 - 2 * t^2 * px1 + t^2 * px2 - ttarget = 0;
      ** (px2 - 2 * px1) t^2 + 2 * px1 * t - ttarget = 0;
      **
      */
      float a = px2 - 2 * px1;
      float b = 2 * px1;
      float c = -ttarget;
      float disc = b * b - 4 * a * c;

      if( a == 0 || disc < 0 )
      {
         // This means we have a line between v0 and nv1
         float frac = timeAlongSegment / r.duration;
         res = frac * lv1 + ( 1 - frac ) * lv0;
      }
      else
      {
         float t = (-b + sqrt( b*b-4*a*c))/(2*a);
         
         if( df < 0 )
            t = pow( t, 1.0 + df * 0.7 );
         if( df > 0 )
            t = pow( t, 1.0 + df * 3 );

         /*
         ** And now evaluate the bezier in y with that t
         */
         res = (1-t)*(1-t)*py0 + 2 * ( 1-t ) * t * py1 + t * t * py2;
      }

      break;
   }

   case MSEGStorage::segment::SCURVE:
   {
      float x = (timeAlongSegment / r.duration - 0.5 ) * 2; // goes from -1 to 1

      float eps = 0.01;
      float mv = lv0; //std::min( lv0, lv1 );
      float xv = lv1; // std::max( lv0, lv1 );
      if( fabs(xv - mv) < eps )
      {
         res = 0.5 * ( mv + xv );
         break;
      }

      if( df != 0 )
      {
         // Shift the center, retain the endpoints
         float udf = df * 8;
         x = ( x + 1 ) * 0.5;
         x = (exp(udf * x) - 1) / (exp(udf) - 1);
         x = ( x - 0.5 ) * 2;
      }

      float cx = r.cpv;
      
      /*
      ** OK so now we have either x = 1/(1+exp(-kx)) if k > 0 or the mirror of that around the line
      ** but we always want it to hit -1,1 (and then scale later)
      */
      float kmax = 20;
      float k = cx * kmax;
      if( k == 0 ) 
      {
         float frac = timeAlongSegment / r.duration;
         res = frac * lv1 + ( 1 - frac ) * lv0;
         break;
      }

      if( k < 0 )
      {
         // Logit function on 0,1. So the problem here is that k is 'too slow' since
         // logit scales differently. So since cx is in -1,0 try this
         kmax = 20;
         k = -pow( -r.cpv, 0.2 ) * kmax ;
         x = ( x + 1 ) * 0.5; // 0,1

         float scale = 0.99999 - 0.5 * ( (kmax + k) / kmax );
         float off = ( 1 - scale ) * 0.5;
         x = x * scale + off;

         float upv = -log( 1/(scale+off) - 1 );
         float dnv = -log( 1 / off - 1 );
         float val = -log( 1/x - 1 );
         val = ( val - dnv ) / ( upv - dnv );
         val = ( xv - mv ) * val + mv;
         res = val;
      }
      else
      {
         // sigmoid
         float xp = 1/(1+exp(-k));
         float xm = 1/(1+exp(k));
         float distance = xp - xm;
      

         float val = 1 / ( 1 + exp( -k * x ) ); // between xm and xp
      
         val = ( val - xm ) / ( xp - xm ); // between 0 and 1
         
         val = ( xv - mv ) * val + mv;
         res = val;
      }
      
      break;
   }

   case MSEGStorage::segment::SINE: 
   case MSEGStorage::segment::SAWTOOTH: 
   case MSEGStorage::segment::TRIANGLE: 
   case MSEGStorage::segment::SQUARE: {
      float pct = ( r.cpv + 1 ) * 0.5;
      float as = 5.0;
      float scaledpct = ( exp( as * pct ) - 1 ) / (exp(as)-1);
      int steps = (int)( scaledpct * 100 );
      auto f = timeAlongSegment /r.duration;

      float a = 1;
      
      if( df < 0 )
         a = 1 + df * 0.5;
      if( df > 0 )
         a = 1 + df * 1.5;

      float kernel = 0;

      /*
       * At this point you need to generate a waveform which starts at 1,
       * ends at -1, and has 'steps' oscillations in between, spanning
       * the phase space f=[0,1].
       */
      if( r.type == MSEGStorage::segment::SINE )
      {
         /*
          * To meet the -1, 1 constraint on the endpoints we use cos, not sine, so
          * the first point is one, and multiply the frequency by 3pi, 5pi, 7pi etc...
          * so we get one and a half waveforms
          */
         float mul = ( 1 + 2 * steps ) * M_PI;
         kernel = cos( mul * f );
      }
      if( r.type == MSEGStorage::segment::SAWTOOTH )
      {
         double mul = steps + 1;
         double phase = mul * f;
         double dphase = phase - (int)phase;
         kernel = 1 - 2 * dphase;
      }
      if( r.type == MSEGStorage::segment::TRIANGLE )
      {
         double mul = ( 0.5 + steps );
         double phase = mul * f;
         double dphase = phase - (int)phase;
         if( dphase < 0.5 )
         {
            // line 1 @ 0 to -1 @ 1/2
            kernel = 1 - 4 * dphase;
         }
         else
         {
            auto qphase = dphase - 0.5;
            // line -1 @ 0 to 1 @ 1/2
            kernel = 4 * qphase - 1;
         }
      }
      if( r.type == MSEGStorage::segment::SQUARE )
      {
         /*
          * Deform does PWM here
          */
         float mul = steps + 1;
         float tphase = mul * f;
         float phase = tphase - (int)tphase;
         float pw = ( df + 1 ) / 2.0; // so now 0 -> 1
         kernel = ( phase < pw ? 1 : -1 );
      }
      
      res = (lv0 - lv1) * pow((kernel + 1) * 0.5, a) + lv1;
      break;
   }

   case MSEGStorage::segment::STEPS: {
      float pct = ( r.cpv + 1 ) * 0.5;
      float as = 5.0;
      float scaledpct = ( exp( as * pct ) - 1 ) / (exp(as)-1);
      int steps = (int)( scaledpct * 100 ) + 2;

      float frac = (float)( (int)( steps * timeAlongSegment / r.duration ) ) / (steps-1);
      if( df < 0 )
         frac = pow( frac, 1.0 + df * 0.7 );
      if( df > 0 )
         frac = pow( frac, 1.0 + df * 3 );
      res = frac * lv1 + ( 1 - frac ) * lv0;
      break;
   }

   
   }
   //std::cout << _D(timeAlongSegment) << _D(r.type) << _D(r.duration) << _D(lv0) << std::endl;

   es->lastOutput = res;
   return res;
}

int timeToSegment( MSEGStorage *ms, double t )
{
   float x;
   return timeToSegment(ms, t, true, x);
}

int timeToSegment( MSEGStorage *ms, double t, bool ignoreLoops, float &amountAlongSegment )
{
   if( ms->totalDuration < MSEGStorage::minimumDuration ) return -1;

   if( ignoreLoops )
   {
      if (t >= ms->totalDuration)
      {
         double nup = t / ms->totalDuration;
         t -= (int)nup * ms->totalDuration;
         if (t < 0)
            t += ms->totalDuration;
      }

      int idx = -1;
      for (int i = 0; i < ms->n_activeSegments; ++i)
      {
         if (t >= ms->segmentStart[i] && t < ms->segmentEnd[i])
         {
            idx = i;
            amountAlongSegment = t - ms->segmentStart[i];
            break;
         }
      }

      return idx;
   }
   else
   {
      // The loop case is more tedious
      int le = (ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1);
      int ls = (ms->loop_start >= 0 ? ms->loop_start : 0 );
      // So are we before the first loop end point
      if( t <= ms->durationToLoopEnd )
      {
         for( int i=0; i<ms->n_activeSegments; ++i )
            if( t >= ms->segmentStart[i] && t <= ms->segmentEnd[i])
            {
               amountAlongSegment = t - ms->segmentStart[i];
               return i;
            }
      }
      else if( ms->loop_start > ms->loop_end && ms->loop_start >= 0 && ms->loop_end >= 0 )
      {
         // This basically means we just iterate around the loop_end endpoint which we do by saying
         // we are basically at the end point all the way along
         auto idx = ms->loop_end;
         amountAlongSegment = ms->segments[idx].duration;
         return idx;
      }
      else
      {
         double nt = t - ms->durationToLoopEnd;
         // OK so now we have a cycle which has length durationFromStartToEnd;
         double nup = nt / ms->durationLoopStartToLoopEnd;
         nt -= (int)nup * ms->durationLoopStartToLoopEnd;
         if (nt < 0)
            nt += ms->durationLoopStartToLoopEnd;

         // and we need to offset it by the starting point
         nt += ms->segmentStart[ls];

         for( int i=0; i<ms->n_activeSegments; ++i )
            if( nt >= ms->segmentStart[i] && nt <= ms->segmentEnd[i])
            {
               amountAlongSegment = nt - ms->segmentStart[i];
               return i;
            }
      }

      return 0;
   }
}

void changeTypeAt( MSEGStorage *ms, float t, MSEGStorage::segment::Type type ) {
   auto idx = timeToSegment( ms, t );
   if( idx < ms->n_activeSegments )
   {
      ms->segments[idx].type = type;
   }
}

void insertAtIndex( MSEGStorage *ms, int insertIndex ) {
   for( int i=std::max( ms->n_activeSegments + 1, max_msegs - 1 ); i > insertIndex; --i )
   {
      ms->segments[i] = ms->segments[i-1];
   }
   ms->segments[insertIndex].type = MSEGStorage::segment::LINEAR;
   ms->segments[insertIndex].v0 = 0;
   ms->segments[insertIndex].duration = 0.25;
   ms->segments[insertIndex].useDeform = true;
   ms->segments[insertIndex].invertDeform = false;

   int nxt = insertIndex + 1;
   if( nxt >= ms->n_activeSegments )
      nxt = 0;
   if( nxt == insertIndex )
   {
      ms->segments[insertIndex].cpv = ms->segments[nxt].v0 * 0.5;
      ms->segments[insertIndex].cpduration = 0.125;
   }
   else
   {
      ms->segments[insertIndex].cpv = ms->segments[nxt].v0 * 0.5;
      ms->segments[insertIndex].cpduration = 0.125;
   }

   /*
    * Handle the loops. We have just inserted at index so if start or end
    * is later we need to push them out
    */
   if( ms->loop_start >= insertIndex ) ms->loop_start ++;
   if( ms->loop_end >= insertIndex-1 ) ms->loop_end ++;

   ms->n_activeSegments ++;
}
   

void insertAfter( MSEGStorage *ms, float t ) {
   auto idx = timeToSegment( ms, t );
   if( idx < 0 ) idx = 0;
   idx++;
   insertAtIndex( ms, idx );
}

void insertBefore( MSEGStorage *ms, float t ) {
   auto idx = timeToSegment( ms, t );
   if( idx < 0 ) idx = 0;
   insertAtIndex( ms, idx );
}

void extendTo( MSEGStorage *ms, float t, float nv ) {
   if( t < ms->totalDuration ) return;
   nv = limit_range( nv, -1.f, 1.f );

   // This will extend the loop end if it is on the last point; but we want that
   insertAtIndex( ms, ms->n_activeSegments );

   auto sn = ms->n_activeSegments - 1;
   ms->segments[sn].type = MSEGStorage::segment::LINEAR;
   if( sn == 0 )
      ms->segments[sn].v0 = 0;
   else
      ms->segments[sn].v0 = ms->segments[sn-1].nv1;

   float dt = t - ms->totalDuration;
   ms->segments[sn].duration = dt;

   ms->segments[sn].cpduration = 0.5;
   ms->segments[sn].cpv = 0;
   ms->segments[sn].nv1 = nv;

   if( ms->endpointMode == MSEGStorage::EndpointMode::LOCKED )
   {
      // The first point has to match where I just clicked. Adjust it and its control point
      float cpdratio = 0.5;
      if( ms->segments[0].duration > 0 )
         cpdratio = ms->segments[0].cpduration / ms->segments[0].duration;
      float cpvratio = 0.5;
      if (ms->segments[0].nv1 != ms->segments[0].v0)
         cpvratio = (ms->segments[0].cpv - ms->segments[0].v0) /
                    (ms->segments[0].nv1 - ms->segments[0].v0);

      ms->segments[0].v0 = nv;

      ms->segments[0].cpduration = cpdratio * ms->segments[0].duration;
      ms->segments[0].cpv =
          (ms->segments[0].nv1 - ms->segments[0].v0) * cpvratio + ms->segments[0].v0;
   }
}

void splitSegment( MSEGStorage *ms, float t, float nv ) {
   int idx = timeToSegment( ms, t );
   nv = limit_range( nv, -1.f, 1.f );
   if( idx >= 0 )
   {
      while( t > ms->totalDuration ) t -= ms->totalDuration;
      while( t < 0 ) t += ms->totalDuration;

      float pv1 = ms->segments[idx].nv1;

      float dt = (t - ms->segmentStart[idx]) / ( ms->segments[idx].duration);
      auto q = ms->segments[idx]; // take a copy
      insertAtIndex(ms, idx + 1);

      ms->segments[idx].nv1 = nv;
      ms->segments[idx].duration *= dt;

      ms->segments[idx+1].v0 = nv;
      ms->segments[idx+1].type = ms->segments[idx].type;
      ms->segments[idx+1].nv1 = pv1;
      ms->segments[idx+1].duration = q.duration * ( 1 - dt );
      ms->segments[idx+1].useDeform = ms->segments[idx].useDeform;
      ms->segments[idx+1].invertDeform = ms->segments[idx].invertDeform;

      // Now these are normalized this is easy
      ms->segments[idx].cpduration = q.cpduration;
      ms->segments[idx].cpv = q.cpv;
      ms->segments[idx+1].cpduration = q.cpduration;
      ms->segments[idx+1].cpv = q.cpv;
   }
}


void unsplitSegment( MSEGStorage *ms, float t ) {
   // Can't unsplit a single segment
   if( ms->n_activeSegments - 1 == 0 ) return;

   int idx = timeToSegment( ms, t );
   int prior = idx;;
   if( ( ms->segmentEnd[idx] - t < t - ms->segmentStart[idx] ) || t >= ms->totalDuration )
   {
      if( idx == ms->n_activeSegments - 1 )
      {
         // We are just deleting the last segment
         deleteSegment( ms, t );
         return;
      }
      idx++;
      prior = idx - 1;
   }
   else
   {
      prior = idx - 1;
   }
   if( prior < 0 ) prior = ms->n_activeSegments - 1;
   if( prior == idx ) return;
   
   
   float cpdratio = ms->segments[prior].cpduration / ms->segments[prior].duration;
   
   float cpvratio = 0.5;
   if( ms->segments[prior].nv1 != ms->segments[prior].v0 )
      cpvratio = ( ms->segments[prior].cpv - ms->segments[prior].v0 ) / ( ms->segments[prior].nv1 - ms->segments[prior].v0 );
   
   ms->segments[prior].duration += ms->segments[idx].duration;
   ms->segments[prior].nv1 = ms->segments[idx].nv1;

   ms->segments[prior].cpduration = cpdratio * ms->segments[prior].duration;
   ms->segments[prior].cpv = (ms->segments[prior].nv1 - ms->segments[prior].v0 ) * cpvratio + ms->segments[prior].v0;

   
   for( int i=idx; i<ms->n_activeSegments - 1; ++i )
   {
      ms->segments[i] = ms->segments[i+1];
   }
   ms->n_activeSegments --;

   if( ms->loop_start > idx ) ms->loop_start --;
   if( ms->loop_end >= idx ) ms->loop_end --;
}
   
void deleteSegment( MSEGStorage *ms, float t ) {
   // Can't delete the last segment
   if( ms->n_activeSegments <= 1 ) return;

   auto idx = timeToSegment( ms, t );
   
   for( int i=idx; i<ms->n_activeSegments - 1; ++i )
   {
      ms->segments[i] = ms->segments[i+1];
   }
   ms->n_activeSegments --;

   if( ms->loop_start > idx ) ms->loop_start --;
   if( ms->loop_end >= idx ) ms->loop_end --;
}

void resetControlPoint( MSEGStorage *ms, float t )
{
   auto idx = timeToSegment( ms, t );
   if( idx >= 0 && idx < ms->n_activeSegments )
   {
      resetControlPoint( ms, idx );
   }
}

void resetControlPoint( MSEGStorage *ms, int idx )
{
   ms->segments[idx].cpduration = 0.5;
   ms->segments[idx].cpv = 0.0;
   if( ms->segments[idx].type == MSEGStorage::segment::QUAD_BEZIER )
      ms->segments[idx].cpv = 0.5 * ( ms->segments[idx].v0 + ms->segments[idx].nv1 );
}

void constrainControlPointAt( MSEGStorage *ms, int idx )
{
   // With the new model this is way easier
   ms->segments[idx].cpduration = limit_range( ms->segments[idx].cpduration, 0.f, 1.f );
   ms->segments[idx].cpv = limit_range( ms->segments[idx].cpv, -1.f, 1.f );

   switch( ms->segments[idx].type )
   {
   }
}
   
}
}
