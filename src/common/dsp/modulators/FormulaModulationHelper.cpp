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

#include "FormulaModulationHelper.h"
#include <thread>

namespace Surge
{
namespace Formula
{
bool prepareForEvaluation(FormulaModulatorStorage *fs, EvaluatorState &s, bool is_display)
{
    static lua_State *audioState = nullptr, *displayState = nullptr;

    if (!is_display)
    {
        static int aid = 1;
        if (audioState == nullptr)
        {
            std::cout << "Creating audioState" << std::endl;
            audioState = lua_open();
            luaL_openlibs(audioState);
        }
        s.L = audioState;
        snprintf(s.funcName, TXT_SIZE, "fm_audio_%u", aid);
        aid++;
        if (aid < 0)
            aid = 1;
    }
    else
    {
        static int did = 1;
        if (displayState == nullptr)
        {
            std::cout << "Creating displayState" << std::endl;
            displayState = lua_open();
            luaL_openlibs(displayState);
        }
        s.L = displayState;
        snprintf(s.funcName, TXT_SIZE, "fm_disp_%u", did);
        did++;
        if (did < 0)
            did = 1;
    }

    snprintf(s.stateName, TXT_SIZE, "%s_state", s.funcName);

    // OK so now evaluate the formula
    const char *lua_script = fs->formula.c_str();
    int load_stat = luaL_loadbuffer(s.L, lua_script, strlen(lua_script), lua_script);
    lua_pcall(s.L, 0, 0, 0); // FIXME error

    // great now get the modfunc
    lua_getglobal(s.L, "process");
    if (lua_isfunction(s.L, -1))
    {
        // Great - rename it
        lua_setglobal(s.L, s.funcName);
        lua_pushnil(s.L);
        lua_setglobal(s.L, "process");

        lua_createtable(s.L, 0, 10);
        lua_setglobal(s.L, s.stateName); // FIXME - we have to clean this up when evaluat is done
    }
    else
    {
        // FIXME error
        std::cout << "ERROR: process is not a function after parsing" << std::endl;
    }

    return true;
}

float valueAt(int phaseIntPart, float phaseFracPart, float deform, FormulaModulatorStorage *fs,
              EvaluatorState *s)
{
    /*
     * So: make the stack my evaluation func then my table; then push my table
     * values; then call my function; then update my global
     */
    lua_getglobal(s->L, s->funcName);
    lua_getglobal(s->L, s->stateName);
    // Stack is now top / table / func so we can update the table
    lua_pushstring(s->L, "intphase");
    lua_pushinteger(s->L, phaseIntPart);
    lua_settable(s->L, -3);

    lua_pushstring(s->L, "phase");
    lua_pushnumber(s->L, phaseFracPart);
    lua_settable(s->L, -3);

    lua_pushstring(s->L, "deform");
    lua_pushnumber(s->L, deform);
    lua_settable(s->L, -3);

    lua_pcall(s->L, 1, 1, 0);

    // Store the value and keep it on top of the stack
    lua_setglobal(s->L, s->stateName);
    lua_getglobal(s->L, s->stateName);
    lua_pushstring(s->L, "output");
    lua_gettable(s->L, -2);
    // top of stack is now the result
    auto res = lua_tonumber(s->L, -1);
    // pop the result and the function
    lua_pop(s->L, 1);
    lua_pop(s->L, 1);

    return res;
}
} // namespace Formula
} // namespace Surge