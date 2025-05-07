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

#ifndef SURGE_SRC_COMMON_DSP_SURGEVOICESTATE_H
#define SURGE_SRC_COMMON_DSP_SURGEVOICESTATE_H

class SurgeStorage;

struct SurgeVoiceState
{
    bool gate;
    bool keep_playing, uberrelease;
    float pitch, scenepbpitch, fvel, pkey, priorpkey, detune, freleasevel;
    MidiKeyState *keyState;
    MidiChannelState *mainChannelState;
    MidiChannelState *voiceChannelState;
    int key, velocity, channel, scene_id, releasevelocity, polylimit, scenemode, polymode,
        splitpoint;
    float portasrc_key, portaphase;
    bool porta_doretrigger{false};

    // These items support the tuning-snap mode in MTS mode
    float keyRetuning;
    int keyRetuningForKey = -1000;

    // note that this does not replace the regular pitch bend modulator, only used to smooth MPE
    // pitch
    ControllerModulationSource mpePitchBend;
    float mpePitchBendRange;
    bool mpeEnabled;
    bool mtsUseChannelWhenRetuning = false;
    int64_t voiceOrderAtCreate{-1};

    float getPitch(SurgeStorage *storage);
};

#endif // SURGE_SRC_COMMON_DSP_SURGEVOICESTATE_H
