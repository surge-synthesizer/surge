#include <iostream>
#include <iomanip>
#include <sstream>

#include "HeadlessUtils.h"
#include "Player.h"
#include "Stress.h"
#include "SurgeError.h"

#define CATCH_CONFIG_RUNNER
#include "catch2.hpp"

TEST_CASE( "We can read a collection of wavetables", "[wav]" )
{
   /*
   ** ToDo: 
   ** .wt file
   ** oneshot
   ** srgmarkers
   ** etc
   */
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   
   SECTION( "Wavetable.wav" )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      surge->storage.load_wt_wav_portable("test-data/wav/Wavetable.wav", wt);
      REQUIRE( wt->size == 2048 );
      REQUIRE( wt->n_tables == 256 );
      REQUIRE( ( wt->flags & wtf_is_sample ) == 0 );
   }
   
   SECTION( "05_BELL.wav" )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      surge->storage.load_wt_wav_portable("test-data/wav/05_BELL.wav", wt);
      REQUIRE( wt->size == 2048 );
      REQUIRE( wt->n_tables == 33 );
      REQUIRE( ( wt->flags & wtf_is_sample ) == 0 );
   }
   
   SECTION( "pluckalgo.wav" )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      surge->storage.load_wt_wav_portable("test-data/wav/pluckalgo.wav", wt);
      REQUIRE( wt->size == 2048 );
      REQUIRE( wt->n_tables == 9 );
      REQUIRE( ( wt->flags & wtf_is_sample ) == 0 );
   }
   
   delete surge;
}

TEST_CASE( "All .wt and .wav factory assets load", "[wav]" )
{
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   for( auto p : surge->storage.wt_list )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      wt->size = -1;
      wt->n_tables = -1;
      surge->storage.load_wt(p.path.generic_string(), wt );
      REQUIRE( wt->size > 0 );
      REQUIRE( wt->n_tables > 0 );
   }
   
   delete surge;
}

TEST_CASE( "Retune Surge to .scl files", "[tun]" )
{
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);

   auto n2f = [surge](int n) { return surge->storage.note_to_pitch(n); };

   //Surge::Storage::Scale s = Surge::Storage::readSCLFile("/Users/paul/dev/music/test_scl/Q4.scl" );
   SECTION( "12-intune SCL file" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/12-intune.scl" );
      surge->storage.retuneToScale(s);
      REQUIRE( n2f(48) == 16 );
      REQUIRE( n2f(48+12) == 16*2 );
      REQUIRE( n2f(48+12+12) == 16*4 );
      REQUIRE( n2f(48-12) == 16/2 );
   }

   SECTION( "Zeus 22" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/zeus22.scl" );
      surge->storage.retuneToScale(s);
      REQUIRE( n2f(48) == 16 );
      REQUIRE( n2f(48+s.count) == 16*2 );
      REQUIRE( n2f(48+2*s.count) == 16*4 );
      REQUIRE( n2f(48-s.count) == 16/2 );
   }
}

TEST_CASE( "Simple Single Oscillator is Constant", "[dsp]" )
{
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);
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
   REQUIRE( zeroCrossings > 135 );
   REQUIRE( zeroCrossings < 160 );
   
   if (data)
      delete[] data;
   delete surge;

}

TEST_CASE( "All Patches are Loadable", "[patch]" )
{
#if 0
   // FIXME - why doesn't this work?
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   int i=0;
   for( auto p : surge->storage.patch_list )
   {
      std::cout << i << " " << p.name << " " << p.path.generic_string() << std::endl;
      surge->loadPatch(i);
      ++i;
   }
   
   delete surge;
#endif   
}



int runAllTests(int argc, char **argv)
{
   int result = Catch::Session().run( argc, argv );
   return result;
}
