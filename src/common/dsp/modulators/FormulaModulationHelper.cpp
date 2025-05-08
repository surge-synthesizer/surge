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

#include "FormulaModulationHelper.h"
#include "LuaSupport.h"
#include "SurgeVoice.h"
#include "SurgeStorage.h"
#include <thread>
#include <functional>
#include "fmt/core.h"
#include "lua/LuaSources.h"

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
#if HAS_LUA
            stateData.audioState = luaL_newstate();
            luaL_openlibs((lua_State *)(stateData.audioState));
#endif
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
#if HAS_LUA
            stateData.displayState = luaL_newstate();
            luaL_openlibs((lua_State *)(stateData.displayState));
#endif
            firstTimeThrough = true;
        }
        s.L = (lua_State *)(stateData.displayState);
        snprintf(s.stateName, TXT_SIZE, "dispstate_%d", did);
        did++;
        if (did < 0)
            did = 1;
    }

#if HAS_LUA

    auto lg = Surge::LuaSupport::SGLD("prepareForEvaluation", s.L);

    if (firstTimeThrough)
    {
        // Setup shared table
        lua_newtable(s.L);
        lua_setglobal(s.L, sharedTableName);

        // Load the Formula prelude
        Surge::LuaSupport::loadSurgePrelude(s.L, Surge::LuaSources::formula_prelude);

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
            s.adderror("Hash collision in function! Bad luck...");
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
            s.adderror("Unable to determine process() or init() function: " + emsg);
            lua_pop(s.L, 2); // Pop process and init (or nil)
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

        // Legacy tables for deprecated macro subscriptions
        // TODO: Remove in XT2
        lua_createtable(s.L, 0, 0);
        lua_createtable(s.L, 0, 0);
        lua_setfield(s.L, -2, "macros");
        lua_setfield(s.L, -2, "subscriptions");

        lua_pushnumber(s.L, storage->samplerate);
        lua_setfield(s.L, -2, "samplerate");

        lua_pushnumber(s.L, BLOCK_SIZE);
        lua_setfield(s.L, -2, "block_size");

        if (lua_isfunction(s.L, -2))
        {
            // CALL HERE
            auto addn = [&s](const char *q, float f) {
                lua_pushnumber(s.L, f);
                lua_setfield(s.L, -2, q);
            };

            auto addi = [&s](const char *q, float f) {
                lua_pushinteger(s.L, f);
                lua_setfield(s.L, -2, q);
            };

            auto addb = [&s](const char *q, bool f) {
                lua_pushboolean(s.L, f);
                lua_setfield(s.L, -2, q);
            };

            addn("delay", s.del);
            addn("attack", s.a);
            addn("hold", s.h);
            addn("decay", s.dec);
            addn("sustain", s.s);
            addn("release", s.r);

            addn("rate", s.rate);
            addn("startphase", s.phase);
            addn("deform", s.deform);
            addn("amplitude", s.amp);

            addn("tempo", s.tempo);
            addn("songpos", s.songpos);
            addb("released", s.released);

            if (s.isVoice)
            {
                addi("channel", s.channel);
                addi("key", s.key);
                addi("velocity", s.velocity);
                addi("voice_id", s.voiceOrderAtCreate);
            }

            addb("is_rendering_to_ui", s.is_display);
            addb("clamp_output", true);

            // Load the macros
            lua_createtable(s.L, n_customcontrollers, 0);
            for (int i = 0; i < n_customcontrollers; ++i)
            {
                lua_pushinteger(s.L, i + 1);
                lua_pushnumber(s.L, s.macrovalues[i]);
                lua_settable(s.L, -3);
            }
            lua_setfield(s.L, -2, "macros");

            auto cres = lua_pcall(s.L, 1, 1, 0);
            if (cres == LUA_OK)
            {
                if (!lua_istable(s.L, -1))
                {
                    s.isvalid = false;
                    s.adderror("The init() function must return a table.\nThis usually means "
                               "that you didn't close the init() function with 'return state' "
                               "before the 'end' statement.");
                    stateData.knownBadFunctions.insert(s.funcName);
                }
            }
            else
            {
                s.isvalid = false;
                std::ostringstream oss;
                const char *err = lua_tostring(s.L, -1);
                // Fallback if error(nil)
                if (!err)
                    err = "Lua error: Value is nil";
                oss << "Failed to evaluate init() function! " << err;
                s.adderror(oss.str());
                lua_pop(s.L, 1); // Pop error
                stateData.knownBadFunctions.insert(s.funcName);
            }
        }

        // FIXME - we have to clean this up when evaluation is done
        lua_setglobal(s.L, s.stateName);

        // the modulator state which is now bound to the state name
        lua_settop(s.L, 0); // Clear all elements from the stack

        s.useEnvelope = true;

        {
            auto sub = Surge::LuaSupport::SGLD("prepareForEvaluation::subscriptions", s.L);

            lua_getglobal(s.L, s.stateName);
            if (!lua_istable(s.L, -1))
            {
                lua_pop(s.L, 1); // Pop non-table
                std::cout << "Not a table!" << std::endl;
            }
            else
            {
                {
                    // read off the envelope control
                    auto gv = Surge::LuaSupport::SGLD("prepareForEvaluation::enveloperead", s.L);
                    lua_getfield(s.L, -1, "use_envelope");
                    if (lua_isboolean(s.L, -1))
                    {
                        s.useEnvelope = lua_toboolean(s.L, -1);
                    }
                    lua_pop(s.L, 1); // Pop use_envelope
                }
                lua_pop(s.L, 1); // Pop the modulator state
            }
        }
    }

    if (is_display)
    {
        // Move to support
        auto dg = Surge::LuaSupport::SGLD("set RNG", s.L);
        // Seed the RNG
        lua_getglobal(s.L, "math"); // > math
        if (lua_isnil(s.L, -1))
        {
            std::cout << "NIL MATH " << std::endl;
        }
        else
        {
            lua_getfield(s.L, -1, "randomseed"); // > math > randomseed
            if (lua_isnil(s.L, -1))
            {
                std::cout << "NUL randomseed" << std::endl;
                lua_pop(s.L, 1); // Pop nil
            }
            else
            {
                lua_pushnumber(s.L, 8675309);
                lua_pcall(s.L, 1, 0, 0);
            }
        }
        lua_pop(s.L, 1); // Pop math (or nil)
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

    if (is_display)
        s.is_display = true;

    if (s.raisedError)
        std::cout << "Error: " << *(s.error) << std::endl;
#endif

    return true;
}

