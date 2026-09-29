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

/*
 * This header provides a set of LUA support functions which are used
 * in the various places we deploy lua (formula modulator, waveform
 * generator, int he future mod mappers etc...)
 */

#ifndef SURGE_SRC_COMMON_LUASUPPORT_H
#define SURGE_SRC_COMMON_LUASUPPORT_H

#if HAS_LUA
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#if SURGE_PORTABLE_LUA
#define LUA_OK 0
int luaopen_bit(lua_State *L);
#endif

#include <pffft.h>
}
#else
typedef int lua_State;
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace Surge
{
namespace LuaSupport
{

#if HAS_LUA
inline void openLibraries(lua_State *state)
{
    luaL_openlibs(state);
#if SURGE_PORTABLE_LUA
    lua_pushcfunction(state, luaopen_bit);
    lua_pushstring(state, "bit");
    lua_call(state, 1, 0);
#endif
}
#endif

/*
 * Given a string which is supposed to be valid lua defining a function
 * with a name, parse the string, look for the function, and if the parse
 * and stuff has no errors, return true with the function on the top of the
 * stack, oterhwise return false with nil on top of the stack. So increases
 * stack by 1.
 */

bool parseStringDefiningFunction(lua_State *L, const std::string &definition,
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
int parseStringDefiningMultipleFunctions(lua_State *L, const std::string &definition,
                                         const std::vector<std::string> &functions,
                                         std::string &errorMessage);

// Compile without executing any top-level script code. On success, leave the
// compiled chunk on the stack; on failure, restore the previous stack height.
// The caller owns this Lua state exclusively throughout the operation.
bool compileString(lua_State *L, const std::string &definition, std::string &errorMessage);

// Consume a previously compiled chunk from the top of the stack, execute it,
// and push the requested functions/nils in the same order as the parse helper.
// Compilation and evaluation may be separated by a registry reference, but
// that reference and the chunk must remain in their original Lua state.
int evaluateCompiledFunctions(lua_State *L, const std::vector<std::string> &functions,
                              std::string &errorMessage);

/*
 * Call this function with the top of your stack being a
 * lua_function and the function will get wrapped in the standard
 * surge environment (math imported, most things stripped, add
 * our C++ functions, etc...)
 */
bool setSurgeFunctionEnvironment(lua_State *s, uint64_t features);

/*
 * Call this function with a LUA state, the std::string at Surge::LuaSources and it will load the
 * prelude in the table "surge"
 */
bool loadSurgePrelude(lua_State *s, const std::string &lua_script);

/*
 * Call this function to get a string representation of the Formula prelude
 */
std::string getFormulaPrelude();

/*
 * Call this function to get a string representation of the WTSE prelude
 */
std::string getWTSEPrelude();

/*
 * Flags representing optional features that can be enabled in the Lua sandbox environment
 */
enum EnvironmentFeatures : uint64_t
{
    BASE = 0,
    HAS_FFT = 1 << 1
};

/*
 * Additional enum classes for PFFFT, since we're not using the library on ARM64EC it lacks the
 * included definitions
 */
enum class FFTDirection
{
    Forward,
    Backward
};

enum class FFTTransform
{
    Real,
    Complex
};

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

/*
 * Global table names
 */
static constexpr const char *surgeTableName{"surge"};
static constexpr const char *sharedTableName{"shared"};

} // namespace LuaSupport
} // namespace Surge

#endif // SURGE_SRC_COMMON_LUASUPPORT_H
