#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

#include "FormulaModulationHelper.h"
#include "WavetableScriptEvaluator.h"
#include "LuaSupport.h"

#include "SurgeSharedBinary.h"

#include "lua/LuaSources.h"

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

TEST_CASE("Surge Prelude", "[lua]")
{
    SECTION("Test the Prelude")
    {
        lua_State *L = lua_open();
        REQUIRE(L);
        luaL_openlibs(L);

        REQUIRE(Surge::LuaSupport::loadSurgePrelude(L));

        std::string emsg;
        REQUIRE(Surge::LuaSupport::parseStringDefiningFunction(
            L, Surge::LuaSources::surge_prelude_test, "test", emsg));
        Surge::LuaSupport::setSurgeFunctionEnvironment(L);

        auto pcall = lua_pcall(L, 0, 1, 0);
        if (pcall != 0)
        {
            std::cout << "LUA Error[" << pcall << "] " << lua_tostring(L, -1) << std::endl;
            INFO("LUA Error " << pcall << " " << lua_tostring(L, -1));
        }

        REQUIRE(lua_isnumber(L, -1));
        REQUIRE(lua_tonumber(L, -1) == 1);

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

TEST_CASE("Parse to Function", "[lua]")
{
    SECTION("Simple Success")
    {
        auto fn = R"FN(
function plus_one(x)
    return x + 1
end
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        bool res = Surge::LuaSupport::parseStringDefiningFunction(L, fn, "plus_one", err);
        REQUIRE(res);
        REQUIRE(lua_gettop(L) == 1);

        // OK so the top of the stack should be my function. Lets call it
        lua_pushnumber(L, 13);
        lua_pcall(L, 1, 1, 0);
        REQUIRE(lua_isnumber(L, -1));
        REQUIRE(lua_tonumber(L, -1) == 14);
        REQUIRE(lua_gettop(L) == 1);
        lua_pop(L, 1);
        REQUIRE(lua_gettop(L) == 0);
        lua_close(L);
    }
    SECTION("Isnt a Function")
    {
        auto fn = R"FN(
shazbot = 13
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        bool res = Surge::LuaSupport::parseStringDefiningFunction(L, fn, "shazbot", err);
        REQUIRE(!res);
        REQUIRE(lua_gettop(L) == 1);
        REQUIRE(lua_isnil(L, -1));
        lua_pop(L, 1);
        lua_close(L);
    }

    SECTION("Isnt Found")
    {
        auto fn = R"FN(
function double(x)
    return x * 2
end
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        bool res = Surge::LuaSupport::parseStringDefiningFunction(L, fn, "triple", err);
        REQUIRE(!res);
        REQUIRE(lua_gettop(L) == 1);
        REQUIRE(lua_isnil(L, -1));
        lua_pop(L, 1);
        lua_close(L);
    }

    SECTION("Doesnt Parse")
    {
        auto fn = R"FN(
function double(x)
    if x > 0 then
        return 2
    else
end
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        bool res = Surge::LuaSupport::parseStringDefiningFunction(L, fn, "triple", err);
        REQUIRE(!res);
        REQUIRE(lua_gettop(L) == 1);
        REQUIRE(lua_isnil(L, -1));
        REQUIRE(err == "Lua Syntax Error: [string \"lua-script\"]:7: 'end' expected (to close "
                       "'function' at line 2) near '<eof>'");
        lua_pop(L, 1);
        lua_close(L);
    }

    SECTION("Doesnt Evaluate")
    {
        auto fn = R"FN(
-- A bit of a contrived case but that's OK
error("I will parse but will not run")
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        bool res = Surge::LuaSupport::parseStringDefiningFunction(L, fn, "triple", err);
        REQUIRE(!res);
        REQUIRE(lua_gettop(L) == 1);
        REQUIRE(lua_isnil(L, -1));
        REQUIRE(err ==
                "Lua Evaluation Error: [string \"lua-script\"]:3: I will parse but will not run");
        lua_pop(L, 1);
        lua_close(L);
    }
}

TEST_CASE("Parse Multiple Functions", "[lua]")
{
    SECTION("A Pair")
    {
        auto fn = R"FN(
function plus_one(x)
    return x + 1
end

function plus_two(x)
    return x + 2
end
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        auto res = Surge::LuaSupport::parseStringDefiningMultipleFunctions(
            L, fn, {"plus_one", "plus_two"}, err);
        REQUIRE(res == 2);
        REQUIRE(lua_gettop(L) == 2);

        // OK so the first function should be +1 so
        lua_pushnumber(L, 4);
        lua_pcall(L, 1, 1, 0);
        REQUIRE(lua_isnumber(L, -1));
        REQUIRE(lua_tonumber(L, -1) == 5);
        lua_pop(L, 1);

        // And the second one is +2 so
        lua_pushnumber(L, 17);
        lua_pcall(L, 1, 1, 0);
        REQUIRE(lua_isnumber(L, -1));
        REQUIRE(lua_tonumber(L, -1) == 19);
    }

    SECTION("Just Two of Three There")
    {
        auto fn = R"FN(
function plus_one(x)
    return x + 1
end

function plus_two(x)
    return x + 2
end
)FN";
        lua_State *L = lua_open();
        REQUIRE(L);
        REQUIRE(lua_gettop(L) == 0);
        luaL_openlibs(L);

        std::string err;
        auto res = Surge::LuaSupport::parseStringDefiningMultipleFunctions(
            L, fn, {"plus_one", "plus_sventeen", "plus_two"}, err);
        REQUIRE(res == 2);
        REQUIRE(lua_gettop(L) == 3);

        REQUIRE(lua_isfunction(L, -1));
        REQUIRE(lua_isnil(L, -2));
        REQUIRE(lua_isfunction(L, -3));
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
    float vVec[Surge::Formula::max_formula_outputs];
    float phase;
};

std::vector<formulaObservation> runFormula(SurgeStorage *storage, FormulaModulatorStorage *fs,
                                           float dPhase, float phaseMax, float deform = 0,
                                           float releaseAfter = -1)
{
    auto res = std::vector<formulaObservation>();
    double phase = 0.0;
    int iphase = 0;
    Surge::Formula::EvaluatorState es;
    Surge::Formula::prepareForEvaluation(storage, fs, es, true);
    es.deform = deform;
    while (phase + iphase < phaseMax)
    {
        bool release = false;
        if (releaseAfter >= 0 && phase + iphase >= releaseAfter)
            release = true;
        es.released = release;

        float r[Surge::Formula::max_formula_outputs];
        Surge::Formula::valueAt(iphase, phase, storage, fs, &es, r);
        res.push_back(formulaObservation(iphase, phase, r[0]));
        for (int i = 0; i < Surge::Formula::max_formula_outputs; ++i)
            res.back().vVec[i] = r[i];

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
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    modstate["output"] = modstate["phase"]
    return modstate
end)FN");
        auto runIt = runFormula(&storage, &fs, 0.0321, 5);
        for (auto c : runIt)
        {
            REQUIRE(c.fPhase == Approx(c.v));
        }
    }

    SECTION("Saw Modulator")
    {
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    modstate["output"] = 2 * modstate["phase"] - 1
    return modstate
end)FN");
        auto runIt = runFormula(&storage, &fs, 0.0321, 5);
        for (auto c : runIt)
        {
            REQUIRE(2 * c.fPhase - 1 == Approx(c.v));
        }
    }

    SECTION("Sin Modulator")
    {
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    modstate["output"] = math.sin( modstate["phase"] * 3.14159 * 2 )
    return modstate
end)FN");
        auto runIt = runFormula(&storage, &fs, 0.0321, 5);
        for (auto c : runIt)
        {
            REQUIRE(std::sin(c.fPhase * 3.14159 * 2) == Approx(c.v));
        }
    }

    SECTION("Test Deform")
    {
        SurgeStorage storage;
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
            auto runIt = runFormula(&storage, &fs, 0.0321, 5, def);
            for (auto c : runIt)
            {
                auto q = pow(c.fPhase, pe) * 2 - 1;
                REQUIRE(q == Approx(c.v));
            }
        }
    }

    SECTION("Vector Output")
    {
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    -- a bipolar saw
    p = modstate["phase"]
    r = { }
    r[1] = p < 0.5 and 1 or -1
    r[2] = p < 0.5 and -1 or 1
    r[3] = 0.72;

    modstate["output"] = r
    return modstate
end)FN");

        for (int id = 0; id <= 10; id++)
        {
            float pe = 0;
            auto runIt = runFormula(&storage, &fs, 0.0321, 5, 0.f);
            for (auto c : runIt)
            {
                auto p = c.fPhase;
                auto s1 = p < 0.5 ? 1.f : -1.f;
                auto s2 = p < 0.5 ? -1.f : 1.f;
                REQUIRE(c.vVec[0] == Approx(s1));
                REQUIRE(c.vVec[1] == Approx(s2));
                REQUIRE(c.vVec[2] == Approx(0.72));
            }
        }
    }
}