void removeFunctionsAssociatedWith(SurgeStorage *storage, FormulaModulatorStorage *fs)
{
#if HAS_LUA
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
#endif
}

bool cleanEvaluatorState(EvaluatorState &s)
{
#if HAS_LUA
    if (s.L && s.stateName[0] != 0)
    {
        lua_pushnil(s.L);
        lua_setglobal(s.L, s.stateName);
        s.stateName[0] = 0;
    }
#endif
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
             FormulaModulatorStorage *fs, EvaluatorState *s, float output[max_formula_outputs],
             bool justSetup)
{
#if HAS_LUA
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

    auto addn = [s](const char *q, float f) {
        lua_pushnumber(s->L, f);
        lua_setfield(s->L, -2, q);
    };

    auto addi = [s](const char *q, int i) {
        lua_pushinteger(s->L, i);
        lua_setfield(s->L, -2, q);
    };

    auto addb = [s](const char *q, bool b) {
        lua_pushboolean(s->L, b);
        lua_setfield(s->L, -2, q);
    };

    auto addnil = [s](const char *q) {
        lua_pushnil(s->L);
        lua_setfield(s->L, -2, q);
    };

    // Stack is now func > table so we can update the table
    addi("intphase", phaseIntPart);
    addi("cycle", phaseIntPart); // Alias cycle for intphase

    // Fake a voice count of one for display calls
    int voiceCount = storage->activeVoiceCount;
    if (voiceCount == 0 && s->is_display)
        voiceCount = 1;
    addi("voice_count", voiceCount);

    addn("delay", s->del);
    addn("decay", s->dec);
    addn("attack", s->a);
    addn("hold", s->h);
    addn("sustain", s->s);
    addn("release", s->r);

    addn("rate", s->rate);
    addn("startphase", s->phase);
    addn("amplitude", s->amp);
    addn("deform", s->deform);

    addn("phase", phaseFracPart);
    addn("tempo", s->tempo);
    addn("songpos", s->songpos);

    addn("pb", s->pitchbend);
    addn("pb_range_up", s->pbrange_up);
    addn("pb_range_dn", s->pbrange_dn);
    addn("chan_at", s->aftertouch);
    addn("cc_mw", s->modwheel);
    addn("cc_breath", s->breath);
    addn("cc_expr", s->expression);
    addn("cc_sus", s->sustain);
    addn("lowest_key", s->lowest_key);
    addn("highest_key", s->highest_key);
    addn("latest_key", s->latest_key);

    addi("poly_limit", s->polylimit);
    addi("scene_mode", s->scenemode);
    addi("play_mode", s->polymode);
    addi("split_point", s->splitpoint);

    addb("released", s->released);
    addb("is_rendering_to_ui", s->is_display);
    addb("mpe_enabled", s->mpeenabled);

    addnil("retrigger_AEG");
    addnil("retrigger_FEG");

    if (s->isVoice)
    {
        addi("key", s->key);
        addi("velocity", s->velocity);
        addi("rel_velocity", s->releasevelocity);
        addi("channel", s->channel);

        addn("poly_at", s->polyat);
        addn("mpe_bend", s->mpebend);
        addn("mpe_bendrange", s->mpebendrange);
        addn("mpe_timbre", s->mpetimbre);
        addn("mpe_pressure", s->mpepressure);

        addb("is_voice", s->isVoice);
        addb("released", s->released);

        // LuaJIT has no exposed API for 64-bit int so push this as number
        addn("voice_id", s->voiceOrderAtCreate);
    }
    else
    {
        addb("is_voice", false);
    }

    // Load the macros
    lua_createtable(s->L, n_customcontrollers, 0);
    for (int i = 0; i < n_customcontrollers; ++i)
    {
        lua_pushinteger(s->L, i + 1);
        lua_pushnumber(s->L, s->macrovalues[i]);
        lua_settable(s->L, -3);
    }
    lua_setfield(s->L, -2, "macros");

    if (justSetup)
    {
        // Don't call but still clear me from the stack
        lua_pop(s->L, 2);
        return;
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
            s->adderror("The return of your Lua function must be a number or table!\nJust return "
                        "input with "
                        "output set.");
            s->isvalid = false;
            lua_pop(s->L, 1);
            return;
        }
        // Store the value and keep it on top of the stack
        lua_setglobal(s->L, s->stateName);
        lua_getglobal(s->L, s->stateName);

        lua_getfield(s->L, -1, "output");
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
                    oss << "Error with vector output!\nThe vector output must be"
                        << " an array with size up to 8. Your table contained"
                        << " index " << idx;
                    if (idx == -1)
                        oss << ", which is not an integer array index.";
                    if (idx > max_formula_outputs)
                        oss << ", which means your array is too large.";
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
                    "You must define the 'output' field in the returned table as a number or a "
                    "float array!");
            stateData.knownBadFunctions.insert(s->funcName);
            s->isvalid = false;
        };
        // pop the result and the function
        lua_pop(s->L, 1);

        auto getBoolDefault = [s](const char *n, bool def) -> bool {
            auto res = def;
            lua_getfield(s->L, -1, n);
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
        const char *err = lua_tostring(s->L, -1);
        // Fallback if error(nil)
        if (!err)
            err = "Lua error: Value is nil";
        oss << "Failed to evaluate the process() function! " << err;
        s->adderror(oss.str());
        lua_pop(s->L, 1);
        return;
    }
