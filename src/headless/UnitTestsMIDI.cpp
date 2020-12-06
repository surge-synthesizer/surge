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

TEST_CASE( "Sustain Pedal and Mono", "[midi]" ) // #1459
{
   auto playingNoteCount = [](std::shared_ptr<SurgeSynthesizer> surge ) {
      int ct =0;
      for (auto v : surge->voices[0])
      {
         if( v->state.gate ) ct ++;
      }
      return ct;
   };
   auto solePlayingNote = [](std::shared_ptr<SurgeSynthesizer> surge) {
      int ct = 0;
      int res = -1;
      for( auto v : surge->voices[0] )
      {
         if( v->state.gate )
         {
            ct++;
            res = v->state.key;
         }
      }
      REQUIRE( ct == 1 );
      return res;
   };
   SECTION( "No Pedal Mono" )
   {
      auto surge = Surge::Headless::createSurge(44100);
      REQUIRE( surge );
      surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
      auto step = [surge]() { for( int i=0; i<25; ++i ) surge->process(); };
      step();

      // Press 4 chromatic keys down
      for( int n=60; n<64; ++n )
      {
         surge->playNote(0, n, 120, 0); step();
         REQUIRE(solePlayingNote(surge) == n );

         for( int i=0; i<128; ++i )
         {
            int xp = ( i >= 60 && i <= n ) ? 120 : 0;
            REQUIRE( surge->channelState[0].keyState[i].keystate == xp );
         }
      }

      // And remove them in reverse order
      for( int n=63; n>=60; --n )
      {
         surge->releaseNote(0, n, 120); step();

         if( n == 60 )
            REQUIRE( playingNoteCount(surge) == 0 );
         else
            REQUIRE( solePlayingNote(surge) == n-1);

         for( int i=0; i<128; ++i )
         {
            int xp = ( i >= 60 && i < n ) ? 120 : 0;
            REQUIRE( surge->channelState[0].keyState[i].keystate == xp );
         }
      }
   }

   SECTION( "Release to Highest by Default" )
   {
      auto surge = Surge::Headless::createSurge(44100);
      REQUIRE( surge );
      surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
      auto step = [surge]() { for( int i=0; i<25; ++i ) surge->process(); };
      step();

      // In order play 60 66 64 65 62
      surge->playNote(0, 60, 120, 0); step();
      surge->playNote(0, 66, 120, 0); step();
      surge->playNote(0, 64, 120, 0); step();
      surge->playNote(0, 65, 120, 0); step();
      surge->playNote(0, 62, 120, 0); step();

      // So note 62 should be playing
      REQUIRE(solePlayingNote( surge ) == 62 );

      // Now release 64 and that should be unch
      surge->releaseNote( 0, 64, 0 ); step();
      REQUIRE(solePlayingNote( surge ) == 62 );

      // And release 62 and 66 should play because we have highest note semantics
      surge->releaseNote( 0, 62, 0 ); step();
      REQUIRE(solePlayingNote( surge ) == 66 );
   }

   SECTION( "Pedal Holds Last Played" )
   {
      auto step = [](auto surge) { for( int i=0; i<25; ++i ) surge->process(); };
      auto testInSurge = [](auto kernel) {
         auto surge = Surge::Headless::createSurge(44100);
         REQUIRE( surge );
         surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
         kernel(surge);
      };

      int susp = 64;
      testInSurge( [&]( auto surge ) {
         // Case one - no pedal play and release
         INFO( "Single Note On and Off" );
         surge->playNote(0, 60, 120, 0); step(surge);
         surge->releaseNote( 0, 60, 0 ); step(surge);
         REQUIRE( playingNoteCount(surge) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Single Pedal Before Note On and Off" );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0 ); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - use pedal play and release
        INFO( "Single Pedal After Note On" );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->releaseNote( 0, 60, 0 ); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Note Release with note held. This is the one Evil wants changed." );
        surge->playNote( 0, 48, 127, 0 ); step( surge );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 ); // We want a mode where this is 48
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( solePlayingNote( surge ) == 48 );
        surge->releaseNote( 0, 48, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Under-Note Release with note held. This is the one Evil wants changed." );
        surge->playNote( 0, 48, 127, 0 ); step( surge );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 ); // We want a mode where this is 48
        surge->releaseNote( 0, 48, 0 ); step( surge );
        REQUIRE( solePlayingNote( surge ) == 60 ); // and in that mode this would stay 48
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Under-Note Release with pedan on." );
        surge->playNote( 0, 48, 127, 0 ); step( surge );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 48, 0); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->releaseNote( 0, 60, 0 ); step( surge );
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });


   }


   SECTION( "Pedal Mode Release" )
   {
      auto step = [](auto surge) { for( int i=0; i<25; ++i ) surge->process(); };
      auto testInSurge = [](auto kernel) {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE( surge );
        surge->storage.monoPedalMode = RELEASE_IF_OTHERS_HELD;
        surge->storage.getPatch().scene[0].polymode.val.i = pm_mono;
        kernel(surge);
      };

      int susp = 64;
      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Single Note On and Off" );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0 ); step(surge);
        REQUIRE( playingNoteCount(surge) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Single Pedal Before Note On and Off" );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0 ); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - use pedal play and release
        INFO( "Single Pedal After Note On" );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->releaseNote( 0, 60, 0 ); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Note Release with note held. This is the one Evil wants changed." );
        surge->playNote( 0, 48, 127, 0 ); step( surge );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0); step(surge);
        REQUIRE( solePlayingNote( surge ) == 48 ); // This is the difference in modes
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( solePlayingNote( surge ) == 48 );
        surge->releaseNote( 0, 48, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Under-Note Release with note held. This is the one Evil wants changed." );
        surge->playNote( 0, 48, 127, 0 ); step( surge );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 60, 0); step(surge);
        REQUIRE( solePlayingNote( surge ) == 48 ); // We want a mode where this is 48
        surge->releaseNote( 0, 48, 0 ); step( surge );
        REQUIRE( solePlayingNote( surge ) == 48 ); // and in that mode this would stay 48
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

      testInSurge( [&]( auto surge ) {
        // Case one - no pedal play and release
        INFO( "Under-Note Release with pedan on." );
        surge->playNote( 0, 48, 127, 0 ); step( surge );
        surge->channelController( 0, 64, 127 ); step( surge );
        surge->playNote(0, 60, 120, 0); step(surge);
        surge->releaseNote( 0, 48, 0); step(surge);
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->releaseNote( 0, 60, 0 ); step( surge );
        REQUIRE( solePlayingNote( surge ) == 60 );
        surge->channelController( 0, 64, 0 ); step( surge );
        REQUIRE( playingNoteCount( surge ) == 0 );
      });

   }
   // add it to user prefs menu
   // add it for patch to play meny rmb
}