TEST_CASE("Init Functions", "[formula]")
{
    SECTION("Test Init Function")
    {
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function init(modstate)
   modstate["av"] = 0.762
   return modstate
end

function process(modstate)
    modstate["output"] = modstate["av"]
    return modstate
end)FN");

        for (int id = 0; id <= 10; id++)
        {
            auto runIt = runFormula(&storage, &fs, 0.0321, 5, 0);
            REQUIRE(!runIt.empty());
            for (auto c : runIt)
            {
                REQUIRE(0.762 == Approx(c.v));
            }
        }
    }
}

TEST_CASE("Clamping", "[formula]")
{
    SECTION("Test Clamped Function")
    {
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function process(modstate)
    modstate["output"] = modstate["phase"] * 3 - 1.5
    return modstate
end)FN");

        for (int id = 0; id <= 10; id++)
        {
            auto runIt = runFormula(&storage, &fs, 0.0321, 5, 0);
            REQUIRE(!runIt.empty());
            for (auto c : runIt)
            {
                REQUIRE(c.v <= 1.0);
                REQUIRE(c.v >= -1.0);
            }
        }
    }
    SECTION("Test Clamped Function")
    {
        SurgeStorage storage;
        FormulaModulatorStorage fs;
        fs.setFormula(R"FN(
function init(modstate)
    modstate["clamp_output"] = false
    return modstate
end

function process(modstate)
    modstate["output"] = modstate["phase"] * 3 - 1.5
    return modstate
end)FN");

        int outOfBounds = 0;
        for (int id = 0; id <= 10; id++)
        {
            auto runIt = runFormula(&storage, &fs, 0.0321, 5, 0);
            REQUIRE(!runIt.empty());
            for (auto c : runIt)
            {
                if (c.v > 1 || c.v < -1)
                    outOfBounds++;
                REQUIRE(c.v <= 1.5);
                REQUIRE(c.v >= -1.5);
            }
        }
        REQUIRE(outOfBounds > 0);
    }
}
TEST_CASE("WavetableScript", "[formula]")
{
    SECTION("Just the Sins")
    {
        const std::string s = R"FN(
function generate(config)
    res = config.xs
    for i,x in ipairs(config.xs) do
        res[i] = math.sin(x * (config.n+1) * 2 * math.pi)
    end
    return res
end
        )FN";
        for (int fno = 0; fno < 4; ++fno)
        {
            auto fr = Surge::WavetableScript::evaluateScriptAtFrame(s, 512, fno, 4);
            REQUIRE(fr.size() == 512);
            auto dp = 1.0 / (512 - 1);
            for (int i = 0; i < 512; ++i)
            {
                auto x = i * dp;
                auto r = sin(x * (fno + 1) * 2 * M_PI);
                REQUIRE(r == Approx(fr[i]));
            }
        }
    }
}

