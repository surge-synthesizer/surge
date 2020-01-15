#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2.hpp"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE( "Channel Split Routes on Channel", "[midi]" )
{
   auto surge = std::shared_ptr<SurgeSynthesizer>( Surge::Headless::createSurge(44100) );
   REQUIRE( surge );
   REQUIRE( surge->loadPatchByPath( "test-data/patches/ChannelSplit-Sin-2OctaveB.fxp", -1, "Test" ) );

   SECTION( "Regular (non-MPE)" )
   {
      surge->mpeEnabled = false;
      for( auto splitChan = 2; splitChan < 14; splitChan ++ )
      {
         auto smc = splitChan * 8;
         surge->storage.getPatch().splitkey.val.i = smc;
         for( auto mc=0; mc<16; ++mc )
         {
            auto fr = frequencyForNote( surge, 69, 2, 0, mc );
            auto targetfr = mc <= splitChan ? 440 : 440 * 4;
            REQUIRE( fr == Approx( targetfr ).margin( 0.1 ) );
         }
      }
   }

   
   SECTION( "MPE Enabled" )
   {
      surge->mpeEnabled = true;
      for( auto splitChan = 2; splitChan < 14; splitChan ++ )
      {
         auto smc = splitChan * 8;
         surge->storage.getPatch().splitkey.val.i = smc;
         for( auto mc=0; mc<16; ++mc )
         {
            auto fr = frequencyForNote( surge, 69, 2, 0, mc );
            auto targetfr = mc <= splitChan ? 440 : 440 * 4;
            REQUIRE( fr == Approx( targetfr ).margin( 0.1 ) );
         }
      }
   }
}


