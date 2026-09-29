// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurgeSynthProcessor.h"
#include "SurgeSynthEditor.h"
#include "PatchDB.h"
#include "PatchFileValidation.h"
#include "dsp/effects/ConvolutionKernelWorker.h"
#include "dsp/effects/EffectRetirementWorker.h"
#include "dsp/modulators/FormulaModulationHelper.h"
#include <emscripten.h>
#include <emscripten/eventloop.h>
#include <emscripten/heap.h>
#include <cstdio>
extern "C" void surge_dispatch_messages();
void surge_attach_audio(SurgeSynthProcessor *);
void surge_poll_audio_startup();
void surge_attach_wavetables(SurgeSynthProcessor *);
void surge_poll_wavetables();
static SurgeSynthProcessor *browserProcessor{};
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_formula_compilation_count()
{
    return browserProcessor
               ? static_cast<double>(browserProcessor->surge->storage.formulaGlobalData
                                         ->audioFunctions.compilationAttempts.load(std::memory_order_relaxed))
               : -1.;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_active_voices()
{
    return browserProcessor ? browserProcessor->surge->storage.activeVoiceCount.load() : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_wasm_memory_bytes()
{
    return static_cast<double>(emscripten_get_heap_size());
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_effects_retired()
{
    return browserProcessor ? browserProcessor->surge->browserEffectRetirement->retired() : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_effects_constructed()
{
    return browserProcessor ? browserProcessor->surge->browserConstructedEffects.load() : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_airwindows_adoptions()
{
    return browserProcessor ? browserProcessor->surge->browserAirwindowsAdoptions.load() : 0;
}
unsigned surge_airwindows_realtime_constructions();
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_airwindows_realtime_constructions()
{
    return surge_airwindows_realtime_constructions();
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_convolution_applied()
{
    return browserProcessor ? browserProcessor->surge->storage.browserConvolutionWorker->applied() : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_convolution_reload_applied(int slot)
{
    return browserProcessor && slot >= 0 && slot < n_fx_slots
        ? browserProcessor->surge->storage.browserConvolutionWorker->appliedForSlot(n_fx_slots + slot) : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_scene()
{
    return browserProcessor ? browserProcessor->surge->storage.getPatch().scene_active.val.i : -1;
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_browser_validate_patch(const char *path)
{
    static std::string error;
    std::vector<char> data;
    Surge::PatchStorage::readValidatedPatch(fs::path(path), data, error);
    return error.c_str();
}
static int requestedPatch = -1, requestToken = 0;
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_browser_control_state()
{
    // Main-thread diagnostics: read only atomic handoff fields, never mutable
    // patch strings while a loader may own them.
    static char result[256];
    if (!browserProcessor) return "null";
    const auto &s = *browserProcessor->surge;
    std::snprintf(result, sizeof(result),
                  "{\"queued\":%d,\"requested\":%d,\"token\":%d,\"ready\":%d,"
                  "\"halted\":%d,\"audioActive\":%d,\"loaderRequested\":%d,\"loaderPending\":%d}",
                  s.patchid_queue.load(), requestedPatch, requestToken, s.browserReadyPatch.load(),
                  int(s.halt_engine.load()), int(s.audio_processing_active.load()),
                  int(s.browserPatchLoadRequested.load()), int(s.browserPatchLoadPending.load()));
    return result;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_patch_count()
{
    return browserProcessor ? browserProcessor->surge->storage.patch_list.size() : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_browser_patch_name()
{
    // Main-thread API: keep a stable snapshot while the loader owns the patch.
    // A new loader can only be started by this thread's later event-loop turn.
    static std::string name;
    if (browserProcessor && !browserProcessor->surge->halt_engine.load(std::memory_order_acquire))
        name = browserProcessor->surge->storage.getPatch().name;
    return name.c_str();
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_search_count(const char *query)
{
    if (!browserProcessor) return -1;
    auto &storage = browserProcessor->surge->storage;
    storage.initializePatchDb();
    if (!storage.patchDBInitialized || storage.patchDB->numberOfJobsOutstanding() > 0) return -1;
    return storage.patchDB->queryFromQueryString(query).size();
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_request_patch(const char *path)
{
    if (!browserProcessor) return 0;
    auto &s = *browserProcessor->surge;
    for (int i = 0; i < s.storage.patch_list.size(); ++i)
        if (s.storage.patch_list[i].path.u8string() == path)
        {
            s.patchid_queue = i;
            return 1;
        }
    return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_patch_prepared(int id, int token, int success,
                                                                 const char *error)
{
    if (!browserProcessor || token != requestToken) return;
    auto &s = *browserProcessor->surge;
    if (s.patchid_queue.load() != id) return;
    if (success)
    {
        s.browserReadyPatch.store(id, std::memory_order_release);
        EM_ASM({ SurgeBrowser.reportFile(""); });
    }
    else
    {
        s.patchid_queue.compare_exchange_strong(id, -1);
        requestedPatch = -1;
        s.browserReadyPatch.store(-1, std::memory_order_release);
        s.refresh_editor = true;
        // clang-format off
        EM_ASM({ SurgeBrowser.reportFile('Patch download failed; current patch retained. ' + UTF8ToString($0)); }, error);
        // clang-format on
    }
}
static void prepareQueuedPatch()
{
    auto &s = *browserProcessor->surge;
    if (s.storage.browserFavoritesRefreshPending &&
        s.storage.patchDB->numberOfJobsOutstanding() == 0)
    {
        std::unique_lock<std::mutex> guard(s.patchLoadSpawnMutex, std::try_to_lock);
        if (guard.owns_lock() && !s.halt_engine.load(std::memory_order_acquire))
        {
            s.storage.refresh_patchlist();
            s.refresh_editor = true;
        }
    }
    s.storage.initializePatchDb();
    if (s.storage.browserConvolutionErrors.exchange(0, std::memory_order_acq_rel))
        s.storage.reportError("Unable to prepare the edited impulse response. The previous kernel remains active.",
                              "Convolution Error");
    const int id = s.patchid_queue.load();
    if (id != requestedPatch)
    {
        requestedPatch = id;
        ++requestToken;
        s.browserReadyPatch.store(-1, std::memory_order_release);
        if (id >= 0 && id < s.storage.patch_list.size())
        {
            const auto path = s.storage.patch_list[id].path.u8string();
            EM_ASM({ SurgeBrowser.reportFile("Loading selected patch...");
                     SurgeFactory.prepare($0, $1, UTF8ToString($2)); }, id, requestToken, path.c_str());
        }
    }
    s.processAudioThreadOpsWhenAudioEngineUnavailable();
    // Maintain the committed snapshot even if the API's first read occurs
    // while a background loader owns the mutable patch string.
    surge_browser_patch_name();
}
static void pumpControls(void *)
{
    EM_ASM({ SurgeRuntime.phase('messages'); });
    surge_dispatch_messages();
    EM_ASM({ SurgeRuntime.phase('audio-startup'); });
    surge_poll_audio_startup();
    EM_ASM({ SurgeRuntime.phase('patch'); });
    prepareQueuedPatch();
    EM_ASM({ SurgeRuntime.phase('effect-construction'); });
    browserProcessor->surge->prepareBrowserEffects();
    EM_ASM({ SurgeRuntime.phase('wavetable'); });
    surge_poll_wavetables();
    EM_ASM({ SurgeRuntime.phase('idle'); });
}
int main()
{
    setenv("JUCE_FONT_PATH", "/fonts", 1);
    setenv("HOME", "/user", 1);
    juce::File("/user").createDirectory();
    juce::File("/factory").createDirectory();
    static juce::ScopedJuceInitialiser_GUI init;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    static SurgeSynthProcessor processor;
    browserProcessor = &processor;
    surge_attach_audio(&processor);
    surge_attach_wavetables(&processor);
    static std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->addToDesktop(0);
    editor->setVisible(true);
    // Animation frames may stop while the page is hidden. Keep engine handoffs
    // and JUCE messages on a timer; only painting depends on animation frames.
    // Chrome can throttle timers too, but control work no longer waits for paint.
    pumpControls(nullptr);
    emscripten_set_interval(pumpControls, 16, nullptr);
    emscripten_set_main_loop(
        [] {
            for (int i = 0; i < juce::ComponentPeer::getNumPeers(); ++i)
                juce::ComponentPeer::getPeer(i)->performAnyPendingRepaintsNow();
        },
        0, false);
}
