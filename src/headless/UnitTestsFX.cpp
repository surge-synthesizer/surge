#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "FastMath.h"

using namespace Surge::Test;

TEST_CASE( "Airwindows Loud", "[fx]" )
{
   SECTION( "Make Loud" )
   {
      auto surge = Surge::Headless::createSurge( 44100 );
      REQUIRE( surge );

      for( int i=0; i<100; ++i ) surge->process();

      auto *pt = &(surge->storage.getPatch().fx[0].type);
      auto awv = fxt_airwindows / ( pt->val_max.i - pt->val_min.i );


      auto did = surge->idForParameter(pt);
      surge->setParameter01(did, awv, false );

      auto *pawt = &( surge->storage.getPatch().fx[0].p[0]);


      for( int i=0; i<500; ++i )
      {
         pawt->val.i = 34;
         surge->process();

         float soo = 0.f;

         INFO( "Swapping aiwrindow attempt " << i );
         for (int s = 0; s < 100; ++s)
         {
            surge->process();
            for (int p = 0; p < BLOCK_SIZE; ++p)
            {
               soo += surge->output[0][p] + surge->output[1][p];
               REQUIRE( fabs( surge->output[0][p] ) < 1e-5 );
               REQUIRE( fabs( surge->output[1][p] ) < 1e-5 );
            }
         }

         REQUIRE( fabs(soo) < 1e-5 );

         // Toggle to something which isn't 'loud'
         pawt->val.i = rand() % 30 + 2;
         for (int s = 0; s < 100; ++s)
         {
            surge->process();
         }
       }

   }
}
