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

namespace Surge {
namespace MSEG {

void rebuildCache( MSEGStorage *ms )
{
   float totald = 0;
   for( int i=0; i<ms->n_activeSegments; ++i )
   {
      ms->segmentStart[i] = totald;
      totald += ms->segments[i].duration;
      ms->segmentEnd[i] = totald;

      int nextseg = i+1;

      if( nextseg >= ms->n_activeSegments )
      {
         if( ms->endpointMode == MSEGStorage::EndpointMode::LOCKED )
            ms->segments[i].nv1 = ms->segments[0].v0;
      }
      else
      {
         ms->segments[i].nv1 = ms->segments[nextseg].v0;
      }
   }

   ms->totalDuration = totald;

   for( int i=0; i<ms->n_activeSegments; ++i )
   {
      constrainControlPointAt( ms, i );
   }
}

float valueAt(int ip, float fup, float df, MSEGStorage *ms, int &lastSegmentEvaluated, float msegState[5], bool isReleased)
{
   if( ms->n_activeSegments == 0 ) return df;

   // This still has some problems but lets try this for now
   double up = (double)ip + fup;

   // If a oneshot is done, it is done
   if( up >= ms->totalDuration && ms->loopMode == MSEGStorage::LoopMode::ONESHOT )
      return ms->segments[ms->n_activeSegments - 1].nv1;

   if( up >= ms->totalDuration ) {
      double nup = up / ms->totalDuration;
      up -= (int)nup * ms->totalDuration;
      if( up < 0 )
         up += ms->totalDuration;
   }

   df = limit_range( df, -1.f, 1.f );
   
   float cd = 0;
   MSEGStorage::segment r;
   int idx = -1;
   for( int i=0; i<ms->n_activeSegments; ++i ) 
   {
      if( up >= ms->segmentStart[i] && up < ms->segmentEnd[i] )
      {
         r = ms->segments[i];
         idx = i;
         break;
      }
   }

   if( idx < 0 ) return 0;

   bool segInit = false;
   if( idx != lastSegmentEvaluated )
   {
      segInit = true;
      lastSegmentEvaluated = idx;
   }

   if( ! ms->segments[idx].useDeform )
      df = 0;

   
   float pd = up - ms->segmentStart[idx];


   if( ms->segments[idx].duration <= MSEGStorage::minimumDuration )
      return ( ms->segments[idx].v0 + ms->segments[idx].nv1 ) * 0.5;

   float res = r.v0;
   switch( r.type )
   {
   case MSEGStorage::segment::LINEAR:
   {
      float frac = pd / r.duration;

      if( df < 0 )
         frac = pow( frac, 1.0 + df * 0.7 );

      if( df > 0 )
         frac = pow( frac, 1.0 + df * 3 );

      res = frac * r.nv1 + ( 1 - frac ) * r.v0;

      break;
   }
   case MSEGStorage::segment::BROWNIAN:
   {
      static constexpr int validx = 0, lasttime = 1, outidx = 2;
      if( segInit )
      {
         msegState[validx] = r.v0;
         msegState[outidx] = r.v0;
         msegState[lasttime] = 0;
      }
      float targetTime = pd/r.duration;
      if( targetTime >= 1 )
      {
         res = r.nv1;
      }
      else if( targetTime <= 0 )
      {
         res = r.v0;
      }
      else if( targetTime <= msegState[lasttime] )
      {
         res = msegState[outidx];
      }
      else
      {
         while( msegState[lasttime] < targetTime && msegState[lasttime] < 1 )
         {
            auto ncd = r.cpduration / r.duration; // 0-1
            // OK so the closer this is to 1 the more spiky we should be, which is basically
            // increasing the dt.

            // The slowest is 5 steps (max dt is 0.2)
            float sdt = 0.2f * ( 1 - ncd ) * ( 1 - ncd );
            float dt = std::min( std::max( sdt, 0.0001f ), 1 - msegState[lasttime] );

            if( msegState[lasttime] < 1 )
            {
               float lincoef  = ( r.nv1 - msegState[validx] ) / ( 1 - msegState[lasttime] );
               float randcoef = 0.1 * r.cpv; // CPV is on -1,1
               
               msegState[validx] += lincoef * dt + randcoef * ( ( 2.f * rand() ) / (float)(RAND_MAX) - 1 );
               msegState[lasttime] += dt;
            }
            else
               msegState[validx] = r.nv1;
         }
         if( r.cpv < 0 )
         {
            // Snap to between 1 and 24 values.
            int a = floor( - r.cpv * 24 ) + 1;
            msegState[outidx] = floor( msegState[validx] * a ) * 1.f / a;
         }
         else
         {
            msegState[outidx] = msegState[validx];
         }
         msegState[outidx] = limit_range(msegState[outidx],-1.f, 1.f );
         res = msegState[outidx];
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

      float cpv = r.cpv;
      float cpt = r.cpduration;

      // If we are positioned exactly at the midpoint our calculation below to find time will fail
      // so walk off a smidge
      if( fabs( cpt - r.duration * 0.5 ) < 1e-5 )
         cpt += 1e-4;
      
      // here's the midpoint along the connecting curve
      float tp = r.duration/2;
      float vp = (r.nv1-r.v0)/2 + r.v0;

      // The distance from tp,vp to cpt,cpv
      float dt = (cpt-tp), dy = (cpv-vp);

      cpv = vp + 2 * dy; 
      cpt = tp + 2 * dt;
      
      // B = (1-t)^2 P0 + 2 t (1-t) P1 + t^2 P2
      float ttarget = pd;
      float px0 = 0, px1 = cpt, px2 = r.duration,
         py0 = r.v0, py1 = cpv, py2 = r.nv1;



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
         float frac = pd / r.duration;
         res = frac * r.nv1 + ( 1 - frac ) * r.v0;
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
      float x = ( pd / r.duration - 0.5 ) * 2; // goes from -1 to 1

      float eps = 0.01;
      float mv = r.v0; //std::min( r.v0, r.nv1 );
      float xv = r.nv1; // std::max( r.v0, r.nv1 );
      if( fabs(xv - mv) < eps )
      {
         res = 0.5 * ( mv + xv );
         break;
      }

      float cx = ( r.cpduration / r.duration - 0.5 ) * 2; // goes from -1 to 1
      
      /*
      ** OK so now we have either x = 1/(1+exp(-kx)) if k > 0 or the mirror of that around the line
      ** but we always want it to hit -1,1 (and then scale later)
      */
      float kmax = 20;
      float k = cx * kmax;
      if( k == 0 ) 
      {
         float frac = pd / r.duration;
         res = frac * r.nv1 + ( 1 - frac ) * r.v0;
         break;
      }

      if( k < 0 )
      {
         // Logit function on 0,1
         x = ( x + 1 ) * 0.5; // 0,1

         float scale = 0.9999 - 0.5 * ( (kmax + k) / kmax );
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
   case MSEGStorage::segment::TRIANGLE: 
   case MSEGStorage::segment::SQUARE: {
      float pct = r.cpduration / r.duration;
      /*
       * We want an exponential map roughly like
       *  0 -> 0
       * .5 -> .05
       *  1 -> 1
       *
       *  screwing around in desmos that gives us
       *  (e^(ax^n)-1)/(e^a-1)
       *  with a = 5 and n= 1
       */
      float as = 5.0;
      float scaledpct = ( exp( as * pct ) - 1 ) / (exp(as)-1);
      int steps = (int)( scaledpct * 100 );
      auto f = pd/r.duration;


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
          * A square wave naturally starts at 1 and ends at -1, so we don't need
          * the odd oscillations. Here we multiply the frequency by 2*steps and then
          * use even/odd for +/-
          */
         float mul = ( 2 * steps ) + 1;
         int ifm = (int)( mul * f );
         kernel = ( ifm % 2 == 0 ? 1 : -1 );
      }
      
      res = (r.v0 - r.nv1) * pow((kernel + 1) * 0.5, a) + r.nv1;
      break;
   }

   case MSEGStorage::segment::STEPS: {
      int steps = (int)( r.cpduration / r.duration * 100 ) + 2;
      float frac = (float)( (int)( steps * pd / r.duration ) ) / (steps-1);
      if( df < 0 )
         frac = pow( frac, 1.0 + df * 0.7 );
      if( df > 0 )
         frac = pow( frac, 1.0 + df * 3 );
      res = frac * r.nv1 + ( 1 - frac ) * r.v0;
      break;
   }

   
   }
   //std::cout << _D(pd) << _D(r.type) << _D(r.duration) << _D(r.v0) << std::endl;

   
   return res;
}


int timeToSegment( MSEGStorage *ms, float t ) {
   if( ms->totalDuration < MSEGStorage::minimumDuration ) return -1;
   
   while( t > ms->totalDuration ) t -= ms->totalDuration;
   while( t < 0 ) t += ms->totalDuration;
   
   int idx = -1;
   for( int i=0; i<ms->n_activeSegments; ++i ) 
   {
      if( t >= ms->segmentStart[i] && t < ms->segmentEnd[i] )
      {
         idx = i;
         break;
      }
   }
   return idx;
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
   
   insertAtIndex( ms, ms->n_activeSegments );

   auto sn = ms->n_activeSegments - 1;
   ms->segments[sn].type = MSEGStorage::segment::LINEAR;
   if( sn == 0 )
      ms->segments[sn].v0 = 0;
   else
      ms->segments[sn].v0 = ms->segments[sn-1].nv1;

   float dt = t - ms->totalDuration;
   ms->segments[sn].duration = dt;

   ms->segments[sn].cpduration = dt/2;
   ms->segments[sn].cpv = ( ms->segments[sn].v0 + nv ) * 0.5;
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
   if( idx >= 0 )
   {
      while( t > ms->totalDuration ) t -= ms->totalDuration;
      while( t < 0 ) t += ms->totalDuration;

      float pv1 = ms->segments[idx].nv1;
      
      float cpdratio = 0.5;
      if( ms->segments[idx].duration > 0 )
         cpdratio = ms->segments[idx].cpduration / ms->segments[idx].duration;
      
      float cpvratio = 0.5;
      if( ms->segments[idx].nv1 != ms->segments[idx].v0 )
         cpvratio = ( ms->segments[idx].cpv - ms->segments[idx].v0 ) / ( ms->segments[idx].nv1 - ms->segments[idx].v0 );
      
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
      
      ms->segments[idx].cpduration = cpdratio * ms->segments[idx].duration;
      ms->segments[idx].cpv = (ms->segments[idx].nv1 - ms->segments[idx].v0 ) * cpvratio + ms->segments[idx].v0;
      ms->segments[idx+1].cpduration = cpdratio * ms->segments[idx+1].duration;
      ms->segments[idx+1].cpv = (ms->segments[idx+1].nv1 - ms->segments[idx+1].v0 ) * cpvratio + ms->segments[idx+1].v0;
   }
}


void unsplitSegment( MSEGStorage *ms, float t ) {
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
}
   
void deleteSegment( MSEGStorage *ms, float t ) {
   auto idx = timeToSegment( ms, t );
   
   for( int i=idx; i<ms->n_activeSegments - 1; ++i )
   {
      ms->segments[i] = ms->segments[i+1];
   }
   ms->n_activeSegments --;
   
   int prior = idx - 1;
   if( prior < 0 )
      prior = ms->n_activeSegments;
   
   // if( prior != idx )
      //ms->segments[idx].v0 = ms->segments[prior].nv1;
}

void resetControlPoint( MSEGStorage *ms, float t )
{
   auto idx = timeToSegment( ms, t );
   if( idx >= 0 && idx < ms->n_activeSegments )
   {
      ms->segments[idx].cpduration = ms->segments[idx].duration * 0.5;
      ms->segments[idx].cpv = ( ms->segments[idx].v0 + ms->segments[idx].nv1 ) * 0.5;
   }
}

void constrainControlPointAt( MSEGStorage *ms, int idx )
{
   switch( ms->segments[idx].type )
   {
   case MSEGStorage::segment::LINEAR:
   {
      // constrain time and value
      ms->segments[idx].cpduration = limit_range( ms->segments[idx].cpduration, 0.f, ms->segments[idx].duration );
      auto l = std::min(ms->segments[idx].v0, ms->segments[idx].nv1 );
      auto h = std::max(ms->segments[idx].v0, ms->segments[idx].nv1 );
      ms->segments[idx].cpv = limit_range( ms->segments[idx].cpv, l, h );
   }
   break;
   case MSEGStorage::segment::QUAD_BEZIER:
   case MSEGStorage::segment::BROWNIAN:
   {
      // Constrain time but space is in -1,1
      ms->segments[idx].cpduration = limit_range( ms->segments[idx].cpduration, 0.f, ms->segments[idx].duration );
      ms->segments[idx].cpv = limit_range( ms->segments[idx].cpv, -1.f, 1.f );

   }
   break;
   default:
   {
      // constrain time and stay at the midpoint
      ms->segments[idx].cpduration = limit_range( ms->segments[idx].cpduration, 0.f, ms->segments[idx].duration );
      ms->segments[idx].cpv = 0.5 * (ms->segments[idx].v0 + ms->segments[idx].nv1);
   }
   break;
   }
}
   
}
}
