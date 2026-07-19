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

#include "WtGenService.h"
#include "SurgeStorage.h"

namespace Surge
{
namespace WavetableScript
{

WtGenJobRequest makeLiveGenerateRequest(SurgeStorage *storage, int scene, int osc)
{
    auto *oscdata = &storage->getPatch().scene[scene].osc[osc];
    WtGenJobRequest req;
    req.mode = WtGenMode::Generate;
    req.scene = scene;
    req.osc = osc;
    req.generateTarget = oscdata;
    req.script = oscdata->wavetable_script;
    req.resolution = resolutionForResBase(oscdata->wavetable_script_res_base);
    req.frameCount = oscdata->wavetable_script_nframes;
    req.snapshot = SnapshotBundle::current(storage, *oscdata);

    // Capture the publish token under waveTableDataMutex. The worker re-checks the token under the
    // same lock before publishing.
    {
        std::lock_guard<std::mutex> g(storage->waveTableDataMutex);
        req.publishToken = storage->wtGenPublishToken[scene * n_oscs + osc];
    }

    return req;
}

WtGenStatus::Ticket WtGenStatus::begin(WtGenMode mode, int idx, bool supersedable,
                                       bool liveGenerate)
{
    const uint64_t reqId = nextRequestId++;
    busyCount[idx].fetch_add(1, std::memory_order_relaxed);
    if (liveGenerate)
    {
        generateCount[idx].fetch_add(1, std::memory_order_relaxed);
    }
    if (mode == WtGenMode::Preview)
    {
        latestPreview[idx].store(reqId, std::memory_order_relaxed);
    }
    else if (supersedable)
    {
        latestGenerate[idx].store(reqId, std::memory_order_relaxed);
    }

    Ticket t;
    t.owner = this;
    t.denseIdx = idx;
    t.id = reqId;
    t.liveGenerate = liveGenerate;
    return t;
}

WtGenService::WtGenService(SurgeStorage *s) : storage(s) {}

WtGenService::~WtGenService()
{
    // Signal the in-progress generation to bail at the next frame, so join() waits at most
    // one frame rather than the whole table.
    cancelGeneration.store(true, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> g(queueLock);
        running = false;
    }
    queueCV.notify_all();

    if (thread.joinable())
    {
        thread.join();
    }
}

int WtGenService::denseIdx(int scene, int osc) { return scene * n_oscs + osc; }

void WtGenService::ensureStarted()
{
    std::call_once(startFlag, [this]() { thread = std::thread([this]() { runThread(); }); });
}

uint64_t WtGenService::previewCacheKey(const std::string &script, int resolution, int frameCount,
                                       uint64_t snapshotsVersion)
{
    // FNV-1a hash over the script bytes, then mix in the scalar inputs.
    // See: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1a_hash
    uint64_t h = 1469598103934665603ull; // offset basis
    constexpr uint64_t prime = 1099511628211ull;
    for (unsigned char c : script)
    {
        h ^= c;
        h *= prime;
    }
    auto mix = [&h](uint64_t v) {
        for (int b = 0; b < 8; ++b)
        {
            h ^= (v & 0xffu);
            h *= prime;
            v >>= 8;
        }
    };
    mix((uint32_t)resolution);
    mix((uint32_t)frameCount);
    mix(snapshotsVersion);
    return h ? h : 1; // never 0, that marks an empty cache slot
}

bool WtGenService::tryGetCachedPreview(int scene, int osc, uint64_t key,
                                       std::vector<std::vector<float>> &outFrames,
                                       int &outFrameCount)
{
    if (key == 0)
    {
        return false;
    }
    std::lock_guard<std::mutex> g(previewCacheMutex);
    const auto &e = previewCache[denseIdx(scene, osc)];
    if (e.key != key)
    {
        return false;
    }
    outFrames = e.frames; // copy out, caller owns its snapshot
    outFrameCount = e.frameCount;

    return true;
}

void WtGenService::storePreview(int scene, int osc, uint64_t key,
                                const std::vector<std::vector<float>> &frames, int frameCount)
{
    if (key == 0 || frames.empty())
    {
        return;
    }
    std::lock_guard<std::mutex> g(previewCacheMutex);
    auto &e = previewCache[denseIdx(scene, osc)];
    e.frames = frames; // copy: the response still owns its frames for the poller to move out
    e.frameCount = frameCount;
    e.key = key;
}

std::future<WtGenJobResponse> WtGenService::submit(WtGenJobRequest request, bool front)
{
    ensureStarted();

    QueuedJob qj;
    qj.req = std::move(request);
    auto fut = qj.promise.get_future(); // obtained before the worker can touch the promise

    {
        std::lock_guard<std::mutex> g(queueLock);
        const int idx = denseIdx(qj.req.scene, qj.req.osc);
        const bool liveGenerate =
            (qj.req.mode == WtGenMode::Generate && qj.req.generateTarget != nullptr);
        qj.ticket = status.begin(qj.req.mode, idx, qj.req.supersedable, liveGenerate);

        if (front)
        {
            queue.push_front(std::move(qj));
        }
        else
        {
            queue.push_back(std::move(qj));
        }
    }

    queueCV.notify_one();

    return fut;
}

WtGenJobResponse WtGenService::submitBlocking(WtGenJobRequest request)
{
    return submit(std::move(request), /*front*/ true).get();
}

bool WtGenService::isBusy(int scene, int osc) const { return status.isBusy(denseIdx(scene, osc)); }

bool WtGenService::isGeneratingToOscillator(int scene, int osc) const
{
    return status.isGenerating(denseIdx(scene, osc));
}

void WtGenService::runThread()
{
    for (;;)
    {
        std::unique_lock<std::mutex> lk(queueLock);
        queueCV.wait(lk, [this]() { return !running || !queue.empty(); });
        if (!running && queue.empty())
        {
            return;
        }

        QueuedJob qj = std::move(queue.front());
        queue.pop_front();

        // Evaluate under the lock, then fulfill with the lock released, like the completion path
        // below.
        const bool dropped =
            (qj.req.mode == WtGenMode::Preview && status.previewSuperseded(qj.ticket)) ||
            (qj.req.mode == WtGenMode::Generate && qj.req.supersedable &&
             status.generateSuperseded(qj.ticket));

        lk.unlock();

        if (dropped)
        {
            // Superseded before it ran: hand back an empty (ok == false) response so a holder of
            // the future never blocks.
            qj.promise.set_value({});
            continue;
        }

        auto &req = qj.req;
        const int idx = denseIdx(req.scene, req.osc);

        WtGenJobResponse resp;
        bool ok = false;
        try
        {
            // Drive the private evaluator entirely from the request. Generation errors land
            // straight in resp.error (first error wins) instead of touching the UI off-thread.
            evaluator.setStorage(storage);
            evaluator.setErrorOut(&resp.error);
            evaluator.setScript(req.script);
            evaluator.setResolution((size_t)req.resolution);
            evaluator.setFrameCount((size_t)req.frameCount);
            evaluator.setSnapshotBundle(req.snapshot);
            evaluator.forceInvalidate();

            // Bail the generation at the next frame's pcall on either teardown (cancelGeneration)
            // or mid-generate supersede.
            auto cancelPred = [this, &qj]() {
                return cancelGeneration.load(std::memory_order_relaxed) ||
                       (qj.req.supersedable && status.generateSuperseded(qj.ticket));
            };

            // One Lua pass yields the preview frames plus (unless previewOnly) the flat BuildWT
            // buffer, owned by gen.samples which frees itself on every path (including a throw).
            const bool previewOnly = (req.mode == WtGenMode::Preview);
            auto gen = evaluator.populateWavetable(cancelPred, previewOnly);
            ok = gen.ok;
            if (ok)
            {
                resp.frames = std::move(gen.frames);
                resp.frameCount = (int)resp.frames.size();
                resp.wtName = evaluator.getSuggestedWavetableName();

                if (req.mode == WtGenMode::Generate && req.generateTarget)
                {
                    // Live-osc publish: BuildWT into the target under waveTableDataMutex. Only the
                    // buffers cross here; current_id/queue_type/display name are poller-owned.
                    // Re-check the publish token inside the lock: if the osc was replaced since
                    // submit, skip.
                    std::lock_guard<std::mutex> g(storage->waveTableDataMutex);
                    if (storage->wtGenPublishToken[idx] == req.publishToken)
                    {
                        req.generateTarget->wt.BuildWT(gen.samples.get(), gen.header,
                                                       gen.header.flags & wtf_is_sample);
                        resp.published = true;
                    }
                }
                else if (req.mode == WtGenMode::Generate)
                {
                    // Export: build into the response-owned exportOut (no shared state, so no
                    // mutex and no token guard). The block-waiting submitter writes it to file.
                    resp.exportOut = std::make_unique<Wavetable>();
                    resp.exportOut->BuildWT(gen.samples.get(), gen.header,
                                            gen.header.flags & wtf_is_sample);
                }

                if (req.mode == WtGenMode::Preview ||
                    (req.mode == WtGenMode::Generate && req.generateTarget))
                {
                    // File the frames in the per-osc cache so a later overlay refresh for this osc
                    // reuses them.
                    const uint64_t key = previewCacheKey(req.script, req.resolution, req.frameCount,
                                                         req.snapshot ? req.snapshot->version : 0);
                    storePreview(req.scene, req.osc, key, resp.frames, resp.frameCount);
                }
            }
        }
        catch (const std::exception &e)
        {
            ok = false;
            if (resp.error.empty())
            {
                resp.error = std::string("Wavetable generation error: ") + e.what();
            }
        }
        catch (...)
        {
            ok = false;
            if (resp.error.empty())
            {
                resp.error = "Unknown error during wavetable generation.";
            }
        }

        evaluator.setErrorOut(nullptr);

        // A job replaced mid-run is a no-op the newer Generate owns, so drop any message (even a
        // genuine failure that is also superseded); otherwise supply a generic one if it failed
        // silently.
        const bool superseded = !ok && req.supersedable && status.generateSuperseded(qj.ticket);
        if (superseded)
        {
            resp.error.clear();
        }
        else if (!ok && resp.error.empty())
        {
            resp.error = "Wavetable script generation failed.";
        }

        resp.ok = ok;

        // Fulfill the future (publishes frames/wtName/published/exportOut to the poller or
        // block-waiter). qj's ticket then decrements the counts at end of scope, so the spinner
        // turns off only once the result is ready.
        qj.promise.set_value(std::move(resp));
    }
}

} // namespace WavetableScript
} // namespace Surge
