#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "catch2/catch2.hpp"
#include "FastMath.h"
#include "MSEGModulationHelper.h"

struct msegObservation {
   msegObservation(int ip, float fp, float va ) {
      iPhase = ip; fPhase = fp; phase = ip + fp; v = va;
   }
   int iPhase;
   float fPhase;
   float v;
   float phase;
};

std::vector<msegObservation> runMSEG( MSEGStorage *ms, float dPhase, float phaseMax, float deform = 0, bool release = false )
{
   auto res = std::vector<msegObservation>();
   double phase = 0.0;
   int iphase = 0;
   int lastSeg;
   float state[6];
   while( phase + iphase < phaseMax )
   {
      auto r = Surge::MSEG::valueAt(iphase, phase, deform, ms, lastSeg, state, release );
      res.push_back( msegObservation( iphase, phase, r ));
      phase += dPhase;
      if( phase > 1 ) { phase -= 1; iphase += 1; }
   }
   return res;
}

/*
 * These tests test the relationship between configuration of MSEG Storage and the phase evaluator
 */
TEST_CASE( "Basic MSEG Evaluation", "[mseg]" )
{
   SECTION( "Simple Square" )
   {
      MSEGStorage ms;
      ms.n_activeSegments = 4;
      ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
      ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
      ms.segments[0].type = MSEGStorage::segment::LINEAR;
      ms.segments[0].v0 = 1.0;

      ms.segments[1].duration = MSEGStorage::minimumDuration;
      ms.segments[1].type = MSEGStorage::segment::LINEAR;
      ms.segments[1].v0 = 1.0;

      ms.segments[2].duration = 0.5 - MSEGStorage::minimumDuration;
      ms.segments[2].type = MSEGStorage::segment::LINEAR;
      ms.segments[2].v0 = -1.0;

      ms.segments[3].duration = MSEGStorage::minimumDuration;
      ms.segments[3].type = MSEGStorage::segment::LINEAR;
      ms.segments[3].v0 = -1.0;

      Surge::MSEG::rebuildCache(&ms);

      // OK so lets go ahead and run it at a variety of forward phases
      auto runIt = runMSEG(&ms, 0.0321, 5 );
      for( auto c : runIt )
      {
         if( c.fPhase < 0.5 - MSEGStorage::minimumDuration )
            REQUIRE( c.v == 1 );
         if( c.fPhase > 0.5 &&  c.fPhase < 1 - MSEGStorage::minimumDuration )
            REQUIRE( c.v == -1 );
      }
   }

   SECTION( "Longer Square" )
   {
      MSEGStorage ms;
      ms.n_activeSegments = 4;
      ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
      ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
      ms.segments[0].type = MSEGStorage::segment::LINEAR;
      ms.segments[0].v0 = 1.0;

      ms.segments[1].duration = MSEGStorage::minimumDuration;
      ms.segments[1].type = MSEGStorage::segment::LINEAR;
      ms.segments[1].v0 = 1.0;

      ms.segments[2].duration = 1.0 - MSEGStorage::minimumDuration;
      ms.segments[2].type = MSEGStorage::segment::LINEAR;
      ms.segments[2].v0 = -1.0;

      ms.segments[3].duration = MSEGStorage::minimumDuration;
      ms.segments[3].type = MSEGStorage::segment::LINEAR;
      ms.segments[3].v0 = -1.0;

      Surge::MSEG::rebuildCache(&ms);

      // OK so lets go ahead and run it at a variety of forward phases
      auto runIt = runMSEG(&ms, 0.0321, 14 );
      for( auto c : runIt )
      {
         auto dbphase = (int)( 2 * c.phase ) % 3;
         INFO( "phase is " << c.phase << " " << dbphase )
         if( dbphase == 0 )
            REQUIRE( c.v == 1 );
         if( dbphase == 1 || dbphase == 2  )
            REQUIRE( c.v == -1 );
      }
   }
}

