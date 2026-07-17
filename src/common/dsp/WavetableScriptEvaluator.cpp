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
#include "PatchFileHeaderStructs.h"
#include "lua/LuaSources.h"

#include "sst/basic-blocks/mechanics/endian-ops.h"

#include <cstring>
#include <fstream>
#include <tinyxml/tinyxml.h>
#include "binn/binn.h"
#include "fmt/core.h"
#include "zstd.h"

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

    lua_State *L{nullptr};
    int snapshotRef{LUA_NOREF}; // Registry ref for cached wt.snapshot table

    // wt.snapshot is always built from this immutable bundle (never live oscdata). Defaults to an
    // empty bundle and is never null; the worker/tests inject a frozen bundle via
    // setSnapshotBundle.
    std::shared_ptr<const SnapshotBundle> snapshotBundle{std::make_shared<SnapshotBundle>()};

    // When deferErrors is set (worker path), generation errors are collected
    // into deferredError instead of storage->reportError which invokes UI error listeners
    // and must not run off the message thread. The poller surfaces the string.
    bool deferErrors{false};
    std::string deferredError;

    void emitError(const std::string &msg, const std::string &title)
    {
        if (deferErrors)
        {
            if (!deferredError.empty())
            {
                deferredError += "\n";
            }
            deferredError += msg;
        }
        else if (storage)
        {
            storage->reportError(msg, title);
        }
        else
        {
            std::cerr << msg << std::endl;
        }
    }

    void invalidate()
    {
        isValid = false;
        frameCache.clear();
    }

    // Push a freshly built wt.snapshot 3D table onto the Lua stack
    void pushSnapshotTable()
    {
        lua_newtable(L); // Outer snapshot table

        // wt.snapshot is built from an injected immutable SnapshotBundle.
        for (int s = 0; s < n_wt_snapshots; ++s)
        {
            lua_newtable(L); // Slot table (empty if not imported)
            const auto &slot = snapshotBundle->slots[s];

            if (slot.nframes > 0 && slot.nsamples > 0)
            {
                const int nsamples = slot.nsamples;

                for (unsigned int t = 0; t < slot.nframes; ++t)
                {
                    lua_newtable(L); // Frame table
                    const float *tbl = slot.data.data() + (size_t)t * nsamples;
                    for (int i = 0; i < nsamples; ++i)
                    {
                        lua_pushnumber(L, tbl[i]); // Push sample value
                        lua_rawseti(L, -2, i + 1); // frame[i + 1] = value
                    }
                    lua_rawseti(L, -2, t + 1); // slot[t + 1] = frame table
                }
            }
            lua_rawseti(L, -2, s + 1); // snapshot[slot + 1] = slot table
        }
    }

    // Build the snapshot table and stash it in the Lua registry so each generate call can reuse it
    void cacheSnapshotTable()
    {
        if (!L)
            return;
        releaseSnapshotTable();
        pushSnapshotTable();
        snapshotRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    void releaseSnapshotTable()
    {
        if (L && snapshotRef != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, snapshotRef);
            snapshotRef = LUA_NOREF;
        }
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
        Surge::LuaSupport::setSurgeFunctionEnvironment(L, wtsFeatures);

        lua_createtable(L, 0, 10);
        int tidx = lua_gettop(L); // Get the index of the new table
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

        // Inject snapshotted wavetables as a single 3D table: wt.snapshot[slot][frame][sample]
        // populateWavetable() pre-builds and caches once per regen cycle so we can reuse it
        if (snapshotRef != LUA_NOREF)
            lua_rawgeti(L, LUA_REGISTRYINDEX, snapshotRef);
        else
            pushSnapshotTable();
        lua_setfield(L, tidx, "snapshot");

        // So stack is now the table and the function
        auto pcr = lua_pcall(L, 1, 1, 0);
        if (pcr == LUA_OK)
        {
            if (lua_istable(L, -1))
            {
                bool gen{true};
                for (size_t i = 0; i < resolution && gen; ++i)
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

            emitError(oss.str(), "Wavetable Script Evaluator Error");
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
            Surge::LuaSupport::setSurgeFunctionEnvironment(L, wtsFeatures);

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
                    emitError("Init function returned a non-table.",
                              "Wavetable Script Evaluator Error");
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
                emitError(oss.str(), "Wavetable Script Evaluator Error");
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
                for (size_t i = 0; i < frameCount; ++i)
                    frameCache.push_back(std::nullopt);
            }

            auto wg = Surge::LuaSupport::SGLD("WavetableScript::details::makeValid", L);
            std::string emsg;
            auto res = Surge::LuaSupport::parseStringDefiningMultipleFunctions(
                L, script, {"init", "generate"}, emsg);
            if (!res)
            {
                std::ostringstream oss;
                oss << "Unable to determine generate() or init() function!";
                if (!emsg.empty())
                {
                    oss << "\n" << emsg;
                }
                emitError(oss.str(), "Wavetable Script Parse Error");
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

void LuaWTEvaluator::setSnapshotBundle(std::shared_ptr<const SnapshotBundle> bundle)
{
#if HAS_LUA
    assert(bundle);
    details->snapshotBundle = std::move(bundle);
#endif
}

void LuaWTEvaluator::setDeferErrors(bool defer)
{
#if HAS_LUA
    details->deferErrors = defer;
    if (defer)
    {
        details->deferredError.clear();
    }
#endif
}

std::string LuaWTEvaluator::takeDeferredError()
{
#if HAS_LUA
    auto err = std::move(details->deferredError);
    details->deferredError.clear();
    return err;
#else
    return {};
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
    n = std::clamp<size_t>(n, 1, 256);
    if (n != details->frameCount)
    {
        details->invalidate();
        details->frameCount = n;
    }
#endif
}

void LuaWTEvaluator::forceInvalidate()
{
#if HAS_LUA
    details->invalidate();
#endif
}

LuaWTEvaluator::frame_t LuaWTEvaluator::getFrame(size_t frame)
{
#if HAS_LUA
    if (!details->makeValid())
        return std::nullopt;
    if (frame >= details->frameCount)
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

LuaWTEvaluator::PopulatedWavetable
LuaWTEvaluator::populateWavetable(const std::function<bool()> &canceled, bool previewOnly)
{
    PopulatedWavetable result;
#if HAS_LUA
    if (!details->makeValid())
    {
        return result; // ok == false
    }

    // Build then cache the wt.snapshot Lua table once for this regen cycle
    struct SnapshotCacheGuard
    {
        Details *d;
        ~SnapshotCacheGuard() { d->releaseSnapshotTable(); }
    } snapshotCacheGuard{details.get()};
    details->cacheSnapshotTable();

    auto resolution = details->resolution;
    auto frames = details->frameCount;

    // A Preview needs only the per-frame vectors, so skip the multi-MB flat buffer. reset(new ...)
    // leaves it uninitialized; we fill every sample below rather than pay to zero it.
    float *wd = nullptr;
    if (!previewOnly)
    {
        result.samples.reset(new float[frames * resolution]);
        wd = result.samples.get();
        result.header.n_samples = resolution;
        result.header.n_tables = frames;
        result.header.flags = 0;
    }

    result.frames.assign(frames, {});

    for (size_t i = 0; i < frames; ++i)
    {
        // Cancellation is checked between each frame's pcall
        if (canceled && canceled())
        {
            return {}; // discard partial buffers, ok == false
        }

        auto v = getFrame(i);
        if (v.has_value())
        {
            if (wd)
            {
                memcpy(&(wd[i * resolution]), &((*v)[0]), resolution * sizeof(float));
            }
            result.frames[i] = std::move(*v); // capture after the memcpy read
        }
        else
        {
            return {}; // failure: empty result
        }
    }
    result.ok = true;
#endif
    return result;
}

#if HAS_LUA
static void loadWtscriptSnapshots(const void *compData, size_t blobSize, SurgeStorage *storage,
                                  OscillatorStorage *oscdata)
{
    auto decompressedSize = ZSTD_getFrameContentSize(compData, blobSize);
    if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN || decompressedSize == ZSTD_CONTENTSIZE_ERROR)
        return;

    std::vector<std::uint8_t> decompressed(decompressedSize);
    decompressedSize = ZSTD_decompress(decompressed.data(), decompressedSize, compData, blobSize);
    if (ZSTD_isError(decompressedSize))
        return;

    if (decompressedSize < sizeof(binn_struct))
        return;

    int sz = decompressedSize;
    if (!binn_is_valid_ex(decompressed.data(), NULL, NULL, &sz))
        return;

    binn *b = binn_open_ex(decompressed.data(), sz);
    SurgePatch::readOscSnapshotsFromBinn(b, *oscdata, storage);
    binn_free(b);
}
#endif

std::optional<LuaWTEvaluator::WtscriptData>
LuaWTEvaluator::parseWtscript(const fs::path &filename, SurgeStorage *storage,
                              OscillatorStorage *oscdata, std::string *errorOut)
{
#if HAS_LUA
    namespace mech = sst::basic_blocks::mechanics;
    using sst::io::wtscript_header;

    /* File layout:
       - Snapshot-less: pure XML text
       - With snapshots: [wtscript_header][XML][zstd-compressed binn object]
        where wtscript_header = { tag "wts1", xmlsize, blobsize }
    */

    // Route parse errors to errorOut when the caller must surface them itself (the OSC thread must
    // not call reportError). Message-thread callers pass errorOut == nullptr and get the
    // direct dialog.
    auto fail = [&](const char *msg) -> std::optional<WtscriptData> {
        if (errorOut)
        {
            *errorOut = msg;
        }
        else
        {
            storage->reportError(msg, "Load Error");
        }
        return std::nullopt;
    };

    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile)
    {
        return fail("Failed to load XML file.");
    }
    const auto fileSize = static_cast<size_t>(inFile.tellg());
    inFile.seekg(0);
    std::vector<char> fileData(fileSize);
    if (fileSize > 0)
    {
        inFile.read(fileData.data(), fileSize);
    }
    inFile.close();

    // Detect the binary header by its 4-byte tag. If absent, treat the whole file as XML
    const bool hasHeader =
        fileSize >= sizeof(wtscript_header) && std::memcmp(fileData.data(), "wts1", 4) == 0;

    size_t xmlOffset = 0;
    size_t xmlSize = fileSize;

    // Snapshots stored as a zstd-compressed binn object in the trailing blob
    if (hasHeader)
    {
        wtscript_header header{};
        std::memcpy(&header, fileData.data(), sizeof(header));
        header.xmlsize = mech::endian_read_int32LE(header.xmlsize);
        header.blobsize = mech::endian_read_int32LE(header.blobsize);

        xmlOffset = sizeof(wtscript_header);
        xmlSize = header.xmlsize;
        size_t blobSize = header.blobsize;
        const size_t bytesAfterHeader = fileSize - xmlOffset;

        if (xmlSize > bytesAfterHeader || blobSize > bytesAfterHeader - xmlSize)
        {
            return fail("Wavetable script file is truncated or corrupt!");
        }

        const void *compData = fileData.data() + xmlOffset + xmlSize;
        loadWtscriptSnapshots(compData, blobSize, storage, oscdata);
    }

    // Parse only the XML portion
    std::string xmlStr(fileData.data() + xmlOffset, xmlSize);
    TiXmlDocument doc;
    doc.Parse(xmlStr.c_str(), nullptr, TIXML_ENCODING_LEGACY);
    if (doc.Error())
    {
        return fail("Failed to parse wavetable script XML!");
    }

    auto wtscript = TINYXML_SAFE_TO_ELEMENT(doc.FirstChildElement("wtscript"));
    if (!wtscript)
    {
        return fail("No root wtscript element found!");
    }

    auto wavetable_script = TINYXML_SAFE_TO_ELEMENT(wtscript->FirstChildElement("script"));
    if (!wavetable_script)
    {
        return fail("No wavetable_script element found!");
    }

    auto b64script = wavetable_script->Attribute("lua");
    if (!b64script || std::strlen(b64script) == 0)
    {
        return fail("Empty or missing lua attribute in wavetable_script!");
    }

    int nframes = 0;
    if (wavetable_script->QueryIntAttribute("frames", &nframes) != TIXML_SUCCESS)
    {
        return fail("Missing or invalid frames attribute!");
    }

    int res_base = 0;
    if (wavetable_script->QueryIntAttribute("samples", &res_base) != TIXML_SUCCESS)
    {
        return fail("Missing or invalid samples attribute!");
    }

    LuaWTEvaluator::WtscriptData data;
    data.script = Surge::Storage::base64_decode(b64script);
    data.nframes = nframes;
    data.res_base = res_base;

    return data;
#else
    return std::nullopt;
#endif
}

bool LuaWTEvaluator::loadWtscriptMetadata(const fs::path &filename, SurgeStorage *storage,
                                          OscillatorStorage *oscdata, std::string *errorOut)
{
#if HAS_LUA
    {
        std::lock_guard<std::mutex> g(storage->wtSnapshotMutex);
        for (auto &snap : oscdata->wtSnapshots)
        {
            snap.reset();
        }
        oscdata->wtSnapshotsVersion++;
    }

    auto data = parseWtscript(filename, storage, oscdata, errorOut);
    if (!data)
    {
        return false;
    }

    oscdata->wavetable_script_nframes = data->nframes;
    oscdata->wavetable_script_res_base = data->res_base;
    oscdata->wavetable_script = data->script;
    return true;
#else
    return false;
#endif
}

void LuaWTEvaluator::loadWtscriptForTesting(const fs::path &filename, SurgeStorage *storage,
                                            OscillatorStorage *oscdata)
{
#if HAS_LUA
    auto data = parseWtscript(filename, storage, oscdata);
    if (!data)
    {
        return;
    }

    setStorage(storage);
    setScript(data->script);
    setResolution(resolutionForResBase(data->res_base));
    setFrameCount(data->nframes);
    setSnapshotBundle(buildSnapshotBundle(*oscdata));

    oscdata->wavetable_display_name = getSuggestedWavetableName();
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

std::shared_ptr<SnapshotBundle> buildSnapshotBundle(const OscillatorStorage &osc)
{
    auto bundle = std::make_shared<SnapshotBundle>();
    for (int s = 0; s < n_wt_snapshots; ++s)
    {
        const auto &snap = osc.wtSnapshots[s];
        if (snap && snap->everBuilt && snap->n_tables > 0 && snap->size > 0)
        {
            auto &slot = bundle->slots[s];
            slot.nframes = snap->n_tables;
            slot.nsamples = snap->size;
            slot.data.resize((size_t)slot.nframes * slot.nsamples);
            for (unsigned int t = 0; t < slot.nframes; ++t)
            {
                memcpy(slot.data.data() + (size_t)t * slot.nsamples,
                       snap->TableF32WeakPointers[0][t], (size_t)slot.nsamples * sizeof(float));
            }
        }
    }
    return bundle;
}

std::shared_ptr<const SnapshotBundle> ensureSnapshotBundle(SurgeStorage *storage,
                                                           OscillatorStorage &osc)
{
    std::lock_guard<std::mutex> g(storage->wtSnapshotMutex);
    if (!osc.wtSnapshotBundle || osc.wtSnapshotBundle->version != osc.wtSnapshotsVersion)
    {
        auto bundle = buildSnapshotBundle(osc);
        bundle->version = osc.wtSnapshotsVersion;
        osc.wtSnapshotBundle = bundle;
    }
    return osc.wtSnapshotBundle;
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
    wt.phase = math.linspace(0, 1, wt.sample_count)
    return wt
end

function generate(wt)
    -- wt will have frame_count, sample_count, frame, and any item from init defined
    local res = {}

    for i, x in ipairs(wt.phase) do
        local val = 0
        for n = 1, wt.frame do
            val = val + 2 * sin(2 * pi * n * x) / (pi * n)
        end
        res[i] = val * 0.8
    end
    return res
end
)FN";
}

} // namespace WavetableScript
} // namespace Surge
