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


TEST_CASE( "We can read a collection of wavetables", "[io]" )
{
   /*
   ** ToDo: 
   ** .wt file
   ** oneshot
   ** srgmarkers
   ** etc
   */
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge.get() );
   
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
   
}

TEST_CASE( "All .wt and .wav factory assets load", "[io]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge.get() );
   for( auto p : surge->storage.wt_list )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      wt->size = -1;
      wt->n_tables = -1;
      surge->storage.load_wt(p.path.generic_string(), wt );
      REQUIRE( wt->size > 0 );
      REQUIRE( wt->n_tables > 0 );
   }
}


TEST_CASE( "All Patches are Loadable", "[io]" )
{
   auto surge = Surge::Headless::createSurge(44100);
   REQUIRE( surge.get() );
   int i=0;
   for( auto p : surge->storage.patch_list )
   {
      surge->loadPatch(i);
      ++i;
   }
}

TEST_CASE( "DAW Streaming and Unstreaming", "[io]" )
{
   // The basic plan of attack is, in a section, set up two surges,
   // stream onto data on the first and off of data on the second
   // and voila

   auto fromto = [](std::shared_ptr<SurgeSynthesizer> src,
                    std::shared_ptr<SurgeSynthesizer> dest)
                    {
                       void *d = nullptr;
                       src->populateDawExtraState();
                       auto sz = src->saveRaw( &d );
                       REQUIRE( src->storage.getPatch().dawExtraState.isPopulated );
                       
                       dest->loadRaw( d, sz, false );
                       dest->loadFromDawExtraState();
                       REQUIRE( dest->storage.getPatch().dawExtraState.isPopulated );

                       // Why does this crash macos?
                       // if(d) free(d);
                    };

   SECTION( "MPE Enabled State Saves" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);

      REQUIRE( surgeSrc->mpeEnabled == false );
      REQUIRE( surgeDest->mpeEnabled == false );

      surgeSrc->mpeEnabled = true;
      REQUIRE( surgeDest->mpeEnabled == false );

      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeDest->mpeEnabled == true );

      surgeSrc->mpeEnabled = false;
      REQUIRE( surgeSrc->mpeEnabled == false );
      REQUIRE( surgeDest->mpeEnabled == true );
      
      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeSrc->mpeEnabled == false );
      REQUIRE( surgeDest->mpeEnabled == false );
   }

   SECTION( "MPE Pitch Bend State Saves" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);
      
      // I purposefully use two values here which are not my default
      auto v1 = 54;
      auto v2 = 13;

      // Test from defaulted dest
      surgeSrc->mpePitchBendRange = v2;
      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeDest->mpePitchBendRange == v2 );

      // Test from set dest
      surgeSrc->mpePitchBendRange = v1;
      surgeDest->mpePitchBendRange = v1;
      REQUIRE( surgeSrc->mpePitchBendRange == v1 );
      REQUIRE( surgeDest->mpePitchBendRange == v1 );
      
      surgeSrc->mpePitchBendRange = v2;
      REQUIRE( surgeSrc->mpePitchBendRange == v2 );
      REQUIRE( surgeDest->mpePitchBendRange == v1 );

      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeDest->mpePitchBendRange == v2 );
   }

   SECTION( "SCL State Saves" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);
      Surge::Storage::Scale s = Surge::Storage::readSCLFile("test-data/scl/zeus22.scl" );

      REQUIRE( surgeSrc->storage.isStandardTuning );
      REQUIRE( surgeDest->storage.isStandardTuning );

      surgeSrc->storage.retuneToScale( s );
      REQUIRE( !surgeSrc->storage.isStandardTuning );
      REQUIRE( surgeDest->storage.isStandardTuning );
      REQUIRE( surgeSrc->storage.currentScale.count != surgeDest->storage.currentScale.count );
      REQUIRE( surgeSrc->storage.currentScale.count == s.count );

      fromto( surgeSrc, surgeDest );
      REQUIRE( !surgeSrc->storage.isStandardTuning );
      REQUIRE( !surgeDest->storage.isStandardTuning );

      REQUIRE( surgeSrc->storage.currentScale.count == surgeDest->storage.currentScale.count );
      REQUIRE( surgeSrc->storage.currentScale.count == s.count );

      REQUIRE( surgeSrc->storage.currentScale.rawText == surgeDest->storage.currentScale.rawText );
   }
   
   SECTION( "Save and Restore KBM" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);

      auto k = Surge::Storage::readKBMFile( "test-data/scl/mapping-a440-constant.kbm" );

      REQUIRE( surgeSrc->storage.isStandardMapping );
      REQUIRE( surgeDest->storage.isStandardMapping );

      surgeSrc->storage.remapToKeyboard( k );
      REQUIRE( !surgeSrc->storage.isStandardMapping );
      REQUIRE( surgeDest->storage.isStandardMapping );

      fromto( surgeSrc, surgeDest );
      REQUIRE( !surgeSrc->storage.isStandardMapping );
      REQUIRE( !surgeDest->storage.isStandardMapping );
      REQUIRE( surgeSrc->storage.currentMapping.tuningConstantNote == 69 );
      REQUIRE( surgeDest->storage.currentMapping.tuningConstantNote == 69 );

      REQUIRE( surgeDest->storage.currentMapping.rawText == surgeSrc->storage.currentMapping.rawText );
      
      surgeSrc->storage.remapToStandardKeyboard( );
      REQUIRE( surgeSrc->storage.isStandardMapping );
      REQUIRE( !surgeDest->storage.isStandardMapping );

      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeSrc->storage.isStandardMapping );
      REQUIRE( surgeDest->storage.isStandardMapping );

   }


}
