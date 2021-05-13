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

bool Surge::LuaSupport::setSurgeFunctionEnvironment(lua_State *L)
{
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

    // And now the stack is back to just the function wrapped
    return true;
}

Surge::LuaSupport::SGLD::~SGLD()
{
    if (L)
    {
        auto nt = lua_gettop(L);
        if (nt != top)
        {
            std::cout << "Guarded stack leak: [" << label << "] exit=" << nt << " enter=" << top
                      << std::endl;
            if (label == "valueAt")
                std::cout << "Q" << std::endl;
        }
    }
}