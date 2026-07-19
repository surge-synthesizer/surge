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
#include <future>
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

enum class WtGenMode
{
    Preview, // regenerate the preview frame cache and refresh the preview display
    Generate // build a wavetable and publish into generateTarget->wt (or exportOut if that is null)
};

// Inputs for one generation. Built by the submitter, then read ONLY by the worker. Move-only:
// ownership transfers into submit(), nothing mutable is shared or copied across the thread
// boundary.
struct WtGenJobRequest
{
    WtGenMode mode{WtGenMode::Preview};
    int scene{0}, osc{0};

    std::string script;
    int resolution{2048}, frameCount{10};

    // Immutable, refcount-shared snapshot copy captured on the submitting thread (under
    // wtSnapshotMutex). The const pointee is safe to read from the worker.
    std::shared_ptr<const SnapshotBundle> snapshot;

    // Generate: publish target. nullptr == export (the worker builds into the response's
    // exportOut).
    OscillatorStorage *generateTarget{nullptr};

    // storage->wtGenPublishToken at submit, re-checked under waveTableDataMutex before a live-osc
    // publish. Ignored for preview/export.
    uint64_t publishToken{0};

    // Poll-based Generates (overlay Generate, drop/menu) set this so a newer same-osc Generate
    // supersedes a still-pending older one, which the worker drops before running, or bails at the
    // next frame if already running. Never set on block-waiting Generates (export/OSC): a parked
    // waiter needs its own result.
    bool supersedable{false};

    WtGenJobRequest() = default;
    WtGenJobRequest(WtGenJobRequest &&) = default;
    WtGenJobRequest &operator=(WtGenJobRequest &&) = default;
    WtGenJobRequest(const WtGenJobRequest &) = delete;
    WtGenJobRequest &operator=(const WtGenJobRequest &) = delete;
};

// Outputs from one generation. Written by the worker, then read by the submitter through the
// future once it is ready (the future's shared state is the sole synchronization: the caller
// cannot observe these fields before the worker has finished writing them). Move-only.
struct WtGenJobResponse
{
    bool ok{false};        // populateWavetable succeeded (frames / exportOut are valid)
    bool published{false}; // live-osc Generate only: true if BuildWT ran into generateTarget->wt
    int frameCount{0};

    std::vector<std::vector<float>> frames; // [frame][sample]

    // Export (generateTarget == nullptr) only. unique_ptr because Wavetable is not movable (user
    // dtor, no move ctor) so it can't ride in a moved-through-future struct by value.
    std::unique_ptr<Wavetable> exportOut;

    std::string wtName, error; // error non-empty => surface to the user
};

// Per-(scene,osc) bookkeeping for the generation worker: the outstanding-job counts the spinner
// polls, and the newest Preview/Generate ID the supersede checks compare against.
// Both are atomic, so queries are safe from any thread; begin() is the exception and must run
// under queueLock.
class WtGenStatus
{
  public:
    // Move-only accounting handle. Its destructor decrements the busy (and, for a live generate,
    // the generate) count exactly once, so the worker cannot leak a count on any exit path. Also
    // carries the requestId + denseIdx the worker needs for the supersede tests.
    class Ticket
    {
      public:
        Ticket() = default;
        ~Ticket() { release(); }
        Ticket(Ticket &&o) noexcept { moveFrom(o); }
        Ticket &operator=(Ticket &&o) noexcept
        {
            if (this != &o)
            {
                release();
                moveFrom(o);
            }
            return *this;
        }
        Ticket(const Ticket &) = delete;
        Ticket &operator=(const Ticket &) = delete;

      private:
        friend class WtGenStatus;
        WtGenStatus *owner{nullptr};
        int denseIdx{0};
        uint64_t id{0};
        bool liveGenerate{false};

        void moveFrom(Ticket &o)
        {
            owner = o.owner;
            denseIdx = o.denseIdx;
            id = o.id;
            liveGenerate = o.liveGenerate;
            o.owner = nullptr;
        }
        void release()
        {
            if (owner)
            {
                owner->busyCount[denseIdx].fetch_sub(1, std::memory_order_relaxed);
                if (liveGenerate)
                {
                    owner->generateCount[denseIdx].fetch_sub(1, std::memory_order_relaxed);
                }
                owner = nullptr;
            }
        }
    };

    // Assign a requestId, bump the counts, record this job as the osc's newest, return its ticket.
    // Call under queueLock and enqueue under the same lock.
    Ticket begin(WtGenMode mode, int idx, bool supersedable, bool liveGenerate);

