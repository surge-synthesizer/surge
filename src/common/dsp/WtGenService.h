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

#ifndef SURGE_SRC_COMMON_DSP_WTGENSERVICE_H
#define SURGE_SRC_COMMON_DSP_WTGENSERVICE_H

#include "WavetableScriptEvaluator.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <latch>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class SurgeStorage;
struct OscillatorStorage;

namespace Surge
{
namespace WavetableScript
{

// Data-only job crossing the thread boundary: the submitter sets the inputs, the worker writes
// the outputs and flips "status" (the sole memory barrier). No Lua or live oscdata crosses, only
// this struct.
struct WtGenJob
{
    enum class Mode
    {
        Preview, // regenerate the preview frame cache and refreshes the preview display
        Generate // build a wavetable and publish into generateTarget->wt
    };

    enum class Status
    {
        Pending,
        Working,
        Complete,
        Failed
    };

    // requestId assigned inside submit() under queueLock; the rest set by the caller.
    int scene{0}, osc{0};
    uint64_t requestId{0};

    // State of storage->wtGenPublishToken at submit, re-checked before a live-osc publish.
    // Ignored for preview/export.
    uint64_t publishToken{0};

    // Content signature (script+res+frames+snapshotVersion), computed in submit(). The worker files
    // the produced frames under it in the per-osc preview cache so a later overlay refresh can skip
    // regen.
    uint64_t cacheKey{0};
    Mode mode{Mode::Preview};

    // Poll-based Generates (overlay Generate, drop/menu) set this so a newer same-osc Generate
    // supersedes a still-pending older one, which the worker drops before running. Never set on
    // block-waiting Generates (export/OSC): a parked waiter needs its own result. Ignored for
    // Preview (its path supersedes separately).
    bool supersedable{false};

    // Input, const once submitted. Read by the worker after it claims the job.
    std::string script;
    int resolution{2048}, frameCount{10};

    // Immutable bundle. Set before submit, then read ONLY by the worker. Pollers/waiters must
    // never touch this handle after submit (const pointee is safe to share; this instance is not).
    std::shared_ptr<const SnapshotBundle> snapshot;

    // Generate: publish wt buffers into ->wt, nullptr == export (build into exportOut).
    OscillatorStorage *generateTarget{nullptr};

    // The worker's terminal release-store, acquired by the poller/waiter.
    std::atomic<Status> status{Status::Pending};

    // Block-wait handshake for synchronous submitters (export, OSC): they wait on this latch, the
    // worker counts it down exactly once right after the status store. Pollers ignore it.
    std::latch done{1};

    // Preview: written by the worker before status=Complete, read by the poller/waiter after.
    std::vector<std::vector<float>> frames; // [frame][sample]

    // Export (generateTarget==nullptr): built here, then the submitter writes it to file.
    Wavetable exportOut;

    std::string wtName, error;

    // live-osc Generate only: true if BuildWT ran into generateTarget->wt.
    bool published{false};
};

// Persistent background worker owning one LuaWTEvaluator / lua_State. Drains a FIFO of jobs,
// superseding stale Previews per (scene,osc). Lazily started, joined at teardown. Owned by
// SurgeStorage, always constructed with a SurgeStorage*.
class WtGenService
{
  public:
    explicit WtGenService(SurgeStorage *storage);
    ~WtGenService();

    WtGenService(const WtGenService &) = delete;
    WtGenService &operator=(const WtGenService &) = delete;

    // Enqueue a job (assigns requestId, records the newest Preview per osc, notifies the
    // worker). Returns the same shared_ptr so the caller can poll job->status. "front" inserts
    // ahead of the pending queue (used by block-waiting submitters to cut head-of-line latency).
    std::shared_ptr<WtGenJob> submit(std::shared_ptr<WtGenJob> job, bool front = false);

    // For headless/synchronous callers (export on the message thread, OSC on its own thread):
    // Submit and block until the job reaches Complete/Failed.
    void submitBlocking(const std::shared_ptr<WtGenJob> &job);