#endif
}

enum showFilter
{
    SHOW_ALL,
    SHOW_USER,
    SHOW_SYSTEM
};

bool isUserDefined(std::string str)
{
    static std::array<std::string, 51> keywords = {
        "amplitude",     "attack",       "block_size",
        "cc_breath",     "cc_expr",      "cc_mw",
        "cc_sus",        "chan_at",      "channel",
        "clamp_output",  "cycle",        "decay",
        "deform",        "delay",        "highest_key",
        "hold",          "intphase",     "is_rendering_to_ui",
        "is_voice",      "key",          "latest_key",
        "lowest_key",    "macros",       "mpe_bend",
        "mpe_bendrange", "mpe_enabled",  "mpe_pressure",
        "mpe_timbre",    "output",       "pb",
        "pb_range_dn",   "pb_range_up",  "phase",
        "play_mode",     "poly_at",      "poly_limit",
        "rate",          "rel_velocity", "release",
        "released",      "samplerate",   "scene_mode",
        "songpos",       "split_point",  "startphase",
        "sustain",       "tempo",        "velocity",
        "voice_count",   "voice_id",     "subscriptions"};

    auto foundInList = std::find(keywords.begin(), keywords.end(), str) != keywords.end();
    // std::cout << "isCustom " << str << " = " << foundInList << "\n";
    return !foundInList;
}

