#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE("Param String Inversion", "[parm]")
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
            char txt[256];
            p.get_display(txt);
            REQUIRE( p.set_value_from_string( std::string( txt ) ) );
            auto v01 = p.get_value_f01();

            INFO( "Type " << type << " val01=" << val << " val.f=" << preval << " txt=" << txt << " roundtrip=" << p.val.f << " roundtrip01=" << v01 );
            REQUIRE( v01 == Approx( val ).margin( .01 ) );
         }
      }
#endif
    }
}
