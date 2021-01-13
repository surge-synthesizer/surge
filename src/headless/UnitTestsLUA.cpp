#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

#if HAS_LUAJIT
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"

#include "lj_arch.h"
}

TEST_CASE( "Lua Hello World", "[lua]" )
{
   SECTION( "HW" )
   {
      lua_State *L = lua_open();
      REQUIRE(L);
      luaL_openlibs(L);

      const char lua_script[] = "print('Hello World from LUAJIT!')";
      int load_stat = luaL_loadbuffer(L,lua_script,strlen(lua_script),lua_script);
      lua_pcall(L, 0, 0, 0);

      lua_close(L);
   }

   SECTION( "Math" )
   {
      lua_State * L = lua_open();
      REQUIRE(L);
      luaL_openlibs(L);

      // execute script
      const char lua_script[] = "function addThings(a, b) return a+b; end";
      int load_stat = luaL_loadbuffer(L,lua_script,strlen(lua_script),lua_script);
      lua_pcall(L, 0, 0, 0);

      // load the function from global
      lua_getglobal(L,"addThings");
      if(lua_isfunction(L, -1) )
      {
         // push function arguments into stack
         lua_pushnumber(L, 5.0);
         lua_pushnumber(L, 6.0);
         lua_pcall(L,2,1,0);
         double sumval = 0.0;
         if (!lua_isnil(L, -1))
         {
            sumval = lua_tonumber(L,-1);
            lua_pop(L,1);
         }
         REQUIRE( sumval == 5 + 6 );
      }

      // cleanup
      lua_close(L);
   }

   SECTION( "Sample Mod FUnction" )
   {
      std::string fn = R"FN(
function modfunc(phase)
   if phase < 0.5 then
      return math.sin( phase * 2.0 * 3.14159 )
   else
      return -1
   end
end
)FN";

      auto cEquiv = [](float p )
      {
         if( p < 0.5 ) return sin(p * 2.0 * 3.14159 );
         else return -1.0;
      };

      lua_State * L = lua_open();
      REQUIRE(L);
      luaL_openlibs(L);

      // execute script
      const char *lua_script = fn.c_str();
      int load_stat = luaL_loadbuffer(L,lua_script,strlen(lua_script),lua_script);
      lua_pcall(L, 0, 0, 0);

      // load the function from global
      int ntests = 174;
      for( int i=0; i<ntests; ++i )
      {
         INFO( "evaluating iteration " << i );
         lua_getglobal(L,"modfunc");
         if(lua_isfunction(L, -1) )
         {
            float p = 1.f * i / ntests;
            // push function arguments into stack
            lua_pushnumber(L, p);
            lua_pcall(L, 1, 1, 0);
            double sumval = 0.0;
            if (!lua_isnil(L, -1))
            {
               sumval = lua_tonumber(L, -1);
               lua_pop(L, 1);
            }
            else
            {
               REQUIRE(0);
            }
            INFO( "Evaluating at " << p );
            REQUIRE(sumval == Approx( cEquiv(p)).margin(1e-5));
         }
      }

      // cleanup
      lua_close(L);
   }

}
#endif