/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2021 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

/*
 * This header provides a set of LUA support functions which are used
 * in the various places we deply lua (formula modulator, waveform
 * generator, int he future mod mappers etc...)
 */

#ifndef SURGE_XT_LUASUPPORT_H
#define SURGE_XT_LUASUPPORT_H

#include <string>
#include <vector>

#if HAS_LUA
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"

#include "lj_arch.h"
}
#else
typedef int lua_State;
#endif

namespace Surge
{
namespace LuaSupport
{

/*
 * Given a string which is supposed to be valid lua defining a function
 * with a name, parse the string, look for the function, and if the parse
 * and stuff has no errors, return true with the function on the top of the
 * stack, oterhwise return false with nil on top of the stack. So increases
 * stack by 1.
 */

bool parseStringDefiningFunction(lua_State *s, const std::string &definition,
                                 const std::string &functionName, std::string &errorMessage);

/*
 * Given a list of functions and a block of lua code, evaluate the lua code
 * and populate the stack with either nil or the function in order. So
 * if you call with {"foo", "bar", "hootie"} you will end up with foo at the
 * top of the stack, bar next and hootie third.
 *
 * Return an integer which is the number of the functions which were resolved and
 * the number which were nil. If the function returns 0 errorMessage will be populated
 * with something.
 */
int parseStringDefiningMultipleFunctions(lua_State *s, const std::string &definition,
                                         const std::vector<std::string> functions,
                                         std::string &errorMessage);

/*
 * Call this function with the top of your stack being a
 * lua_function and the function will get wrapped in the standard
 * surge environment (math imported, most things stripped, add
 * our C++ functions, etc...)
 */
bool setSurgeFunctionEnvironment(lua_State *s);

/*
 * Call this function with a LUA state and it will introduce the global
 * 'surge' which is the surge prelude
 */
bool loadSurgePrelude(lua_State *s);

/*
 * Call this function to get a string representation of the prelude
 */
std::string getSurgePrelude();

/*
 * A little leak debugger. Make this on your stack and if you exit the
 * block with a different stack than you start, it complains for you
 * with both a print
 */
struct SGLD
{
    SGLD(const std::string &lab, lua_State *L) : label(lab), L(L)
    {
#if HAS_LUA
        if (L)
        {
            top = lua_gettop(L);
        }
#endif
    }
    ~SGLD();

    std::string label;
    lua_State *L;
    int top;
};
} // namespace LuaSupport
} // namespace Surge

#endif // SURGE_XT_LUASUPPORT_H
