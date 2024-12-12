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

#include "LuaSupport.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include "basic_dsp.h"
#if HAS_JUCE
#include "SurgeSharedBinary.h"
#endif
#include "lua/LuaSources.h"

bool Surge::LuaSupport::parseStringDefiningFunction(lua_State *L, const std::string &definition,
                                                    const std::string &functionName,
                                                    std::string &errorMessage)
{
    auto res = parseStringDefiningMultipleFunctions(L, definition, {functionName}, errorMessage);
    if (res != 1)
        return false;
    return true;
}

int Surge::LuaSupport::parseStringDefiningMultipleFunctions(
    lua_State *L, const std::string &definition, const std::vector<std::string> functions,
    std::string &errorMessage)
{
#if HAS_LUA
    const char *lua_script = definition.c_str();
    auto lerr = luaL_loadbuffer(L, lua_script, strlen(lua_script), "lua-script");
    if (lerr != LUA_OK)
    {
        std::ostringstream oss;
        switch (lerr)
        {
        case LUA_ERRSYNTAX:
            oss << "Lua syntax error: ";
            break;
        case LUA_ERRMEM:
            oss << "Lua memory allocation error: ";
            break;
        default:
            // The default case should never get called unless the underlying Lua library source
            // gets modified, but we can handle it anyway
            oss << "Lua unknown error: ";
            break;
        }
        oss << lua_tostring(L, -1);
        errorMessage = oss.str();
        lua_pop(L, 1);
        for (const auto &f : functions)
            lua_pushnil(L);
        return 0;
    }

    lerr = lua_pcall(L, 0, 0, 0);
    if (lerr != LUA_OK)
    {
        std::ostringstream oss;
        switch (lerr)
        {
        case LUA_ERRRUN:
            oss << "Lua evaluation error: ";
            break;
        case LUA_ERRMEM:
            oss << "Lua memory allocation error: ";
            break;
        case LUA_ERRERR:
            // We're running pcall without an error function now but we might in the future
            oss << "Lua error handler function error: ";
            break;
        default:
            oss << "Lua unknown error: ";
            break;
        }
        oss << lua_tostring(L, -1);
        errorMessage = oss.str();
        lua_pop(L, 1);
        for (const auto &f : functions)
            lua_pushnil(L);
        return 0;
    }

    // sloppy
    int res = 0;
    std::vector<std::string> frev(functions.rbegin(), functions.rend());
    for (const auto &functionName : frev)
    {
        lua_getglobal(L, functionName.c_str());
        if (lua_isfunction(L, -1))
            res++;
        else if (!lua_isnil(L, -1))
        {
            // Replace whatever is there with a nil
            lua_pop(L, 1);
            lua_pushnil(L);
        }
    }

    return res;
#else
    return 0;
#endif
}

static int lua_limitRange(lua_State *L)
{
#if HAS_LUA
    auto x = luaL_checknumber(L, -3);
    auto low = luaL_checknumber(L, -2);
    auto high = luaL_checknumber(L, -1);
    auto res = limit_range(x, low, high);
    lua_pushnumber(L, res);
#endif
    return 1;
}

// custom print that outputs limited amount of arguments and restricts use to strings and numbers
static int lua_sandboxPrint(lua_State *L)
{
#if HAS_LUA
    int n = lua_gettop(L); // number of arguments
    if (n > 20)
        n = 20;
    for (int i = 1; i <= n; i++)
    {
        if (!lua_isstring(L, i))
            return luaL_error(L, "Error: print() only accepts strings or numbers!");
        const char *s = lua_tostring(L, i); // get the string
        fputs(s, stdout);                   // print the string
    }
    fputs("\n", stdout);
#endif
    return 0;
}

