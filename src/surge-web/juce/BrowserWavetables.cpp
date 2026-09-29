// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurgeSynthProcessor.h"
#include <emscripten.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

namespace
{
constexpr int slotsCount = n_scenes * n_oscs;
enum State { idle, downloading, ready, applying, retired };
struct Prepared
{
    OscillatorStorage oscillator;
    uint64_t publishToken{};
    int id{-1};
    std::string path;
    bool applied{false}, reslice{false}, fileSelection{false};
};
struct Slot
{
    std::atomic<State> state{idle};
    unsigned request{};
    std::unique_ptr<Prepared> prepared;
};
std::array<Slot, slotsCount> slots;
SurgeSynthesizer *synth{};
OscillatorStorage &oscillator(int slot)
{
    return synth->storage.getPatch().scene[slot / n_oscs].osc[slot % n_oscs];
}
void report(const std::string &error)
{
    EM_ASM({ SurgeBrowser.reportFile(UTF8ToString($0)); }, error.c_str());
}
bool current(int index)
{
    return slots[index].prepared &&
           slots[index].prepared->publishToken == synth->storage.wtGenPublishToken[index].load();
}
void fail(int index, const std::string &error)
{
    report("Unable to load wavetable; current table retained. " + error);
    slots[index].prepared.reset();
    slots[index].state.store(idle, std::memory_order_release);
}
}

void surge_attach_wavetables(SurgeSynthProcessor *processor)
{
    synth = processor->surge.get();
    // Oscillator changes can precede downloads. Keep a valid embedded table until
    // the requested replacement is ready, even when no table has been used yet.
    for (int i = 0; i < slotsCount; ++i)
    {
        auto &osc = oscillator(i);
        if (!osc.wt.everBuilt && !synth->storage.wt_list.empty() &&
            fs::file_size(synth->storage.wt_list[0].path) > 0)
            synth->storage.load_wt(0, &osc.wt, &osc);
        if (!osc.wt.everBuilt)
            if (auto initial = Surge::Storage::getSurgeCommonBinaryResource("memoryWavetable.wt"))
                synth->storage.load_wt_wt_mem(initial->data(), initial->size(), &osc.wt);
        osc.wt.queue_id = -1;
    }
    synth->storage.browserManagesWavetables = true;
}

