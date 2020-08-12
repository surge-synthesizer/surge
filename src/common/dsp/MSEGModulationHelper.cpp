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
   
}

float MSEGModulationHelper::valueAt(float up, float df, MSEGStorage *ms)
{
   if( ms->n_activeSegments == 0 ) return df;
   
   while( up >= ms->totalDuration ) up -= ms->totalDuration;

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
   
   auto pd = up - ms->segmentStart[idx];

   float res = r.v0;
   switch( r.type )
   {
   case MSEGStorage::segment::CONSTANT:
      res = r.v0;
      break;
   case MSEGStorage::segment::LINEAR:
   {
      float frac = pd / r.duration;
      res = frac * r.v1 + ( 1 - frac ) * r.v0;
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
      if( a == 0 )
      {
         // This means we have a line between v0 and v1
         float frac = pd / r.duration;
         res = frac * r.v1 + ( 1 - frac ) * r.v0;
      }
      else if( disc < 0 )
      {
         res = 0;
      }
      else
      {
         float t = (-b + sqrt( b*b-4*a*c))/(2*a);

         /*
         ** And now evaluate the bezier in y with that t
         */
         res = (1-t)*(1-t)*py0 + 2 * ( 1-t ) * t * py1 + t * t * py2;
      }

      break;
   }

   }
   //std::cout << _D(pd) << _D(r.type) << _D(r.duration) << _D(r.v0) << std::endl;

   
   return res;
}
