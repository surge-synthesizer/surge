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
#include "UnitConversions.h"

#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE("Param String Inversion", "[param]")
{
    SECTION("Inverting")
    {
        Parameter pq;
        std::vector<int> supportedTypes;
        for (int i = 0; i < num_ctrltypes; ++i)
        {
            pq.set_type(i);
            if (pq.can_setvalue_from_string() && pq.valtype == vt_float)
            {
                supportedTypes.push_back(i);
            }
        }
        REQUIRE(supportedTypes.size() > 0);
#if 0      
      for( auto type : supportedTypes )
      {
         INFO( "Testing " << type );
         for( int i=0; i<25000; ++i )
         {
            Parameter p;
            p.set_type(type);
            REQUIRE( p.can_setvalue_from_string() );
            
            auto val = 1.f * rand() / RAND_MAX;
            p.set_value_f01(val);
            auto preval = p.val.f;
            char txt[TXT_SIZE];
            p.get_display(txt);
            REQUIRE( p.set_value_from_string( std::string( txt, "" ) ) );
            auto v01 = p.get_value_f01();

            INFO( "Type " << type << " val01=" << val << " val.f=" << preval << " txt=" << txt << " roundtrip=" << p.val.f << " roundtrip01=" << v01 );
            REQUIRE( v01 == Approx( val ).margin( .01 ) );
         }
      }
#endif
    }
}

TEST_CASE("Type-in Is Locale Independent", "[param]")
{
    // Users type the decimal separator their keyboard and locale give them.
    // Whichever one it is, the fractional part must survive the parse rather
    // than being silently truncated. See issue #7574.
    SECTION("Both Decimal Separators Parse")
    {
        for (const auto &l : {"de_DE.UTF-8", "pl_PL.UTF-8", "hr_HR.UTF-8", "C"})
        {
            // not every locale is installed on every machine, so skip silently
            try
            {
                std::locale::global(std::locale(l));
            }
            catch (const std::runtime_error &)
            {
                continue;
            }

            INFO("locale " << l);

            REQUIRE(clocalestr_to_double("0.5") == Approx(0.5));
            REQUIRE(clocalestr_to_double("0,5") == Approx(0.5));
            REQUIRE(clocalestr_to_double("-12.75") == Approx(-12.75));
            REQUIRE(clocalestr_to_double("-12,75") == Approx(-12.75));

            // trailing units are ignored, as with atof
            REQUIRE(clocalestr_to_double("3.5 dB") == Approx(3.5));

            bool parsed{true};
            REQUIRE(clocalestr_to_double("not a number", &parsed) == Approx(0.0));
            REQUIRE(!parsed);

            clocalestr_to_double("1.25", &parsed);
            REQUIRE(parsed);
        }

        std::locale::global(std::locale::classic());
    }

    SECTION("Parameter Type-In Keeps The Fraction")
    {
        for (const auto &l : {"de_DE.UTF-8", "pl_PL.UTF-8", "C"})
        {
            try
            {
                std::locale::global(std::locale(l));
            }
            catch (const std::runtime_error &)
            {
                continue;
            }

            INFO("locale " << l);

            Parameter p;
            p.set_type(ct_decibel);

            for (const auto &t : {"-6.5", "-6,5"})
            {
                INFO("typein " << t);
                std::string errMsg;
                REQUIRE(p.set_value_from_string(t, errMsg));
                REQUIRE(p.val.f == Approx(-6.5f).margin(0.001));
            }
        }

        std::locale::global(std::locale::classic());
    }
}
