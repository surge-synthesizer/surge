// SPDX-License-Identifier: GPL-3.0-or-later
#include "LuaSupport.h"
#include "dsp/modulators/FormulaModulationHelper.h"
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>

int checkLuaCompilation(const char *resources)
{
#if HAS_LUA
    try
    {
        const auto require = [](bool condition, const char *message) {
            if (!condition) throw std::runtime_error(message);
        };
        std::unique_ptr<lua_State, decltype(&lua_close)> owner(luaL_newstate(), &lua_close);
        auto *L = owner.get();
        require(L != nullptr, "Unable to allocate Lua state");
        Surge::LuaSupport::openLibraries(L);
        const std::vector<std::string> names{"process", "init", "missing"};
        const std::string source =
            "marker = (marker or 0) + 1 "
            "function process(n) return n + marker end function init(n) return n end";
        std::string error;
        lua_pushstring(L, "stack sentinel");
        require(Surge::LuaSupport::compileString(L, source, error), "Valid compilation failed");
        require(lua_gettop(L) == 2 && lua_isfunction(L, -1), "Compilation stack mismatch");
        const int reference = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_getglobal(L, "marker");
        require(lua_isnil(L, -1), "Compilation executed top-level code");
        lua_pop(L, 1);

        // Exclusive state ownership transfers to the evaluator after compilation.
        // The chunk must observe this later value, not the state at compile time.
        lua_pushnumber(L, 41);
        lua_setglobal(L, "marker");
        std::exception_ptr failure;
        std::thread evaluator([&] {
            try
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, reference);
                luaL_unref(L, LUA_REGISTRYINDEX, reference);
                require(Surge::LuaSupport::evaluateCompiledFunctions(L, names, error) == 2,
                        "Deferred function discovery failed");
                require(lua_gettop(L) == 4 && lua_isfunction(L, -1) &&
                        lua_isfunction(L, -2) && lua_isnil(L, -3), "Deferred function order changed");
                lua_pushnumber(L, 3);
                require(lua_pcall(L, 1, 1, 0) == LUA_OK && lua_tonumber(L, -1) == 45,
                        "Deferred execution did not observe current globals");
                lua_settop(L, 1);
            }
            catch (...) { failure = std::current_exception(); }
        });
        evaluator.join();
        if (failure) std::rethrow_exception(failure);
        lua_getglobal(L, "marker");
        require(lua_tonumber(L, -1) == 42, "Top-level code did not run exactly once");
        lua_pop(L, 1);

        error.clear();
        require(!Surge::LuaSupport::compileString(L, "function process( broken", error),
                "Invalid syntax compiled");
        require(lua_gettop(L) == 1 && error.find("Lua syntax error:") == 0,
                "Compile failure changed stack or diagnostics");

        error.clear();
        require(Surge::LuaSupport::compileString(L, "error('deferred failure')", error),
                "Runtime error executed during compilation");
        require(Surge::LuaSupport::evaluateCompiledFunctions(L, names, error) == 0,
                "Runtime error was accepted");
        require(error.find("Lua evaluation error:") == 0 &&
                error.find("deferred failure") != std::string::npos && lua_gettop(L) == 4,
                "Runtime error changed stack or diagnostics");
        for (int i = 1; i <= 3; ++i) require(lua_isnil(L, -i), "Runtime error omitted nil result");
        lua_pop(L, 3);

        lua_pushnumber(L, 7);
        require(Surge::LuaSupport::evaluateCompiledFunctions(L, names, error) == 0 &&
                error.find("Missing compiled script chunk") != std::string::npos &&
                lua_gettop(L) == 4, "Invalid compiled chunk changed stack contract");
        lua_pop(L, 3);
        error.clear();
        require(Surge::LuaSupport::parseStringDefiningMultipleFunctions(L, source, names, error) == 2,
                "Synchronous compatibility helper failed");
        require(lua_gettop(L) == 4 && lua_isfunction(L, -1) &&
                lua_isfunction(L, -2) && lua_isnil(L, -3), "Synchronous function order changed");
        lua_pop(L, 3);
        lua_getglobal(L, "marker");
        require(lua_tonumber(L, -1) == 43, "Synchronous execution changed");
        lua_pop(L, 1);
        require(std::string(lua_tostring(L, 1)) == "stack sentinel", "Caller stack was overwritten");

        int finalizedStates = 0;
        auto storage = std::make_unique<SurgeStorage>(resources);
        auto &formula = storage->getPatch().formulamods[0][0];
        const std::string formulaSource =
            "compilation_marker = (compilation_marker or 0) + 1 "
            "local captured = compilation_marker "
            "function init(s) shared.calls = (shared.calls or 0) + 1 "
            "s.marker = captured s.calls = shared.calls return s end "
            "function process(s) s.output = s.marker / 1000 return s end";
        for (bool display : {false, true})
        {
            auto &global = *storage->formulaGlobalData;
            auto &cache = global.functions(display);
            formula.setFormula(formulaSource);
            require(Surge::Formula::prepareCompilation(storage.get(), &formula, display),
                    "Formula precompilation failed");
            const auto firstCount = cache.compilationAttempts.load(std::memory_order_relaxed);
            require(firstCount == 1, "Unexpected formula compilation count");
            require(Surge::Formula::prepareCompilation(storage.get(), &formula, display) &&
                    cache.compilationAttempts == firstCount, "Identical preparation recompiled");
            auto *state = static_cast<lua_State *>(display ? global.displayState : global.audioState);
            lua_getglobal(state, "compilation_marker");
            require(lua_isnil(state, -1), "Formula preparation executed user code");
            lua_pop(state, 1);
            lua_pushnumber(state, 41); lua_setglobal(state, "compilation_marker");

            Surge::Formula::EvaluatorState held{}, replacement{}, invalid{};
            Surge::Formula::initEvaluatorState(held);
            Surge::Formula::initEvaluatorState(replacement);
            Surge::Formula::initEvaluatorState(invalid);
            Surge::Formula::prepareForEvaluation(storage.get(), &formula, held, display);
            require(held.isvalid && cache.compilationAttempts == firstCount,
                    "Prepared formula recompiled on evaluation");
            float output[Surge::Formula::max_formula_outputs]{};
            Surge::Formula::valueAt(0, 0, storage.get(), &formula, &held, output);
            require(std::abs(output[0] - 0.042f) < 1e-6f, "Prepared formula captured early globals");

            lua_pushnumber(state, 100); lua_setglobal(state, "compilation_marker");
            formula.setFormula(formulaSource + "\n-- replacement");
            Surge::Formula::requestSharedDataWipe(storage.get());
            require(Surge::Formula::prepareCompilation(storage.get(), &formula, display),
                    "Replacement compilation failed");
            auto &wipe = display ? global.displaySharedWipeRequested : global.audioSharedWipeRequested;
            require(wipe.load(), "Compilation consumed a pending shared reset");
            Surge::Formula::valueAt(0, 0, storage.get(), &formula, &held, output);
            require(std::abs(output[0] - 0.042f) < 1e-6f, "Compilation changed a held evaluator");
            const auto replacementCount = cache.compilationAttempts.load(std::memory_order_relaxed);
            Surge::Formula::prepareForEvaluation(storage.get(), &formula, replacement, display);
            require(replacement.isvalid && !wipe.load() && cache.compilationAttempts == replacementCount,
                    "Replacement evaluation recompiled or missed its shared reset");
            const auto calls = Surge::Formula::extractModStateKeyForTesting("calls", replacement);
            require(std::get_if<float>(&calls) && std::get<float>(calls) == 1,
                    "Shared reset did not occur at evaluation time");
            Surge::Formula::valueAt(0, 0, storage.get(), &formula, &replacement, output);
            require(std::abs(output[0] - 0.101f) < 1e-6f, "Replacement executed at the wrong time");
            Surge::Formula::valueAt(0, 0, storage.get(), &formula, &held, output);
            require(std::abs(output[0] - 0.042f) < 1e-6f, "Replacement invalidated a held function");

            formula.setFormula("function process( broken");
            require(!Surge::Formula::prepareCompilation(storage.get(), &formula, display),
                    "Invalid formula precompiled");
            const auto invalidCount = cache.compilationAttempts.load(std::memory_order_relaxed);
            Surge::Formula::prepareForEvaluation(storage.get(), &formula, invalid, display);
            require(!invalid.isvalid && invalid.error &&
                    invalid.error->find("Lua syntax error:") != std::string::npos &&
                    cache.compilationAttempts == invalidCount, "Cached syntax failure lost diagnostics or recompiled");

            lua_gc(state, LUA_GCCOLLECT, 0);
            const auto memoryBefore = lua_gc(state, LUA_GCCOUNT, 0);
            for (int i = 0; i < 1000; ++i)
            {
                formula.setFormula(formulaSource + "\n-- unused replacement " + std::to_string(i));
                require(Surge::Formula::prepareCompilation(storage.get(), &formula, display),
                        "Repeated preparation failed");
            }
            lua_gc(state, LUA_GCCOLLECT, 0);
            require(lua_gc(state, LUA_GCCOUNT, 0) <= memoryBefore + 16,
                    "Superseded compiled chunks accumulated in the Lua registry");
            Surge::Formula::cleanEvaluatorState(held);
            Surge::Formula::cleanEvaluatorState(replacement);
            Surge::Formula::cleanEvaluatorState(invalid);

            // Retain a finalizer until state destruction. Prepared registry
            // chunks and the rest of each interpreter must die with storage.
            auto **counter = static_cast<int **>(lua_newuserdata(state, sizeof(int *)));
            *counter = &finalizedStates;
            lua_newtable(state);
            lua_pushcfunction(state, [](lua_State *L) -> int {
                ++**static_cast<int **>(lua_touserdata(L, 1));
                return 0;
            });
            lua_setfield(state, -2, "__gc");
            lua_setmetatable(state, -2);
            lua_setglobal(state, "compilation_lifetime_sentinel");
        }
        require(finalizedStates == 0, "Formula interpreter finalized before storage release");
        storage.reset();
        require(finalizedStates == 2, "Formula interpreters leaked after storage release");
        std::puts("Lua compilation preserved deferred execution, ownership and error stack contracts");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "Lua compilation check: %s\n", error.what());
        return 1;
    }
#else
    std::fputs("Lua compilation check requires Lua support\n", stderr);
    return 1;
#endif
}
