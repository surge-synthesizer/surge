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
        if (lerr == LUA_ERRSYNTAX)
        {
            std::ostringstream oss;
            oss << "Lua Syntax Error: " << lua_tostring(L, -1);
            errorMessage = oss.str();
        }
        else
        {
            std::ostringstream oss;
            oss << "Lua Unknown Error: " << lua_tostring(L, -1);
            errorMessage = oss.str();
        }
        lua_pop(L, 1);
        for (auto f : functions)
            lua_pushnil(L);
        return 0;
    }

    lerr = lua_pcall(L, 0, 0, 0);
    if (lerr != LUA_OK)
    {
        // FIXME obviously
        std::ostringstream oss;
        oss << "Lua Evaluation Error: " << lua_tostring(L, -1);
        errorMessage = oss.str();
        lua_pop(L, 1);
        for (auto f : functions)
            lua_pushnil(L);
        return 0;
    }

    // sloppy
    int res = 0;
    std::vector<std::string> frev(functions.rbegin(), functions.rend());
    for (auto functionName : frev)
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

int lua_limitRange(lua_State *L)
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

bool Surge::LuaSupport::setSurgeFunctionEnvironment(lua_State *L)
{
#if HAS_LUA
    if (!lua_isfunction(L, -1))
    {
        return false;
    }

    // Stack is ...>func
    lua_createtable(L, 0, 10);
    // stack is now func > table

    lua_pushstring(L, "math");
    lua_getglobal(L, "math");
    // stack is now func > table > "math" > (math) so set math on table
    lua_settable(L, -3);

    lua_pushstring(L, "surge");
    lua_getglobal(L, "surge");
    // stack is now func > table > "surge" > (surge) so set math on table
    lua_settable(L, -3);

    // Now a list of functions we do include
    std::vector<std::string> functionWhitelist = {"ipairs", "error"};
    for (const auto &f : functionWhitelist)
    {
        lua_pushstring(L, f.c_str());
        lua_getglobal(L, f.c_str());
        lua_settable(L, -3);
    }

    lua_pushstring(L, "limit_range");
    lua_pushcfunction(L, lua_limitRange);
    lua_settable(L, -3);

    lua_pushstring(L, "clamp");
    lua_pushcfunction(L, lua_limitRange);
    lua_settable(L, -3);

    // stack is now func > table again *BUT* now load math in stripped
    lua_getglobal(L, "math");
    lua_pushnil(L);

    // func > table > (math) > nil so lua next -2 will iterate over (math)
    while (lua_next(L, -2))
    {
        // stack is now f>t>(m)>k>v
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        // stack is now f>t>(m)>k>v>k>v and we want k>v in the table
        lua_settable(L, -6); // that -6 reaches back to 2
        // stack is now f>t>(m)>k>v and we want the key on top for next so
        lua_pop(L, 1);
    }
    // when lua_next returns false it has nothing on stack so
    // stack is now f>t>(m). Pop m
    lua_pop(L, 1);

    // and now we are back to f>t so we can setfenv it
    lua_setfenv(L, -2);

#endif

    // And now the stack is back to just the function wrapped
    return true;
}

bool Surge::LuaSupport::loadSurgePrelude(lua_State *s)
{
#if HAS_LUA
    auto guard = SGLD("loadPrologue", s);
    // now load the surge library
    auto &lua_script = LuaSources::surge_prelude;
    auto lua_size = lua_script.size();
    auto load_stat = luaL_loadbuffer(s, lua_script.c_str(), lua_size, lua_script.c_str());
    auto pcall = lua_pcall(s, 0, 1, 0);
    lua_setglobal(s, "surge");
#endif
    return true;
}

std::string Surge::LuaSupport::getSurgePrelude() { return LuaSources::surge_prelude; }

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
        }
#endif
    }
}
