#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "FastMath.h"

using namespace Surge::Test;

// These first two are mostly useful targets for valgrind runs
TEST_CASE("Run Every Filter", "[flt]")
{
    for (int fn = 0; fn < sst::filters::num_filter_types; fn++)
    {
        DYNAMIC_SECTION("Test Filter " << sst::filters::filter_type_names[fn])
        {
            auto nst = std::max(1, sst::filters::fut_subcount[fn]);
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);
            for (int fs = 0; fs < nst; ++fs)
            {
                INFO("SUBTYPE IS " << fs);
                surge->storage.getPatch().scene[0].filterunit[0].type.val.i = fn;
                surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = fs;

                int len = BLOCK_SIZE * 5;
                Surge::Headless::playerEvents_t heldC = Surge::Headless::makeHoldMiddleC(len);
                REQUIRE(heldC.size() == 2);

                float *data = NULL;
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

TEST_CASE("Run Every Waveshaper", "[flt]")
{
    for (int wt = 0; wt < (int)sst::waveshapers::WaveshaperType::n_ws_types; wt++)
    {
        DYNAMIC_SECTION("Test WaveShaper " << sst::waveshapers::wst_names[wt])
        {
            auto surge = Surge::Headless::createSurge(44100);
            REQUIRE(surge);

            for (int q = 0; q < n_filter_configs; ++q)
            {
                surge->storage.getPatch().scene[0].wsunit.type.val.i = wt;
                surge->storage.getPatch().scene[0].wsunit.drive.set_value_f01(0.8);

                int len = BLOCK_SIZE * 4;
                Surge::Headless::playerEvents_t heldC = Surge::Headless::makeHoldMiddleC(len);
                REQUIRE(heldC.size() == 2);

                float *data = NULL;
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
