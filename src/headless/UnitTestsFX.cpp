#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2.hpp"

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
      
   }
}
