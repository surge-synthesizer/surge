#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

#include "FormulaModulationHelper.h"

#if HAS_LUAJIT
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"

#include "lj_arch.h"
}

TEST_CASE("LUA Hello World", "[lua]")
{
    SECTION("HW")
    {
        lua_State *L = lua_open();
        REQUIRE(L);
        luaL_openlibs(L);

        const char lua_script[] = "print('Hello World from LUAJIT!')";
        int load_stat = luaL_loadbuffer(L, lua_script, strlen(lua_script), lua_script);
        lua_pcall(L, 0, 0, 0);

        lua_close(L);
    }
}

TEST_CASE("LUA Sample Operations", "[lua]")
{
    SECTION("Math")
    {
        lua_State *L = lua_open();
        REQUIRE(L);
        luaL_openlibs(L);

        // execute script
        const char lua_script[] = "function addThings(a, b) return a+b; end";
        int load_stat = luaL_loadbuffer(L, lua_script, strlen(lua_script), lua_script);
        lua_pcall(L, 0, 0, 0);

        // load the function from global
        lua_getglobal(L, "addThings");
        if (lua_isfunction(L, -1))
        {
            // push function arguments into stack
            lua_pushnumber(L, 5.0);
            lua_pushnumber(L, 6.0);
            lua_pcall(L, 2, 1, 0);
            double sumval = 0.0;
            if (!lua_isnil(L, -1))
            {
                sumval = lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
            REQUIRE(sumval == 5 + 6);
        }

        // cleanup
        lua_close(L);
    }

    SECTION("Sample Mod FUnction")
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

        auto cEquiv = [](float p) {
            if (p < 0.5)
                return sin(p * 2.0 * 3.14159);
            else
                return -1.0;
        };

        lua_State *L = lua_open();
        REQUIRE(L);
        luaL_openlibs(L);

        // execute script
        const char *lua_script = fn.c_str();
        int load_stat = luaL_loadbuffer(L, lua_script, strlen(lua_script), lua_script);
        lua_pcall(L, 0, 0, 0);

        // load the function from global
        int ntests = 174;
        for (int i = 0; i < ntests; ++i)
        {
            INFO("evaluating iteration " << i);
            lua_getglobal(L, "modfunc");
            if (lua_isfunction(L, -1))
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
                INFO("Evaluating at " << p);
                REQUIRE(sumval == Approx(cEquiv(p)).margin(1e-5));
            }
        }

        // cleanup
        lua_close(L);
    }
}

TEST_CASE("LUA Table API", "[lua]")
{
    SECTION("Push and get Element")
    {
        lua_State *L = lua_open();
        REQUIRE(L);
        luaL_openlibs(L);

        std::string fn = R"FN(
function modfunc(record)
   return record["a"] + record["b"]
end
)FN";
        const char *lua_script = fn.c_str();
        int load_stat = luaL_loadbuffer(L, lua_script, strlen(lua_script), lua_script);
        lua_pcall(L, 0, 0, 0);

        lua_getglobal(L, "modfunc");
        if (lua_isfunction(L, -1))
        {
            lua_createtable(L, 0, 4);

            lua_pushstring(L, "a");
            lua_pushnumber(L, 13);
            lua_settable(L, -3); /* 3rd element from the stack top */

            lua_pushstring(L, "b");
            lua_pushnumber(L, 27);
            lua_settable(L, -3);

            // So now the table is the top of the stack so
            lua_pcall(L, 1, 1, 0);

            if (!lua_isnil(L, -1))
            {
                auto sumval = lua_tonumber(L, -1);
                lua_pop(L, 1);
                REQUIRE(sumval == 40);
            }
            else
            {
                REQUIRE(0);
            }
        }
        else
        {
            bool luaIsFunc = false;
            REQUIRE(luaIsFunc);
        }
    }
}

struct formulaObservation
{
    formulaObservation(int ip, float fp, float va)
    {
        iPhase = ip;
        fPhase = fp;
        phase = ip + fp;
        v = va;
    }
    int iPhase;
    float fPhase;
    float v;
    float phase;
};

std::vector<formulaObservation> runFormula(FormulaModulatorStorage *fs, float dPhase,
                                           float phaseMax, float deform = 0,
                                           float releaseAfter = -1)
{
    auto res = std::vector<formulaObservation>();
    double phase = 0.0;
    int iphase = 0;
    Surge::Formula::EvaluatorState es;
    Surge::Formula::prepareForEvaluation(fs, es, true);
    es.deform = deform;
    while (phase + iphase < phaseMax)
    {
        bool release = false;
        if (releaseAfter >= 0 && phase + iphase >= releaseAfter)
            release = true;
        es.released = release;

        auto r = Surge::Formula::valueAt(iphase, phase, fs, &es);
        res.push_back(formulaObservation(iphase, phase, r));
        phase += dPhase;
        if (phase > 1)
        {
            phase -= 1;
            iphase += 1;
        }
    }
    return res;
}

TEST_CASE("Basic Formula Evaluation", "[formula]")
{
    SECTION("Identity Modulator")
    {
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    modstate["output"] = modstate["phase"]
    return modstate
end)FN");
        auto runIt = runFormula(&fs, 0.0321, 5);
        for (auto c : runIt)
        {
            REQUIRE(c.fPhase == Approx(c.v));
        }
    }

    SECTION("Saw Modulator")
    {
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    modstate["output"] = 2 * modstate["phase"] - 1
    return modstate
end)FN");
        auto runIt = runFormula(&fs, 0.0321, 5);
        for (auto c : runIt)
        {
            REQUIRE(2 * c.fPhase - 1 == Approx(c.v));
        }
    }

    SECTION("Sin Modulator")
    {
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    modstate["output"] = math.sin( modstate["phase"] * 3.14159 * 2 )
    return modstate
end)FN");
        auto runIt = runFormula(&fs, 0.0321, 5);
        for (auto c : runIt)
        {
            REQUIRE(std::sin(c.fPhase * 3.14159 * 2) == Approx(c.v));
        }
    }

    SECTION("Test Deform")
    {
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    p = modstate["phase"]
    d = modstate["deform"]
    r = math.pow( p, 3 * d + 1) * 2 - 1
    modstate["output"] = r
    return modstate
end)FN");

        for (int id = 0; id <= 10; id++)
        {
            float def = id / 10.0;
            float pe = 3 * def + 1;
            auto runIt = runFormula(&fs, 0.0321, 5, def);
            for (auto c : runIt)
            {
                auto q = pow(c.fPhase, pe) * 2 - 1;
                REQUIRE(q == Approx(c.v));
            }
        }
    }
}
#endif