void setUserDefined(DebugRow &row, int depth, bool custom)
{

    if ((isUserDefined(row.label) && depth == 0) || custom == true)
        row.isUserDefined = true;
}

std::vector<DebugRow> createDebugDataOfModState(const EvaluatorState &es, std::string filter,
                                                bool state[8])
{
#if HAS_LUA
    std::vector<DebugRow> rows;
    Surge::LuaSupport::SGLD guard("debugViewGuard", es.L);

    std::function<void(const int, bool, bool, std::string, showFilter, int)> rec;
    rec = [&rows, &es, &rec](const int depth, bool internal, bool parentIsUser, std::string filter,
                             showFilter show, int g) {
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
                    skeys.insert(skeys.begin(), lua_tostring(es.L, -2));
                    // skeys.push_back(lua_tostring(es.L, -2));
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
                    rows.back().group = g;
                }
                else if (lua_isstring(es.L, -1))
                {
                    rows.emplace_back(depth, lab, lua_tostring(es.L, -1));
                    rows.back().group = g;
                }
                else if (lua_isboolean(es.L, -1))
                {
                    rows.emplace_back(depth, lab, (lua_toboolean(es.L, -1) ? "true" : "false"));
                    rows.back().group = g;
                }
                else if (lua_istable(es.L, -1))
                {

                    int maxDepth = parentIsUser || (isUserDefined(lab) && depth == 0) ? 2 : 2;

                    if (depth < maxDepth)
                    {
                        rows.emplace_back(depth, lab);
                        internal = internal || (lab == "subscriptions");

                        rows.back().isInternal = internal;
                        rows.back().group = g;
                        rec(depth + 1, internal, rows.back().isUserDefined, "", show, g);
                    }
                    else
                    {
                        rows.emplace_back(depth, lab, "(table)");
                        rows.back().group = g;
                    }
                }
                else if (lua_isfunction(es.L, -1))
                {
                    if (depth == 0)
                    {
                        rows.emplace_back(depth, lab, "(function)");
                        rows.back().isInternal = internal;
                    }
                }
                else if (lua_isnil(es.L, -1))
                {
                    rows.emplace_back(depth, lab, "(nil)");
                    rows.back().isInternal = internal;
                }
                else
                {
                    rows.emplace_back(depth, lab, "(unknown)");
                    rows.back().isInternal = internal;
                }
            };

            for (auto k : ikeys)
            {
                std::ostringstream oss;
                oss << "." << k;
                auto label = oss.str();

                lua_pushinteger(es.L, k);
                lua_gettable(es.L, -2);
                guts(label);
                lua_pop(es.L, 1);
            }

            for (auto s : skeys)
            {
                if (show == SHOW_ALL || (show == SHOW_USER && isUserDefined(s)) ||
                    (show == SHOW_SYSTEM && !isUserDefined(s)))
                {
                    lua_getfield(es.L, -1, s.c_str());
                    guts(s);
                    lua_pop(es.L, 1);
                }
            }
        }
    };

    struct
    {
        const char *var;
        std::string label;
        int id = 0;
        showFilter show;
    } groups[3];

    // Groups

    groups[0] = {es.stateName, "User variables", 0, SHOW_USER};
    groups[1] = {sharedTableName, "Shared", 1, SHOW_ALL};
    groups[2] = {es.stateName, "Built-in variables", 2, SHOW_SYSTEM};

    int i = 0;
    for (const auto &t : groups)
    {
        lua_getglobal(es.L, t.var);

        // create header

        rows.emplace_back(0, " " + groups[i].label, "");
        rows.back().isHeader = true;
        rows.back().group = groups[i].id;

        if (!lua_istable(es.L, -1))
        {
            if (state[i] == true)
            {

                rows.emplace_back(0, "Error", "Not a table");
                rows.back().group = groups[i].id;
            }
        }
        else
        {
            if (state[i] == true)
            {
                rec(0, false, false, filter, groups[i].show, groups[i].id);
            }
        }
        lua_pop(es.L, 1); // Pop global (table or non-table)
        i++;
    }

    // filtering

    if (filter != "")
    {
        for (int i = 0; i < rows.size(); i++)
        {
            auto search = rows[i].label.find(filter, 0);
            if ((search != std::string::npos))
            {
                rows[i].filterFlag = DebugRow::Found;

                // tag all of its children
                for (int k = i + 1; k < rows.size(); k++)
                {
                    if (rows[k].depth > rows[i].depth)
                        rows[k].filterFlag = DebugRow::Child;

                    if (rows[k].depth == rows[i].depth)
                        break;
                }
            }
        }

        for (int i = rows.size() - 1; i > -1; i--)
        {

            if (rows[i].filterFlag == DebugRow::Found)
            {

                // found
                // iterate through its parent objects until it reaches .state

                int currentDepth = rows[i].depth;

                if (currentDepth > 0)
                {
                    for (int k = i - 1; k > -1; k--)
                    {

                        if (rows[k].depth < currentDepth)
                        {
                            // parent found
                            // tag as ignore

                            currentDepth = rows[k].depth;
                            rows[k].filterFlag = DebugRow::Ignore;

                            if (currentDepth == 0)
                                break;
                        }
                    }
                }
            }
        }

        for (int i = rows.size() - 1; i > -1; i--)
        {
            if (rows[i].filterFlag != DebugRow::Found && rows[i].filterFlag != DebugRow::Ignore &&
                rows[i].filterFlag != DebugRow::Child && !rows[i].isHeader)
            {
                rows.erase(rows.begin() + i);
            }
        }
    }

    return rows;
