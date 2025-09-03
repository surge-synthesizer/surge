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

#include "WavetableScriptEvaluator.h"
#include "LuaSupport.h"
#include "lua/LuaSources.h"
#include "tinyxml/tinyxml.h"

// #define LOG(...) std::cout << __FILE__ << ":" << __LINE__ << " " << __VA_ARGS__ << std::endl;
#define LOG(...)

namespace Surge
{
namespace WavetableScript
{

static constexpr const char *statetable{"statetable"};

#if HAS_LUA
struct LuaWTEvaluator::Details
{
    SurgeStorage *storage{nullptr};
    std::string script{};
    size_t resolution{2048};
    size_t frameCount{10};

    bool isValid{false};
    std::vector<std::optional<frame_t>> frameCache;
    std::string wtName{"Scripted Wavetable"};
    void prepareIfInvalid();

    lua_State *L{nullptr};

    void invalidate()
    {
        isValid = false;
        frameCache.clear();
    }
    void makeEmptyState(bool pushToGlobal)
    {
        lua_createtable(L, 0, 10);
        lua_pushinteger(L, frameCount);
        lua_setfield(L, -2, "frame_count");
        lua_pushinteger(L, resolution);
        lua_setfield(L, -2, "sample_count");

        if (pushToGlobal)
            lua_setglobal(L, statetable);
    }

    LuaWTEvaluator::frame_t generateScriptAtFrame(size_t frame)
    {
        LOG("generateScriptAtFrame " << frame);
        auto &eqn = script;

        if (!makeValid())
            return std::nullopt;

        LuaWTEvaluator::frame_t res{std::nullopt};
        auto values = std::vector<float>();

        auto wgp = Surge::LuaSupport::SGLD("WavetableScript::evaluateInner", L);
        lua_getglobal(L, "generate");
        if (!lua_isfunction(L, -1))
        {
            // Just return here, we get a more helpful error message in makeValid()
            lua_pop(L, 1); // pop the generate non-function
            return std::nullopt;
        }
        Surge::LuaSupport::setSurgeFunctionEnvironment(L);

        lua_createtable(L, 0, 10);
        int tidx = lua_gettop(L); // Get the global table
        lua_getglobal(L, statetable);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 2); // pop tables
            return std::nullopt;
        }
        // Copy all key-value pairs
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            // Stack: new table, global table, key, value
            lua_pushvalue(L, -2);  // Duplicate key
            lua_pushvalue(L, -2);  // Duplicate value
            lua_settable(L, tidx); // New table[key] = value
            lua_pop(L, 1);         // Pop original value, keep key for next iteration
        }
        lua_pop(L, 1); // pop global table
        lua_pushinteger(L, frame + 1);
        lua_setfield(L, tidx, "frame");
        lua_pushinteger(L, frameCount);
        lua_setfield(L, tidx, "frame_count");
        lua_pushinteger(L, resolution);
        lua_setfield(L, tidx, "sample_count");

        // So stack is now the table and the function
        auto pcr = lua_pcall(L, 1, 1, 0);
        if (pcr == LUA_OK)
        {
            if (lua_istable(L, -1))
            {
                bool gen{true};
                for (auto i = 0; i < resolution && gen; ++i)
                {
                    lua_pushinteger(L, i + 1);
                    lua_gettable(L, -2);
                    if (lua_isnumber(L, -1))
                    {
                        values.push_back(lua_tonumber(L, -1));
                    }
                    else
                    {
                        values.push_back(0.f);
                        gen = false;
                    }
                    lua_pop(L, 1);
                }
                if (gen)
                    res = values;
            }
        }
        else
        {
            // If pcr is not LUA_OK then lua pushes an error string onto the stack. Show this error
            std::ostringstream oss;
            const char *err = lua_tostring(L, -1);
            // Fallback if error(nil)
            if (!err)
                err = "Lua error: Value is nil.";
            oss << "Failed to evaluate the generate() function!\n" << err;

            if (storage)
                storage->reportError(oss.str(), "Wavetable Evaluator Runtime Error");
            else
                std::cerr << oss.str();
        }
        lua_pop(L, 1); // Error string or pcall result

