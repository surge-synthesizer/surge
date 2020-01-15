#include <memory>
#include "SurgeSynthesizer.h"
#include "Player.h"
#include "catch2.hpp"

namespace Surge
{
namespace Test
{
double frequencyForNote( std::shared_ptr<SurgeSynthesizer> surge, int note,
                         int seconds, bool audioChannel,
                         int midiChannel )
{
   auto events = Surge::Headless::makeHoldNoteFor( note, 44100 * seconds, 64, midiChannel );
   float *buffer;
   int nS, nC;
   Surge::Headless::playAsConfigured( surge, events, &buffer, &nS, &nC );

   REQUIRE( nC == 2 );
   REQUIRE( nS >= 44100 * seconds );
   REQUIRE( nS <= 44100 * seconds + 4 * BLOCK_SIZE );

   // Trim off the leading and trailing
   int nSTrim = (int)(nS / 2 * 0.8);
   int start = (int)( nS / 2 * 0.05 );
   float *leftTrimmed = new float[nSTrim];

   for( int i=0; i<nSTrim; ++i )
      leftTrimmed[i] = buffer[ (i + start) * 2 + audioChannel ];

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



}
}
