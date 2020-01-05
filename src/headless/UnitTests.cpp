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
   
   SECTION( "05_BELL.WAV" )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      surge->storage.load_wt_wav_portable("test-data/wav/05_BELL.WAV", wt);
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
      REQUIRE( n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch() );
      REQUIRE( n2f(surge->storage.scaleConstantNote()+12) == surge->storage.scaleConstantPitch()*2 );
      REQUIRE( n2f(surge->storage.scaleConstantNote()+12+12) == surge->storage.scaleConstantPitch()*4 );
      REQUIRE( n2f(surge->storage.scaleConstantNote()-12) == surge->storage.scaleConstantPitch()/2 );
   }

   SECTION( "Zeus 22" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/zeus22.scl" );
      surge->storage.retuneToScale(s);
      REQUIRE( n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch() );
      REQUIRE( n2f(surge->storage.scaleConstantNote()+s.count) == surge->storage.scaleConstantPitch()*2 );
      REQUIRE( n2f(surge->storage.scaleConstantNote()+2*s.count) == surge->storage.scaleConstantPitch()*4 );
      REQUIRE( n2f(surge->storage.scaleConstantNote()-s.count) == surge->storage.scaleConstantPitch()/2 );
   }

   SECTION( "6 exact" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/6-exact.scl" );
      surge->storage.retuneToScale(s);
      REQUIRE( n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch() );
      REQUIRE( n2f(surge->storage.scaleConstantNote()+s.count) == surge->storage.scaleConstantPitch()*2 );
      REQUIRE( n2f(surge->storage.scaleConstantNote()+2*s.count) == surge->storage.scaleConstantPitch()*4 );
      REQUIRE( n2f(surge->storage.scaleConstantNote()-s.count) == surge->storage.scaleConstantPitch()/2 );
   }
}

/*
** Create a surge pointer on init sine
*/
std::shared_ptr<SurgeSynthesizer> surgeOnSine()
{
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);

   std::string otp = "Init Sine";
   bool foundInitSine = false;
   for (int i = 0; i < surge->storage.patch_list.size(); ++i)
   {
      Patch p = surge->storage.patch_list[i];
      if (p.name == otp)
      {
         surge->loadPatch(i);
         foundInitSine = true;
         break;
      }
   }
   if( ! foundInitSine )
      return nullptr;
   else
      return std::shared_ptr<SurgeSynthesizer>(surge);
}

/*
** An approximation of the frequency of a signal using a simple zero crossing
** frequency measure (which works great for the sine patch and poorly for others
** At one day we could do this with autocorrelation instead but no need now.
*/
double frequencyForNote( std::shared_ptr<SurgeSynthesizer> surge, int note, int seconds = 2, bool channel = 0 )
{
   auto events = Surge::Headless::makeHoldNoteFor( note, 44100 * seconds, 64 );
   float *buffer;
   int nS, nC;
   Surge::Headless::playAsConfigured( surge.get(), events, &buffer, &nS, &nC );

   REQUIRE( nC == 2 );
   REQUIRE( nS >= 44100 * seconds );
   REQUIRE( nS <= 44100 * seconds + 4 * BLOCK_SIZE );

   // Trim off the leading and trailing
   int nSTrim = (int)(nS / 2 * 0.8);
   int start = (int)( nS / 2 * 0.05 );
   float *leftTrimmed = new float[nSTrim];

   for( int i=0; i<nSTrim; ++i )
      leftTrimmed[i] = buffer[ (i + start) * 2 + channel ];

   // OK so now look for sample times between positive/negative crosses
   int v = -1;
   uint64_t dSample = 0, crosses = 0;
   for( int i=0; i<nSTrim -1; ++i )
      if( leftTrimmed[i] < 0 && leftTrimmed[i+1] > 0 )
      {
         if( v > 0 )
         {
            dSample += ( i - v );
            crosses ++;
         }
         v = i;
      }

   float aSample = 1.f * dSample / crosses;

   float time = aSample / 44100.0;
   float freq = 1.0 / time;

   
   delete[] leftTrimmed;
   delete[] buffer;

   return freq;
}

TEST_CASE( "Notes at Appropriate Frequencies", "[tun]" )
{
   auto surge = surgeOnSine();
   REQUIRE( surge.get() );

   SECTION( "Untuned - so regular tuning" )
   {
      auto f60 = frequencyForNote( surge, 60 );
      auto f72 = frequencyForNote( surge, 72 );
      auto f69 = frequencyForNote( surge, 69 );

      REQUIRE( f60 == Approx( 261.63 ).margin( .1 ) );
      REQUIRE( f72 == Approx( 261.63 * 2 ).margin( .1 ) );
      REQUIRE( f69 == Approx( 440.0 ).margin( .1 ) );
   }

   SECTION( "Straight tuning scl file" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/12-intune.scl" );
      surge->storage.retuneToScale(s);
      auto f60 = frequencyForNote( surge, 60 );
      auto f72 = frequencyForNote( surge, 72 );
      auto f69 = frequencyForNote( surge, 69 );
      
      REQUIRE( f60 == Approx( 261.63 ).margin( .1 ) );
      REQUIRE( f72 == Approx( 261.63 * 2 ).margin( .1 ) );
      REQUIRE( f69 == Approx( 440.0 ).margin( .1 ) );

      auto fPrior = f60;
      auto twoToTwelth = pow( 2.0f, 1.0/12.0 );
      for( int i=61; i<72; ++i )
      {
         auto fNow = frequencyForNote( surge, i );
         REQUIRE( fNow / fPrior == Approx( twoToTwelth ).margin( .0001 ) );
         fPrior = fNow;
      }
   }

   SECTION( "Zeus 22" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/zeus22.scl" );
      surge->storage.retuneToScale(s);
      auto f60 = frequencyForNote( surge, 60 );
      auto fDouble = frequencyForNote( surge, 60 + s.count );
      auto fHalf = frequencyForNote( surge, 60 - s.count );
      
      REQUIRE( f60 == Approx( 261.63 ).margin( .1 ) );
      REQUIRE( fDouble == Approx( 261.63 * 2 ).margin( .1 ) );
      REQUIRE( fHalf == Approx( 261.63 / 2 ).margin( .1 ) );
   }

   SECTION( "6 exact" )
   {
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/6-exact.scl" );
      surge->storage.retuneToScale(s);

      auto f60 = frequencyForNote( surge, 60 );
      auto fDouble = frequencyForNote( surge, 60 + s.count );
      auto fHalf = frequencyForNote( surge, 60 - s.count );
      
      REQUIRE( f60 == Approx( 261.63 ).margin( .1 ) );
      REQUIRE( fDouble == Approx( 261.63 * 2 ).margin( .1 ) );
      REQUIRE( fHalf == Approx( 261.63 / 2 ).margin( .1 ) );
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
   REQUIRE( zeroCrossings > 130 );
   REQUIRE( zeroCrossings < 160 );
   
   if (data)
      delete[] data;
   delete surge;

}

TEST_CASE( "Unison Absolute and Relative", "[osc]" )
{
   auto surge = std::shared_ptr<SurgeSynthesizer>( Surge::Headless::createSurge(44100) );
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
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );

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
   delete surge;
}

TEST_CASE( "All Patches are Loadable", "[patch]" )
{
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge );
   int i=0;
   for( auto p : surge->storage.patch_list )
   {
      // std::cout << i << " " << p.name << " " << p.path.generic_string() << std::endl;
      surge->loadPatch(i);
      ++i;
   }
   
   delete surge;
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



int runAllTests(int argc, char **argv)
{
   int result = Catch::Session().run( argc, argv );
   return result;
}
