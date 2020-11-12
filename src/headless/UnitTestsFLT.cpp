#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "FastMath.h"

using namespace Surge::Test;

TEST_CASE( "Run Every Filter", "[flt]" )
{
   for( int fn = 0; fn < n_fu_type; fn ++ )
   {
      auto nst = std::max( 1, fut_subcount[fn] );
      for( int fs=0; fs<nst; ++fs )
      {
         DYNAMIC_SECTION( "Test Filter " << fut_names[fn] << " st: " <<  fs )
         {
            std::cout << "Testing " << fut_names[fn] << " with subtypes " << fs << std::endl;
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);

            surge->storage.getPatch().scene[0].filterunit[0].type.val.i = fn;
            surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = fs;

            int len = 4410 * 5;
            Surge::Headless::playerEvents_t heldC = Surge::Headless::makeHoldMiddleC(len);
            REQUIRE(heldC.size() == 2);

            float* data = NULL;
            int nSamples, nChannels;

            Surge::Headless::playAsConfigured(surge, heldC, &data, &nSamples, &nChannels);
            REQUIRE(data);
            REQUIRE(std::abs(nSamples - len) <= BLOCK_SIZE);
            REQUIRE(nChannels == 2);

            if (data)
               delete[] data;
         }
      }
   }
}
