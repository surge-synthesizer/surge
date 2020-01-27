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

TEST_CASE( "Simple Single Oscillator is Constant", "[dsp]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   // surge->storage.getPatch().scene[0].osc[0].type.val.i = ot_sinus;

   int len = 4410 * 5;
   //int len = BLOCK_SIZE * 20;
   Surge::Headless::playerEvents_t heldC = Surge::Headless::makeHoldMiddleC(len);
   REQUIRE( heldC.size() == 2 );
   
   float* data = NULL;
   int nSamples, nChannels;

   Surge::Headless::playAsConfigured(surge, heldC, &data, &nSamples, &nChannels);
   REQUIRE( data );
   REQUIRE( std::abs( nSamples - len ) <= BLOCK_SIZE );
   REQUIRE( nChannels == 2 );

   float rms = 0;
   for( int i=0; i<nSamples * nChannels; ++i )
   {
      rms += data[i] * data[ i ];
   }
   rms /= nSamples * nChannels;
   rms = sqrt(rms);
   REQUIRE( rms > 0.1 );
   REQUIRE( rms < 0.101 );


   int zeroCrossings = 0;
   for( int i=0; i<nSamples * nChannels - 2 ; i += 2 )
   {
      if( data[i] > 0 && data[i+2] < 0 )
         zeroCrossings ++;
   }
   // Somewhere in here
   REQUIRE( zeroCrossings > 130 );
   REQUIRE( zeroCrossings < 160 );
   
   if (data)
      delete[] data;

}
TEST_CASE( "Unison Absolute and Relative", "[osc]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );

   auto assertRelative = [surge](const char* pn) {
                            REQUIRE( surge->loadPatchByPath( pn, -1, "Test" ) );
                            auto f60_0 = frequencyForNote( surge, 60, 5, 0 );
                            auto f60_1 = frequencyForNote( surge, 60, 5, 1 );
                            
                            auto f60_avg = 0.5 * ( f60_0 + f60_1 );
                            
                            auto f72_0 = frequencyForNote( surge, 72, 5, 0 );
                            auto f72_1 = frequencyForNote( surge, 72, 5, 1 );
                            auto f72_avg = 0.5 * ( f72_0 + f72_1 );
                            
                            // In relative mode, the average frequencies should double, as should the individual outliers
                            REQUIRE( f72_avg / f60_avg == Approx( 2 ).margin( 0.01 ) );
                            REQUIRE( f72_0 / f60_0 == Approx( 2 ).margin( 0.01 ) );
                            REQUIRE( f72_1 / f60_1 == Approx( 2 ).margin( 0.01 ) );
                         };

   auto assertAbsolute = [surge](const char* pn, bool print = false) {
                            REQUIRE( surge->loadPatchByPath( pn, -1, "Test" ) );
                            auto f60_0 = frequencyForNote( surge, 60, 5, 0 );
                            auto f60_1 = frequencyForNote( surge, 60, 5, 1 );
                            
                            auto f60_avg = 0.5 * ( f60_0 + f60_1 );
                            
                            auto f72_0 = frequencyForNote( surge, 72, 5, 0 );
                            auto f72_1 = frequencyForNote( surge, 72, 5, 1 );
                            auto f72_avg = 0.5 * ( f72_0 + f72_1 );
                            
                            // In absolute mode, the average frequencies should double, but the channels should have constant difference
                            REQUIRE( f72_avg / f60_avg == Approx( 2 ).margin( 0.01 ) );
                            REQUIRE( ( f72_0 - f72_1 ) / ( f60_0 - f60_1 ) == Approx( 1 ).margin( 0.01 ) );
                            if( print )
                            {
                               std::cout << "F60 " << f60_avg << " " << f60_0 << " " << f60_1 << " " << f60_0 - f60_1 << std::endl;
                               std::cout << "F72 " << f72_avg << " " << f72_0 << " " << f72_1 << " " << f60_0 - f60_1 << std::endl;
                            }
                         };
   
   SECTION( "Wavetable Oscillator" )
   {
      assertRelative("test-data/patches/Wavetable-Sin-Uni2-Relative.fxp");
      assertAbsolute("test-data/patches/Wavetable-Sin-Uni2-Absolute.fxp");
   }

   SECTION( "Window Oscillator" )
   {
      assertRelative("test-data/patches/Window-Sin-Uni2-Relative.fxp");
      assertAbsolute("test-data/patches/Window-Sin-Uni2-Absolute.fxp");
   }

   SECTION( "Classic Oscillator" )
   {
      assertRelative("test-data/patches/Classic-Uni2-Relative.fxp");
      assertAbsolute("test-data/patches/Classic-Uni2-Absolute.fxp");
   }

   SECTION( "SH Oscillator" )
   {
      assertRelative("test-data/patches/SH-Uni2-Relative.fxp");
      assertAbsolute("test-data/patches/SH-Uni2-Absolute.fxp");
   }

}

