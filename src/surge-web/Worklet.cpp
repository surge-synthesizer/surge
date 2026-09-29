// SPDX-License-Identifier: GPL-3.0-or-later
#include "Engine.h"
#include <emscripten.h>
#include <emscripten/webaudio.h>
#include <atomic>
#include <array>
namespace
{
alignas(16) std::array<unsigned char, 1024 * 1024> audioStack;
SurgeWebEngine *engine{};
EMSCRIPTEN_WEBAUDIO_T node{};
std::atomic<int> state{0}; // 0 idle, 1 starting, 2 ready, -1 failed
bool process(int ni, const AudioSampleFrame *in, int no, AudioSampleFrame *out, int,
             const AudioParamFrame *, void *)
{
    if (no != 1 || out[0].numberOfChannels != 2)
        return true;
    const int n = out[0].samplesPerChannel;
    const float *l = ni > 0 && in[0].numberOfChannels > 0 ? in[0].data : nullptr;
    const float *r = ni > 0 && in[0].numberOfChannels > 1 ? in[0].data + n : l;
    surge_render(engine, l, r, out[0].data, out[0].data + n, n);
    return true;
}
void ready(EMSCRIPTEN_WEBAUDIO_T context, bool success, void *)
{
    if (!success)
    {
        state.store(-1);
        return;
    }
    int channels[] = {2};
    EmscriptenAudioWorkletNodeCreateOptions options{};
    options.numberOfInputs = 1;
    options.numberOfOutputs = 1;
    options.outputChannelCounts = channels;
    node = emscripten_create_wasm_audio_worklet_node(context, "surge", &options, process, nullptr);
    emscripten_audio_node_connect(node, context, 0, 0);
    state.store(2);
}
void started(EMSCRIPTEN_WEBAUDIO_T context, bool success, void *)
{
    if (!success)
    {
        state.store(-1);
        return;
    }
    WebAudioWorkletProcessorCreateOptions options{};
    options.name = "surge";
    emscripten_create_wasm_audio_worklet_processor_async(context, &options, ready, nullptr);
}
} // namespace
extern "C" EMSCRIPTEN_KEEPALIVE int surge_start_audio(int context, SurgeWebEngine *instance)
{
    int expected = 0;
    if (!instance || !state.compare_exchange_strong(expected, 1))
        return 0;
    engine = instance;
    emscripten_start_wasm_audio_worklet_thread_async(context, audioStack.data(), audioStack.size(),
                                                     started, nullptr);
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_audio_state() { return state.load(); }
