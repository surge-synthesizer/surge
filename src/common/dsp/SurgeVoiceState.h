//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

struct SurgeVoiceState
{
   bool gate;
   bool keep_playing, uberrelease;
   float pitch, fvel, pkey, detune, freleasevel;
   MidiKeyState* keyState;
   MidiChannelState* mainChannelState;
   MidiChannelState* voiceChannelState;
   int key, velocity, channel, scene_id, releasevelocity;
   float portasrc_key, portaphase;

   float getPitch();
};