TEST_CASE( "All Patches have Bounded Output", "[dsp]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge.get() );

   Surge::Headless::playerEvents_t scale =
       Surge::Headless::make120BPMCMajorQuarterNoteScale(0, 44100);

   auto callBack = [](const Patch& p, const PatchCategory& pc, const float* data, int nSamples,
                      int nChannels) -> void {
      bool writeWav = false; // toggle this to true to write each sample to a wav file
      REQUIRE( nSamples * nChannels > 0 );
      
      if (nSamples * nChannels > 0)
      {
         const auto minmaxres = std::minmax_element(data, data + nSamples * nChannels);
         auto mind = minmaxres.first;
         auto maxd = minmaxres.second;

         float rms=0, L1=0;
         for( int i=0; i<nSamples*nChannels; ++i)
         {
            rms += data[i]*data[i];
            L1 += fabs(data[i]);
         }
         L1 = L1 / (nChannels*nSamples);
         rms = sqrt(rms / nChannels / nSamples );

         REQUIRE( L1 < 1 );
         REQUIRE( rms < 1 );
         REQUIRE( *maxd < 6 );
         REQUIRE( *maxd >= 0 );
         REQUIRE( *mind > -6 );
         REQUIRE( *mind <= 0 );

         /*
         std::cout << "cat/patch = " <<  pc.name << " / " << std::left << std::setw(30) << p.name;
         std::cout << "  range = [" << std::setw(10)
                   << std::fixed << *mind << ", " << std::setw(10) << std::fixed << *maxd << "]"
                   << " L1=" << L1
                   << " rms=" << rms
                   << " samp=" << nSamples << " chan=" << nChannels << std::endl;
         */

      }
   };

   Surge::Headless::playOnNRandomPatches(surge, scale, 100, callBack);
}


TEST_CASE( "lipol_ps class", "[dsp]" )
{
   lipol_ps mypol;
   float prevtarget = -1.0;
   mypol.set_target(prevtarget);
   mypol.instantize();

   constexpr size_t nfloat = 64;
   constexpr size_t nfloat_quad = 16;
   float storeTarget alignas(16)[nfloat];
   mypol.store_block(storeTarget, nfloat_quad);

   for( auto i=0; i<nfloat; ++i )
      REQUIRE(storeTarget[i] == prevtarget); // should be constant in the first instance

   for( int i=0; i<10; ++i )
   {
      float target = (i)*(i) / 100.0;
      mypol.set_target(target);
      
      mypol.store_block(storeTarget, nfloat_quad);
      
      REQUIRE(storeTarget[nfloat-1] == Approx(target));
      
      float dy = storeTarget[1] - storeTarget[0];
      for( auto i=1; i<nfloat; ++i )
      {
         REQUIRE( storeTarget[i] - storeTarget[i-1] == Approx(dy).epsilon(1e-3) );
      }

      REQUIRE( prevtarget + dy == Approx(storeTarget[0]) );
      
      prevtarget = target;
   }
      
}

// When we return to #1514 this is a good starting point
#if 0
TEST_CASE( "NaN Patch from Issue 1514", "[dsp]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   REQUIRE( surge->loadPatchByPath( "test-data/patches/VinceyCrash1514.fxp", -1, "Test" ) );

   for( int d=0; d<10; d++ )
   {
      auto events = Surge::Headless::makeHoldNoteFor( 60 + 24, 4410, 100, 0 );
      for( auto &e : events )
         e.atSample += 1000;

      surge->allNotesOff();
      for( int i=0; i<100; ++i )
         surge->process();
      
      float *res;
      int nS, nC;
      playAsConfigured(surge, events, &res, &nS, &nC );
      surge->storage.getPatch().scene[0].lfo[0].decay.set_value_f01( .325 + d/1000.f );
      char txt[512];
      surge->storage.getPatch().scene[0].lfo[0].decay.get_display( txt, false, 0.f );
      const auto minmaxres = std::minmax_element(res, res + nS * nC);
      auto mind = minmaxres.first;
      auto maxd = minmaxres.second;

      // std::cout << "minMax at " << d << " / " << txt << " is " << *mind << " / " << *maxd << std::endl;
      std::string title = "delay = " + std::string( txt );
      std::string fname = "/tmp/nanPatch_" + std::to_string( d ) + ".png";
      makePlotPNGFromData( fname, title, res, nS, nC );

      delete[] res;
   }
   
}
#endif
