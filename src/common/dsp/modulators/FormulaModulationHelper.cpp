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
#include "LuaSupport.h"
#include "SurgeVoice.h"
#include "SurgeStorage.h"
#include <thread>
#include <functional>
#include "fmt/core.h"

namespace Surge
{
namespace Formula
{

void setupStorage(SurgeStorage *s) { s->formulaGlobalData = std::make_unique<GlobalData>(); }
bool prepareForEvaluation(SurgeStorage *storage, FormulaModulatorStorage *fs, EvaluatorState &s,
                          bool is_display)
{
    auto &stateData = *storage->formulaGlobalData;
    bool firstTimeThrough = false;
    if (!is_display)
    {
        static int aid = 1;
        if (stateData.audioState == nullptr)
        {
            stateData.audioState = lua_open();
            luaL_openlibs((lua_State *)(stateData.audioState));
            firstTimeThrough = true;
        }
        s.L = (lua_State *)(stateData.audioState);
        snprintf(s.stateName, TXT_SIZE, "audiostate_%d", aid);
        aid++;
        if (aid < 0)
            aid = 1;
    }
    else
    {
        static int did = 1;
        if (stateData.displayState == nullptr)
        {
            stateData.displayState = lua_open();
            luaL_openlibs((lua_State *)(stateData.displayState));
            firstTimeThrough = true;
        }
        s.L = (lua_State *)(stateData.displayState);
        snprintf(s.stateName, TXT_SIZE, "dispstate_%d", did);
        did++;
        if (did < 0)
            did = 1;
    }

    auto lg = Surge::LuaSupport::SGLD("prepareForEvaluation", s.L);

    if (firstTimeThrough)
    {
        Surge::LuaSupport::loadSurgePrelude(s.L);
        auto reserved0 = std::string(R"FN(
function surge_reserved_formula_error_stub(m)
    return 0;
end
)FN");
        std::string emsg;
        bool r0 = Surge::LuaSupport::parseStringDefiningFunction(
            s.L, reserved0, "surge_reserved_formula_error_stub", emsg);
        if (r0)
        {
            lua_setglobal(s.L, "surge_reserved_formula_error_stub");
        }
    }

    // OK so now evaluate the formula. This is a mistake - the loading and
    // compiling can be expensive so lets look it up by hash first
    auto h = fs->formulaHash;
    auto pvn = std::string("pvn") + std::to_string(is_display) + "_" + std::to_string(h);
    auto pvf = pvn + "_f";
    auto pvfInit = pvn + "_fInit";
    snprintf(s.funcName, TXT_SIZE, "%s", pvf.c_str());
    snprintf(s.funcNameInit, TXT_SIZE, "%s", pvfInit.c_str());

    // Handle hash collisions
    lua_getglobal(s.L, pvn.c_str());
    s.isvalid = false;

    bool hasString = false;
    if (lua_isstring(s.L, -1))
    {
        if (fs->formulaString != lua_tostring(s.L, -1))
        {
            s.adderror("Hash Collision in function. Bad luck!");
        }
        else
        {
            hasString = true;
        }
    }
    lua_pop(s.L, 1); // we don't need the string or whatever on the stack
    if (hasString)
    {
        snprintf(s.funcName, TXT_SIZE, "%s", pvf.c_str());
        snprintf(s.funcNameInit, TXT_SIZE, "%s", pvfInit.c_str());
        // CHECK that I can actually get the function here
        lua_getglobal(s.L, s.funcName);
        s.isvalid = lua_isfunction(s.L, -1);
        lua_pop(s.L, 1);

        if (stateData.knownBadFunctions.find(s.funcName) != stateData.knownBadFunctions.end())
        {
            s.isvalid = false;
        }
    }
    else
    {
        std::string emsg;
        int res = Surge::LuaSupport::parseStringDefiningMultipleFunctions(
            s.L, fs->formulaString, {"process", "init"}, emsg);

        if (res >= 1)
        {
            // Great - rename it and nuke process
            lua_setglobal(s.L, s.funcName);
            lua_pushnil(s.L);
            lua_setglobal(s.L, "process");

            // Then get it and set its env
            lua_getglobal(s.L, s.funcName);
            Surge::LuaSupport::setSurgeFunctionEnvironment(s.L);
            lua_pop(s.L, 1);

            lua_setglobal(s.L, s.funcNameInit);
            lua_pushnil(s.L);
            lua_setglobal(s.L, "init");

            // Then get it and set its env
            lua_getglobal(s.L, s.funcNameInit);
            Surge::LuaSupport::setSurgeFunctionEnvironment(s.L);
            lua_pop(s.L, 1);

            stateData.functionsPerFMS[fs].insert(s.funcName);
            stateData.functionsPerFMS[fs].insert(s.funcNameInit);

            s.isvalid = true;
        }
        else
        {
            s.adderror("Unable to determine 'process' or 'init' function : " + emsg);
            lua_pop(s.L, 1); // process
            lua_pop(s.L, 1); // process
            stateData.knownBadFunctions.insert(s.funcName);
        }

        // this happens here because we did parse it at least. Don't parse again until it is changed
        lua_pushstring(s.L, fs->formulaString.c_str());
        lua_setglobal(s.L, pvn.c_str());
    }

    if (s.isvalid)
    {
        // Create my state object each time
        lua_getglobal(s.L, s.funcNameInit);
        lua_createtable(s.L, 0, 10);

        // add subscription hooks
        lua_pushstring(s.L, "subscriptions");
        lua_createtable(s.L, 0, 5);
        lua_pushstring(s.L, "macros");
        lua_createtable(s.L, n_customcontrollers, 0);
        for (int i = 0; i < n_customcontrollers; ++i)
        {
            lua_pushnumber(s.L, i + 1);
            lua_pushboolean(s.L, false);
            lua_settable(s.L, -3);
        }
        lua_settable(s.L, -3);

        lua_pushstring(s.L, "lfo_envelope");
        lua_pushboolean(s.L, false);
        lua_settable(s.L, -3);

        lua_pushstring(s.L, "lfo_params");
        lua_pushboolean(s.L, true);
        lua_settable(s.L, -3);

        lua_pushstring(s.L, "voice");
        lua_pushboolean(s.L, false);
        lua_settable(s.L, -3);

        lua_pushstring(s.L, "timing");
        lua_pushboolean(s.L, true);
        lua_settable(s.L, -3);

        lua_settable(s.L, -3);

        lua_pushstring(s.L, "samplerate");
        lua_pushnumber(s.L, storage->samplerate);
        lua_settable(s.L, -3);

        lua_pushstring(s.L, "block_size");
        lua_pushnumber(s.L, BLOCK_SIZE);
        lua_settable(s.L, -3);

        if (lua_isfunction(s.L, -2))
        {
            // CALL HERE
            auto addn = [&s](const char *q, float f) {
                lua_pushstring(s.L, q);
                lua_pushnumber(s.L, f);
                lua_settable(s.L, -3);
            };

            addn("delay", s.del);
            addn("decay", s.dec);
            addn("attack", s.a);
            addn("hold", s.h);
            addn("sustain", s.s);
            addn("release", s.r);
            addn("rate", s.rate);
            addn("amplitude", s.amp);
            addn("startphase", s.phase);
            addn("deform", s.deform);
            addn("tempo", s.tempo);
            addn("songpos", s.songpos);

            auto cres = lua_pcall(s.L, 1, 1, 0);
            if (cres == LUA_OK)
            {
                if (!lua_istable(s.L, -1))
                {
                    s.isvalid = false;
                    s.adderror("Your 'init' function must return a table. This usually means "
                               "that you didn't end your init function with 'return modstate' "
                               "before the end statement.");
                    stateData.knownBadFunctions.insert(s.funcName);
                }
            }
            else
            {
                s.isvalid = false;
                std::ostringstream oss;
                oss << "Failed to evaluate 'init' function. " << lua_tostring(s.L, -1);
                s.adderror(oss.str());
                stateData.knownBadFunctions.insert(s.funcName);
            }
        }

        // FIXME - we have to clean this up when evaluation is done
        lua_setglobal(s.L, s.stateName);

        // the modstate which is now bound to the state name
        lua_pop(s.L, -1);

        s.useEnvelope = true;

        {
            auto sub = Surge::LuaSupport::SGLD("prepareForEvaluation::subscriptions", s.L);

            lua_getglobal(s.L, s.stateName);
            if (!lua_istable(s.L, -1))
            {
                lua_pop(s.L, -1);
                std::cout << "Not a table" << std::endl;
            }
            else
            {
                {
                    // read off the envelope control
                    auto gv = Surge::LuaSupport::SGLD("prepareForEvaluation::enveloperead", s.L);
                    lua_pushstring(s.L, "use_envelope");
                    lua_gettable(s.L, -2);
                    if (lua_isboolean(s.L, -1))
                    {
                        s.useEnvelope = lua_toboolean(s.L, -1);
                    }
                    lua_pop(s.L, 1);
                }

                // now let's read off those subscriptions
                lua_pushstring(s.L, "subscriptions");
                lua_gettable(s.L, -2);

                auto gv = [&s](const char *k) -> bool {
                    auto gv =
                        Surge::LuaSupport::SGLD("prepareForEvaluation::subscriptions::gv", s.L);

                    auto res = false;
                    lua_pushstring(s.L, k);
                    lua_gettable(s.L, -2);
                    if (lua_isboolean(s.L, -1))
                    {
                        res = lua_toboolean(s.L, -1);
                    }
                    lua_pop(s.L, 1);
                    return res;
                };
                s.subLfoEnvelope = gv("lfo_envelope");
                s.subLfoParams = gv("lfo_params");
                s.subVoice = gv("voice");
                s.subTiming = gv("timing");

                lua_pushstring(s.L, "macros");
                lua_gettable(s.L, -2);
                if (lua_isboolean(s.L, -1))
                {
                    auto b = lua_toboolean(s.L, -1);
                    s.subAnyMacro = b;
                    for (int i = 0; i < n_customcontrollers; ++i)
                        s.subMacros[i] = b;
                }
                else if (lua_istable(s.L, -1))
                {
                    for (int i = 0; i < n_customcontrollers; ++i)
                    {
                        lua_pushnumber(s.L, i + 1);
                        lua_gettable(s.L, -2);
                        bool res = false;
                        if (lua_isboolean(s.L, -1))
                            res = lua_toboolean(s.L, -1);
                        lua_pop(s.L, 1);
                        s.subAnyMacro = s.subAnyMacro | res;
                        s.subMacros[i] = res;
                    }
                }
                lua_pop(s.L, 1);

                lua_pop(s.L, 1); // the subscriptions
                lua_pop(s.L, 1); // the modstate
            }
        }
    }

    if (is_display)
    {
        // Move to support
        auto dg = Surge::LuaSupport::SGLD("set RNG", s.L);
        // Seed the RNG
        lua_getglobal(s.L, "math");
        // > math
        if (lua_isnil(s.L, -1))
        {
            std::cout << "NIL MATH " << std::endl;
        }
        else
        {
            lua_pushstring(s.L, "randomseed");
            lua_gettable(s.L, -2);
            // > math > randomseed
            if (lua_isnil(s.L, -1))
            {
                std::cout << "NUL randomseed" << std::endl;
                lua_pop(s.L, -1);
            }
            else
            {
                lua_pushnumber(s.L, 8675309);
                lua_pcall(s.L, 1, 0, 0);
            }
        }
        // math or nil so
        lua_pop(s.L, 1);
    }

    s.del = 0;
    s.dec = 0;
    s.a = 0;
    s.h = 0;
    s.r = 0;
    s.s = 0;
    s.rate = 0;
    s.phase = 0;
    s.amp = 0;
    s.deform = 0;
    s.tempo = 120;

    if (s.raisedError)
        std::cout << "ERROR: " << s.error << std::endl;
    return true;
}

void removeFunctionsAssociatedWith(SurgeStorage *storage, FormulaModulatorStorage *fs)
{
    auto &stateData = *storage->formulaGlobalData;

    auto S = stateData.audioState;
    if (!S)
        return;
    if (stateData.functionsPerFMS.find(fs) == stateData.functionsPerFMS.end())
        return;

#if 0
    for (const auto &fn : functionsPerFMS[fs])
    {
        lua_pushnil(S);
        lua_setglobal(S, fn.c_str());
    }
#endif

    stateData.functionsPerFMS.erase(fs);
}

bool cleanEvaluatorState(EvaluatorState &s)
{
    if (s.L && s.stateName[0] != 0)
    {
        lua_pushnil(s.L);
        lua_setglobal(s.L, s.stateName);
        s.stateName[0] = 0;
    }
    return true;
}

bool initEvaluatorState(EvaluatorState &s)
{
    s.funcName[0] = 0;
    s.funcNameInit[0] = 0;
    s.stateName[0] = 0;
    s.L = nullptr;
    return true;
}
void valueAt(int phaseIntPart, float phaseFracPart, SurgeStorage *storage,
             FormulaModulatorStorage *fs, EvaluatorState *s, float output[max_formula_outputs])
{
    s->activeoutputs = 1;
    memset(output, 0, max_formula_outputs * sizeof(float));
    if (s->L == nullptr)
        return;

    if (!s->isvalid)
        return;

    auto gs = Surge::LuaSupport::SGLD("valueAt", s->L);
    struct OnErrorReplaceWithZero
    {
        OnErrorReplaceWithZero(lua_State *L, std::string fn) : L(L), fn(fn) {}
        ~OnErrorReplaceWithZero()
        {
            if (replace)
            {
                // std::cout << "Would nuke " << fn << std::endl;
                lua_getglobal(L, "surge_reserved_formula_error_stub");
                lua_setglobal(L, fn.c_str());
            }
        }
        lua_State *L;
        std::string fn;
        bool replace = true;
    } onerr(s->L, s->funcName);
    /*
     * So: make the stack my evaluation func then my table; then push my table
     * values; then call my function; then update my global
     */
    lua_getglobal(s->L, s->funcName);
    if (!lua_isfunction(s->L, -1))
    {
        s->isvalid = false;
        lua_pop(s->L, 1);
        return;
    }
    lua_getglobal(s->L, s->stateName);

    lua_pushstring(s->L, "av");
    lua_gettable(s->L, -2);
    lua_pop(s->L, 1);

    // Stack is now func > table  so we can update the table
    lua_pushstring(s->L, "intphase");
    lua_pushinteger(s->L, phaseIntPart);
    lua_settable(s->L, -3);

    auto addn = [s](const char *q, float f) {
        lua_pushstring(s->L, q);
        lua_pushnumber(s->L, f);
        lua_settable(s->L, -3);
    };

    auto addb = [s](const char *q, bool b) {
        lua_pushstring(s->L, q);
        lua_pushboolean(s->L, b);
        lua_settable(s->L, -3);
    };

    auto addnil = [s](const char *q) {
        lua_pushstring(s->L, q);
        lua_pushnil(s->L);
        lua_settable(s->L, -3);
    };

    addn("phase", phaseFracPart);

    if (s->subLfoEnvelope)
    {
        addn("delay", s->del);
        addn("decay", s->dec);
        addn("attack", s->a);
        addn("hold", s->h);
        addn("sustain", s->s);
        addn("release", s->r);
    }
    if (s->subLfoParams)
    {
        addn("rate", s->rate);
        addn("amplitude", s->amp);
        addn("startphase", s->phase);
        addn("deform", s->deform);
    }

    if (s->subTiming)
    {
        addn("tempo", s->tempo);
        addn("songpos", s->songpos);
        addb("released", s->released);
    }

    if (s->subVoice && s->isVoice)
    {
        addb("is_voice", s->isVoice);
        addn("key", s->key);
        addn("velocity", s->velocity);
        addn("channel", s->channel);
    }

    addnil("retrigger_AEG");
    addnil("retrigger_FEG");

    if (s->subAnyMacro)
    {
        // load the macros
        lua_pushstring(s->L, "macros");
        lua_createtable(s->L, n_customcontrollers, 0);
        for (int i = 0; i < n_customcontrollers; ++i)
        {
            if (s->subMacros[i])
            {
                lua_pushinteger(s->L, i + 1);
                lua_pushnumber(s->L, s->macrovalues[i]);
                lua_settable(s->L, -3);
            }
        }
        lua_settable(s->L, -3);
    }

    auto lres = lua_pcall(s->L, 1, 1, 0);
    // stack is now just the result
    if (lres == LUA_OK)
    {
        s->isFinite = true;
        auto checkFinite = [s](float f) {
            if (!std::isfinite(f))
            {
                s->isFinite = false;
                return 0.f;
            }
            return f;
        };

        if (lua_isnumber(s->L, -1))
        {
            // OK so you returned a value. Just use it
            auto r = lua_tonumber(s->L, -1);
            lua_pop(s->L, 1);
            output[0] = checkFinite(r);
            return;
        }
        if (!lua_istable(s->L, -1))
        {
            s->adderror(
                "The return of your LUA function must be a number or table. Just return input with "
                "output set.");
            s->isvalid = false;
            lua_pop(s->L, 1);
            return;
        }
        // Store the value and keep it on top of the stack
        lua_setglobal(s->L, s->stateName);
        lua_getglobal(s->L, s->stateName);

        lua_pushstring(s->L, "output");
        lua_gettable(s->L, -2);
        // top of stack is now the result
        float res = 0.0;
        if (lua_isnumber(s->L, -1))
        {
            output[0] = checkFinite(lua_tonumber(s->L, -1));
        }
        else if (lua_istable(s->L, -1))
        {
            auto len = 0;

            lua_pushnil(s->L);
            while (lua_next(s->L, -2)) // because we pushed nil
            {
                int idx = -1;
                // now key is -2, value is -1
                if (lua_isnumber(s->L, -2))
                {
                    idx = lua_tointeger(s->L, -2);
                }
                if (idx <= 0 || idx > max_formula_outputs)
                {
                    std::ostringstream oss;
                    oss << "Error with vector output. The vector output must be"
                        << " an array with size up to 8. Your table contained"
                        << " index " << idx;
                    if (idx == -1)
                        oss << " which is not an integer array index.";
                    if (idx > max_formula_outputs)
                        oss << " which means your result is too long.";
                    s->adderror(oss.str());
                    auto &stateData = *storage->formulaGlobalData;
                    stateData.knownBadFunctions.insert(s->funcName);
                    s->isvalid = false;

                    idx = 0;
                }

                // Remember - LUA is 0 based
                if (idx > 0)
                    output[idx - 1] = checkFinite(lua_tonumber(s->L, -1));
                lua_pop(s->L, 1);
                len = std::max(len, idx - 1);
            }
            s->activeoutputs = len + 1;
        }
        else
        {
            auto &stateData = *storage->formulaGlobalData;

            if (stateData.knownBadFunctions.find(s->funcName) != stateData.knownBadFunctions.end())
                s->adderror(
                    "You must define the 'output' field in the returned table as a number or "
                    "float array");
            stateData.knownBadFunctions.insert(s->funcName);
            s->isvalid = false;
        };
        // pop the result and the function
        lua_pop(s->L, 1);

        auto getBoolDefault = [s](const char *n, bool def) -> bool {
            auto res = def;
            lua_pushstring(s->L, n);
            lua_gettable(s->L, -2);
            if (lua_isboolean(s->L, -1))
            {
                res = lua_toboolean(s->L, -1);
            }
            lua_pop(s->L, 1);
            return res;
        };

        s->useEnvelope = getBoolDefault("use_envelope", true);
        s->retrigger_AEG = getBoolDefault("retrigger_AEG", false);
        s->retrigger_FEG = getBoolDefault("retrigger_FEG", false);

        auto doClamp = getBoolDefault("clamp_output", true);
        if (doClamp)
        {
            for (int i = 0; i < 8; ++i)
            {
                output[i] = limitpm1(output[i]);
            }
        }

        // Finally pop the table result
        lua_pop(s->L, 1);
        onerr.replace = false;
        return;
    }
    else
    {
        s->isvalid = false;
        std::ostringstream oss;
        oss << "Failed to evaluate 'process' function." << lua_tostring(s->L, -1);
        s->adderror(oss.str());
        lua_pop(s->L, 1);
        return;
    }
}

std::vector<DebugRow> createDebugDataOfModState(const EvaluatorState &es)
{
    std::vector<DebugRow> rows;
    Surge::LuaSupport::SGLD guard("debugViewGuard", es.L);
    lua_getglobal(es.L, es.stateName);
    if (!lua_istable(es.L, -1))
    {
        lua_pop(es.L, -1);
        rows.emplace_back(0, "Error", "Not a Table");
        return rows;
    }
    std::function<void(const int, bool)> rec;
    rec = [&rows, &es, &rec](const int depth, bool internal) {
        Surge::LuaSupport::SGLD guardR("rec[" + std::to_string(depth) + "]", es.L);

        if (lua_istable(es.L, -1))
        {
            // gather and sort keys for display
            std::vector<std::string> skeys;
            std::vector<int> ikeys;

            lua_pushnil(es.L);
            while (lua_next(es.L, -2)) // because we pushed nil
            {
                // now key is -2, value is -1
                if (lua_isnumber(es.L, -2))
                {
                    ikeys.push_back(lua_tointeger(es.L, -2));
                }
                else if (lua_isstring(es.L, -2))
                {
                    skeys.push_back(lua_tostring(es.L, -2));
                }
                lua_pop(es.L, 1);
            }

            if (!skeys.empty())
                std::sort(skeys.begin(), skeys.end(), [](const auto &a, const auto &b) {
                    if (a == "subscriptions")
                        return false;
                    if (b == "subscriptions")
                        return true;
                    return a < b;
                });
            if (!ikeys.empty())
                std::sort(ikeys.begin(), ikeys.end());

            auto guts = [&](const std::string &lab) {
                if (lua_isnumber(es.L, -1))
                {
                    rows.emplace_back(depth, lab, lua_tonumber(es.L, -1));
                }
                else if (lua_isstring(es.L, -1))
                {
                    rows.emplace_back(depth, lab, lua_tostring(es.L, -1));
                }
                else if (lua_isnil(es.L, -1))
                {
                    rows.emplace_back(depth, lab, "(nil)");
                }
                else if (lua_isboolean(es.L, -1))
                {
                    rows.emplace_back(depth, lab, (lua_toboolean(es.L, -1) ? "true" : "false"));
                }
                else if (lua_istable(es.L, -1))
                {
                    rows.emplace_back(depth, lab);
                    internal = internal || (lab == "subscriptions");
                    rows.back().isInternal = internal;
                    rec(depth + 1, internal);
                }
                else
                {
                    rows.emplace_back(depth, lab, "(unknown)");
                }
                rows.back().isInternal = internal;
            };
            for (auto k : ikeys)
            {
                std::ostringstream oss;
                oss << "." << k;
                lua_pushinteger(es.L, k);
                lua_gettable(es.L, -2);
                guts(oss.str());
                lua_pop(es.L, 1);
            }

            for (auto s : skeys)
            {
                lua_pushstring(es.L, s.c_str());
                lua_gettable(es.L, -2);
                guts(s);
                lua_pop(es.L, 1);
            }
        }
    };

    rec(0, false);
    lua_pop(es.L, -1);
    return rows;
}
std::string createDebugViewOfModState(const EvaluatorState &es)
{
    auto r = createDebugDataOfModState(es);
    std::ostringstream oss;
    for (const auto d : r)
    {
        oss << std::string(d.depth, ' ') << std::string(d.depth, ' ') << d.label << ": ";
        if (!d.hasValue)
        {
        }
        else if (auto fv = std::get_if<float>(&d.value))
            oss << *fv;
        else if (auto sv = std::get_if<std::string>(&d.value))
            oss << *sv;
        else
            oss << "(error)";

        oss << "\n";
    }
    return oss.str();
}

void createInitFormula(FormulaModulatorStorage *fs)
{
    fs->setFormula(R"FN(function init(modstate)
    -- this function is called at the creation of each LFO (so voice on etc...)
    -- and allows you to adjust the modstate with pre-calculated values
    return modstate
end

function process(modstate)
    -- this is the per-block'process'. input will contain keys 'phase' 'intphase',
    -- 'deform'. You must set the output value and return it. See the manual or
    -- tutorials for more

    modstate["output"] = modstate["phase"] * 2 - 1
    return modstate
end)FN");
    fs->interpreter = FormulaModulatorStorage::LUA;
}

void setupEvaluatorStateFrom(EvaluatorState &s, const SurgePatch &p)
{
    for (int i = 0; i < n_customcontrollers; ++i)
    {
        auto ms = p.scene[0].modsources[ms_ctrl1 + i];
        auto cms = dynamic_cast<ControllerModulationSource *>(ms);
        if (cms)
        {
            s.macrovalues[i] = cms->get_output(0);
        }
    }
}
void setupEvaluatorStateFrom(EvaluatorState &s, const SurgeVoice *v)
{
    s.key = v->state.key;
    s.channel = v->state.channel;
    s.velocity = v->state.velocity;
}

std::variant<float, std::string, bool> runOverModStateForTesting(const std::string &query,
                                                                 const EvaluatorState &es)
{
    Surge::LuaSupport::SGLD guard("runOverModStateForTesting", es.L);

    std::string emsg;
    bool r0 = Surge::LuaSupport::parseStringDefiningFunction(es.L, query, "query", emsg);
    if (!r0)
    {
        return false;
    }

    lua_getglobal(es.L, es.stateName);
    if (!lua_istable(es.L, -1))
    {
        lua_pop(es.L, -1);
        return false;
    }

    lua_pcall(es.L, 1, 1, 0);

    if (lua_isnumber(es.L, -1))
    {
        auto res = lua_tonumber(es.L, -1);
        lua_pop(es.L, -1);
        return (float)res;
    }

    if (lua_isboolean(es.L, -1))
    {
        auto res = lua_toboolean(es.L, -1);
        lua_pop(es.L, -1);
        return (float)res;
    }

    if (lua_isstring(es.L, -1))
    {
        auto res = lua_tostring(es.L, -1);
        lua_pop(es.L, -1);
        return res;
    }
    lua_pop(es.L, -1);
    return false;
}

std::variant<float, std::string, bool> extractModStateKeyForTesting(const std::string &key,
                                                                    const EvaluatorState &s)
{
    auto query = fmt::format(R"FN(
function query(modstate)
   return modstate["{}"];
end
)FN",
                             key);
    return runOverModStateForTesting(query, s);
}

} // namespace Formula
} // namespace Surge