TEST_CASE("Simple Used Formula Modulator", "[formula]")
{
    SECTION("Run Formula on Voice and Scene")
    {
        auto surge = Surge::Test::surgeOnSine();
        surge->storage.getPatch().scene[0].lfo[0].shape.val.i = lt_formula;
        auto pitchId = surge->storage.getPatch().scene[0].osc[0].pitch.id;
        surge->setModDepth01(pitchId, ms_lfo1, 0, 0, 0.1);

        surge->storage.getPatch().formulamods[0][0].setFormula(R"FN(
function init(modstate)
   modstate["depth"] = 0.2
   modstate["count"] = -1   -- There is an initial attack which will also run process once
   return modstate
end

function process(modstate)
    modstate["output"] = (modstate["phase"] * 2 - 1) * modstate["depth" ]
    modstate["count"] = modstate["count"] + 1
    return modstate
end)FN");
        for (int i = 0; i < 10; ++i)
            surge->process();

        surge->playNote(0, 60, 100, 0);
        for (int i = 0; i < 10; ++i)
            surge->process();

        REQUIRE(!surge->voices[0].empty());
        auto ms = surge->voices[0].front()->modsources[ms_lfo1];
        REQUIRE(ms);
        auto lms = dynamic_cast<LFOModulationSource *>(ms);
        REQUIRE(lms);

        auto c = Surge::Formula::extractModStateKeyForTesting("count", lms->formulastate);
        auto ival = std::get_if<float>(&c);
        REQUIRE(ival);
        REQUIRE(*ival == 10);
    }
}

