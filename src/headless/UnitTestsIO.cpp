#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2.hpp"

#include "UnitTestUtilities.h"

#include <unordered_map>

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
      surge->storage.load_wt_wav_portable("test-data/wav/Wavetable.wav", wt );
      REQUIRE( wt->size == 2048 );
      REQUIRE( wt->n_tables == 256 );
      REQUIRE( ( wt->flags & wtf_is_sample ) == 0 );
   }
   
   SECTION( "05_BELL.WAV" )
   {
      auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
      surge->storage.load_wt_wav_portable("test-data/wav/05_BELL.WAV", wt );
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
      surge->storage.load_wt(p.path.generic_string(), wt, &(surge->storage.getPatch().scene[0].osc[0]) );
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

      // A tiny oddity that the surge state pops up if we have tuning patches in the library so
      surge->storage.remapToStandardKeyboard();
      surge->storage.retuneToStandardTuning();
   }
}

TEST_CASE( "DAW Streaming and Unstreaming", "[io][mpe][tun]" )
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
   
   SECTION( "Everything Standard Stays Standard" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);
      REQUIRE( surgeSrc->storage.isStandardTuning );
      REQUIRE( surgeSrc->storage.isStandardMapping );
      fromto(surgeSrc, surgeDest );
      REQUIRE( surgeSrc->storage.isStandardTuning );
      REQUIRE( surgeSrc->storage.isStandardMapping );
      REQUIRE( surgeDest->storage.isStandardTuning );
      REQUIRE( surgeDest->storage.isStandardMapping );
   }

   SECTION( "SCL State Saves" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);
      Tunings::Scale s = Tunings::readSCLFile("test-data/scl/zeus22.scl" );

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

      auto k = Tunings::readKBMFile( "test-data/scl/mapping-a440-constant.kbm" );

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

   SECTION( "Save and Restore Param Midi Controls - Simple" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);

      // Simplest case
      surgeSrc->storage.getPatch().param_ptr[118]->midictrl = 57;
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[118]->midictrl != 57 );

      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[118]->midictrl == 57 );
   }

   SECTION( "Save and Restore Param Midi Controls - Empty" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);

      fromto( surgeSrc, surgeDest );
      for( int i=0; i<n_global_params + n_scene_params; ++i )
      {
         REQUIRE( surgeSrc->storage.getPatch().param_ptr[i]->midictrl ==
                  surgeDest->storage.getPatch().param_ptr[i]->midictrl );
         REQUIRE( surgeSrc->storage.getPatch().param_ptr[i]->midictrl == -1 );
      }
   }

   SECTION( "Save and Restore Param Midi Controls - Multi" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);

      // Bigger Case
      surgeSrc->storage.getPatch().param_ptr[118]->midictrl = 57;
      surgeSrc->storage.getPatch().param_ptr[123]->midictrl = 59;
      surgeSrc->storage.getPatch().param_ptr[172]->midictrl = 82;
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57 );
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[123]->midictrl == 59 );
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[172]->midictrl == 82 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[118]->midictrl != 57 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[123]->midictrl != 59 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[172]->midictrl != 82 );

      fromto( surgeSrc, surgeDest );
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57 );
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[123]->midictrl == 59 );
      REQUIRE( surgeSrc->storage.getPatch().param_ptr[172]->midictrl == 82 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[118]->midictrl == 57 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[123]->midictrl == 59 );
      REQUIRE( surgeDest->storage.getPatch().param_ptr[172]->midictrl == 82 );
   }

   SECTION( "Save and Restore Custom Controllers" )
   {
      auto surgeSrc = Surge::Headless::createSurge(44100);
      auto surgeDest = Surge::Headless::createSurge(44100);

      for( int i=0; i<n_customcontrollers; ++i )
      {
         REQUIRE( surgeSrc->storage.controllers[i] == 41 + i );
         REQUIRE( surgeDest->storage.controllers[i] == 41 + i );
      }

      surgeSrc->storage.controllers[2] = 75;
      surgeSrc->storage.controllers[4] = 79;
      fromto(surgeSrc, surgeDest );
      for( int i=0; i<n_customcontrollers; ++i )
      {
         REQUIRE( surgeSrc->storage.controllers[i] == surgeDest->storage.controllers[i] );
      }
      REQUIRE( surgeDest->storage.controllers[2] == 75 );
      REQUIRE( surgeDest->storage.controllers[4] == 79 );
   }

}

