/*
** Surge XT replacement for Lua 5.2.4's linit.c.
**
** Derived from lua-5.2.4/src/linit.c, Copyright (C) 1994-2015 Lua.org, PUC-Rio,
** used under the terms of the MIT license (see the notice at the end of
** lua-5.2.4/src/lua.h).
**
** Two changes from upstream, both so that the set of libraries a formula sees
** matches the set LuaJIT gives it:
**
**   - Lua BitOp's "bit" is opened, because Surge's formula sandbox whitelists it
**     (see sandboxLibraryTables in src/common/LuaSupport.cpp) and LuaJIT has it
**     built in. Doing it here means none of Surge's lua_State creation sites need
**     to know which backend they are talking to.
**
**   - Lua 5.2's own "bit32" is NOT opened, and lbitlib.c is left out of the
**     vendored drop entirely, because LuaJIT has no bit32. Adding it would let a
**     formula written against this backend fail on a LuaJIT build.
**
** Upstream's empty preloadedlibs loop is also dropped; package.preload is created
** by luaopen_package regardless, and Surge preloads nothing.
**
** Upstream's linit.c is deliberately left out of the vendored source drop so that
** this file is the only definition of luaL_openlibs(). See README.md.
*/

#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

/* Lua BitOp's entry point. Not declared in lualib.h, as it is not a PUC Lua
** library. On 5.2 it returns the module table, which is what luaL_requiref
** expects. */
LUALIB_API int luaopen_bit(lua_State *L);

static const luaL_Reg loadedlibs[] = {{"_G", luaopen_base},
                                      {LUA_LOADLIBNAME, luaopen_package},
                                      {LUA_COLIBNAME, luaopen_coroutine},
                                      {LUA_TABLIBNAME, luaopen_table},
                                      {LUA_IOLIBNAME, luaopen_io},
                                      {LUA_OSLIBNAME, luaopen_os},
                                      {LUA_STRLIBNAME, luaopen_string},
                                      {"bit", luaopen_bit},
                                      {LUA_MATHLIBNAME, luaopen_math},
                                      {LUA_DBLIBNAME, luaopen_debug},
                                      {NULL, NULL}};

LUALIB_API void luaL_openlibs(lua_State *L)
{
    const luaL_Reg *lib;

    for (lib = loadedlibs; lib->func; lib++)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); /* remove lib */
    }
}