    // Lock-free queries (any thread).
    bool isBusy(int idx) const { return busyCount[idx].load(std::memory_order_relaxed) > 0; }
    bool isGenerating(int idx) const
    {
        return generateCount[idx].load(std::memory_order_relaxed) > 0;
    }

    // Worker-side supersede tests: has a newer same-osc job of this kind been submitted?
    bool previewSuperseded(const Ticket &t) const
    {
        return t.id < latestPreview[t.denseIdx].load(std::memory_order_relaxed);
    }
    bool generateSuperseded(const Ticket &t) const
    {
        return t.id < latestGenerate[t.denseIdx].load(std::memory_order_relaxed);
    }

  private:
    std::array<std::atomic<int>, n_scenes * n_oscs> busyCount{};     // outstanding jobs
    std::array<std::atomic<int>, n_scenes * n_oscs> generateCount{}; // outstanding live generates
    std::array<std::atomic<uint64_t>, n_scenes * n_oscs> latestPreview{};  // newest Preview ID
    std::array<std::atomic<uint64_t>, n_scenes * n_oscs> latestGenerate{}; // newest supersedable ID
    uint64_t nextRequestId{1};                                             // under the queue lock
};

// Persistent background worker owning one LuaWTEvaluator/lua_State. Drains a FIFO of jobs,
// superseding stale Previews/Generates per (scene,osc). Lazily started, joined at teardown.
class WtGenService
{
  public:
    explicit WtGenService(SurgeStorage *storage);
    ~WtGenService();

    WtGenService(const WtGenService &) = delete;
    WtGenService &operator=(const WtGenService &) = delete;

    // Enqueue a job (transfers ownership of the request, notifies the worker). Returns the future
    // that becomes ready once the worker finishes it. "front" inserts ahead of the pending queue
    // (used by block-waiting submitters to cut head-of-line latency).
    std::future<WtGenJobResponse> submit(WtGenJobRequest request, bool front = false);

    // For synchronous callers (export on the message thread, OSC on its own thread): submit and
    // block until the job is done, returning its response.
    WtGenJobResponse submitBlocking(WtGenJobRequest request);

    // Spinner component queries:
    // "isBusy" = any job outstanding (Preview or Generate).
    // "isGeneratingToOscillator" = only a live-osc Generate outstanding.
    bool isBusy(int scene, int osc) const;
    bool isGeneratingToOscillator(int scene, int osc) const;

    // Content signature of a generation's inputs: equal inputs hash to equal keys, so a cached
    // frame set is reused when the caller's current inputs produce the same key. Never returns 0
    // (0 marks an empty cache slot).
    static uint64_t previewCacheKey(const std::string &script, int resolution, int frameCount,
                                    uint64_t snapshotsVersion);

    // Per-osc preview cache lookup (message thread).
    bool tryGetCachedPreview(int scene, int osc, uint64_t key,
                             std::vector<std::vector<float>> &outFrames, int &outFrameCount);

  private:
    void ensureStarted();
    void runThread();

    // Worker-side: file a produced filmstrip under its content key (copies frames under the
    // cache mutex).
    void storePreview(int scene, int osc, uint64_t key,
                      const std::vector<std::vector<float>> &frames, int frameCount);

    static int denseIdx(int scene, int osc);

    SurgeStorage *storage{nullptr};
    LuaWTEvaluator evaluator; // private - sole owner of the Lua state

    // Declared before `queue`: every QueuedJob in the queue owns a WtGenStatus::Ticket that
    // decrements a count on `status` when destroyed, so `status` must outlive the queue (members
    // destruct in reverse declaration order).
    WtGenStatus status;

    // One queued unit of work: the inputs, the promise whose future the submitter holds, and the
    // accounting ticket.
    struct QueuedJob
    {
        WtGenJobRequest req;
        std::promise<WtGenJobResponse> promise;
        WtGenStatus::Ticket ticket;
    };
    std::deque<QueuedJob> queue;

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
    bool running{true}; // under queueLock

    // set at teardown: polled between frames so an in-progress generation stops within one frame.
    std::atomic<bool> cancelGeneration{false};
};

// Build a live-osc Generate request from oscdata's current script/res/frames/snapshots
// (generateTarget set to that osc). Shared by drop/menu (poll) and OSC (block-wait). Must be called
// on a thread that owns oscdata: it captures the snapshot bundle under wtSnapshotMutex.
WtGenJobRequest makeLiveGenerateRequest(SurgeStorage *storage, int scene, int osc);

} // namespace WavetableScript
} // namespace Surge

#endif // SURGE_SRC_COMMON_DSP_WTGENSERVICE_H