    // Spinner component queries:
    // "isBusy" = any job outstanding (Preview or Generate).
    // "isGeneratingToOscillator" = only a live-osc Generate outstanding.
    bool isBusy(int scene, int osc) const;
    bool isGeneratingToOscillator(int scene, int osc) const;

    // Content signature of a generation's inputs. Two generations with equal keys produce
    // identical frames, so a cached frame set filed under "key" is reusable iff the caller's
    // current inputs hash to the same key. Never returns 0 (0 marks an empty cache slot).
    static uint64_t previewCacheKey(const std::string &script, int resolution, int frameCount,
                                    uint64_t snapshotsVersion);

    // Per-osc preview cache lookup (message thread). On a hit, copies the cached filmstrip into
    // outFrames/outFrameCount and returns true; the overlay uses this to skip a regeneration when
    // switching oscillators or refreshing after a drop/menu generate already produced the frames.
    bool tryGetCachedPreview(int scene, int osc, uint64_t key,
                             std::vector<std::vector<float>> &outFrames, int &outFrameCount);

  private:
    void ensureStarted();
    void runThread();

    // Worker-side: file a produced filmstrip under its content key (copies frames under the
    // cache mutex). Called for Preview + live-osc Generate, not export.
    void storePreview(int scene, int osc, uint64_t key,
                      const std::vector<std::vector<float>> &frames, int frameCount);

    static int denseIdx(int scene, int osc);

    SurgeStorage *storage{nullptr};
    LuaWTEvaluator evaluator; // PRIVATE — sole owner of the lua_State

    std::deque<std::shared_ptr<WtGenJob>> queue;
    std::array<uint64_t, n_scenes * n_oscs> latestPreviewPerOsc{}; // denseIdx -> newest Preview id

    // denseIdx -> newest supersedable Generate id. Atomic because the running generation polls it
    // lock-free every frame (mid-flight supersede), while submit() writes it under queueLock.
    std::array<std::atomic<uint64_t>, n_scenes * n_oscs> latestGeneratePerOsc{};

    std::array<std::atomic<int>, n_scenes * n_oscs> busyCountPerOsc{}; // denseIdx -> outstanding
    std::array<std::atomic<int>, n_scenes * n_oscs>
        oscGenerateCountPerOsc{}; // denseIdx -> live generates

    // Per-osc preview frame cache (denseIdx). Written by the worker after a successful
    // Preview / live-osc Generate, read by the overlay on refresh. Validity is purely by key match,
    // so no explicit invalidation is needed: a change to script/res/frames/snapshots yields a
    // different key and misses. Bounded to n_scenes*n_oscs entries, each overwritten on regen.
    struct PreviewCacheEntry
    {
        std::vector<std::vector<float>> frames;
        int frameCount{0};
        uint64_t key{0}; // 0 == empty
    };
    std::array<PreviewCacheEntry, n_scenes * n_oscs> previewCache;
    std::mutex previewCacheMutex;

    std::mutex queueLock;
    std::condition_variable queueCV;
    std::thread thread;
    std::once_flag startFlag;
    uint64_t nextRequestId{1}; // under queueLock
    bool running{true};        // under queueLock

    // set at teardown: polled between frames so an in-progress generation stops within one frame.
    std::atomic<bool> cancelGeneration{false};
};

// Build a live-osc Generate job from oscdata's current script/res/frames/snapshots (generateTarget
// set to that osc). Shared by drop/menu (poll) and OSC (block-wait). Must be called on a thread
// that owns oscdata: it captures the snapshot bundle under wtSnapshotMutex.
std::shared_ptr<WtGenJob> makeLiveGenerateJob(SurgeStorage *storage, int scene, int osc);

} // namespace WavetableScript
} // namespace Surge

#endif // SURGE_SRC_COMMON_DSP_WTGENSERVICE_H
