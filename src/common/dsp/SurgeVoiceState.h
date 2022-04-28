/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

class SurgeStorage;

struct SurgeVoiceState
{
    bool gate;
    bool keep_playing, uberrelease;
    float pitch, scenepbpitch, fvel, pkey, priorpkey, detune, freleasevel;
    MidiKeyState *keyState;
    MidiChannelState *mainChannelState;
    MidiChannelState *voiceChannelState;
    int key, velocity, channel, scene_id, releasevelocity;
    float portasrc_key, portaphase;
    bool porta_doretrigger;

    // These items support the tuning-snap mode in MTS mode
    float keyRetuning;
    int keyRetuningForKey = -1000;

    // note that this does not replace the regular pitch bend modulator, only used to smooth MPE
    // pitch
    ControllerModulationSource mpePitchBend;
    float mpePitchBendRange;
    bool mpeEnabled;
    int64_t voiceOrderAtCreate{-1};

    float getPitch(SurgeStorage *storage);
};