bool Surge::LuaSupport::setSurgeFunctionEnvironment(lua_State *L)
{
#if HAS_LUA
    if (!lua_isfunction(L, -1))
    {
        return false;
    }

    lua_createtable(L, 0, 40); // stack: ... > function > table
    int eidx = lua_gettop(L);  // environment table index

    // add custom functions
    lua_pushcfunction(L, lua_limitRange);
    lua_setfield(L, eidx, "limit_range");
    lua_pushcfunction(L, lua_limitRange);
    lua_setfield(L, eidx, "clamp");
    lua_pushcfunction(L, lua_sandboxPrint);
    lua_setfield(L, eidx, "print");

    // add global tables
    lua_getglobal(L, surgeTableName);
    lua_setfield(L, eidx, surgeTableName);
    lua_getglobal(L, sharedTableName);
    lua_setfield(L, eidx, sharedTableName);

    // add whitelisted functions and modules

    // clang-format off
    static constexpr std::initializer_list<const char *> sandboxWhitelist
                                                 {"pairs",    "ipairs",       "unpack",
                                                 "next",     "type",         "tostring",
                                                 "tonumber", "setmetatable", "pcall",
                                                 "xpcall", "error"};
    // clang-format on

    for (const auto &f : sandboxWhitelist)
    {
        lua_getglobal(L, f);  // stack: f>t>f
        if (lua_isnil(L, -1)) // check if the global exists
        {
            lua_pop(L, 1);
            std::cout << "Error: Global not found! [ " << f << " ]" << std::endl;
            continue;
        }
        lua_setfield(L, -2, f); // stack: f>t
    }

    // add library tables
    // clang-format off
    static constexpr std::initializer_list<const char *> sandboxLibraryTables = {"math", "string", "table", "bit"};
    // clang-format on
    for (const auto &t : sandboxLibraryTables)
    {
        lua_getglobal(L, t); // stack: f>t>(t)
        int gidx = lua_gettop(L);
        if (!lua_istable(L, gidx))
        {
            lua_pop(L, 1);
            std::cout << "Error: Not a table! [ " << t << " ]" << std::endl;
            continue;
        }

        // we want to add to a local table so the entries in the global table can't be overwritten
        lua_createtable(L, 0, 10); // stack: f>t>(t)>t
        lua_setfield(L, eidx, t);
        lua_getfield(L, eidx, t);
        int lidx = lua_gettop(L);

        lua_pushnil(L);
        while (lua_next(L, gidx))
        {
            // stack: f>t>(t)>t>k>v
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            // stack is now f>t>(t)>t>k>v>k>v and we want k>v in the local library table
            lua_settable(L, lidx);
            // stack is now f>t>(t)>t>k>v and we want the key on top for next so
            lua_pop(L, 1);
        }
        // when next returns false it has nothing on stack so stack is now f>t>(t)>t
        lua_pop(L, 2); // pop global and local tables and stack is back to f>t
    }

    // we want to also load in the math functions stripped so go over math again
    lua_getglobal(L, "math"); // stack: f>t>(m)

    lua_pushnil(L);
    // func > table > (math) > nil so lua next -2 will iterate over (math)
    while (lua_next(L, -2))
    {
        // stack: f>t>(m)>k>v
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        // stack is now f>t>(m)>k>v>k>v and we want k>v added to the environment table
        lua_settable(L, eidx);
        lua_pop(L, 1);
    }
    lua_pop(L, 1); // pop global math table and stack is back to f>t

    // retrieve shared table and set entries to nil
    lua_getglobal(L, "shared");
    if (lua_istable(L, -1))
    {
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
            lua_pop(L, 1);        // pop value
            lua_pushvalue(L, -1); // duplicate the key
            lua_pushnil(L);
            lua_settable(L, -4); // clear the key
        }
    }
    // pop the retrieved value (either table or nil) from the stack
    lua_pop(L, 1);

    // and now we are back to f>t so we can setfenv it
    lua_setfenv(L, -2);

#endif

    // And now the stack is back to just the function wrapped
    return true;
}

bool Surge::LuaSupport::loadSurgePrelude(lua_State *L, const std::string &lua_script)
{
#if HAS_LUA
    auto guard = SGLD("loadSurgePrelude", L);
    // Load the specified Lua script into the global table "surge"
    auto lua_size = lua_script.size();
    auto status = luaL_loadbuffer(L, lua_script.c_str(), lua_size, lua_script.c_str());
    if (status != 0)
    {
        std::cout << "Error: Failed to load Lua file! [ " << lua_script.c_str() << " ]"
                  << std::endl;
        return false;
    }
    auto pcall = lua_pcall(L, 0, 1, 0);
    if (pcall != 0)
    {
        std::cout << "Error: Failed to run Lua file! [ " << lua_script.c_str() << " ]" << std::endl;
        return false;
    }
    lua_setglobal(L, surgeTableName);
#endif
    return true;
}

std::string Surge::LuaSupport::getFormulaPrelude() { return LuaSources::formula_prelude; }

std::string Surge::LuaSupport::getWTSEPrelude() { return LuaSources::wtse_prelude; }

Surge::LuaSupport::SGLD::~SGLD()
{
    if (L)
    {
#if HAS_LUA
        auto nt = lua_gettop(L);
        if (nt != top)
        {
            std::cout << "Guarded stack leak: [" << label << "] exit=" << nt << " enter=" << top
                      << std::endl;
            for (int i = nt; i >= top; i--)
            {
                std::cout << "  " << i << " -> " << lua_typename(L, lua_type(L, i)) << std::endl;
            }
        }
#endif
    }
}
