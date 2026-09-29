// SPDX-License-Identifier: GPL-3.0-or-later
#include "Engine.h"
#include "PatchFileValidation.h"
#include "SurgeStorage.h"
#include "dsp/WavetableScriptEvaluator.h"
#include "dsp/modulators/FormulaModulationHelper.h"
#include <fstream>
#include "binn/binn.h"
#include "zstd.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>
#include <thread>
#include <atomic>
int checkLuaCompilation(const char *resources);
int main(int argc, char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--check-lua-compilation")
        return checkLuaCompilation(argv[2]);
    if (argc == 3 && std::string(argv[1]) == "--check-formula-contexts")
    {
        auto storage = std::make_unique<SurgeStorage>(argv[2]);
        auto &formulas = storage->getPatch().formulamods[0];
        for (int i = 0; i < 4; ++i)
            formulas[i].setFormula(i % 2 == 0
                ? "function init(s) shared.counter = (shared.counter or 0) + 1 s.counter = shared.counter return s end "
                  "function process(s) s.output = 0.125 return s end"
                : "function init(s) return 1 end function process(s) return s end");
        std::atomic<int> ready{0}, midpoint{0}, resetReady{0}, failures{0};
        const auto run = [&](bool display) {
            ready.fetch_add(1);
            while (ready.load() != 2) std::this_thread::yield();
            for (int i = 0; i < 1000; ++i)
            {
                if (i == 500)
                {
                    midpoint.fetch_add(1);
                    while (midpoint.load() != 2) std::this_thread::yield();
                    if (!display) Surge::Formula::requestSharedDataWipe(storage.get());
                    resetReady.fetch_add(1);
                    while (resetReady.load() != 2) std::this_thread::yield();
                }
                Surge::Formula::EvaluatorState valid{}, invalid{};
                Surge::Formula::initEvaluatorState(valid);
                Surge::Formula::initEvaluatorState(invalid);
                const int index = display ? 2 : 0;
                Surge::Formula::prepareForEvaluation(storage.get(), &formulas[index], valid, display);
                if (!valid.isvalid) ++failures;
                const auto counter = Surge::Formula::extractModStateKeyForTesting("counter", valid);
                const auto *number = std::get_if<float>(&counter);
                if (!number || *number != i % 500 + 1) ++failures;
                float output[Surge::Formula::max_formula_outputs]{};
                Surge::Formula::valueAt(0, 0, storage.get(), &formulas[index], &valid, output);
                if (output[0] != 0.125f) ++failures;
                Surge::Formula::prepareForEvaluation(storage.get(), &formulas[index + 1], invalid, display);
                if (invalid.isvalid) ++failures;
                if (i == 0 && (!invalid.raisedError || !invalid.error ||
                    invalid.error->find("must return a table") == std::string::npos)) ++failures;
                Surge::Formula::cleanEvaluatorState(valid);
                Surge::Formula::cleanEvaluatorState(invalid);
            }
        };
        std::thread audio(run, false), display(run, true);
        audio.join(); display.join();
        if (failures.load()) { std::fprintf(stderr, "%d formula context failures\n", failures.load()); return 1; }
        std::puts("Concurrent formula contexts preserved independent caches and shared tables");
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--check-script-import")
    {
        auto storage = std::make_unique<SurgeStorage>(argv[2]);
        auto &osc = storage->getPatch().scene[0].osc[0];
        osc.wavetable_script = "original script";
        osc.wavetable_script_nframes = 7;
        osc.wavetable_script_res_base = 3;
        osc.wtSnapshots[0] = std::make_unique<Wavetable>();
        std::array<float, 64> samples{};
        samples[16] = 0.5f;
        wt_header header{}; header.n_samples = 64; header.n_tables = 1;
        if (!osc.wtSnapshots[0]->BuildWT(samples.data(), header, false)) return 1;
        auto *snapshot = osc.wtSnapshots[0].get();
        const auto version = osc.wtSnapshotsVersion;
        const auto path = fs::path(argv[2]) / "invalid.wtscript";
        { std::ofstream out(path); out << "<wtscript><broken/>"; }
        std::string error;
        if (Surge::WavetableScript::LuaWTEvaluator::loadWtscriptMetadata(path, storage.get(), &osc, &error) ||
            error.empty() || osc.wtSnapshots[0].get() != snapshot ||
            osc.wtSnapshotsVersion != version || osc.wavetable_script != "original script" ||
            osc.wavetable_script_nframes != 7 || osc.wavetable_script_res_base != 3)
        {
            fprintf(stderr, "Invalid script import changed existing metadata or snapshots\n");
            return 1;
        }
        // Produce the same binary container as the desktop script exporter.
        const std::string script = "function init(wt) wt.name=\"Snapshot import\" return wt end "
                                   "function generate(wt) return wt.snapshot[1][1] end";
        const auto encoded = Surge::Storage::base64_encode(
            reinterpret_cast<const unsigned char *>(script.data()), script.size());
        const std::string xml = "<wtscript><script lua=\"" + encoded +
                                "\" frames=\"1\" samples=\"2\"/></wtscript>";
        auto *object = binn_object();
        if (!SurgePatch::writeOscSnapshotsToBinn(object, osc)) { binn_free(object); return 1; }
        std::vector<char> compressed(ZSTD_compressBound(binn_size(object)));
        const auto size = ZSTD_compress(compressed.data(), compressed.size(),
                                        binn_ptr(object), binn_size(object), 3);
        binn_free(object);
        if (ZSTD_isError(size)) return 1;
        const auto valid = fs::path(argv[2]) / "snapshot.wtscript";
        {
            std::ofstream out(valid, std::ios::binary);
            out.write("wts1", 4);
            for (uint32_t length : {uint32_t(xml.size()), uint32_t(size)})
                for (int byte = 0; byte < 4; ++byte) out.put(char((length >> (byte * 8)) & 255));
            out.write(xml.data(), xml.size());
            out.write(compressed.data(), size);
        }
        error.clear();
        if (!Surge::WavetableScript::LuaWTEvaluator::loadWtscriptMetadata(valid, storage.get(), &osc, &error) ||
            !error.empty() || osc.wavetable_script != script || osc.wavetable_script_nframes != 1 ||
            osc.wavetable_script_res_base != 2 || osc.wtSnapshotsVersion != version + 1 ||
            !osc.wtSnapshots[0] || osc.wtSnapshots[0]->size != 64)
            return 1;
        for (int i = 0; i < 64; ++i)
            if (osc.wtSnapshots[0]->TableF32WeakPointers[0][0][i] != samples[i]) return 1;
        printf("Binary script import retained exact snapshot samples\n");
        printf("Invalid script import retained metadata and snapshots\n");
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--validate-factory")
    {
        int checked = 0, failed = 0;
        for (const auto &entry : fs::recursive_directory_iterator(argv[2]))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".fxp") continue;
            std::vector<char> bytes;
            std::string error;
            ++checked;
            if (!Surge::PatchStorage::readValidatedPatch(entry.path(), bytes, error))
            {
                ++failed;
                fprintf(stderr, "%s: %s\n", entry.path().string().c_str(), error.c_str());
            }
        }
        printf("Validated %d factory patches; %d rejected\n", checked, failed);
        return failed || !checked ? 1 : 0;
    }
    const bool effectOnly = argc >= 6 && std::string(argv[1]) == "--effect-only";
    if (effectOnly) { --argc; ++argv; }
    const bool audioInput = argc >= 6 && std::string(argv[1]) == "--audio-input";
    if (audioInput) { --argc; ++argv; }
    const bool convolution = argc == 6 && std::string(argv[1]) == "--convolution";
    if (convolution) { --argc; ++argv; }
    int effectType = -1;
    if (argc >= 7 && std::string(argv[1]) == "--effect-type")
    {
        char *end = nullptr;
        const long parsed = std::strtol(argv[2], &end, 10);
        if (end == argv[2] || *end || parsed < 0 || parsed >= n_fx_types) return 2;
        effectType = static_cast<int>(parsed);
        argc -= 2; argv += 2;
    }
    int oscillatorType = -1;
    if (argc >= 7 && std::string(argv[1]) == "--oscillator-type")
    {
        char *end = nullptr;
        const long parsed = std::strtol(argv[2], &end, 10);
        if (end == argv[2] || *end || parsed < 0 || parsed >= n_osc_types) return 2;
        oscillatorType = static_cast<int>(parsed);
        argc -= 2; argv += 2;
    }
    bool seedStorage = false;
    uint32_t storageSeed = 0;
    bool seedMseg = false;
    uint32_t msegSeed = 0;
    const auto readSeed = [&](const char *option, bool &enabled, uint32_t &seed) {
        if (argc < 7 || std::string(argv[1]) != option) return true;
        const std::string value(argv[2]);
        if (value.empty() || value.size() > 10 || value.find_first_not_of("0123456789") != std::string::npos)
            return false;
        const auto parsed = std::strtoull(argv[2], nullptr, 10);
        if (parsed > UINT32_MAX) return false;
        seed = static_cast<uint32_t>(parsed);
        enabled = true;
        argc -= 2; argv += 2;
        return true;
    };
    if (!readSeed("--storage-seed", seedStorage, storageSeed) ||
        !readSeed("--mseg-seed", seedMseg, msegSeed)) return 2;
    std::vector<std::pair<int, float>> effectParameters;
    while (argc >= 8 && std::string(argv[1]) == "--effect-parameter")
    {
        char *end = nullptr;
        const long parsed = std::strtol(argv[2], &end, 10);
        if (end == argv[2] || *end || parsed < 0 || parsed >= n_fx_params) return 2;
        const auto effectValue = std::strtof(argv[3], &end);
        if (end == argv[3] || *end || !std::isfinite(effectValue) || effectValue < 0 || effectValue > 1)
            return 2;
        effectParameters.emplace_back(static_cast<int>(parsed), effectValue);
        argc -= 3; argv += 3;
    }
    if (argc != 5 || (effectOnly && (audioInput || oscillatorType >= 0 || seedMseg)))
    {
        fprintf(stderr,
                "Usage: surge-engine-reference [--audio-input | --effect-only] [--convolution | --effect-type TYPE] [--oscillator-type TYPE] [--storage-seed UINT32] [--mseg-seed UINT32] [--effect-parameter INDEX NORMALIZED]... DATA_DIR PATCH.fxp SAMPLE_RATE OUTPUT.f32\n");
        return 2;
    }
    const auto rate = std::atoi(argv[3]);
    std::unique_ptr<SurgeWebEngine, decltype(&surge_destroy)> engine(surge_create(rate, argv[1]),
                                                                     surge_destroy);
    if (!engine || !surge_load_patch(engine.get(), argv[2]))
    {
        fprintf(stderr, "%s\n", surge_error());
        return 1;
    }
    if (oscillatorType >= 0 && !surge_set_oscillator_type(engine.get(), 0, 0, oscillatorType))
    {
        fprintf(stderr, "%s\n", surge_error());
        return 1;
    }
    if (effectType >= 0 && !surge_set_effect_type(engine.get(), 0, effectType))
    {
        fprintf(stderr, "%s\n", surge_error());
        return 1;
    }
    for (const auto &[parameter, value] : effectParameters)
    {
        if (!surge_set_effect_parameter(engine.get(), 0, parameter, value))
        {
            fprintf(stderr, "%s\n", surge_error());
            return 1;
        }
    }
    if (convolution)
    {
        std::array<float, 4096> left{}, right{};
        left[0] = .8f; left[63] = .2f; left[511] = -.1f; left[2048] = .05f;
        right[0] = .4f; right[127] = -.25f; right[1500] = .1f; right[3072] = .01f;
        if (!surge_set_impulse(engine.get(), 0, 32000, left.data(), right.data(), left.size()))
        {
            fprintf(stderr, "%s\n", surge_error());
            return 1;
        }
    }
    auto *out = fopen(argv[4], "wb");
    if (!out)
        return 1;
    std::array<float, 32> left{}, right{}, inputLeft{}, inputRight{};
    if (seedStorage && !surge_seed_storage_rng(engine.get(), storageSeed))
    {
        fclose(out);
        fprintf(stderr, "%s\n", surge_error());
        return 1;
    }
    if (!effectOnly) surge_midi(engine.get(), 0x90, 60, 100);
    if (seedMseg && !surge_seed_voice_mseg(engine.get(), 0, 0, msegSeed))
    {
        fclose(out);
        fprintf(stderr, "%s\n", surge_error());
        return 1;
    }
    for (int frame = 0; frame < rate * 3; frame += 32)
    {
        if (!effectOnly && frame <= rate && frame + 32 > rate)
            surge_midi(engine.get(), 0x80, 60, 0);
        // Integer-period saws with exact binary amplitudes match the browser
        // fixture without relying on platform-specific trigonometric rounding.
        for (int i = 0; (audioInput || effectOnly) && i < 32; ++i)
        {
            const bool active = !effectOnly || frame + i < rate;
            inputLeft[i] = active ? (((frame + i) * (effectOnly ? 17 : 1)) % 257 - 128) / 256.f : 0.f;
            inputRight[i] = active ? (((frame + i) * (effectOnly ? 29 : 1)) % 193 - 96) / 256.f : 0.f;
        }
        const auto rendered = effectOnly ? surge_render_effect_block(engine.get(), 0,
            inputLeft.data(), inputRight.data(), left.data(), right.data()) :
            surge_render(engine.get(), audioInput ? inputLeft.data() : nullptr,
                         audioInput ? inputRight.data() : nullptr, left.data(), right.data(), 32);
        if (!rendered)
            return 1;
        for (int i = 0; i < 32 && frame + i < rate * 3; ++i)
        {
            if (!std::isfinite(left[i]) || !std::isfinite(right[i]))
                return 1;
            if (fwrite(&left[i], sizeof(float), 1, out) != 1 ||
                fwrite(&right[i], sizeof(float), 1, out) != 1)
                return 1;
        }
    }
    return fclose(out) == 0 ? 0 : 1;
}