TEST_CASE( "Stream WaveTable Names", "[io]" )
{
   SECTION( "Name Restored for Old Patch" )
   {
      auto surge = Surge::Headless::createSurge(44100);
      REQUIRE( surge );
      REQUIRE( surge->loadPatchByPath( "test-data/patches/Church.fxp", -1, "Test" ) );
      REQUIRE( std::string( surge->storage.getPatch().scene[0].osc[0].wavetable_display_name ) == "(Patch Wavetable)" );
   }

   SECTION( "Name Set when Loading Factory" )
   {
      auto surge = Surge::Headless::createSurge(44100);
      REQUIRE( surge );

      auto patch = &(surge->storage.getPatch());
      patch->scene[0].osc[0].type.val.i = ot_wavetable;

      for( int i=0; i<40; ++i )
      {
         int wti = rand() % surge->storage.wt_list.size();
         INFO( "Loading random wavetable " << wti << " at run " << i );

         surge->storage.load_wt(wti, &patch->scene[0].osc[0].wt, &patch->scene[0].osc[0]);
         REQUIRE( std::string( patch->scene[0].osc[0].wavetable_display_name ) == surge->storage.wt_list[wti].name );
      }
   }

   SECTION( "Name Survives Restore" )
   {
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
      
      auto surgeS = Surge::Headless::createSurge(44100);
      auto surgeD = Surge::Headless::createSurge(44100);
      REQUIRE( surgeD );

      for( int i=0; i<50; ++i )
      {
         auto patch = &(surgeS->storage.getPatch());
         std::vector<bool> iswts;
         std::vector<std::string> names;
         
         for( int s = 0; s < 2; ++s )
            for( int o = 0; o < n_oscs; ++o )
            {
               bool isWT = 1.0 * rand() / RAND_MAX > 0.7;
               iswts.push_back(isWT);

               auto patch = &(surgeS->storage.getPatch());
               if( isWT )
               {
                  patch->scene[s].osc[o].type.val.i = ot_wavetable;
                  int wti = rand() % surgeS->storage.wt_list.size();
                  surgeS->storage.load_wt(wti, &patch->scene[s].osc[o].wt, &patch->scene[s].osc[o]);
                  REQUIRE( std::string( patch->scene[s].osc[o].wavetable_display_name ) == surgeS->storage.wt_list[wti].name );

                  if( 1.0 * rand() / RAND_MAX  > 0.8 )
                  {
                     auto sn = std::string( "renamed blurg " ) + std::to_string(rand());
                     strncpy( patch->scene[s].osc[o].wavetable_display_name, sn.c_str(), 256 );
                     REQUIRE( std::string( patch->scene[s].osc[o].wavetable_display_name ) == sn );
                  }
                  names.push_back( patch->scene[s].osc[o].wavetable_display_name );
               }
               else
               {
                  patch->scene[s].osc[o].type.val.i = ot_sinus;
                  names.push_back( "" );
               }
            }

         fromto(surgeS, surgeD);
         auto patchD = &(surgeD->storage.getPatch());

         int idx = 0;
         for( int s = 0; s < 2; ++s )
            for( int o = 0; o < n_oscs; ++o )
            {
               if( iswts[idx] )
                  REQUIRE( std::string( patchD->scene[s].osc[o].wavetable_display_name ) == names[idx] );
               idx++;
            }
               
      }
   }
}