TEST_CASE("Voice Features and Flags", "[formula]")
{
    SECTION("IsVoice is set correctly")
    {
        auto surge = Surge::Test::surgeOnSine();
        surge->storage.getPatch().scene[0].lfo[0].shape.val.i = lt_formula;
        surge->storage.getPatch().scene[0].lfo[6].shape.val.i = lt_formula;
        auto pitchId = surge->storage.getPatch().scene[0].osc[0].pitch.id;
        surge->setModDepth01(pitchId, ms_lfo1, 0, 0, 0.1);
        surge->setModDepth01(pitchId, ms_slfo1, 0, 0, 0.1);

        surge->storage.getPatch().formulamods[0][0].setFormula(R"FN(
function init(modstate)
   modstate["subscriptions"]["voice"] = true
   return modstate
end

function process(modstate)
    modstate["output"] = (modstate["phase"] * 2 - 1)
    return modstate
end)FN");
        for (int i = 0; i < 10; ++i)
            surge->process();

        surge->playNote(0, 60, 100, 0);
        for (int i = 0; i < 10; ++i)
            surge->process();

        REQUIRE(!surge->voices[0].empty());
        auto ms = surge->voices[0].front()->modsources[ms_lfo1];
        REQUIRE(ms);
        auto lms = dynamic_cast<LFOModulationSource *>(ms);
        REQUIRE(lms);

        auto c = Surge::Formula::extractModStateKeyForTesting("is_voice", lms->formulastate);
        auto ival = std::get_if<float>(&c);
        REQUIRE(ival);
        REQUIRE(*ival == 1);

        auto sms = dynamic_cast<LFOModulationSource *>(
            surge->storage.getPatch().scene[0].modsources[ms_slfo1]);
        REQUIRE(sms);

        auto cs = Surge::Formula::extractModStateKeyForTesting("is_voice", sms->formulastate);
        auto sval = std::get_if<float>(&cs);
        REQUIRE(!sval); // a change - we don't even set is_voice if not voice subscribed
    }

    SECTION("Gate and Channel")
    {
        auto surge = Surge::Test::surgeOnSine();
        surge->storage.getPatch().scene[0].lfo[0].shape.val.i = lt_formula;
        surge->storage.getPatch().scene[0].lfo[6].shape.val.i = lt_formula;
        surge->storage.getPatch().scene[0].adsr[0].r.val.f = 2;
        auto pitchId = surge->storage.getPatch().scene[0].osc[0].pitch.id;
        surge->setModDepth01(pitchId, ms_lfo1, 0, 0, 0.1);
        surge->setModDepth01(pitchId, ms_slfo1, 0, 0, 0.1);

        surge->storage.getPatch().formulamods[0][0].setFormula(R"FN(
function init(modstate)
   modstate["subscriptions"]["voice"] = true
   return modstate
end

function process(modstate)
    modstate["output"] = (modstate["phase"] * 2 - 1)
    return modstate
end)FN");

        for (int i = 0; i < 10; ++i)
            surge->process();

        surge->playNote(0, 87, 92, 0);
        for (int i = 0; i < 20; ++i)
            surge->process();

        REQUIRE(!surge->voices[0].empty());
        auto ms = surge->voices[0].front()->modsources[ms_lfo1];
        REQUIRE(ms);
        auto lms = dynamic_cast<LFOModulationSource *>(ms);
        REQUIRE(lms);
        auto sms = dynamic_cast<LFOModulationSource *>(
            surge->storage.getPatch().scene[0].modsources[ms_slfo1]);
        REQUIRE(sms);

        auto check = [](const std::string &key, float val, LFOModulationSource *ms) {
            auto c = Surge::Formula::extractModStateKeyForTesting(key, ms->formulastate);
            auto ival = std::get_if<float>(&c);
            INFO("Confirming that " << key << " is " << val);
            REQUIRE(ival);
            REQUIRE(*ival == val);
        };
        auto checkMissing = [](const std::string &key, LFOModulationSource *ms) {
            auto c = Surge::Formula::extractModStateKeyForTesting(key, ms->formulastate);
            auto ival = std::get_if<float>(&c);
            INFO("Confirming that " << key << " is missing");
            REQUIRE(!ival);
        };
        check("is_voice", 1, lms);
        checkMissing("is_voice", sms);
        check("released", 0, lms);
        check("key", 87, lms);
        checkMissing("key", sms);
        check("channel", 0, lms);
        checkMissing("channel", sms);
        check("velocity", 92, lms);
        checkMissing("velocity", sms);

        surge->releaseNote(0, 87, 100);
        for (int i = 0; i < 20; ++i)
            surge->process();

        check("is_voice", 1, lms);
        checkMissing("is_voice", sms);
        check("released", 1, lms);
        check("key", 87, lms);
    }
}

