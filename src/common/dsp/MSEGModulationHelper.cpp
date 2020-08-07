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

MSEGModulationHelper::MSEGModulationHelper()
{
   { segment s; s.duration = 0.2; s.v0 = 0; s.v1 = 1;   s.type=segment::LINEAR;   segments.push_back(s); }
   { segment s; s.duration = 0.2; s.v0 = 0; s.v1 = -1 ; s.type=segment::LINEAR;   segments.push_back(s); }
   { segment s; s.duration = 0.4; s.v0 = 0.5;           s.type=segment::CONSTANT; segments.push_back(s); }
   { segment s; s.duration = 0.3; s.v0 = -0.3;          s.type=segment::CONSTANT; segments.push_back(s); }
   { segment s; s.duration = 0.5; s.v0 = -0.3; s.v1 = 0.8; s.cpduration = 0.45; s.cpv = -0.3;   s.type=segment::QUADBEZ; segments.push_back(s); }
   { segment s; s.duration = 0.8; s.v0 = 0.8; s.v1 = 0.0; s.cpduration = 0.1; s.cpv = -0.2;   s.type=segment::QUADBEZ; segments.push_back(s); }
}

float MSEGModulationHelper::valueAt(float up, float df)
{
   // THIS IS INSANELY INNEFFICIENT RIGHT NOW
   float totald = 0;
   for( auto q : segments ) totald += q.duration;
   while( up > totald ) up -= totald;

   float cd = 0;
   segment r;
   bool found = false;
   for( auto q : segments )
   {
      if( cd + q.duration > up )
      {
         r = q;
         found = true;
         break;
      }
      cd += q.duration;
   }

   if( ! found ) return 0;
   
   auto pd = up - cd;

   float res = r.v0;
   switch( r.type )
   {
   case segment::CONSTANT:
      res = r.v0;
      break;
   case segment::LINEAR:
   {
      float frac = pd / r.duration;
      res = frac * r.v1 + ( 1 - frac ) * r.v0;
      break;
   }
   case segment::QUADBEZ:
   {
      // B = (1-t)^2 P0 + 2 t (1-t) P1 + t^2 P2
      float ttarget = pd;
      float px0 = 0, px1 = r.cpduration, px2 = r.duration,
         py0 = r.v0, py1 = r.cpv, py2 = r.v1;

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
      float t = (-b + sqrt( b*b-4*a*c))/(2*a);

      /*
      ** And now evaluate the bezier in y with that t
      */
      res = (1-t)*(1-t)*py0 + 2 * ( 1-t ) * t * py1 + t * t * py2;
   }
   }
   //std::cout << _D(pd) << _D(r.type) << _D(r.duration) << _D(r.v0) << std::endl;

   
   return res;
}