#else
    return {};
#endif
}

std::string createDebugViewOfModState(const EvaluatorState &es)
{

    bool groupState[8] = {true, true, true, true, true, true, true, true};
    auto r = createDebugDataOfModState(es, "", groupState);
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
    fs->setFormula(R"FN(function init(state)
    -- This function is called when each Formula modulator is created (voice on, etc.)
    -- and allows you to adjust the state with pre-calculated values.
    return state
end

function process(state)
    -- This is the per-block 'process()' function.
    -- You must set the output value for the state and return it.
    -- See the tutorial patches for more info.

    state.output = state.phase * 2 - 1

    return state
end)FN");
    fs->interpreter = FormulaModulatorStorage::LUA;
}

void setupEvaluatorStateFrom(EvaluatorState &s, const SurgePatch &patch, int sceneIndex)
{
    for (int i = 0; i < n_customcontrollers; ++i)
    {
        // macros are all in scene 0
        auto ms = patch.scene[0].modsources[ms_ctrl1 + i];
        auto cms = dynamic_cast<ControllerModulationSource *>(ms);
        if (cms)
        {
            s.macrovalues[i] = cms->get_output(0);
        }
    }
    auto &scene = patch.scene[sceneIndex];
    s.pitchbend = scene.modsources[ms_pitchbend]->get_output(0);
    s.pbrange_up = (float)scene.pbrange_up.val.i * (scene.pbrange_up.extend_range ? 0.01f : 1.f);
    s.pbrange_dn = (float)scene.pbrange_dn.val.i * (scene.pbrange_dn.extend_range ? 0.01f : 1.f);

    s.mpeenabled = patch.storage->mpeEnabled;

    s.aftertouch = scene.modsources[ms_aftertouch]->get_output(0);
    s.modwheel = scene.modsources[ms_modwheel]->get_output(0);
    s.breath = scene.modsources[ms_breath]->get_output(0);
    s.expression = scene.modsources[ms_expression]->get_output(0);
    s.sustain = scene.modsources[ms_sustain]->get_output(0);
    s.lowest_key = scene.modsources[ms_lowest_key]->get_output(0);
    s.highest_key = scene.modsources[ms_highest_key]->get_output(0);
    s.latest_key = scene.modsources[ms_latest_key]->get_output(0);

    s.polylimit = patch.polylimit.val.i;
    s.scenemode = patch.scenemode.val.i;
    s.polymode = patch.scene[sceneIndex].polymode.val.i;
    s.splitpoint = patch.splitpoint.val.i;
    if (s.scenemode == sm_chsplit)
        s.splitpoint = (int)(s.splitpoint / 8 + 1);
}

