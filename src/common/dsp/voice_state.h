//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

struct sub3_voice_state
{
	bool gate;
	bool keep_playing,uberrelease;
	float pitch, fvel, pkey, detune;
	MidiKeyState* keyState;
   MidiChannelState* mainChannelState;
   MidiChannelState* voiceChannelState;
	int key,velocity,channel,scene_id;
	float portasrc_key,portaphase;

   float getPitch();
};