        return res;
    }
    void callInitFn()
    {
        LOG("callInitFn");
        auto wg = Surge::LuaSupport::SGLD("WavetableScript::details::callInitFn", L);

        lua_getglobal(L, "init");
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, -1);
            makeEmptyState(true);
        }
        else
        {
            Surge::LuaSupport::setSurgeFunctionEnvironment(L);

            makeEmptyState(false);

            auto res = lua_pcall(L, 1, 1, 0);
            if (res == LUA_OK)
            {
                if (lua_istable(L, -1))
                {
                    lua_setglobal(L, statetable);
                }
                else
                {
                    if (storage)
                        storage->reportError("Init function returned a non-table.",
                                             "Wavetable Script Evaluator");
                    makeEmptyState(true);
                }
            }
            else
            {
                std::ostringstream oss;
                const char *err = lua_tostring(L, -1);
                // Fallback if error(nil)
                if (!err)
                    err = "Lua error: Value is nil.";
                oss << "Failed to evaluate init() function!\n" << err;
                if (storage)
                    storage->reportError(oss.str(), "Wavetable Evaluator Init Error");
                else
                    std::cerr << oss.str();
                lua_pop(L, -1);

                makeEmptyState(true);
            }
        }
    }

    bool makeValid()
    {
        if (L == nullptr)
        {
            LOG("creating Lua State ");

            L = luaL_newstate();
            luaL_openlibs(L);

            auto wg = Surge::LuaSupport::SGLD("WavetableScript::prelude", L);

            Surge::LuaSupport::loadSurgePrelude(L, Surge::LuaSources::wtse_prelude);
        }

        if (!isValid)
        {
            LOG("Validating");

            {
                // Have a separate guard for this just to make sure I match
                auto lwg = Surge::LuaSupport::SGLD("WavetableScript::details::clearGlobals", L);
                lua_pushnil(L);
                lua_setglobal(L, "generate");
                lua_pushnil(L);
                lua_setglobal(L, "init");
                lua_pushnil(L);
                lua_setglobal(L, statetable);
                wtName = "Scripted Wavetable";

                frameCache.clear();
                for (int i = 0; i < frameCount; ++i)
                    frameCache.push_back(std::nullopt);
            }

            auto wg = Surge::LuaSupport::SGLD("WavetableScript::details::makeValid", L);
            std::string emsg;
            auto res = Surge::LuaSupport::parseStringDefiningMultipleFunctions(
                L, script, {"init", "generate"}, emsg);
            if (!res)
            {
                if (storage)
                {
                    std::ostringstream oss;
                    oss << "Unable to determine generate() or init() function!";
                    if (!emsg.empty())
                        oss << "\n" << emsg;
                    storage->reportError(oss.str(), "Wavetable Script Parse Error");
                }
            }
            lua_pop(L, 2); // remove the 2 functions added in the global state

            callInitFn();

            {
                auto wgn =
                    Surge::LuaSupport::SGLD("WavetableScript::details::makeValid::wtName", L);
                lua_getglobal(L, statetable);
                if (lua_istable(L, -1))
                {
                    lua_getfield(L, -1, "name");
                    if (lua_isstring(L, -1))
                    {
                        wtName = lua_tostring(L, -1);
                    }

                    lua_pop(L, -1);
                }

                lua_pop(L, -1);
            }

            isValid = true;

            return res;
        }
        return true;
    }
};
#else
struct LuaWTEvaluator::Details
{
};
#endif

LuaWTEvaluator::LuaWTEvaluator() { details = std::make_unique<Details>(); }

LuaWTEvaluator::~LuaWTEvaluator() = default;

void LuaWTEvaluator::setStorage(SurgeStorage *s)
{
#if HAS_LUA
    details->storage = s;
#endif
}
void LuaWTEvaluator::setScript(const std::string &e)
{
#if HAS_LUA
    if (e != details->script)
    {
        details->script = e;
        details->invalidate();
    }
#endif
}
void LuaWTEvaluator::setResolution(size_t r)
{
#if HAS_LUA
    if (r != details->resolution)
    {
        details->resolution = r;
        details->invalidate();
    }
#endif
}
void LuaWTEvaluator::setFrameCount(size_t n)
{
#if HAS_LUA
    if (n != details->frameCount)
    {
        details->invalidate();
        details->frameCount = n;
    }
#endif
}

LuaWTEvaluator::frame_t LuaWTEvaluator::getFrame(size_t frame)
{
#if HAS_LUA
    if (!details->makeValid())
        return std::nullopt;
    if (frame > details->frameCount)
        return std::nullopt;
    assert(frame < details->frameCache.size());
    if (!details->frameCache[frame].has_value())
    {
        details->frameCache[frame] = details->generateScriptAtFrame(frame);
    }
    if (details->frameCache[frame].has_value())
    {
        return *(details->frameCache[frame]);
    }
    return std::nullopt;

#else
    return std::nullopt;
#endif
}