TEST_CASE( "Load Patches with Embedded KBM" )
{
   std::vector<std::string> patches = {
   };
   SECTION( "Check Restore" )
   {
      {
         auto surge = Surge::Headless::createSurge(44100);
         surge->loadPatchByPath("test-data/patches/HasKBM.fxp",-1, "Test" );
         REQUIRE( !surge->storage.isStandardTuning );
         REQUIRE( !surge->storage.isStandardMapping );
      }

      {
         auto surge = Surge::Headless::createSurge(44100);
         surge->loadPatchByPath("test-data/patches/HasSCL.fxp",-1, "Test" );
         REQUIRE( !surge->storage.isStandardTuning );
         REQUIRE( surge->storage.isStandardMapping );
      }

      {
         auto surge = Surge::Headless::createSurge(44100);
         surge->loadPatchByPath("test-data/patches/HasSCLandKBM.fxp",-1, "Test" );
         REQUIRE( !surge->storage.isStandardTuning );
         REQUIRE( !surge->storage.isStandardMapping );
      }

      {
         auto surge = Surge::Headless::createSurge(44100);
         surge->loadPatchByPath("test-data/patches/HasSCL_165Vintage.fxp",-1, "Test" );
         REQUIRE( !surge->storage.isStandardTuning );
         REQUIRE( surge->storage.isStandardMapping );
      }
   }
}

TEST_CASE( "IDs are Stable", "[io]" )
{
#define GENERATE_ID_TO_STDOUT 0
#if GENERATE_ID_TO_STDOUT
   SECTION( "GENERATE IDS" )
   {
      auto surge = Surge::Headless::createSurge(44100);
      auto patch = &(surge->storage.getPatch());

      int np = patch->param_ptr.size();
      std::cout << "param_ptr_size=" << np << std::endl;
      
      for( int i=0; i<np; ++i )
      {
         std::cout << "name=" << patch->param_ptr[i]->get_storage_name()
                   << "|id=" << patch->param_ptr[i]->id
            // << "|scene=" << patch->param_ptr[i]->scene
                   << std::endl;
      }
   }
#endif

   SECTION( "Compare IDs" )
   {
      std::string idfile = "test-data/param-ids-1.6.5.txt";
      INFO( "Comparing current surge with " << idfile );

      // Step one: Read the file
      std::ifstream fp(idfile.c_str());
      std::string line;

      auto splitOnPipeEquals = [](std::string s)
                                  {
                                     std::vector<std::string> bars;
                                     size_t barpos;
                                     std::unordered_map<std::string,std::string> res;
                                     while( (barpos = s.find( "|" )) != std::string::npos )
                                     {
                                        auto f = s.substr(0,barpos);
                                        s = s.substr(barpos+1);
                                        bars.push_back(f);
                                     }
                                     bars.push_back(s);
                                     for( auto b : bars )
                                     {
                                        auto eqpos = b.find( "=" );
                                        REQUIRE( eqpos != std::string::npos );
                                        auto n = b.substr(0,eqpos);
                                        auto v = b.substr(eqpos + 1 );
                                        res[n] = v;
                                     }
                                     return res;
                                  };

      std::unordered_map<std::string, int> nameIdMap;
      while( std::getline( fp, line ) )
      {
         if( line.find("name=",0) == 0 )
         {
            auto m = splitOnPipeEquals(line);
            REQUIRE( m.find( "name" ) != m.end() );
            REQUIRE( m.find( "id" ) != m.end() );
            auto n = m["name"];
            nameIdMap[n] = std::atoi(m["id"].c_str());
         }
      }
      
      auto surge = Surge::Headless::createSurge(44100);
      auto patch = &(surge->storage.getPatch());

      int np = patch->param_ptr.size();
      for( int i=0; i<np; ++i )
      {
         std::string sn = patch->param_ptr[i]->get_storage_name();
         int id = patch->param_ptr[i]->id;
         INFO( "Comparing " << sn << " with id " << id );
         REQUIRE( nameIdMap.find( sn ) != nameIdMap.end() );
         REQUIRE( nameIdMap[sn] == id );
      }
   }
}
