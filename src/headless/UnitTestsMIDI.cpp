#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2/catch2.hpp"

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
         surge->storage.getPatch().splitpoint.val.i = smc;
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
         surge->storage.getPatch().splitpoint.val.i = smc;
         for( auto mc=0; mc<16; ++mc )
         {
            auto fr = frequencyForNote( surge, 69, 2, 0, mc );
            auto targetfr = mc <= splitChan ? 440 : 440 * 4;
            REQUIRE( fr == Approx( targetfr ).margin( 0.1 ) );
         }
      }
   }
}

TEST_CASE( "Duplicate Note Channel Management Issue 3084", "[midi]" )
{
   SECTION( "MPE Notes quickly off" )
   {
      auto surge = Surge::Headless::createSurge(44100);
      REQUIRE( surge );
      surge->mpeEnabled = true;
      for( int i=0; i<5; ++i ) surge->process();
      surge->playNote(1, 60, 127, 0 );
      surge->process();
      int ct = 0;
      for( auto v : surge->voices[0] )
      {
         REQUIRE( ct == 0 );
         REQUIRE( v->state.channel == 1 );
         REQUIRE( v->state.key == 60 );
         REQUIRE( v->state.gate );
         ct++;
      }

      surge->playNote(2, 60, 127, 0 );
      surge->process();

      REQUIRE( surge->voices->size() == 2 );
      int ch = 1;
      for( auto v : surge->voices[0] )
      {
         REQUIRE( v->state.channel == ch );
         REQUIRE( v->state.key == 60 );
         REQUIRE( v->state.gate );
         ch++;
      }

      surge->releaseNote(2, 60, 0 );
      surge->process();
      REQUIRE( surge->voices->size() == 2 );
      ch = 1;
      for( auto v : surge->voices[0] )
      {
         REQUIRE( v->state.channel == ch );
         REQUIRE( v->state.key == 60 );
         REQUIRE( v->state.gate == ( ch == 1 ) );
         ch++;
      }
   }

   SECTION( "MPE Notes absent process" )
   {
      // Basically the same test just without a call to process between key modifications
      auto surge = Surge::Headless::createSurge(44100);
      REQUIRE( surge );
      surge->mpeEnabled = true;
      for( int i=0; i<5; ++i ) surge->process();

      surge->playNote(1, 60, 127, 0 );
      int ct = 0;
      for( auto v : surge->voices[0] )
      {
         REQUIRE( ct == 0 );
         REQUIRE( v->state.channel == 1 );
         REQUIRE( v->state.key == 60 );
         REQUIRE( v->state.gate );
         ct++;
      }

      surge->playNote(2, 60, 127, 0 );

      REQUIRE( surge->voices->size() == 2 );
      int ch = 1;
      for( auto v : surge->voices[0] )
      {
         REQUIRE( v->state.channel == ch );
         REQUIRE( v->state.key == 60 );
         REQUIRE( v->state.gate );
         ch++;
      }

      surge->releaseNote(2, 60, 0 );
      REQUIRE( surge->voices->size() == 2 );
      ch = 1;
      for( auto v : surge->voices[0] )
      {
         REQUIRE( v->state.channel == ch );
         REQUIRE( v->state.key == 60 );
         REQUIRE( v->state.gate == ( ch == 1 ) );
         ch++;
      }
   }
}
