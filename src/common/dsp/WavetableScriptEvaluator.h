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

#ifndef SURGE_SRC_COMMON_DSP_WAVETABLESCRIPTEVALUATOR_H
#define SURGE_SRC_COMMON_DSP_WAVETABLESCRIPTEVALUATOR_H

#include "LuaSupport.h"
#include "SurgeStorage.h"
#include "Wavetable.h"
#include "filesystem/import.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Surge
{
namespace WavetableScript
{

// The wavetable-script resolution parameter is a 1-based exponent: base 1 -> 32 samples, doubling
// each step (32 << (base - 1)).
inline int resolutionForResBase(int resBase) { return 32 << (std::clamp(resBase, 1, 8) - 1); }

// Immutable, refcount-shared snapshot of an oscillator's wtSnapshots. Built copy-on-mutate
// (only when wtSnapshots actually changes) so a background generator can read a stable copy
// while the message/OSC thread resets the live snapshots underneath it. "version" is the
// wtSnapshotsVersion this bundle was built from.
struct SnapshotBundle
{
    struct Slot
    {
        unsigned nframes{0};
        int nsamples{0};
        std::vector<float> data; // flat, nframes * nsamples; data[t * nsamples + i]
    };
    std::array<Slot, n_wt_snapshots> slots;
    uint64_t version{0};
};

// Copy the live wtSnapshots of osc into a fresh bundle. Caller must hold storage->wtSnapshotMutex
// (it reads the snapshot sample data, which mutation sites write under that mutex).
std::shared_ptr<SnapshotBundle> buildSnapshotBundle(const OscillatorStorage &osc);

// Return osc.wtSnapshotBundle, rebuilding it (under storage->wtSnapshotMutex) if it is
// missing or its set version is stale.
std::shared_ptr<const SnapshotBundle> ensureSnapshotBundle(SurgeStorage *storage,
                                                           OscillatorStorage &osc);

struct LuaWTEvaluator
{
    LuaWTEvaluator();
    ~LuaWTEvaluator();

    static constexpr uint64_t wtsFeatures = Surge::LuaSupport::EnvironmentFeatures::BASE |
                                            Surge::LuaSupport::EnvironmentFeatures::HAS_FFT;

    void setStorage(SurgeStorage *);

    // Inject the immutable snapshot bundle the next generation builds its wt.snapshot Lua table
    // from, rather than touching live oscdata. This is how the worker reads snapshots safely off
    // the message thread.
    void setSnapshotBundle(std::shared_ptr<const SnapshotBundle>);

    // Worker error routing: when enabled, generation errors are collected rather than shown
    // via storage->reportError (which touches UI and must not run off the message thread)
    // takeDeferredError() returns the accumulated text and clears it.
    void setDeferErrors(bool);
    std::string takeDeferredError();

    void setScript(const std::string &);
    void setResolution(size_t);
    void setFrameCount(size_t);
    void forceInvalidate();

    using validFrame_t = std::vector<float>;
    using frame_t = std::optional<validFrame_t>;

    // Product of one generation pass: the owning flat BuildWT buffer (null in previewOnly mode),
    // its header, and the per-frame preview vectors. On failure/cancellation ok is false and the
    // buffers are empty.
    struct PopulatedWavetable
    {
        bool ok{false};
        wt_header header{};
        std::unique_ptr<float[]> samples;       // flat header.n_tables * header.n_samples
        std::vector<std::vector<float>> frames; // [frame][sample]
    };

    // Generate all frames in one Lua pass, returning the result by value (buffer ownership
    // transfers to the caller). previewOnly skips the flat-buffer alloc/fill; canceled() true stops
    // at the next frame pcall and returns an empty (ok==false) result.
    PopulatedWavetable populateWavetable(const std::function<bool()> &canceled = {},
                                         bool previewOnly = false);

    void loadWtscriptForTesting(const fs::path &filename, SurgeStorage *storage,
                                OscillatorStorage *oscdata);

    // Parse a .wtscript into oscdata (script/res/frames+snapshots+version bump) WITHOUT
    // generating. Lua-free and instance-free, so load sites can parse on the caller thread and
    // then submit a Generate to the WtGenService instead of blocking on synchronous generation.
    // Returns false on a parse error. If errorOut is non-null the error text is written there for
    // the caller to surface (eg. OSC->sendError, off the message thread), otherwise it is
    // reported directly via storage->reportError (message-thread callers).
    static bool loadWtscriptMetadata(const fs::path &filename, SurgeStorage *storage,
                                     OscillatorStorage *oscdata, std::string *errorOut = nullptr);

    frame_t getFrame(size_t frame);

    std::string getSuggestedWavetableName();

    static std::string defaultWavetableScript();

  private:
    struct Details;
    std::unique_ptr<Details> details;

    struct WtscriptData
    {
        std::string script;
        int nframes = 0;
        int res_base = 0;
    };

    static std::optional<WtscriptData> parseWtscript(const fs::path &filename,
                                                     SurgeStorage *storage,
                                                     OscillatorStorage *oscdata,
                                                     std::string *errorOut = nullptr);
};

} // namespace WavetableScript
} // namespace Surge
#endif // SURGE_SRC_COMMON_DSP_WAVETABLESCRIPTEVALUATOR_H