TEST_CASE("Macros Are Available", "[formula]")
{
    auto surge = Surge::Test::surgeOnSine();
    surge->storage.getPatch().scene[0].lfo[0].shape.val.i = lt_formula;
    surge->storage.getPatch().scene[0].adsr[0].r.val.f = 2;
    auto pitchId = surge->storage.getPatch().scene[0].osc[0].pitch.id;
    surge->setModDepth01(pitchId, ms_lfo1, 0, 0, 0.1);
    REQUIRE(1);
}

TEST_CASE("Nan Clampsr", "[formula]")
{
    SECTION("Nan Formula")
    {
        auto surge = Surge::Test::surgeOnSine();
        surge->storage.getPatch().scene[0].lfo[0].shape.val.i = lt_formula;
        auto pitchId = surge->storage.getPatch().scene[0].osc[0].pitch.id;
        surge->setModDepth01(pitchId, ms_lfo1, 0, 0, 0.1);

        surge->storage.getPatch().formulamods[0][0].setFormula(R"FN(
function init(modstate)
   modstate["depth"] = 0
   return modstate
end

function process(modstate)
    modstate["output"] = (modstate["phase"] * 2 - 1) / modstate["depth" ]
    return modstate
end)FN");
        for (int i = 0; i < 10; ++i)
            surge->process();

        surge->playNote(0, 60, 100, 0);
        for (int i = 0; i < 10; ++i)
            surge->process();

        REQUIRE(!surge->voices[0].empty());
        auto ms = surge->voices[0].front()->modsources[ms_lfo1];
        REQUIRE(ms);
        auto lms = dynamic_cast<LFOModulationSource *>(ms);
        REQUIRE(lms);

        REQUIRE(lms->get_output(0) == 0.f);
        REQUIRE(!lms->formulastate.isFinite);
    }

    SECTION("Not Nan Formula")
    {
        auto surge = Surge::Test::surgeOnSine();
        surge->storage.getPatch().scene[0].lfo[0].shape.val.i = lt_formula;
        auto pitchId = surge->storage.getPatch().scene[0].osc[0].pitch.id;
        surge->setModDepth01(pitchId, ms_lfo1, 0, 0, 0.1);

        surge->storage.getPatch().formulamods[0][0].setFormula(R"FN(
function init(modstate)
   modstate["depth"] = 1
   return modstate
end

function process(modstate)
    modstate["output"] = (modstate["phase"] * 2 - 1) / modstate["depth" ]
    return modstate
end)FN");
        for (int i = 0; i < 10; ++i)
            surge->process();

        surge->playNote(0, 60, 100, 0);
        for (int i = 0; i < 10; ++i)
            surge->process();

        REQUIRE(!surge->voices[0].empty());
        auto ms = surge->voices[0].front()->modsources[ms_lfo1];
        REQUIRE(ms);
        auto lms = dynamic_cast<LFOModulationSource *>(ms);
        REQUIRE(lms);

        REQUIRE(lms->formulastate.isFinite);
    }
}

TEST_CASE("Two Surges", "[formula]")
{
    // this attempts but fails to reproduce 5753 but i left it here anyway
    SECTION("Two Surges on Tutorial 3")
    {
        auto s1 = Surge::Test::surgeOnSine();
        auto s2 = Surge::Test::surgeOnSine();

        REQUIRE(s1->loadPatchByPath("resources/data/patches_factory/Tutorials/Formula Modulator/03 "
                                    "The Init Function And State.fxp",
                                    -1, "Tutorials"));
        REQUIRE(s2->loadPatchByPath("resources/data/patches_factory/Tutorials/Formula Modulator/03 "
                                    "The Init Function And State.fxp",
                                    -1, "Tutorials"));

        for (int i = 0; i < 10; ++i)
        {
            s1->process();
            s2->process();
        }
        s1->playNote(0, 60, 127, 0);
        for (int i = 0; i < 50; ++i)
        {
            s1->process();
            s2->process();
        }
        s2->playNote(0, 60, 127, 0);
        for (int i = 0; i < 50; ++i)
        {
            s1->process();
            s2->process();
        }
        s1->releaseNote(0, 60, 127);
        for (int i = 0; i < 50; ++i)
        {
            s1->process();
            s2->process();
        }
    }
}