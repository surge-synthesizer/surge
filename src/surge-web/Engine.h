// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    // All functions except surge_render require exclusive engine ownership.
    // No C++ exceptions may cross this boundary. Errors persist until the next call.
    typedef struct SurgeWebEngine SurgeWebEngine;
    SurgeWebEngine *surge_create(double sample_rate, const char *data_path);
    void surge_destroy(SurgeWebEngine *engine);
    const char *surge_error(void);
    int surge_load_patch(SurgeWebEngine *engine, const char *path);
    int surge_save_patch(SurgeWebEngine *engine, const char *path);
    int surge_parameter_count(SurgeWebEngine *engine);
    // Development diagnostic; requires exclusive ownership, outside rendering.
    double surge_formula_compilation_count(SurgeWebEngine *engine);
    const char *surge_parameter_info(SurgeWebEngine *engine, int id);
    int surge_set_parameter(SurgeWebEngine *engine, int id, float normalized);
    // Offline oscillator selection uses the original control/default path.
    // May construct an oscillator or load a table; exclusive ownership required.
    int surge_set_oscillator_type(SurgeWebEngine *engine, int scene, int oscillator, int type);
    // Development-only reproducibility control for storage.rngGen, not every
    // independent DSP RNG. Call before notes with exclusive engine ownership.
    int surge_seed_storage_rng(SurgeWebEngine *engine, uint32_t seed);
    // Development-only: seed an existing voice MSEG after note-on and before
    // rendering. Requires exclusive ownership; rejects absent/non-MSEG voices.
    int surge_seed_voice_mseg(SurgeWebEngine *engine, int scene, int lfo, uint32_t seed);
    // Change effect family through the parameter path; exclusive ownership required.
    int surge_set_effect_type(SurgeWebEngine *engine, int slot, int type);
    // Offline edits resolve a pending Airwindows selector before editing its
    // sub-effect controls. May construct an effect; never call from audio.
    int surge_set_effect_parameter(SurgeWebEngine *engine, int slot, int parameter, float normalized);
    int surge_midi(SurgeWebEngine *engine, int status, int data1, int data2);
    int surge_set_transport(SurgeWebEngine *engine, double bpm, double ppq);
    // Copies planar impulse data into an FX slot. A null right channel means mono.
    // This preparation API requires exclusive ownership and must not run in an audio callback.
    int surge_set_impulse(SurgeWebEngine *engine, int slot, double sample_rate,
                          const float *left, const float *right, int frames);
    // Planar stereo; arbitrary frame counts are adapted to Surge's 32-frame blocks.
    int surge_render(SurgeWebEngine *engine, const float *input_left, const float *input_right,
                     float *left, float *right, int frames);
    // Development comparison harness: exactly 32 frames through one effect,
    // without voices, routing or modulation. Requires exclusive engine ownership.
    int surge_render_effect_block(SurgeWebEngine *engine, int slot, const float *input_left,
                                  const float *input_right, float *left, float *right);
#ifdef __cplusplus
}
#endif
