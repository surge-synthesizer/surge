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

#include "TiltNoiseAdapter.h"
#include "SurgeVoice.h"
#include <sst/voice-effects/generator/TiltNoise.h>

float VoiceTiltNoiseAdapter::getFloatParam(const StorageContainer *o, size_t index)
{
    using TN = sst::voice_effects::generator::TiltNoise<VoiceTiltNoiseAdapter>;

    // Multiple float parameters.
    switch (index)
    {
    case TN::fpLevel:
        // TiltNoise expects it in dB, converts to linear internally.
        return o->s->localcopy[o->s->lag_id[le_noise]].f;
    case TN::fpTilt:
    {
        // TiltNoise scale is -6 to +6; our slider is -1 to +1.
        int id = o->s->scene->noise_colour.param_id_in_scene;
        float col = limit_range(o->s->localcopy[id].f, -1.f, 1.f);
        return col * 6.f;
        break;
    }
    case TN::fpStereoWidth:
        return 1.f;
    default:
        std::cout << "Attempted to access an undefined float parameter # " << index
                  << " in VoiceTiltNoiseAdapter!" << std::endl;
        return 0.f;
    }
    return 0.f;
}

int VoiceTiltNoiseAdapter::getIntParam(const StorageContainer *o, size_t index)
{
    // TiltNoise only has a single int param, stereo.
    int id = o->s->scene->noise_colour.param_id_in_scene;
    bool is_wide = o->s->scene->filterblock_configuration.val.i == fc_wide;
    // Inverted from the deform bit.
    if ((id & 1) && is_wide)
    {
        return 1;
    }
    return 0;
}

float VoiceTiltNoiseAdapter::dbToLinear(const StorageContainer *o, float db)
{
    return o->s->storage->db_to_linear(db);
}

float VoiceTiltNoiseAdapter::equalNoteToPitch(const StorageContainer *o, float f)
{
    return o->s->storage->note_to_pitch(f);
}

float VoiceTiltNoiseAdapter::getSampleRate(const StorageContainer *o)
{
    return o->s->storage->samplerate;
}
float VoiceTiltNoiseAdapter::getSampleRateInv(const StorageContainer *o)
{
    return 1.0 / o->s->storage->samplerate;
}

void VoiceTiltNoiseAdapter::setFloatParam(StorageContainer *o, size_t index, float val)
{
    std::cout << "Unsupported attempt to set a float parameter in VoiceTiltNoiseAdapter."
              << std::endl;
}

void VoiceTiltNoiseAdapter::setIntParam(StorageContainer *o, size_t index, int val)
{
    std::cout << "Unsupported attempt to set an int parameter in VoiceTiltNoiseAdapter."
              << std::endl;
}

void VoiceTiltNoiseAdapter::preReservePool(StorageContainer *o, size_t size)
{
    std::cout << "Unsupported attempt to call preReservePool in VoiceTiltNoiseAdapter."
              << std::endl;
}

uint8_t *VoiceTiltNoiseAdapter::checkoutBlock(StorageContainer *o, size_t size)
{
    std::cout << "Unsupported attempt to call checkoutBlock in VoiceTiltNoiseAdapter." << std::endl;
    return NULL;
}

void VoiceTiltNoiseAdapter::returnBlock(StorageContainer *o, uint8_t *b, size_t size)
{
    std::cout << "Unsupported attempt to call returnBlock in VoiceTiltNoiseAdapter." << std::endl;
}
