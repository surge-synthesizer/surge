#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

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

void copyScenedataSubset(SurgeStorage *storage, int scene, int start, int end) {
   int s = storage->getPatch().scene_start[scene];
   for(int i=start; i<end; ++i )
   {
      storage->getPatch().scenedata[scene][i-s].i =
         storage->getPatch().param_ptr[i]->val.i;
   }
}

void setupStorageRanges(Parameter *start, Parameter *endIncluding,
                        int &storage_id_start, int &storage_id_end) {
   int min_id = 100000, max_id = -1;
   Parameter *oap = start;
   while( oap <= endIncluding )
   {
      if( oap->id >= 0 )
      {
         if( oap->id > max_id ) max_id = oap->id;
         if( oap->id < min_id ) min_id = oap->id;
      }
      oap++;
   }
   
   storage_id_start = min_id;
   storage_id_end = max_id + 1;
}

TEST_CASE( "ADSR Envelope Behaviour", "[mod]" )
{
   
   std::shared_ptr<SurgeSynthesizer> surge( Surge::Headless::createSurge(44100) );
   REQUIRE( surge.get() );

   /*
   ** OK so lets set up a pretty simple setup 
   */

   auto runAdsr = [surge](float a, float d, float s, float r,
                          int a_s, int d_s, int r_s,
                          bool isAnalog,
                          float releaseAfter, float runUntil )
                     {
                        auto* adsrstorage = &(surge->storage.getPatch().scene[0].adsr[0]);
                        std::shared_ptr<AdsrEnvelope> adsr( new AdsrEnvelope() );
                        adsr->init( &(surge->storage), adsrstorage, surge->storage.getPatch().scenedata[0], nullptr );
                        REQUIRE( adsr.get() );
                        
                        int ids, ide;
                        setupStorageRanges(&(adsrstorage->a), &(adsrstorage->mode), ids, ide );
                        REQUIRE( ide > ids );
                        REQUIRE( ide >= 0 );
                        REQUIRE( ids >= 0 );
                        
                        auto svn = [](Parameter *p, float vn)
                                      {
                                         p->set_value_f01( p->value_to_normalized( limit_range( vn, p->val_min.f, p->val_max.f ) ) );
                                      };
                        
                        auto inverseEnvtime = [](float desiredTime)
                                                 {
                                                    // 2^x = desired time
                                                    auto x = log(desiredTime)/log(2.0);
                                                    return x;
                                                 };
                        
                        svn(&(adsrstorage->a), inverseEnvtime(a));
                        svn(&(adsrstorage->d), inverseEnvtime(d));
                        svn(&(adsrstorage->s), s);
                        svn(&(adsrstorage->r), inverseEnvtime(r));
                        
                        svn(&(adsrstorage->a_s), a_s);
                        svn(&(adsrstorage->d_s), d_s);
                        svn(&(adsrstorage->r_s), r_s);
                        
                        adsrstorage->mode.val.b = isAnalog;
                        
                        copyScenedataSubset(&(surge->storage), 0, ids, ide);
                        adsr->attack();

                        bool released = false;
                        int i = 0;
                        std::vector<std::pair<float,float>> res;
                        res.push_back(std::make_pair(0.f, 0.f));
                        
                        while( true )
                        {
                           auto t = 1.0 * (i+1) * BLOCK_SIZE * dsamplerate_inv;
                           i++;
                           if( t > runUntil || runUntil < 0 )
                              break;
                           
                           if( t > releaseAfter && ! released )
                           {
                              adsr->release();
                              released = true;
                           }
                           
                           adsr->process_block();
                           res.push_back( std::make_pair( (float)t, adsr->output ) );
                           if( false && i > 270 && i < 290 )
                              std::cout << i << " " << t << " " << adsr->output << " " << adsr->getEnvState() 
                                        << std::endl;
                        }
                        return res;
                     };

   auto detectTurnarounds = [](std::vector<std::pair<float,float>> data) {
                               auto pv = -1000.0;
                               int dir = 1;
                               std::vector<std::pair<float,int>> turns;
                               turns.push_back( std::make_pair( 0, 1 ) );
                               for( auto &p : data )
                               {
                                  auto t = p.first;
                                  auto v = p.second;
                                  if( pv >= 0 )
                                  {
                                     int ldir = 0;
                                     if( v > 0.999999f ) ldir = dir; // sometimes we get a double '1'
                                     if( fabs( v - pv ) < 5e-6 && fabs( v ) < 1e-5) ldir = 0; // bouncing off of 0 is annoying
                                     else if( fabs( v - pv ) < 5e-7 ) ldir = 0;
                                     else if( v > pv ) ldir = 1;
                                     else ldir = -1;

                                     if( v != 1 )
                                     {
                                        if( ldir != dir )
                                        {
                                           turns.push_back(std::make_pair(t, ldir) );
                                        }
                                        dir = ldir;
                                     }
                                  }
                                  pv = v;
                               }
                               return turns;
                            };
   
   // With 0 sustain I should decay in decay time
   auto runCompare = [&](float a, float d, float s, float r, int a_s, int d_s, int r_s,  bool isAnalog )
                        {
                           float sustime = 0.1;
                           float endtime = 0.1;
                           float totaltime = a + d + sustime + r + endtime;
                           
                           auto simple = runAdsr( a, d, s, r, a_s, d_s, r_s, isAnalog, a + d + sustime, totaltime );
                           auto sturns = detectTurnarounds(simple);
                           if( false )
                              std::cout << "ADSR: " << a << " " << d << " " << s << " " << r << " switches: " << a_s << " " << d_s << " " << r_s << std::endl;
                           if( s == 0 )
                           {
                              if( sturns.size() != 3 )
                              {
                                 for( auto s : simple )
                                    std::cout << s.first << " " << s.second << std::endl;
                                 for( auto s : sturns )
                                    std::cout << s.first << " " << s.second << std::endl;
                              }
                              REQUIRE( sturns.size() == 3 );
                              REQUIRE( sturns[0].first == 0 );
                              REQUIRE( sturns[0].second == 1 );
                              REQUIRE( sturns[1].first == Approx( a ).margin( 0.01 ) );
                              REQUIRE( sturns[1].second == -1 );
                              REQUIRE( sturns[2].first == Approx( a + d ).margin( 0.01 ) );
                              REQUIRE( sturns[2].second == 0 );
                           }
                           else
                           {
                              if( sturns.size() != 5 )
                              {
                                 for( auto s : simple )
                                    std::cout << s.first << " " << s.second << std::endl;
                                 for( auto s : sturns )
                                    std::cout << s.first << " " << s.second << std::endl;
                              }
                              REQUIRE( sturns.size() == 5 );
                              REQUIRE( sturns[0].first == 0 );
                              REQUIRE( sturns[0].second == 1 );
                              REQUIRE( sturns[1].first == Approx( a ).margin( 0.01 ) );
                              REQUIRE( sturns[1].second == -1 );
                              if( d_s == 0 )
                              {
                                 // this equality only holds in the linear case; in the polynomial case you get faster reach to non-zero sustain
                                 REQUIRE( sturns[2].first == Approx( a + d * ( 1.0 - s ) ).margin( 0.01 ) );
                              }
                              else if( a + d * ( 1.0 - s ) > 0.1 && d > 0.05 )
                              {
                                 REQUIRE( sturns[2].first < a + d * ( 1.0 - s ) + 0.01 );
                              }
                              REQUIRE( sturns[2].second == 0 );
                              REQUIRE( sturns[3].first == Approx( a + d + sustime ).margin( 0.01 ) );
                              REQUIRE( sturns[3].second == -1 );
                              if( r_s == 0 || s > 0.1 && r > 0.05 ) // if we are in the non-linear releases at low sustain we get there early
                              {
                                 REQUIRE( sturns[4].first == Approx( a + d + sustime + r ).margin( ( r_s == 0 ? 0.01 : ( r * 0.1 ) ) ) );
                                 REQUIRE( sturns[4].second == 0 );
                              }
                           }
                        };

   SECTION( "Test the Digital Envelope" )
   {
      for( int as=0;as<3;++as )
         for( int ds=0; ds<3; ++ds )
            for( int rs=0; rs<3; ++rs )
            {
               runCompare( 0.2, 0.3, 0.0, 0.1, as, ds, rs, false );
               runCompare( 0.2, 0.3, 0.5, 0.1, as, ds, rs, false );
               
               for( int rc=0;rc<10; ++rc )
               {
                  auto a = rand() * 1.0 / RAND_MAX;
                  auto d = rand() * 1.0 / RAND_MAX;
                  auto s = 0.8 * rand() * 1.0 / RAND_MAX + 0.1; // we have tested the s=0 case above
                  auto r = rand() * 1.0 / RAND_MAX;
                  runCompare( a, d, s, r, as, ds, rs, false );
               }
            }
   }

   SECTION( "Test the Analog Envelope" )
   {
      // OK so we can't check the same thing here since the turns aren't as tight in analog mode
      // Also the analog ADSR sustains at half the given sustain.
      auto testAnalog = [&](float a, float d, float s, float r )
                           {
                              // std::cout << "ANALOG " << a << " " << d << " " << s << " " << r << std::endl;
                              auto holdFor = a + d + d + 0.5;
                              
                              auto ae = runAdsr( a, d, s, r, 0, 0, 0, true, holdFor, holdFor + 4 * r );
                              auto aturns = detectTurnarounds(ae);

                              float maxt=0, maxv=0;
                              float zerot=0;
                              float valAtRelEnd = -1;
                              std::vector<float> heldPeriod;
                              for( auto obs : ae )
                              {
                                 //std::cout << obs.first << " " << obs.second << std::endl;

                                 if( obs.first > a + d + d * 0.5 && obs.first < holdFor && s > 0.05 ) // that 0.1 lets the delay ring off
                                 {
                                    REQUIRE( obs.second <  s * s * 1.02 );
                                    heldPeriod.push_back(obs.second);
                                 }
                                 if( obs.first > a + d && obs.second < 1e-6 && zerot == 0 )
                                    zerot = obs.first;
                                 if( obs.first > holdFor + r && valAtRelEnd < 0 )
                                    valAtRelEnd = obs.second;
                                 
                                 if( obs.second > maxv )
                                 {
                                    maxv = obs.second;
                                    maxt = obs.first;
                                 }
                              }

                              // In the held period are we mostly constant
                              if( heldPeriod.size() > 10 )
                              {
                                 float sum = 0;
                                 for( auto p : heldPeriod )
                                    sum += p;
                                 float mean = sum / heldPeriod.size();
                                 float var = 0;
                                 for( auto p : heldPeriod )
                                    var += ( p - mean ) * ( p - mean );
                                 var /= ( heldPeriod.size() - 1 );
                                 float stddev = sqrt( var );
                                 REQUIRE( stddev < d * 5e-3 );
                              }
                              REQUIRE( maxt < a );
                              REQUIRE( maxv > 0.99 );
                              if( s > 0.05 )
                              {
                                 REQUIRE( zerot > holdFor + r );
                                 REQUIRE( valAtRelEnd < s * 0.025 );
                              }
                           };

      testAnalog( 0.1, 0.2, 0.5, 0.1 );
      testAnalog( 0.1, 0.2, 0.0, 0.1 );
      for( int rc=0;rc<50; ++rc )
      {
         auto a = rand() * 1.0 / RAND_MAX + 0.01;
         auto d = rand() * 1.0 / RAND_MAX + 0.01;
         auto s = 0.7 * rand() * 1.0 / RAND_MAX + 0.2; // we have tested the s=0 case above
         auto r = rand() * 1.0 / RAND_MAX;
         testAnalog( a, d, s, r);
      }

   }


   /*
   ** This section recreates the somewhat painful SSE code in readable stuff
   */
   SECTION( "Clone the Analog" )
   {
      auto analogClone = [](float a_sec, float d_sec, float s, float r_sec, float releaseAfter, float runUntil )
                            {
                               float a = log(a_sec)/log(2.0);
                               float d = log(d_sec)/log(2.0);
                               float r = log(r_sec)/log(2.0);
                               
                               int i = 0;
                               bool released = false;
                               std::vector<std::pair<float,float>> res;
                               res.push_back(std::make_pair(0.f, 0.f));

                               auto v_c1 = 0.f;
                               auto v_c1_delayed = 0.f;
                               bool discharge = false;
                               const float v_cc = 1.5f;
                               
                               while( true )
                               {
                                  auto t = 1.0 * (i+1) * BLOCK_SIZE * dsamplerate_inv;
                                  i++;
                                  if( t > runUntil || runUntil < 0 )
                                     break;
                           
                                  if( t > releaseAfter && ! released )
                                  {
                                     released = true;
                                  }

                                  auto gate = !released;
                                  auto v_gate = gate ? v_cc : 0.f;

                                  // discharge = _mm_and_ps(_mm_or_ps(_mm_cmpgt_ss(v_c1_delayed, one), discharge), v_gate);
                                  discharge = ( ( v_c1_delayed > 1 ) || discharge ) && gate;
                                  v_c1_delayed = v_c1;

                                  auto S = s * s;

                                  auto v_attack = discharge ? 0 : v_gate;
                                  auto v_decay = discharge ? S : v_cc;
                                  auto v_release = v_gate;

                                  auto diff_v_a = std::max( 0.f, v_attack  - v_c1 );
                                  auto diff_v_d = std::min( 0.f, v_decay   - v_c1 );
                                  auto diff_v_r = std::min( 0.f, v_release - v_c1 );

                                  const float shortest = 6.f;
                                  const float longest = -2.f;
                                  const float coeff_offset = 2.f - log( samplerate / BLOCK_SIZE ) / log( 2.f );

                                  float coef_A = pow( 2.f, std::min( 0.f, coeff_offset - a ) );
                                  float coef_D = pow( 2.f, std::min( 0.f, coeff_offset - d ) );
                                  float coef_R = pow( 2.f, std::min( 0.f, coeff_offset - r ) );

                                  v_c1 = v_c1 + diff_v_a * coef_A;
                                  v_c1 = v_c1 + diff_v_d * coef_D;
                                  v_c1 = v_c1 + diff_v_r * coef_R;
                                     
                           
                                  // adsr->process_block();
                                  auto output = v_c1;
                                  if( !gate && !discharge && v_c1 < 1e-6 )
                                     output = 0;
                                  
                                  res.push_back( std::make_pair( (float)t, output ) );
                               }
                               return res;
                            };

      auto surgeA = runAdsr( 0.1, 0.2, 0.5, 0.1, 0, 0, 0, true, .5, 1.0 );
      auto replA  = analogClone( 0.1, 0.2, 0.5, 0.1, 0.5, 1.0 );

      REQUIRE( surgeA.size() == replA.size() );

      for( auto i=0; i<surgeA.size(); ++i )
      {
         REQUIRE( replA[i].second == Approx( surgeA[i].second ).margin( 1e-6 ) );
      }
   }

}



int runAllTests(int argc, char **argv)
{
   int result = Catch::Session().run( argc, argv );
   return result;
}
