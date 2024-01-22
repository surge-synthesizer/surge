/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"

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
                INFO("Subtype is " << fs);
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
        DYNAMIC_SECTION("Test Waveshaper " << sst::waveshapers::wst_names[wt])
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
