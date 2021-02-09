#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE("ID Issuance is Reflexive", "[id]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge);
    SECTION("Valid for all index points")
    {
        for (int i = 0; i < n_total_params + num_metaparameters; ++i)
        {
            auto id = SurgeSynthesizer::ID();
            REQUIRE(surge->fromDAWSideIndex(i, id));
        }
    }
    SECTION("Generate by Any Means")
    {
        std::vector<SurgeSynthesizer::ID> ids;
        for (int i = 0; i < n_total_params + num_metaparameters; ++i)
        {
            auto id = SurgeSynthesizer::ID();
            REQUIRE(surge->fromDAWSideIndex(i, id));
            ids.push_back(id);
        }

        INFO("Checking recretable by dawIndex");
        for (const auto &sid : ids)
        {
            auto id = SurgeSynthesizer::ID();
            REQUIRE(surge->fromDAWSideIndex(sid.getDawSideIndex(), id));
            REQUIRE(sid == id);
        }

        INFO("Checking recretable by dawID");
        for (const auto &sid : ids)
        {
            auto id = SurgeSynthesizer::ID();
            REQUIRE(surge->fromDAWSideId(sid.getDawSideId(), id));
            REQUIRE(sid == id);
        }

        INFO("Checking recretable by syntID");
        for (const auto &sid : ids)
        {
            auto id = SurgeSynthesizer::ID();
            REQUIRE(surge->fromSynthSideId(sid.getSynthSideId(), id));
            REQUIRE(sid == id);
        }
    }
}
