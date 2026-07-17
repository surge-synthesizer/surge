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

std::shared_ptr<WtGenJob> makeLiveGenerateJob(SurgeStorage *storage, int scene, int osc)
{
    auto *oscdata = &storage->getPatch().scene[scene].osc[osc];
    auto job = std::make_shared<WtGenJob>();
    job->scene = scene;
    job->osc = osc;
    job->mode = WtGenJob::Mode::Generate;
    job->generateTarget = oscdata;
    job->script = oscdata->wavetable_script;
    job->resolution = resolutionForResBase(oscdata->wavetable_script_res_base);
    job->frameCount = oscdata->wavetable_script_nframes;
    job->snapshot = SnapshotBundle::current(storage, *oscdata);

    // Lock-free capture of the config token, re-checked under waveTableDataMutex at publish.
    job->publishToken =
        storage->wtGenPublishToken[scene * n_oscs + osc].load(std::memory_order_relaxed);

    return job;
}

WtGenService::WtGenService(SurgeStorage *s) : storage(s) {}

WtGenService::~WtGenService()
{
    // Signal the in-flight generation to bail at the next frame, so join() waits at most
    // one frame rather than the whole table
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
    e.frames = frames; // copy: the job still owns its frames for the poller to move out
    e.frameCount = frameCount;
    e.key = key;
}

std::shared_ptr<WtGenJob> WtGenService::submit(std::shared_ptr<WtGenJob> job, bool front)
{
    ensureStarted();
    job->cacheKey = previewCacheKey(job->script, job->resolution, job->frameCount,
                                    job->snapshot ? job->snapshot->version : 0);
    {
        std::lock_guard<std::mutex> g(queueLock);
        const int idx = denseIdx(job->scene, job->osc);
        job->requestId = nextRequestId++;
        busyCountPerOsc[idx].fetch_add(1, std::memory_order_relaxed);

        if (job->mode == WtGenJob::Mode::Generate && job->generateTarget)
        {
            oscGenerateCountPerOsc[idx].fetch_add(1, std::memory_order_relaxed);
        }

        if (job->mode == WtGenJob::Mode::Preview)
        {
            latestPreviewPerOsc[idx] = job->requestId;
        }

        if (job->mode == WtGenJob::Mode::Generate && job->supersedable)
        {
            latestGeneratePerOsc[idx].store(job->requestId, std::memory_order_relaxed);
        }

        if (front)
        {
            queue.push_front(job);
        }
        else
        {
            queue.push_back(job);
        }
    }

    queueCV.notify_one();

    return job;
}

void WtGenService::submitBlocking(const std::shared_ptr<WtGenJob> &job)
{
    submit(job, /*front*/ true);
    job->done.get_future().wait();
}

bool WtGenService::isBusy(int scene, int osc) const
{
    return busyCountPerOsc[denseIdx(scene, osc)].load(std::memory_order_relaxed) > 0;
}

bool WtGenService::isGeneratingToOscillator(int scene, int osc) const
{
    return oscGenerateCountPerOsc[denseIdx(scene, osc)].load(std::memory_order_relaxed) > 0;
}