bool LuaWTEvaluator::populateWavetable(wt_header &wh, float **wavdata)
{
#if HAS_LUA
    if (!details->makeValid())
        return false;

    auto resolution = details->resolution;
    auto frames = details->frameCount;

    auto wd = new float[frames * resolution];
    wh.n_samples = resolution;
    wh.n_tables = frames;
    wh.flags = 0;
    *wavdata = wd;

    for (int i = 0; i < frames; ++i)
    {
        auto v = getFrame(i);
        if (v.has_value())
        {
            memcpy(&(wd[i * resolution]), &((*v)[0]), resolution * sizeof(float));
        }
        else
        {
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

void LuaWTEvaluator::generateWavetable(SurgeStorage *storage, OscillatorStorage *oscdata)
{
#if HAS_LUA
    auto res_base = oscdata->wavetable_formula_res_base;
    auto nframes = oscdata->wavetable_formula_nframes;
    auto script = oscdata->wavetable_formula;

    auto respt = 32;
    for (int i = 1; i < res_base; ++i)
        respt *= 2;

    setStorage(storage);
    setScript(script);
    setResolution(respt);
    setFrameCount(nframes);

    wt_header wh;
    float *wd = nullptr;

    if (populateWavetable(wh, &wd))
    {
        storage->waveTableDataMutex.lock();
        oscdata->wt.BuildWT(wd, wh, wh.flags & wtf_is_sample);
        oscdata->wavetable_display_name = getSuggestedWavetableName();
        storage->waveTableDataMutex.unlock();

        delete[] wd;
    }
#endif
}

void LuaWTEvaluator::loadWtscript(const fs::path &filename, SurgeStorage *storage,
                                  OscillatorStorage *oscdata)
{
#if HAS_LUA
    TiXmlDocument doc;
    if (!doc.LoadFile(filename))
    {
        storage->reportError("Failed to load XML file.", "XML Load Error");
        return;
    }

    auto wtscript = TINYXML_SAFE_TO_ELEMENT(doc.FirstChildElement("wtscript"));
    if (!wtscript)
    {
        storage->reportError("No root <wtscript> element found.", "XML Load Error");
        return;
    }

    auto wavetable_script = TINYXML_SAFE_TO_ELEMENT(wtscript->FirstChildElement("script"));
    if (!wavetable_script)
    {
        storage->reportError("No <wavetable_script> element found.", "XML Load Error");
        return;
    }

    auto b64script = wavetable_script->Attribute("lua");
    if (!b64script || std::strlen(b64script) == 0)
    {
        storage->reportError("Empty or missing lua attribute in <wavetable_script>.",
                             "XML Load Error");
        return;
    }

    int nframes = 0;
    if (wavetable_script->QueryIntAttribute("frames", &nframes) != TIXML_SUCCESS)
    {
        storage->reportError("Missing or invalid frames attribute.", "XML Load Error");
        return;
    }

    int res_base = 0;
    if (wavetable_script->QueryIntAttribute("samples", &res_base) != TIXML_SUCCESS)
    {
        storage->reportError("Missing or invalid samples attribute.", "XML Load Error");
        return;
    }

    oscdata->wavetable_formula_nframes = nframes;
    oscdata->wavetable_formula_res_base = res_base;
    oscdata->wavetable_formula = Surge::Storage::base64_decode(b64script);

    generateWavetable(storage, oscdata);
#endif
}

std::string LuaWTEvaluator::getSuggestedWavetableName()
{
#if HAS_LUA
    details->makeValid();
    return details->wtName;
#else
    return "";
#endif
}

std::string LuaWTEvaluator::defaultWavetableScript()
{
    return R"FN(
-- This script serves as the default example for the wavetable script editor. Unlike the formula editor, which executes
-- repeatedly every block, the Lua code here runs only upon applying new settings or receiving GUI inputs like the frame
-- slider.
--
-- When the Generate button is pressed, this function is called for each frame, and the results are collected and sent
-- to the Wavetable oscillator. The oscillator can sweep through these frames to evolve the sound produced using the
-- Morph parameter.
--
-- The for loops iterate over an array of sample values (phase) and a frame number (n) and generate the result for the n-th
-- frame. This example uses additive synthesis, a technique that adds sine waves to create waveshapes. The initial frame
-- starts with a single sine wave, and additional sine waves are added in subsequent frames. This process creates a Fourier
-- series sawtooth wave defined by the formula: sum 2 / pi n * sin n x. See the tutorial scripts for more info.
--
-- The first time the script is loaded, the engine will call the 'init' function and the resulting state it provides
-- will be available in every subsequent call as the variables provided in the wt table.

function init(wt)
    -- wt will have frame_count and sample_count defined
    wt.name = "Fourier Saw"
    wt.phase = math.linspace(0.0, 1.0, wt.sample_count)
    return wt
end

function generate(wt)
    -- wt will have frame_count, sample_count, frame, and any item from init defined
    local res = {}

    for i,x in ipairs(wt.phase) do
        local lv = 0
        for q = 1,(wt.frame) do
            lv = lv + 2 * sin(2 * pi * q * x) / (pi * q)
        end
        res[i] = lv
    end
    return res
end
)FN";
}
} // namespace WavetableScript
} // namespace Surge