// Called only at an audio block boundary (or on the control thread while audio
// is stopped). No files, decoding, allocation, destruction, or blocking waits.
void surge_publish_wavetables()
{
    if (!synth || synth->halt_engine) return;
    for (int i = 0; i < slotsCount; ++i)
    {
        // Scripted generation shares the same block-boundary ownership rule as
        // downloaded tables. Never copy or release a shared_ptr on this thread.
        {
            std::unique_lock<std::mutex> lock(synth->storage.waveTableDataMutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                using Publication = SurgeStorage::BrowserGeneratedWavetable;
                auto &publication = synth->storage.browserGeneratedWavetables[i];
                auto expected = Publication::pending;
                if (publication && publication->state.compare_exchange_strong(expected, Publication::applying,
                                                                               std::memory_order_acquire))
                {
                    if (publication->publishToken == synth->storage.wtGenPublishToken[i].load())
                    {
                        oscillator(i).wt.swapData(*publication->table);
                        publication->state.store(Publication::published, std::memory_order_release);
                    }
                    else publication->state.store(Publication::cancelled, std::memory_order_release);
                }
            }
        }
        auto &slot = slots[i];
        State expected = ready;
        if (!slot.state.compare_exchange_strong(expected, applying, std::memory_order_acquire))
            continue;
        std::unique_lock<std::mutex> lock(synth->storage.waveTableDataMutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            slot.state.store(ready, std::memory_order_release);
            continue;
        }
        if (current(i))
        {
            oscillator(i).wt.swapData(slot.prepared->oscillator.wt);
            slot.prepared->applied = true;
        }
        slot.state.store(retired, std::memory_order_release);
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_wt_downloaded(int index, unsigned request,
                                                                int success, const char *error)
{
    if (!synth || index < 0 || index >= slotsCount) return;
    auto &slot = slots[index];
    if (slot.state.load() != downloading || request != slot.request) return;
    if (!current(index))
    {
        slot.prepared.reset();
        slot.state.store(idle);
        return;
    }
    if (!success) { fail(index, error); return; }
    try
    {
        auto &job = *slot.prepared;
        synth->storage.load_wt(job.path, &job.oscillator.wt, &job.oscillator);
        if (!job.oscillator.wt.everBuilt) { fail(index, "The file could not be decoded"); return; }
        slot.state.store(ready, std::memory_order_release);
    }
    catch (const std::exception &exception) { fail(index, exception.what()); }
}

void surge_poll_wavetables()
{
    if (!synth || synth->halt_engine) return;
    for (int i = 0; i < slotsCount; ++i)
    {
        auto &slot = slots[i];
        auto &osc = oscillator(i);
        {
            std::lock_guard<std::mutex> lock(synth->storage.waveTableDataMutex);
            using Publication = SurgeStorage::BrowserGeneratedWavetable;
            auto &publication = synth->storage.browserGeneratedWavetables[i];
            if (publication)
            {
                const auto state = publication->state.load(std::memory_order_acquire);
                if (state == Publication::published || state == Publication::cancelled)
                    publication.reset(); // Old buffers retire here or on the worker, never audio.
            }
        }
        auto state = slot.state.load(std::memory_order_acquire);
        if (state == retired)
        {
            if (slot.prepared->applied && current(i))
            {
                auto &job = *slot.prepared;
                osc.wt.current_id = job.id;
                osc.wt.current_filename = job.path;
                osc.wt.frame_size_if_absent = job.oscillator.wt.frame_size_if_absent;
                if (!job.reslice)
                {
                    osc.wavetable_display_name = std::move(job.oscillator.wavetable_display_name);
                    osc.wavetable_script = std::move(job.oscillator.wavetable_script);
                    osc.wavetable_script_res_base = job.oscillator.wavetable_script_res_base;
                    osc.wavetable_script_nframes = job.oscillator.wavetable_script_nframes;
                    osc.wt.refresh_script_editor = true;
                }
                if (job.fileSelection && !uses_wavetabledata(osc.type.val.i))
                    osc.queue_type = ot_wavetable;
                osc.wt.refresh_display = true;
                osc.wt.force_refresh_display = true;
                synth->storage.getPatch().isDirty = true;
                synth->refresh_editor = true;
                report("");
            }
            // This owns the former live buffers after swapData. Free them here,
            // never on the audio thread.
            slot.prepared.reset();
            slot.state.store(idle, std::memory_order_release);
            state = idle;
        }
        if (osc.wt.queue_id < 0 && osc.wt.queue_filename.empty() && !osc.wt.queue_reslice)
            continue;
        if (state == applying) continue;
        if (state == ready)
        {
            State expected = ready;
            if (!slot.state.compare_exchange_strong(expected, idle, std::memory_order_acquire))
                continue;
        }
        auto job = std::make_unique<Prepared>();
        job->id = osc.wt.queue_id;
        job->path = osc.wt.queue_filename;
        job->fileSelection = !job->path.empty();
        job->reslice = !job->fileSelection && job->id < 0 && osc.wt.queue_reslice;
        job->oscillator.wt.frame_size_if_absent = osc.wt.frame_size_if_absent;
        {
            std::lock_guard<std::mutex> lock(synth->storage.waveTableDataMutex);
            job->publishToken = ++synth->storage.wtGenPublishToken[i];
            if (job->reslice) job->oscillator.wt.Copy(&osc.wt);
        }
        const auto resliceSize = osc.wt.reslice_size;
        const auto resliceFrames = osc.wt.reslice_frames;
        const auto resliceFlags = osc.wt.reslice_flags;
        osc.wt.queue_id = -1;
        osc.wt.queue_filename.clear();
        osc.wt.queue_reslice = false;
        osc.wt.reslice_size = osc.wt.reslice_frames = -1;
        osc.wt.reslice_flags = 0;
        if (job->id >= 0 && job->id < synth->storage.wt_list.size())
            job->path = synth->storage.wt_list[job->id].path.u8string();
        else if (job->fileSelection)
            for (int id = 0; id < synth->storage.wt_list.size(); ++id)
                if (synth->storage.wt_list[id].path.u8string() == job->path) { job->id = id; break; }
        slot.prepared = std::move(job);
        ++slot.request;
        if (slot.prepared->reslice)
        {
            if (!slot.prepared->oscillator.wt.Reslice(resliceSize, resliceFrames, resliceFlags))
                fail(i, "Invalid reslice dimensions");
            else slot.state.store(ready, std::memory_order_release);
        }
        else if (slot.prepared->path.empty()) fail(i, "Unknown wavetable");
        else
        {
            slot.state.store(downloading, std::memory_order_release);
            report("Loading selected wavetable...");
            EM_ASM({ SurgeFactory.prepareWavetable($0, $1, UTF8ToString($2)); },
                   i, slot.request, slot.prepared->path.c_str());
        }
    }
    if (!synth->audio_processing_active) surge_publish_wavetables();
}

extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_wt_count()
{
    return synth ? synth->storage.wt_list.size() : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_browser_wt_name(int index)
{
    return synth && index >= 0 && index < slotsCount ? oscillator(index).wavetable_display_name.c_str() : "";
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_request_wt(int index, const char *path)
{
    if (!synth || index < 0 || index >= slotsCount) return 0;
    for (int id = 0; id < synth->storage.wt_list.size(); ++id)
        if (synth->storage.wt_list[id].path.u8string() == path)
        {
            oscillator(index).wt.queue_id = id;
            return 1;
        }
    return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_wt_size(int index)
{
    return synth && index >= 0 && index < slotsCount ? oscillator(index).wt.size : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_wt_frames(int index)
{
    return synth && index >= 0 && index < slotsCount ? oscillator(index).wt.n_tables : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE float surge_browser_wt_sample(int index, int frame, int sample)
{
    if (!synth || index < 0 || index >= slotsCount) return 0.f;
    const auto &table = oscillator(index).wt;
    if (!table.everBuilt || frame < 0 || frame >= table.n_tables || sample < 0 || sample >= table.size)
        return 0.f;
    return table.TableF32WeakPointers[0][frame][sample];
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_request_wt_file(int index, const char *path)
{
    if (!synth || index < 0 || index >= slotsCount || !path) return 0;
    oscillator(index).wt.queue_filename = path;
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_reslice_wt(int index, int size, int frames)
{
    if (!synth || index < 0 || index >= slotsCount) return 0;
    auto &table = oscillator(index).wt;
    table.reslice_size = size;
    table.reslice_frames = frames;
    table.reslice_flags = table.flags;
    table.queue_reslice = true;
    return 1;
}
