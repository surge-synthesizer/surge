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

#ifndef SURGE_SRC_COMMON_DSP_TILTNOISEADAPTER_H
#define SURGE_SRC_COMMON_DSP_TILTNOISEADAPTER_H

#include "globals.h"

class SurgeVoice;

// Adapter for sst-effects TiltNoise to allow it to be used directly from SurgeVoice.
struct VoiceTiltNoiseAdapter
{
    // Simple wrapper that holds the SurgeVoice storage and params. Be VERY CAREFUL with
    // lifetime here. These are raw pointers! They're only valid for when they're valid in
    // SurgeVoice.
    struct StorageContainer
    {
        SurgeVoice const *s;

        StorageContainer() = default;
        StorageContainer(const StorageContainer &) = delete;
        StorageContainer(StorageContainer &&) = delete;
    };

    static constexpr uint16_t blockSize{BLOCK_SIZE_OS};
    using BaseClass = StorageContainer;

    static float getFloatParam(const StorageContainer *o, size_t index);
    static int getIntParam(const StorageContainer *o, size_t index);
    static float dbToLinear(const StorageContainer *o, float db);
    static float equalNoteToPitch(const StorageContainer *o, float f);
    static float getSampleRate(const StorageContainer *o);
    static float getSampleRateInv(const StorageContainer *o);

    // None of these are used by TiltNoise and will print errors to the console if they are
    // called.
    static void setFloatParam(StorageContainer *o, size_t index, float val);
    static void setIntParam(StorageContainer *o, size_t index, int val);
    static void preReservePool(StorageContainer *o, size_t size);
    static uint8_t *checkoutBlock(StorageContainer *o, size_t size);
    static void returnBlock(StorageContainer *o, uint8_t *b, size_t size);
};

#endif // SURGE_SRC_COMMON_DSP_TILTNOISEADAPTER_H
