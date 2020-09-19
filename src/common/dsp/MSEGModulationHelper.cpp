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

void MSEGModulationHelper::rebuildCache( MSEGStorage *ms )
{
   float totald = 0;
   for( int i=0; i<ms->n_activeSegments; ++i )
   {
      ms->segmentStart[i] = totald;
      totald += ms->segments[i].duration;
      ms->segmentEnd[i] = totald;

      // set up some aux points for convenience
      switch( ms->segments[i].type )
      {
      case MSEGStorage::segment::CONSTANT:
      {
         ms->segments[i].v1 = ms->segments[i].v0;
         ms->segments[i].cpv = ms->segments[i].v0;
         ms->segments[i].cpduration = ms->segments[i].duration / 2;
         break;
      }
      case MSEGStorage::segment::LINEAR:
      {
         ms->segments[i].cpv = 0.5 * ( ms->segments[i].v0 + ms->segments[i].v1 );
         ms->segments[i].cpduration = ms->segments[i].duration / 2;
         break;
      }
      default:
         break;
      }
   }
   ms->totalDuration = totald;
   ms->lastSegmentEvaluated = -1;
}

float MSEGModulationHelper::valueAt(int ip, float fup, float df, MSEGStorage *ms)
{
   if( ms->n_activeSegments == 0 ) return df;

   // This still has some problems but lets try this for now
   double up = (double)ip + fup;
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
   if( idx != ms->lastSegmentEvaluated )
   {
      segInit = true;
      ms->lastSegmentEvaluated = idx;
   }
   
   float pd = up - ms->segmentStart[idx];

   float res = r.v0;
   switch( r.type )
   {
   case MSEGStorage::segment::CONSTANT:
      res = r.v0;
      break;
   case MSEGStorage::segment::LINEAR:
   {
      float frac = pd / r.duration;
      if( df < 0 )
         frac = pow( frac, 1.0 + df * 0.7 );
      if( df > 0 )
         frac = pow( frac, 1.0 + df * 3 );
      res = frac * r.v1 + ( 1 - frac ) * r.v0;
      break;
   }
   case MSEGStorage::segment::BROWNIAN:
   {
      const float sdt = 0.001;
      static constexpr int validx = 0, lasttime = 1;
      if( segInit )
      {
         r.state[validx] = r.v0;
         r.state[lasttime] = 0;
      }
      float targetTime = pd/r.duration;
      if( targetTime >= 1 )
      {
         res = r.v1;
      }
      else if( targetTime <= 0 )
      {
         res = r.v0;
      }
      else if( targetTime <= r.state[lasttime] )
      {
         res = r.state[validx];
      }
      else
      {
         while( r.state[lasttime] < targetTime && r.state[lasttime] < 1 )
         {
            float dt = std::min( sdt, targetTime - r.state[lasttime] );
            
            if( r.state[lasttime] < 1 )
            {
               float lincoef  = ( r.v1 - r.state[validx] ) / ( 1 - r.state[lasttime] );
               float randcoef = 0.1 * r.cpduration / r.duration;
               
               r.state[validx] += lincoef * dt + randcoef * ( ( 2.f * rand() ) / (float)(RAND_MAX) - 1 );
               r.state[lasttime] += dt;
            }
            else
               r.state[validx] = r.v1;
         }
         res = r.state[validx];
      }

      ms->segments[idx].state[validx] = r.state[validx];
      ms->segments[idx].state[lasttime] = r.state[lasttime];
      
      break;
   }
   case MSEGStorage::segment::QUADBEZ:
   {
      /*
      ** First lets correct the control point so that the
      ** user specified one is through the curve. This means
      ** that the line from the center of the segment connecting
      ** the endpoints to the control point pushes out 2x
      */

      float cpv = r.cpv;
      float cpt = r.cpduration;

      // If we are positioned exactly at the midpoint our calculateion below to find time will fail
      // so walk off a smidge
      if( fabs( cpt - r.duration * 0.5 ) < 1e-5 )
         cpt += 1e-4;
      
      // here's the midpoint along the connecting curve
      float tp = r.duration/2;
      float vp = (r.v1-r.v0)/2 + r.v0;

      // The distance from tp,vp to cpt,cpv
      float dt = (cpt-tp), dy = (cpv-vp);

      cpv = vp + 2 * dy; 
      cpt = tp + 2 * dt;
      
      // B = (1-t)^2 P0 + 2 t (1-t) P1 + t^2 P2
      float ttarget = pd;
      float px0 = 0, px1 = cpt, px2 = r.duration,
         py0 = r.v0, py1 = cpv, py2 = r.v1;



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
         // This means we have a line between v0 and v1
         float frac = pd / r.duration;
         res = frac * r.v1 + ( 1 - frac ) * r.v0;
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
      float mv = r.v0; //std::min( r.v0, r.v1 );
      float xv = r.v1; // std::max( r.v0, r.v1 );
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
         res = frac * r.v1 + ( 1 - frac ) * r.v0;
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

   case MSEGStorage::segment::WAVE: {
      int steps = (int)( r.cpduration / r.duration * 15 );
      float mul = ( 1 + 2 * steps ) * M_PI;
      auto f = pd/r.duration;
      float a = 1;
      
      if( df < 0 )
         a = 1 + df * 0.5;
      if( df > 0 )
         a = 1 + df * 1.5;

      res = ( r.v0-r.v1 ) * pow( ( cos( mul * f ) + 1 ) * 0.5, a ) + r.v1;
      break;
   }

   case MSEGStorage::segment::DIGILINE: {
      int steps = (int)( r.cpduration / r.duration * 18 ) + 2;
      float frac = (float)( (int)( steps * pd / r.duration ) ) / (steps-1);
      if( df < 0 )
         frac = pow( frac, 1.0 + df * 0.7 );
      if( df > 0 )
         frac = pow( frac, 1.0 + df * 3 );
      res = frac * r.v1 + ( 1 - frac ) * r.v0;
      break;
   }

   
   }
   //std::cout << _D(pd) << _D(r.type) << _D(r.duration) << _D(r.v0) << std::endl;

   
   return res;
}
