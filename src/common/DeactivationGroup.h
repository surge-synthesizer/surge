// -*-c++-*-
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

#pragma once
#include "Parameter.h"

template< int N >
struct DeactivationGroup 
{
   std::array<Parameter *, N> groupParams;
   std::array<int, N> deactivationValue;

   void addDeactivationGroupMember( int i, Parameter *p ) {
      groupParams[i] = p;
      deactivationValue[i] = p->deactivated;
   }

   void scan() {
      std::cout << "SCAN" << std::endl;
      bool newvalue = false;
      for( int i=0; i<N; ++i )
      {
         if( groupParams[i]->deactivated != deactivationValue[i] )
         {
            newvalue = groupParams[i]->deactivated;
            goto apply;
         }
         deactivationValue[i] = groupParams[i]->deactivated;
      }
      return;
   apply:
      for( int i=0; i<N; ++i )
      {
         std::cout << "Activating " << groupParams[i]->get_name() << std::endl;
         groupParams[i]->deactivated = newvalue;
      }
   }
   
};