void WtGenService::runThread()
{
    for (;;)
    {
        std::shared_ptr<WtGenJob> job;
        {
            std::unique_lock<std::mutex> lk(queueLock);
            queueCV.wait(lk, [this]() { return !running || !queue.empty(); });
            if (!running && queue.empty())
            {
                return;
            }

            job = std::move(queue.front());
            queue.pop_front();

            // Drop a superseded pending Preview.
            if (job->mode == WtGenJob::Mode::Preview)
            {
                if (job->requestId < latestPreviewPerOsc[denseIdx(job->scene, job->osc)])
                {
                    busyCountPerOsc[denseIdx(job->scene, job->osc)].fetch_sub(
                        1, std::memory_order_relaxed);
                    continue; // Drop without running
                }
            }

            // Same for a supersedable Generate replaced by a newer same-osc Generate while pending.
            if (job->mode == WtGenJob::Mode::Generate && job->supersedable)
            {
                if (job->requestId < latestGeneratePerOsc[denseIdx(job->scene, job->osc)].load(
                                         std::memory_order_relaxed))
                {
                    busyCountPerOsc[denseIdx(job->scene, job->osc)].fetch_sub(
                        1, std::memory_order_relaxed);
                    if (job->generateTarget)
                    {
                        oscGenerateCountPerOsc[denseIdx(job->scene, job->osc)].fetch_sub(
                            1, std::memory_order_relaxed);
                    }
                    job->status.store(WtGenJob::Status::Complete, std::memory_order_release);
                    job->done.set_value();
                    continue; // Drop without running
                }
            }
        }

        const int idx = denseIdx(job->scene, job->osc);
        job->status.store(WtGenJob::Status::Working, std::memory_order_relaxed);

        bool ok = false;
        try
        {
            // Drive the private evaluator entirely from the job. Generation errors land straight
            // in job->error (first error wins) instead of touching the UI off-thread.
            evaluator.setStorage(storage);
            evaluator.setErrorOut(&job->error);
            evaluator.setScript(job->script);
            evaluator.setResolution((size_t)job->resolution);
            evaluator.setFrameCount((size_t)job->frameCount);
            evaluator.setSnapshotBundle(job->snapshot);
            evaluator.forceInvalidate();

            // Bail the generation at the next frame's pcall on either teardown (cancelGeneration)
            // or mid-generate supersede.
            auto cancelPred = [this, &job, idx]() {
                return cancelGeneration.load(std::memory_order_relaxed) ||
                       (job->supersedable &&
                        job->requestId < latestGeneratePerOsc[idx].load(std::memory_order_relaxed));
            };

            // One Lua pass yields the preview frames plus (unless previewOnly) the flat BuildWT
            // buffer, owned by gen.samples which frees itself on every path (including a throw).
            const bool previewOnly = (job->mode == WtGenJob::Mode::Preview);
            auto gen = evaluator.populateWavetable(cancelPred, previewOnly);
            ok = gen.ok;
            if (ok)
            {
                job->frames = std::move(gen.frames);
                job->wtName = evaluator.getSuggestedWavetableName();
                if (job->mode == WtGenJob::Mode::Generate && job->generateTarget)
                {
                    // Live-osc publish: BuildWT into the target under waveTableDataMutex. Only the
                    // buffers, current_id/queue_type/display name are poller-owned. Re-check
                    // the config token inside the lock: if the osc was replaced (patch load/undo)
                    // since submit, skip.
                    std::lock_guard<std::mutex> g(storage->waveTableDataMutex);
                    if (storage->wtGenPublishToken[idx].load(std::memory_order_relaxed) ==
                        job->publishToken)
                    {
                        job->generateTarget->wt.BuildWT(gen.samples.get(), gen.header,
                                                        gen.header.flags & wtf_is_sample);
                        job->published = true;
                    }
                }
                else if (job->mode == WtGenJob::Mode::Generate)
                {
                    // Export: build into the job-owned exportOut (no shared state, so no
                    // mutex and no config guard). The block-waiting submitter writes it to file.
                    job->exportOut.BuildWT(gen.samples.get(), gen.header,
                                           gen.header.flags & wtf_is_sample);
                }

                if (job->mode == WtGenJob::Mode::Preview ||
                    (job->mode == WtGenJob::Mode::Generate && job->generateTarget))
                {
                    // Preview and live-osc (not export): file the frames in the per-osc
                    // cache so a later overlay refresh for this osc reuses them.
                    storePreview(job->scene, job->osc, job->cacheKey, job->frames, job->frameCount);
                }
            }
        }
        catch (const std::exception &e)
        {
            ok = false;
            if (job->error.empty())
            {
                job->error = std::string("Wavetable generation error: ") + e.what();
            }
        }
        catch (...)
        {
            ok = false;
            if (job->error.empty())
            {
                job->error = "Unknown error during wavetable generation.";
            }
        }

        // A job replaced mid-run is not a failure. Checked here, not in the cancel predicate, so
        // a genuine failure that is also superseded is cleared.
        const bool superseded =
            !ok && job->supersedable &&
            job->requestId < latestGeneratePerOsc[idx].load(std::memory_order_relaxed);

        // The first error was set into job->error via the error sink. Stop aiming at this job's
        // string now the run is done. A superseded job is a no-op the newer Generate owns, so drop
        // any message, otherwise supply a generic one if it failed silently.
        evaluator.setErrorOut(nullptr);
        if (superseded)
        {
            job->error.clear();
        }
        else if (!ok && job->error.empty())
        {
            job->error = "Wavetable script generation failed.";
        }

        // Release-store the status (publishes frames/wtName/published/exportOut to the poller or
        // block-waiter), then unblock any block-waiter, then drop the active count so the spinner
        // turns off only once the result is ready.
        job->status.store((ok || superseded) ? WtGenJob::Status::Complete
                                             : WtGenJob::Status::Failed,
                          std::memory_order_release);
        job->done.set_value(); // unblocks submitBlocking (export/OSC); pollers never wait on it
        busyCountPerOsc[idx].fetch_sub(1, std::memory_order_relaxed);
        if (job->mode == WtGenJob::Mode::Generate && job->generateTarget)
        {
            oscGenerateCountPerOsc[idx].fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

} // namespace WavetableScript
} // namespace Surge
