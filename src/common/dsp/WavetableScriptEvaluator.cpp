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

#include "WavetableScriptEvaluator.h"
#include "LuaSupport.h"

namespace Surge
{
namespace WavetableScript
{
std::vector<float> evaluateScriptAtFrame(const std::string &eqn, int resolution, int frame,
                                         int nFrames)
{
    static lua_State *L = nullptr;
    if (L == nullptr)
    {
        L = lua_open();
        luaL_openlibs(L);
    }

    auto values = std::vector<float>();

    auto wg = Surge::LuaSupport::SGLD("WavetableScript::evaluate", L);
    std::string emsg;
    auto res = Surge::LuaSupport::parseStringDefiningFunction(L, eqn.c_str(), "generate", emsg);
    if (res)
    {
        Surge::LuaSupport::setSurgeFunctionEnvironment(L);
        /*
         * Alright so we want the stack to be the config table which
         * contains the xs, contains n, contains ntables, etc.. so
         */
        lua_createtable(L, 0, 10);

        // xs is an array of the x locations in phase space
        lua_pushstring(L, "xs");
        lua_createtable(L, resolution, 0);
        double dp = 1.0 / (resolution - 1);
        for (auto i = 0; i < resolution; ++i)
        {
            lua_pushinteger(L, i + 1); // lua has a 1 based convention
            lua_pushnumber(L, i * dp);
            lua_settable(L, -3);
        }
        lua_settable(L, -3);

        lua_pushstring(L, "n");
        lua_pushinteger(L, frame);
        lua_settable(L, -3);

        lua_pushstring(L, "nTables");
        lua_pushinteger(L, nFrames);
        lua_settable(L, -3);

        // So stack is now the table and the function
        auto pcr = lua_pcall(L, 1, 1, 0);
        if (pcr == LUA_OK)
        {
            if (lua_istable(L, -1))
            {
                for (auto i = 0; i < resolution; ++i)
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
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
    }
    else
    {
        std::cout << emsg << std::endl;
        lua_pop(L, 1);
    }
    return values;
}

bool constructWavetable(const std::string &eqn, int resolution, int frames, wt_header &wh,
                        float **wavdata)
{
    auto wd = new float[frames * resolution];
    wh.n_samples = resolution;
    wh.n_tables = frames;
    wh.flags = 0;
    *wavdata = wd;

    for (int i = 0; i < frames; ++i)
    {
        auto v = evaluateScriptAtFrame(eqn, resolution, i, frames);
        memcpy(&(wd[i * resolution]), &(v[0]), resolution * sizeof(float));
    }
    return true;
}
std::string defaultWavetableFormula()
{
    return R"FN(function generate(config)
--- This function was inserted as a guide, since the wavetable editor in this patch/oscillator has no
--- generator function. The function takes an array of x values (xs) and a frame number (n) and
--- generates the result as the n-th frame. The sample below generates a Fourier sine to saw
--- which, remember, is: sum 2 / pi n * sin n x
    res = {}
    for i,x in ipairs(config.xs) do
        lv = 0
        for q = 1,(config.n+1) do
            lv = lv + 2 * sin ( q * x * 2 * pi ) / ( pi * q )
        end
        res[i] = lv
    end
    return res
end)FN";
}
} // namespace WavetableScript
} // namespace Surge