void setupEvaluatorStateFrom(EvaluatorState &s, const SurgeVoice *v)
{
    s.key = v->state.key;
    s.channel = v->state.channel;
    s.velocity = v->state.velocity;
    s.releasevelocity = v->state.releasevelocity;

    s.voiceOrderAtCreate = v->state.voiceOrderAtCreate;

    s.polyat =
        v->storage
            ->poly_aftertouch[v->state.scene_id & 1][v->state.channel & 15][v->state.key & 127];
    s.mpebend = (v->state.mpeEnabled ? v->state.mpePitchBend.get_output(0) : 0);
    s.mpetimbre = (v->state.mpeEnabled ? v->timbreSource.get_output(0) : 0);
    s.mpepressure = (v->state.mpeEnabled ? v->monoAftertouchSource.get_output(0) : 0);
    s.mpebendrange = (v->state.mpeEnabled ? v->state.mpePitchBendRange : 0);
}

std::variant<float, std::string, bool> runOverModStateForTesting(const std::string &query,
                                                                 const EvaluatorState &es)
{
#if HAS_LUA
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
        lua_pop(es.L, 1);
        return false;
    }

    lua_pcall(es.L, 1, 1, 0);

    if (lua_isnumber(es.L, -1))
    {
        auto res = lua_tonumber(es.L, -1);
        lua_pop(es.L, 1);
        return (float)res;
    }

    if (lua_isboolean(es.L, -1))
    {
        auto res = lua_toboolean(es.L, -1);
        lua_pop(es.L, 1);
        return (float)res;
    }

    if (lua_isstring(es.L, -1))
    {
        auto res = lua_tostring(es.L, -1);
        lua_pop(es.L, 1);
        return res;
    }
    lua_pop(es.L, 1); // Pop evaluator state
#endif
    return false;
}

std::variant<float, std::string, bool> extractModStateKeyForTesting(const std::string &key,
                                                                    const EvaluatorState &s)
{
    auto query = fmt::format(R"FN(
function query(state)
   return state.{};
end
)FN",
                             key);
    return runOverModStateForTesting(query, s);
}

} // namespace Formula
} // namespace Surge
