// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurgeSynthProcessor.h"
#include "../MidiQueue.h"
#include "../Transport.h"
#include "dsp/modulators/FormulaModulationHelper.h"
#include <emscripten.h>
#include <emscripten/webaudio.h>
#include <array>
#include <atomic>
#include <sstream>
void surge_publish_wavetables();
void surge_set_convolution_realtime(bool);
extern "C" int surge_enable_audio();
extern "C" void surge_browser_audio_changed(int);
extern "C" void surge_browser_audio_failed(int);
extern "C" void surge_retire_audio_thread(void *);
namespace
{
SurgeSynthProcessor *processor{};
EMSCRIPTEN_WEBAUDIO_T context{}, node{};
alignas(16) std::array<unsigned char, 1024 * 1024> audioStack;
juce::MidiBuffer midi;
Surge::Web::MidiQueue performanceEvents;
Surge::Web::Transport transport;
std::atomic<unsigned> requestedTempo{0};
double audioSampleRate = 48000.;
bool offlineActive = false;
uint64_t offlineFrame = 0;
class BrowserPlayhead : public juce::AudioPlayHead
{
  public:
    Surge::Web::Transport::State state{};
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo result;
        result.setBpm(processor->standaloneTempo.load());
        result.setTimeSignature(TimeSignature{state.numerator, state.denominator});
        result.setIsPlaying(state.playing);
        result.setPpqPosition(state.ppq);
        const double barLength = state.numerator * 4. / state.denominator;
        result.setPpqPositionOfLastBarStart(std::floor(state.ppq / barLength) * barLength);
        return result;
    }
} playhead;
std::atomic<int> status{0};
std::atomic<unsigned> blocks{0}, callbacks{0};
// UI-readable progress markers; no logging or allocation on the callback.
// 0 idle/completed, 1 format check, 2 MIDI, 3 transport, 4 wavetable handoff, 5 DSP.
std::atomic<int> renderStage{0}, renderFrames{0}, renderOutputs{0};
double startupDeadline = 0;
bool initializationPending = false, closing = false, processorFailed = false;
bool audioThreadInitialized = false, preparationPending = false;
void report(int value, const char *message)
{
    status.store(value);
    if (value < 0)
        EM_ASM({ globalThis.SurgeAudioInput?.input?.audioFailed(UTF8ToString($0)); }, message);
    EM_ASM({ document.getElementById('status').textContent = UTF8ToString($0); }, message);
}
void process(juce::AudioBuffer<float> &buffer, uint64_t firstFrame)
{
    // The loader owns the engine between the audio fade and its release-store.
    // Preserve queued performance events until that state handoff is complete.
    if (processor->surge->halt_engine.load(std::memory_order_acquire))
    {
        buffer.clear();
        transport.advance(buffer.getNumSamples(), audioSampleRate, processor->standaloneTempo.load());
        renderStage.store(0, std::memory_order_relaxed);
        return;
    }
    renderStage.store(2, std::memory_order_relaxed);
    midi.clear();
    if (performanceEvents.drain(firstFrame, buffer.getNumSamples(), [](const auto &event, int offset) {
            midi.addEvent(event.bytes.data(), event.length, offset);
        }))
        for (int channel = 1; channel <= 16; ++channel)
        {
            midi.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(channel, 123, 0), 0);
        }
    renderStage.store(3, std::memory_order_relaxed);
    playhead.state = transport.beginBlock();
    if (const auto tempo = requestedTempo.exchange(0, std::memory_order_acq_rel))
    {
        processor->standaloneTempo.store(tempo / 100.f);
        processor->surge->storage.unstreamedTempo = tempo / 100.f;
        processor->surge->refresh_vkb = true;
    }
    // Respect the original preference for recalling a patch's standalone tempo.
    if (processor->surge->storage.unstreamedTempo > 0)
        processor->standaloneTempo.store(processor->surge->storage.unstreamedTempo);
    renderStage.store(4, std::memory_order_relaxed);
    surge_publish_wavetables();
    renderStage.store(5, std::memory_order_relaxed);
    surge_set_convolution_realtime(!offlineActive);
    processor->processBlock(buffer, midi);
    surge_set_convolution_realtime(false);
    transport.advance(buffer.getNumSamples(), audioSampleRate, processor->surge->time_data.tempo);
    renderStage.store(0, std::memory_order_relaxed);
    // Publishing from Surge's 32-sample process() would let the loader race
    // the remainder of this 128-sample JUCE callback and its post-processing.
    // Transfer ownership only after all engine accesses above have finished.
    if (processor->surge->browserPatchLoadRequested.exchange(false, std::memory_order_acq_rel))
        processor->surge->browserPatchLoadPending.store(true, std::memory_order_release);
}
bool render(int ni, const AudioSampleFrame *in, int no, AudioSampleFrame *out, int,
            const AudioParamFrame *, void *)
{
    callbacks.fetch_add(1, std::memory_order_relaxed);
    renderStage.store(1, std::memory_order_relaxed);
    renderOutputs.store(no > 0 ? out[0].numberOfChannels : 0, std::memory_order_relaxed);
    renderFrames.store(no > 0 ? out[0].samplesPerChannel : 0, std::memory_order_relaxed);
    if (no != 1 || out[0].numberOfChannels != 2)
        return true;
    const int n = out[0].samplesPerChannel;
    float *channels[] = {out[0].data, out[0].data + n};
    for (int ch = 0; ch < 2; ++ch)
    {
        if (ni > 0 && in[0].numberOfChannels > 0)
            std::copy_n(in[0].data + std::min(ch, in[0].numberOfChannels - 1) * n, n, channels[ch]);
        else
            std::fill_n(channels[ch], n, 0.f);
    }
    juce::AudioBuffer<float> buffer(channels, 2, n);
    const auto firstFrame = static_cast<uint64_t>(EM_ASM_DOUBLE({ return currentFrame; }));
    process(buffer, firstFrame);
    blocks.fetch_add(1, std::memory_order_relaxed);
    return true;
}
void ready(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *)
{
    if (audioContext != context || closing || !initializationPending)
        return;
    if (!success)
    {
        initializationPending = false;
        processor->surge->audio_processing_active = false;
        report(-1, "Unable to create the audio processor. Check Chrome's console.");
        return;
    }
    initializationPending = false;
    int channelCounts[] = {2};
    EmscriptenAudioWorkletNodeCreateOptions options{};
    options.numberOfInputs = 1;
    options.numberOfOutputs = 1;
    options.outputChannelCounts = channelCounts;
    // Reserve processing before the first callback can start. The main event
    // loop must no longer take the synchronous, audio-inactive patch-load path.
    processor->surge->audio_processing_active = true;
    node = emscripten_create_wasm_audio_worklet_node(audioContext, "surge-xt", &options, render,
                                                     nullptr);
    if (!node) { processor->surge->audio_processing_active = false; report(-1, "Unable to create the audio node."); return; }
    // clang-format off
    EM_ASM({
        const handle = $0;
        SurgeAudioLifecycle.watchProcessor(emscriptenGetAudioObject(handle), emscriptenGetAudioObject($1),
            () => Module._surge_browser_audio_failed(handle));
    }, audioContext, node);
    // clang-format on
    emscripten_audio_node_connect(node, audioContext, 0, 0);
    EM_ASM({ globalThis.SurgeAudioInput?.input?.setGraph(emscriptenGetAudioObject($0), emscriptenGetAudioObject($1)); }, audioContext, node);
    surge_browser_audio_changed(audioContext);
}
void started(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *)
{
    if (audioContext != context || closing || !initializationPending)
        return;
    if (!success)
    {
        initializationPending = false;
        processor->surge->audio_processing_active = false;
        report(-1, "Unable to start AudioWorklet. Verify cross-origin isolation.");
        return;
    }
    WebAudioWorkletProcessorCreateOptions options{};
    options.name = "surge-xt";
    emscripten_create_wasm_audio_worklet_processor_async(audioContext, &options, ready, nullptr);
}
void prepareAndStart()
{
    if (!preparationPending || !context || closing) return;
    // Context replacement may overlap a loader started by the old worklet.
    // Never change sample rate or warm up effects while that loader owns state.
    std::unique_lock<std::mutex> lock(processor->surge->patchLoadSpawnMutex, std::try_to_lock);
    if (!lock.owns_lock() || processor->surge->halt_engine.load(std::memory_order_acquire))
        return;
    const double rate = EM_ASM_DOUBLE({ return emscriptenGetAudioObject($0).sampleRate; }, context);
    audioSampleRate = rate;
    midi.ensureSize(65536);
    midi.clear();
    processor->prepareToPlay(rate, 128);
    // No worklet is running yet and the loader mutex is held. This also covers
    // formula edits made while audio was disabled, without executing user code.
    Surge::Formula::preparePatchCompilation(&processor->surge->storage);
    // Prepare the initial patch and effects before the real-time callback starts.
    juce::AudioBuffer<float> warmup(2, 128);
    warmup.clear();
    processor->processBlock(warmup, midi);
    // Patches remain editable while the worklet module loads. The ready callback
    // reserves processing before creating/connecting the node on the main thread.
    processor->surge->audio_processing_active = false;
    if (processor->surge->browserPatchLoadRequested.exchange(false, std::memory_order_acq_rel))
        processor->surge->browserPatchLoadPending.store(true, std::memory_order_release);
    preparationPending = false;
    startupDeadline = emscripten_get_now() + 15000;
    report(1, "Starting audio…");
    emscripten_resume_audio_context_sync(context);
    initializationPending = true;
    audioThreadInitialized = true;
    emscripten_start_wasm_audio_worklet_thread_async(context, audioStack.data(), audioStack.size(),
                                                     started, nullptr);
}
} // namespace
void surge_attach_audio(SurgeSynthProcessor *p)
{
    processor = p;
    playhead.state = transport.beginBlock();
    processor->setPlayHead(&playhead);
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_enable_audio()
{
    if (!processor)
        return 0;
    if (closing) return 0;
    if (context && status.load() < 0)
    {
        if (initializationPending)
        {
            // A timeout is not cancellation. Keep waiting on the same import;
            // never create a second worklet using the same stack.
            startupDeadline = emscripten_get_now() + 15000;
            report(1, "Still starting audio…");
            emscripten_resume_audio_context_sync(context);
            return 1;
        }
        closing = true;
        report(3, "Closing the previous audio context before retrying…");
        // clang-format off
        EM_ASM({
            const handle = $0;
            SurgeAudioLifecycle.close(emscriptenGetAudioObject(handle), $1 ? emscriptenGetAudioObject($1) : null)
                .then(() => Module._surge_browser_audio_closed(handle, 1),
                      () => Module._surge_browser_audio_closed(handle, 0));
        }, context, node);
        // clang-format on
        return 1;
    }
    if (context)
    {
        if (node) processor->surge->audio_processing_active = true;
        emscripten_resume_audio_context_sync(context);
        return 1;
    }
    offlineActive = false;
    processorFailed = false;
    performanceEvents.panic();
    EmscriptenWebAudioCreateAttributes attributes{};
    attributes.latencyHint = "interactive";
    context = emscripten_create_audio_context(&attributes);
    if (!context)
    {
        report(-1, "Unable to create an audio context.");
        return 0;
    }
    // clang-format off
    EM_ASM({
        const handle = $0;
        SurgeAudioLifecycle.guard(emscriptenGetAudioObject(handle));
        emscriptenGetAudioObject(handle).onstatechange = () => Module._surge_browser_audio_changed(handle);
    }, context);
    // clang-format on
    preparationPending = true;
    report(1, "Waiting for patch preparation before starting audio…");
    // Preserve the user activation even when engine preparation must wait.
    emscripten_resume_audio_context_sync(context);
    prepareAndStart();
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_audio_failed(int handle)
{
    if (!context || handle != context || closing) return;
    processorFailed = true;
    performanceEvents.panic();
    report(-1, "The audio processor stopped unexpectedly. Click Enable audio to restart.");
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_audio_changed(int handle)
{
    if (!context || handle != context || closing) return;
    const auto state = emscripten_audio_context_state(context);
    if (state == AUDIO_CONTEXT_STATE_CLOSED)
    {
        initializationPending = false;
        preparationPending = false;
        processor->surge->audio_processing_active = false;
        performanceEvents.panic();
        report(-1, "The audio context closed. Click Enable audio to restart.");
    }
    else if (node && !processorFailed)
    {
        if (state == AUDIO_CONTEXT_STATE_RUNNING)
            report(2, "Audio enabled");
        else
        {
            processor->surge->audio_processing_active = false;
            performanceEvents.panic();
            report(4, "Audio suspended or interrupted. Click Enable audio to resume.");
        }
    }
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_audio_closed(int handle, int success)
{
    if (!closing || handle != context) return;
    closing = false;
    if (!success)
    {
        report(-1, "Unable to close the audio context. Click Enable audio to retry cleanup.");
        return;
    }
    if (audioThreadInitialized)
    {
        surge_retire_audio_thread(audioStack.data());
        audioThreadInitialized = false;
    }
    // The pinned SDK's destroy_audio_context only suspends. After close() has
    // resolved, remove its registry handles without calling suspend on a closed
    // context. emAudio is the registry used by Emscripten 6.0.10's WebAudio API.
    EM_ASM({ delete emAudio[$0]; if ($1) delete emAudio[$1]; }, context, node);
    context = node = 0;
    initializationPending = false;
    processor->surge->audio_processing_active = false;
    report(0, "Restarting audio…");
    surge_enable_audio();
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_audio_status() { return status.load(); }
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_audio_blocks() { return blocks.load(); }
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_browser_audio_diagnostics()
{
    static std::string value;
    std::ostringstream json;
    json << "{\"callbacks\":" << callbacks.load() << ",\"blocks\":" << blocks.load()
         << ",\"stage\":" << renderStage.load() << ",\"frames\":" << renderFrames.load()
         << ",\"outputChannels\":" << renderOutputs.load() << "}";
    value = json.str();
    return value.c_str();
}

void surge_poll_audio_startup()
{
    if (preparationPending)
    {
        prepareAndStart();
        return;
    }
    if (status.load() == 1 && emscripten_get_now() > startupDeadline)
        report(-1, "AudioWorklet is still pending. Check the audio device and click Enable audio "
                   "to continue waiting.");
}

extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_midi(int statusByte, int first, int second, double frame)
{
    return (status.load() == 2 || offlineActive) && performanceEvents.push(statusByte, first, second, frame);
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_panic() { performanceEvents.panic(); }
extern "C" EMSCRIPTEN_KEEPALIVE unsigned surge_browser_midi_overflows()
{
    return performanceEvents.overflowCount();
}
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_audio_time()
{
    return context ? EM_ASM_DOUBLE({ return emscriptenGetAudioObject($0).currentTime; }, context) : 0.;
}
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_audio_rate()
{
    return context ? EM_ASM_DOUBLE({ return emscriptenGetAudioObject($0).sampleRate; }, context) : 0.;
}

extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_transport(int playing, int numerator, int denominator)
{
    return transport.configure(playing != 0, numerator, denominator);
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_rewind() { transport.rewind(); }
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_transport_tempo(double bpm)
{
    if (!std::isfinite(bpm) || bpm < 1 || bpm > 999) return 0;
    requestedTempo.store(static_cast<unsigned>(std::round(bpm * 100)), std::memory_order_release);
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_transport_bpm()
{
    const auto pending = requestedTempo.load(std::memory_order_acquire);
    return pending ? pending / 100. : processor ? processor->standaloneTempo.load() : 120.;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_transport_playing()
{
    return transport.requested().playing;
}

// Exclusive offline rendering exercises the same processor/event/transport path.
// It is unavailable once a browser AudioContext has been created, so it cannot
// run concurrently with an AudioWorklet callback. Render lengths must be
// multiples of the engine's 32-sample block (maximum 192000 samples per call).
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_offline_begin(double rate)
{
    if (!processor || context || !std::isfinite(rate) || rate < 8000 || rate > 192000) return 0;
    audioSampleRate = rate;
    offlineFrame = 0;
    performanceEvents.panic();
    performanceEvents.drain(0, 128, [](const auto &, int) {});
    midi.ensureSize(65536);
    midi.clear();
    processor->prepareToPlay(rate, 128);
    processor->surge->stopSound();
    offlineActive = true;
    return 1;
}
static double renderOffline(int frames, const float *left, const float *right)
{
    if (!offlineActive || context || frames < 1 || frames > 192000 || frames % BLOCK_SIZE != 0) return -1.;
    double energy = 0.;
    juce::AudioBuffer<float> buffer(2, 128);
    for (int remaining = frames; remaining > 0;)
    {
        const int count = std::min(128, remaining);
        buffer.setSize(2, count, false, false, true);
        buffer.clear();
        const int offset = frames - remaining;
        if (left) std::copy_n(left + offset, count, buffer.getWritePointer(0));
        if (right || left) std::copy_n((right ? right : left) + offset, count, buffer.getWritePointer(1));
        process(buffer, offlineFrame);
        offlineFrame += count;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < count; ++sample)
            {
                const auto value = buffer.getSample(channel, sample);
                energy += double(value) * value;
            }
        remaining -= count;
    }
    return energy;
}
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_offline_render(int frames)
{
    return renderOffline(frames, nullptr, nullptr);
}
extern "C" EMSCRIPTEN_KEEPALIVE double surge_browser_offline_render_input(int frames, const float *left, const float *right)
{
    return renderOffline(frames, left, right);
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_browser_offline_state()
{
    if (!offlineActive || context) return "{}";
    const auto &time = processor->surge->time_data;
    std::ostringstream state;
    state.precision(15);
    state << "{\"ppq\":" << transport.position() << ",\"enginePpq\":" << time.ppqPos
          << ",\"tempo\":" << time.tempo << ",\"playing\":" << (time.isPlaying ? "true" : "false")
          << ",\"numerator\":" << time.timeSigNumerator << ",\"denominator\":" << time.timeSigDenominator << "}";
    static std::string result;
    result = state.str();
    return result.c_str();
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_browser_offline_end()
{
    if (!offlineActive || context) return;
    processor->surge->stopSound();
    processor->surge->audio_processing_active = false;
    processor->releaseResources();
    offlineActive = false;
    performanceEvents.panic();
}