TEST_CASE( "Unlocked Endpoitns", "[mseg]" )
{
   SECTION( "Simple Square Default Locks" )
   {
      MSEGStorage ms;
      ms.n_activeSegments = 3;
      ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
      ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
      ms.segments[0].type = MSEGStorage::segment::LINEAR;
      ms.segments[0].v0 = 1.0;

      ms.segments[1].duration = MSEGStorage::minimumDuration;
      ms.segments[1].type = MSEGStorage::segment::LINEAR;
      ms.segments[1].v0 = 1.0;

      ms.segments[2].duration = 0.5;
      ms.segments[2].type = MSEGStorage::segment::LINEAR;
      ms.segments[2].v0 = -1.0;

      Surge::MSEG::rebuildCache(&ms);

      // OK so lets go ahead and run it at a variety of forward phases
      auto runIt = runMSEG(&ms, 0.0321, 5 );
      for( auto c : runIt )
      {
         if( c.fPhase < 0.5 - MSEGStorage::minimumDuration )
            REQUIRE( c.v == 1 );
         if( c.fPhase > 0.5 &&  c.fPhase < 1 - MSEGStorage::minimumDuration )
            REQUIRE( c.v > -1 );
      }
   }

   SECTION( "Free Endpoint is a Square" )
   {
      MSEGStorage ms;
      ms.n_activeSegments = 3;
      ms.endpointMode = MSEGStorage::EndpointMode::FREE;
      ms.segments[0].duration = 0.5 - MSEGStorage::minimumDuration;
      ms.segments[0].type = MSEGStorage::segment::LINEAR;
      ms.segments[0].v0 = 1.0;

      ms.segments[1].duration = MSEGStorage::minimumDuration;
      ms.segments[1].type = MSEGStorage::segment::LINEAR;
      ms.segments[1].v0 = 1.0;

      ms.segments[2].duration = 0.5;
      ms.segments[2].type = MSEGStorage::segment::LINEAR;
      ms.segments[2].v0 = -1.0;
      ms.segments[2].nv1 = -1.0; // The free mode will preserve this

      Surge::MSEG::rebuildCache(&ms);

      // OK so lets go ahead and run it at a variety of forward phases
      auto runIt = runMSEG(&ms, 0.0321, 5 );
      for( auto c : runIt )
      {
         if( c.fPhase < 0.5 - MSEGStorage::minimumDuration )
            REQUIRE( c.v == 1 );
         if( c.fPhase > 0.5 &&  c.fPhase < 1 - MSEGStorage::minimumDuration )
            REQUIRE( c.v == -1 );
      }
   }
}

TEST_CASE( "Deform per Segment", "[mseg]" )
{
   SECTION( "Triangle with Deform" )
   {
      MSEGStorage ms;
      ms.n_activeSegments = 2;
      ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
      ms.segments[0].duration = 0.5;
      ms.segments[0].type = MSEGStorage::segment::LINEAR;
      ms.segments[0].v0 = -1.0;

      ms.segments[1].duration = 0.5;
      ms.segments[1].type = MSEGStorage::segment::LINEAR;
      ms.segments[1].v0 = 1.0;

      Surge::MSEG::rebuildCache(&ms);

      // OK so lets go ahead and run it at a variety of forward phases
      auto runIt = runMSEG(&ms, 0.0321, 3 );
      for( auto c : runIt )
      {
         if( c.fPhase < 0.5  )
            REQUIRE( c.v == Approx( 2 * c.fPhase / 0.5 - 1 ) );
         if( c.fPhase > 0.5 )
            REQUIRE( c.v == Approx( 1 - 2 * (c.fPhase - 0.5 ) / 0.5 ) );
      }

      auto runDef = runMSEG(&ms, 0.0321, 3, 0.9 );
      for( auto c : runDef )
      {
         if( c.fPhase < 0.5 && c.v != -1 )
            REQUIRE( c.v < 2 * c.fPhase / 0.5 - 1 );
         if( c.fPhase > 0.5 && c.v != 1 )
            REQUIRE( c.v > 1 - 2 * (c.fPhase - 0.5 ) / 0.5 );
      }

      ms.segments[0].useDeform = false;
      auto runDefPartial = runMSEG(&ms, 0.0321, 3, 0.9 );
      for( auto c : runDefPartial )
      {
         if( c.fPhase < 0.5 && c.v != -1 )
            REQUIRE( c.v == Approx( 2 * c.fPhase / 0.5 - 1 ) );
         if( c.fPhase > 0.5 && c.v != 1 )
            REQUIRE( c.v > 1 - 2 * (c.fPhase - 0.5 ) / 0.5 );
      }
   }
}

TEST_CASE("OneShot vs Loop", "[mseg]" )
{
   SECTION( "Triangle Loop" )
   {
      MSEGStorage ms;
      ms.n_activeSegments = 2;
      ms.endpointMode = MSEGStorage::EndpointMode::LOCKED;
      ms.segments[0].duration = 0.5;
      ms.segments[0].type = MSEGStorage::segment::LINEAR;
      ms.segments[0].v0 = -1.0;

      ms.segments[1].duration = 0.5;
      ms.segments[1].type = MSEGStorage::segment::LINEAR;
      ms.segments[1].v0 = 1.0;

      Surge::MSEG::rebuildCache(&ms);

      // OK so lets go ahead and run it at a variety of forward phases
      auto runIt = runMSEG(&ms, 0.0321, 3 );
      for( auto c : runIt )
      {
         if( c.fPhase < 0.5  )
            REQUIRE( c.v == Approx( 2 * c.fPhase / 0.5 - 1 ) );
         if( c.fPhase > 0.5 )
            REQUIRE( c.v == Approx( 1 - 2 * (c.fPhase - 0.5 ) / 0.5 ) );
      }

      ms.loopMode = MSEGStorage::LoopMode::ONESHOT;
      auto runOne = runMSEG(&ms, 0.0321, 3 );
      for( auto c : runOne )
      {
         if( c.phase < 1 )
         {
            if (c.fPhase < 0.5)
               REQUIRE(c.v == Approx(2 * c.fPhase / 0.5 - 1));
            if (c.fPhase > 0.5)
               REQUIRE(c.v == Approx(1 - 2 * (c.fPhase - 0.5) / 0.5));
         }
         else
         {
            REQUIRE( c.v == -1 );
         }
      }


   }

}