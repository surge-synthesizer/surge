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

#include "SurgeSynthesizer.h"
#include "DspUtilities.h"
#include <time.h>
#if MAC || LINUX
#include <pthread.h>
#else
#include <windows.h>
#include <process.h>
#endif

#if TARGET_AUDIOUNIT
#include "aulayer.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
#elif TARGET_VST3
#include "SurgeVst3Processor.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
#elif TARGET_LV2
#include "SurgeLv2Wrapper.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
#elif TARGET_APP
#include "PluginLayer.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
#elif TARGET_HEADLESS
#include "HeadlessPluginLayerProxy.h"
#else
#include "Vst2PluginInstance.h"

#if LINUX
#include "../linux/linux-aeffguieditor.h"
#else
#include "vstgui/plugin-bindings/aeffguieditor.h"
#endif
#endif
#include "SurgeParamConfig.h"

#include "UserDefaults.h"

using namespace std;

SurgeSynthesizer::SurgeSynthesizer(PluginLayer* parent, std::string suppliedDataPath)
    //: halfband_AL(false),halfband_AR(false),halfband_BL(false),halfband_BR(false),
    : storage(suppliedDataPath)
    , hpA(&storage)
   , hpB(&storage)
   , _parent(parent)
   , halfbandA(6, true)
   , halfbandB(6, true)
   , halfbandIN(6, true)
{
   switch_toggled_queued = false;
   audio_processing_active = false;
   halt_engine = false;
   release_if_latched[0] = true;
   release_if_latched[1] = true;
   release_anyway[0] = false;
   release_anyway[1] = false;
   load_fx_needed = true;
   process_input = false; // hosts set this if there are input busses
   
   fx_suspend_bitmask = 0;

   demo_counter = 10;

   memset(fx, 0, sizeof(void*) * 8);
   srand((unsigned)time(nullptr));
   memset(storage.getPatch().scenedata[0], 0, sizeof(pdata) * n_scene_params);
   memset(storage.getPatch().scenedata[1], 0, sizeof(pdata) * n_scene_params);
   memset(storage.getPatch().globaldata, 0, sizeof(pdata) * n_global_params);
   memset(mControlInterpolatorUsed, 0, sizeof(bool) * num_controlinterpolators);
   memset((void*)fxsync, 0, sizeof(FxStorage) * 8);
   for (int i = 0; i < 8; i++)
   {
      memcpy((void*)&fxsync[i], (void*)&storage.getPatch().fx[i], sizeof(FxStorage));
      fx_reload[i] = false;
   }

   allNotesOff();

   for (int i = 0; i < MAX_VOICES; i++)
   {
      voices_usedby[0][i] = 0;
      voices_usedby[1][i] = 0;
   }

   FBQ[0] =
       (QuadFilterChainState*)_aligned_malloc((MAX_VOICES >> 2) * sizeof(QuadFilterChainState), 16);
   FBQ[1] =
       (QuadFilterChainState*)_aligned_malloc((MAX_VOICES >> 2) * sizeof(QuadFilterChainState), 16);

   for(int i=0; i<(MAX_VOICES >> 2); ++i)
   {
       InitQuadFilterChainStateToZero(&(FBQ[0][i]));
       InitQuadFilterChainStateToZero(&(FBQ[1][i]));
   }


   SurgePatch& patch = storage.getPatch();

   patch.polylimit.val.i = 8;
   for (int sc = 0; sc < 2; sc++)
   {
      SurgeSceneStorage& scene = patch.scene[sc];
      scene.modsources.resize(n_modsources);
      for (int i = 0; i < n_modsources; i++)
         scene.modsources[i] = 0;
      scene.modsources[ms_modwheel] = new ControllerModulationSource();
      scene.modsources[ms_aftertouch] = new ControllerModulationSource();
      scene.modsources[ms_pitchbend] = new ControllerModulationSource();

      for (int l = 0; l < n_lfos_scene; l++)
      {
         scene.modsources[ms_slfo1 + l] = new LfoModulationSource();
         ((LfoModulationSource*)scene.modsources[ms_slfo1 + l])
            ->assign(&storage, &scene.lfo[n_lfos_voice + l],
            storage.getPatch().scenedata[sc], 0,
            &patch.stepsequences[sc][n_lfos_voice + l]);
      }
   }
   for (int i = 0; i < n_customcontrollers; i++)
   {
      patch.scene[0].modsources[ms_ctrl1 + i] = new ControllerModulationSource();
      patch.scene[1].modsources[ms_ctrl1 + i] =
         patch.scene[0].modsources[ms_ctrl1 + i];
   }

   amp.set_blocksize(BLOCK_SIZE);
   FX1.set_blocksize(BLOCK_SIZE);
   FX2.set_blocksize(BLOCK_SIZE);
   send[0][0].set_blocksize(BLOCK_SIZE);
   send[0][1].set_blocksize(BLOCK_SIZE);
   send[1][0].set_blocksize(BLOCK_SIZE);
   send[1][1].set_blocksize(BLOCK_SIZE);

   polydisplay = 0;
   refresh_editor = false;
   patch_loaded = false;
   storage.getPatch().category = "Init";
   storage.getPatch().name = "Init";
   storage.getPatch().comment = "";
   storage.getPatch().author = "";
   midiprogramshavechanged = false;

   for (int i = 0; i < BLOCK_SIZE; i++)
   {
      input[0][i] = 0.f;
      input[1][i] = 0.f;
   }

   masterfade = 1.f;
   current_category_id = -1;
   patchid_queue = -1;
   patchid = -1;
   CC0 = 0;
   CC32 = 0;
   PCH = 0;
   for (int i = 0; i < 8; i++)
   {
      refresh_ctrl_queue[i] = -1;
      refresh_parameter_queue[i] = -1;
   }
   for (int i = 0; i < 8; i++)
      vu_peak[i] = 0.f;
   learn_param = -1;
   learn_custom = -1;

   for (int i = 0; i < 16; i++)
   {
      memset(&channelState[i], 0, sizeof(MidiChannelState));
   }

   mpeEnabled = false;
   mpeVoices = 0;
   mpePitchBendRange = Surge::Storage::getUserDefaultValue(&storage, "mpePitchBendRange", 48);
   mpeGlobalPitchBendRange = 0;

#if TARGET_VST3 || TARGET_VST2 || TARGET_AUDIOUNIT 
   // If we are in a daw hosted environment, choose a preset from the preset library
   // Skip LV2 until we sort out the patch change dynamics there
   int pid = 0;
   for (auto p : storage.patch_list)
   {
      if (p.name == "Init Saw" && storage.patch_category[p.category].name == "Init")
      {
         // patchid_queue = pid;
         // This is the wrong thing to do. I *think* what we need to do here is to
         // explicitly load the file directly inline on thread using loadPatchFromFile
         // and set up the location int rather than defer the load. But do that
         // later
         break;
      }
      pid++;
   }
#endif   
}

SurgeSynthesizer::~SurgeSynthesizer()
{
   allNotesOff();

   _aligned_free(FBQ[0]);
   _aligned_free(FBQ[1]);

   for (int sc = 0; sc < 2; sc++)
   {
      delete storage.getPatch().scene[sc].modsources[ms_modwheel];
      delete storage.getPatch().scene[sc].modsources[ms_aftertouch];
      delete storage.getPatch().scene[sc].modsources[ms_pitchbend];
      for (int i = 0; i < n_lfos_scene; i++)
         delete storage.getPatch().scene[sc].modsources[ms_slfo1 + i];
   }
   for (int i = 0; i < n_customcontrollers; i++)
      delete storage.getPatch().scene[0].modsources[ms_ctrl1 + i];
}

int SurgeSynthesizer::calculateChannelMask(int channel, int key)
{
   /*
   ** Just because I always forget
   **
   ** A voice is routed to scene n if channelmask & n. So "1" means scene A, "2" means scene B and "3" (= 2 | 1 ) = both.
   */
   int channelmask = channel;

   if ((channel == 0) || (channel > 2) || mpeEnabled || storage.getPatch().scenemode.val.i == sm_chsplit ) 
   {
      switch (storage.getPatch().scenemode.val.i)
      {
      case sm_single:
         //	case sm_morph:
         if (storage.getPatch().scene_active.val.i == 1)
            channelmask = 2;
         else
            channelmask = 1;
         break;
      case sm_dual:
         channelmask = 3;
         break;
      case sm_split:
         if (key < storage.getPatch().splitkey.val.i)
            channelmask = 1;
         else
            channelmask = 2;
         break;
      case sm_chsplit:
         if( channel < ( (int)( storage.getPatch().splitkey.val.i / 8 ) + 1 ) )
            channelmask = 1;
         else
            channelmask = 2;

         break;
      }
   }
   else if(storage.getPatch().scenemode.val.i == sm_single)
   {
       if (storage.getPatch().scene_active.val.i == 1)
           channelmask = 2;
       else
           channelmask = 1;
   }
   
   return channelmask;
}

void SurgeSynthesizer::playNote(char channel, char key, char velocity, char detune)
{
   if (halt_engine)
      return;

   // For split/dual
   // MIDI Channel 1 plays the split/dual
   // MIDI Channel 2 plays A
   // MIDI Channel 3 plays B

   int channelmask = calculateChannelMask(channel, key);

   if (channelmask & 1)
   {
      playVoice(0, channel, key, velocity, detune);
   }
   if (channelmask & 2)
   {
      playVoice(1, channel, key, velocity, detune);
   }

   channelState[channel].keyState[key].keystate = velocity;
   channelState[channel].keyState[key].lastdetune = detune;

   /*
   ** OK so why is there hold stuff here? This is play not release.
   ** Well if you release a key with the pedal down it goes into the
   ** 'release me later' buffer. If you press the key again it stays there
   ** so even with the key held, you end up releasing it when you pedal. 
   **
   ** Or: NoteOn / Pedal On / Note Off / Note On / Pedal Off should leave the note ringing
   **
   ** and right now it doesn't
   */
   bool noHold = ! channelState[channel].hold;
   if( mpeEnabled )
      noHold = noHold && ! channelState[0].hold;

   if( ! noHold )
   {
      for( int s=0; s<2; ++s )
      {
         for( auto &h : holdbuffer[s] )
         {            
            if( h.first == channel && h.second == key )
            {
               h.first = -1;
               h.second = -1;
            }
         }
      }
   }


}

void SurgeSynthesizer::softkillVoice(int s)
{
   list<SurgeVoice*>::iterator iter, max_playing, max_released;
   int max_age = 0, max_age_release = 0;
   iter = voices[s].begin();

   while (iter != voices[s].end())
   {
      SurgeVoice* v = *iter;
      assert(v);
      if (v->state.gate)
      {
         if (v->age > max_age)
         {
            max_age = v->age;
            max_playing = iter;
         }
      }
      else if (!v->state.uberrelease)
      {
         if (v->age_release > max_age_release)
         {
            max_age_release = v->age_release;
            max_released = iter;
         }
      }
      iter++;
   }
   if (max_age_release)
      (*max_released)->uber_release();
   else if (max_age)
      (*max_playing)->uber_release();
}

// only allow 'margin' number of voices to be softkilled simultaneously
void SurgeSynthesizer::enforcePolyphonyLimit(int s, int margin)
{
   list<SurgeVoice*>::iterator iter;
   if (voices[s].size() > (storage.getPatch().polylimit.val.i + margin))
   {
      int excess_voices =
          max(0, (int)voices[s].size() - (storage.getPatch().polylimit.val.i + margin));
      iter = voices[s].begin();

      while (iter != voices[s].end())
      {
         if (excess_voices < 1)
            break;

         SurgeVoice* v = *iter;
         if (v->state.uberrelease)
         {
            excess_voices--;
            freeVoice(v);
            iter = voices[s].erase(iter);
         }
         else
            iter++;
      }
   }
}

int SurgeSynthesizer::getNonUltrareleaseVoices(int s)
{
   int count = 0;

   list<SurgeVoice*>::iterator iter;
   iter = voices[s].begin();
   while (iter != voices[s].end())
   {
      SurgeVoice* v = *iter;
      assert(v);
      if (!v->state.uberrelease)
         count++;
      iter++;
   }

   return count;
}

int SurgeSynthesizer::getNonReleasedVoices(int s)
{
   int count = 0;

   list<SurgeVoice*>::iterator iter;
   iter = voices[s].begin();
   while (iter != voices[s].end())
   {
      SurgeVoice* v = *iter;
      assert(v);
      if (v->state.gate)
         count++;
      iter++;
   }

   return count;
}

SurgeVoice* SurgeSynthesizer::getUnusedVoice(int scene)
{
   for (int i = 0; i < MAX_VOICES; i++)
   {
      if (!voices_usedby[scene][i])
      {
         voices_usedby[scene][i] = scene + 1;
         return &voices_array[scene][i];
      }
   }
   return 0;
}

void SurgeSynthesizer::freeVoice(SurgeVoice* v)
{
   for (int i = 0; i < MAX_VOICES; i++)
   {
      if (voices_usedby[0][i] && (v == &voices_array[0][i]))
      {
         voices_usedby[0][i] = 0;
      }
      if (voices_usedby[1][i] && (v == &voices_array[1][i]))
      {
         voices_usedby[1][i] = 0;
      }
   }
   v->freeAllocatedElements();
}

int SurgeSynthesizer::getMpeMainChannel(int voiceChannel, int key)
{
   if (mpeEnabled)
   {
      return 0;
   }

   return voiceChannel;
}

void SurgeSynthesizer::playVoice(int scene, char channel, char key, char velocity, char detune)
{
   if (getNonReleasedVoices(scene) == 0)
   {
      for (int l = 0; l < n_lfos_scene; l++)
         storage.getPatch().scene[scene].modsources[ms_slfo1 + l]->attack();
   }

   int excessVoices =
       max(0, (int)getNonUltrareleaseVoices(scene) - storage.getPatch().polylimit.val.i + 1);

   for (int i = 0; i < excessVoices; i++)
   {
      softkillVoice(scene);
   }
   enforcePolyphonyLimit(scene, 3);

   switch (storage.getPatch().scene[scene].polymode.val.i)
   {
   case pm_poly:
   {
      // sub3_voice *nvoice = (sub3_voice*) _aligned_malloc(sizeof(sub3_voice),16);
      SurgeVoice* nvoice = getUnusedVoice(scene);
      if (nvoice)
      {
         int mpeMainChannel = getMpeMainChannel(channel, key);

         voices[scene].push_back(nvoice);
         new (nvoice) SurgeVoice(&storage, &storage.getPatch().scene[scene],
                                 storage.getPatch().scenedata[scene], key, velocity, channel, scene,
                                 detune, &channelState[channel].keyState[key],
                                 &channelState[mpeMainChannel], &channelState[channel], mpeEnabled);
      }
      break;
   }
   case pm_mono:
   case pm_mono_fp:
   case pm_latch:
   {
      list<SurgeVoice*>::const_iterator iter;
      bool glide = false;

      for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
      {
         SurgeVoice* v = *iter;
         if (v->state.scene_id == scene)
         {
            if (v->state.gate)
            {
               glide = true;
            }
            v->uber_release();
         }
      }
      // sub3_voice *nvoice = (sub3_voice*) _aligned_malloc(sizeof(sub3_voice),16);
      SurgeVoice* nvoice = getUnusedVoice(scene);
      if (nvoice)
      {
         int mpeMainChannel = getMpeMainChannel(channel, key);

         voices[scene].push_back(nvoice);
         if ((storage.getPatch().scene[scene].polymode.val.i == pm_mono_fp) && !glide)
            storage.last_key[scene] = key;
         new (nvoice) SurgeVoice(&storage, &storage.getPatch().scene[scene],
                                 storage.getPatch().scenedata[scene], key, velocity, channel, scene,
                                 detune, &channelState[channel].keyState[key],
                                 &channelState[mpeMainChannel], &channelState[channel], mpeEnabled);
      }
   }
   break;
   case pm_mono_st:
   case pm_mono_st_fp:
   {
      bool found_one = false;
      list<SurgeVoice*>::const_iterator iter;
      for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
      {
         SurgeVoice* v = *iter;
         if ((v->state.scene_id == scene) && (v->state.gate))
         {
            v->legato(key, velocity, detune);
            found_one = true;
            if (mpeEnabled)
            {
               /*
               ** This voice was created on a channel but is being legato held to another channel
               ** so it needs to borrow the channel and channelState. Obviously this can only
               ** happen in MPE mode.
               */
               v->state.channel = channel;
               v->state.voiceChannelState = &channelState[channel];
            }
            break;
         }
         else
         {
            if (v->state.scene_id == scene)
               v->uber_release(); // make this optional for poly legato
         }
      }
      if (!found_one)
      {
         int mpeMainChannel = getMpeMainChannel(channel, key);

         // sub3_voice *nvoice = (sub3_voice*) _aligned_malloc(sizeof(sub3_voice),16);
         SurgeVoice* nvoice = getUnusedVoice(scene);
         if (nvoice)
         {
            voices[scene].push_back(nvoice);
            new (nvoice) SurgeVoice(&storage, &storage.getPatch().scene[scene],
                                    storage.getPatch().scenedata[scene], key, velocity, channel,
                                    scene, detune, &channelState[channel].keyState[key],
                                    &channelState[mpeMainChannel], &channelState[channel], mpeEnabled);
         }
      }
   }
   break;
   }
}

void SurgeSynthesizer::releaseScene(int s)
{
   list<SurgeVoice*>::const_iterator iter;
   for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
   {
      //_aligned_free(*iter);
      freeVoice(*iter);
   }
   voices[s].clear();
}

void SurgeSynthesizer::releaseNote(char channel, char key, char velocity)
{
   for( int s=0; s<2; ++s )
   {
      for( auto *v : voices[s] )
      {
         if ((v->state.key == key) && (v->state.channel == channel))
            v->state.releasevelocity = velocity;
      }
   }

   bool noHold = ! channelState[channel].hold;
   if( mpeEnabled )
      noHold = noHold && ! channelState[0].hold;
   
   for( int s=0; s<2; ++s )
   {
      if (noHold)
         releaseNotePostHoldCheck(s, channel, key, velocity);
      else
      {
         holdbuffer[s].push_back(std::make_pair(channel,key)); // hold pedal is down, add to bufffer
      }
   }
}

void SurgeSynthesizer::releaseNotePostHoldCheck(int scene, char channel, char key, char velocity)
{
   channelState[channel].keyState[key].keystate = 0;
   list<SurgeVoice*>::const_iterator iter;
   for (int s = 0; s < 2; s++)
   {
      bool do_switch = false;
      int k = 0;

      for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
      {
         SurgeVoice* v = *iter;
         int lowkey = 0, hikey = 127;
         if (storage.getPatch().scenemode.val.i == sm_split)
         {
            if (v->state.scene_id == 0)
               hikey = storage.getPatch().splitkey.val.i - 1;
            else
               lowkey = storage.getPatch().splitkey.val.i;
         }

         switch (storage.getPatch().scene[v->state.scene_id].polymode.val.i)
         {
         case pm_poly:
            if ((v->state.key == key) && (v->state.channel == channel))
               v->release();
            break;
         case pm_mono:
         case pm_mono_fp:
         case pm_latch:
         {
            /*
            ** In these modes, our job when we release a note is to see if
            ** any ohter note is held.
            **
            ** In normal midi mode, that means scanning the keystate of our
            ** channel looking for another note.
            **
            ** In MPE mode, where each note is per channel, that means
            ** scanning all non-main channels rather than ourself for the
            ** highest note
            */
            if ((v->state.key == key) && (v->state.channel == channel))
            {
	       int activateVoiceKey = 60, activateVoiceChannel = 0; // these will be overriden

               // v->release();
               if (!mpeEnabled)
               {
                  for (k = hikey; k >= lowkey && !do_switch; k--) // search downwards
                  {
                     if (channelState[channel].keyState[k].keystate)
                     {
                        do_switch = true;
                        activateVoiceKey = k;
                        activateVoiceChannel = channel;
                        break;
                     }
                  }
               }
               else
               {
                  for (k = hikey; k >= lowkey && !do_switch; k--)
                  {
                     for (int mpeChan = 1; mpeChan < 16; ++mpeChan)
                     {
                        if (mpeChan != channel && channelState[mpeChan].keyState[k].keystate)
                        {
                           do_switch = true;
                           activateVoiceChannel = mpeChan;
                           activateVoiceKey = k;
                           break;
                        }
                     }
                  }
               }
               if (!do_switch)
               {
                  if (storage.getPatch().scene[v->state.scene_id].polymode.val.i != pm_latch)
                     v->release();
               }
               else
               {
                  // confirm that no notes are active
                  v->uber_release();
                  if (getNonUltrareleaseVoices(scene) == 0)
                  {
                     playVoice(scene, activateVoiceChannel, activateVoiceKey, velocity,
                               channelState[activateVoiceChannel].keyState[k].lastdetune);
                  }
               }
            }
         }
         break;
         case pm_mono_st:
         case pm_mono_st_fp:
         {
            /*
            ** In these modes the note will collide on the main channel
            */
            int stateChannel = getMpeMainChannel(v->state.channel, v->state.key);
            int noteChannel = getMpeMainChannel(channel, key);

            if ((v->state.key == key) && (stateChannel == noteChannel))
            {
               bool do_release = true;
               /*
               ** Again the note I need to legato to is on my channel in non MPE and
               ** is on another channel in MPE mode
               */
               if (!mpeEnabled)
               {
                  for (k = hikey; k >= lowkey && do_release; k--) // search downwards
                  {
                     if (channelState[channel].keyState[k].keystate)
                     {
                        v->legato(k, velocity, channelState[channel].keyState[k].lastdetune);
                        do_release = false;
                        break;
                     }
                  }
               }
               else
               {
                  for (k = hikey; k >= lowkey && do_release; k--) // search downwards
                  {
                     for (int mpeChan = 1; mpeChan < 16; ++mpeChan)
                     {
                        if (mpeChan != channel && channelState[mpeChan].keyState[k].keystate)
                        {
                           v->legato(k, velocity, channelState[mpeChan].keyState[k].lastdetune);
                           do_release = false;
                           // See the comment above at the other _st legato spot
                           v->state.channel = mpeChan;
                           v->state.voiceChannelState = &channelState[mpeChan];
                           break;
                        }
                     }
                  }
               }

               if (do_release)
                  v->release();
            }
         }
         break;
         };
      }
   }

   // for(int scene=0; scene<2; scene++)
   {
      if (getNonReleasedVoices(scene) == 0)
      {
         for (int l = 0; l < n_lfos_scene; l++)
            storage.getPatch().scene[scene].modsources[ms_slfo1 + l]->release();
      }
   }
}

void SurgeSynthesizer::pitchBend(char channel, int value)
{
   if (mpeEnabled)
   {
      float bendNormalized = value / 8192.f;
      channelState[channel].pitchBend = value;

      if (channel == 0)
      {
         channelState[channel].pitchBendInSemitones = bendNormalized * mpeGlobalPitchBendRange;
      }
      else
      {
         channelState[channel].pitchBendInSemitones = bendNormalized * mpePitchBendRange;
      }
   }

   /*
   ** So here's the thing. We want global pitch bend modulation to work for other things in MPE mode.
   ** This code has beenhere forever. But that means we need to ignore the channel[0] mpe pitchbend
   ** elsewhere, especially since the range was hardwired to 2 (but is now 0). As far as I know the
   ** main MPE devices don't have a global pitch bend anyway so this just screws up regular keyboards
   ** sending channel 0 pitch bend in MPE mode.
   */
   if (!mpeEnabled || channel == 0)
   {
      storage.pitch_bend = value / 8192.f;
      ((ControllerModulationSource*)storage.getPatch().scene[0].modsources[ms_pitchbend])
          ->set_target(storage.pitch_bend);
      ((ControllerModulationSource*)storage.getPatch().scene[1].modsources[ms_pitchbend])
          ->set_target(storage.pitch_bend);
   }
}
void SurgeSynthesizer::channelAftertouch(char channel, int value)
{
   float fval = (float)value / 127.f;

   channelState[channel].pressure = fval;

   if (!mpeEnabled || channel == 0)
   {
      ((ControllerModulationSource*)storage.getPatch().scene[0].modsources[ms_aftertouch])
          ->set_target(fval);
      ((ControllerModulationSource*)storage.getPatch().scene[1].modsources[ms_aftertouch])
          ->set_target(fval);
   }
}

void SurgeSynthesizer::polyAftertouch(char channel, int key, int value)
{
   float fval = (float)value / 127.f;
   storage.poly_aftertouch[0][key & 127] = fval;
   storage.poly_aftertouch[1][key & 127] = fval;
}

void SurgeSynthesizer::programChange(char channel, int value)
{
   PCH = value;
   // load_patch((CC0<<7) + PCH);
   patchid_queue = (CC0 << 7) + PCH;
}

void SurgeSynthesizer::updateDisplay()
{
#if ! TARGET_AUDIOUNIT
   getParent()->updateDisplay();
#endif
   refresh_editor = true;
}

void SurgeSynthesizer::sendParameterAutomation(long index, float value)
{
   int externalparam = remapInternalToExternalApiId(index);

#if TARGET_VST3 || TARGET_AUDIOUNIT
   if( index >= metaparam_offset )
      externalparam = index;
#endif

   if (externalparam >= 0)
   {
#if TARGET_AUDIOUNIT
      getParent()->ParameterUpdate(externalparam);
#elif TARGET_VST3
      getParent()->setParameterAutomated(externalparam, value);
#elif TARGET_HEADLESS || TARGET_APP
      // NO OP
#else
      getParent()->setParameterAutomated(externalparam, value);
#endif
   }
}

void SurgeSynthesizer::onRPN(int channel, int lsbRPN, int msbRPN, int lsbValue, int msbValue)
{
    /* OK there are two things we are dealing with here
     
     1: The MPE specification v1.0 section 2.1.1 says the message format for RPN is
     
        Bn 64 06
        Bn 65 00
        Bn 06 mm
     
     where n = 0 is lower zone, n=F is upper zone, all others are invalid, and mm=0 means no MPE and mm=1->F is zone.
     
     So you would think the Roli Seaboard would send, since it is one zone
     
        B0 64 06 B0 65 00 B0 06 0F
     
     and be done with it. If it did this code would work.
     
     But it doesn't. At least with Roli Seaboard Firmware 1.1.7.
     
     Instead on *each* channel it sends
     
        Bn 64 04 Bn 64 0 Bn 06 00
        Bn 64 03 Bn 64 0 Bn 06 00
     
     for each channel. Which seems unrelated to the spec. But as a result the original onRPN code
     means you get no MPE with a Roli Seaboard.

     Hey one year later an edit: Those aren't coming from ROLI they are coming from Logic PRO and
     now that I correct modify and stream MPE state, we should not listen to those messages.
     */
    
   if (lsbRPN == 0 && msbRPN == 0) // PITCH BEND RANGE
   {
      if (channel == 1)
      {
         mpePitchBendRange = msbValue;
      }
      else if (channel == 0)
      {
         mpeGlobalPitchBendRange = msbValue;
      }
   }
   else if (lsbRPN == 6 && msbRPN == 0) // MPE mode
   {
      mpeEnabled = msbValue > 0;
      mpeVoices = msbValue & 0xF;
      if( mpePitchBendRange < 0 )
         mpePitchBendRange = Surge::Storage::getUserDefaultValue(&storage, "mpePitchBendRange", 48);
      mpeGlobalPitchBendRange = 0;
      return;
   }
   else if (lsbRPN == 4 && msbRPN == 0 && channel != 0 && channel != 0xF )
   {
      /*
      ** This is code sent by logic in all cases for some reason. In ancient times
      ** I thought it came from a roli. But I since changed the MPE state management so
      ** with 1.6.5 do this:
      */
#if 0      
       // This is the invalid message which the ROLI sends. Rather than have the Roli not work
       mpeEnabled = true;
       mpeVoices = msbValue & 0xF;
       mpePitchBendRange = Surge::Storage::getUserDefaultValue(&storage, "mpePitchBendRange", 48);
       std::cout << __LINE__ << " " << __FILE__ << " MPEE=" << mpeEnabled << " MPEPBR=" << mpePitchBendRange << std::endl;
       mpeGlobalPitchBendRange = 0;
#endif       
       return;
   }
}

void SurgeSynthesizer::onNRPN(int channel, int lsbNRPN, int msbNRPN, int lsbValue, int msbValue)
{}

float int7ToBipolarFloat(int x)
{
   if (x > 64)
   {
      return (x - 64) * (1.f / 63.f);
   }
   else if (x < 64)
   {
      return (x - 64) * (1.f / 64.f);
   }

   return 0;
}

void SurgeSynthesizer::channelController(char channel, int cc, int value)
{
   float fval = (float)value * (1.f / 127.f);
   // store all possible NRPN & RPNs in a short array .. just amounts for 128kb or thereabouts
   // anyway
   switch (cc)
   {
   case 0:
      CC0 = value;
      return;
   case 1:
      ((ControllerModulationSource*)storage.getPatch().scene[0].modsources[ms_modwheel])
          ->set_target(fval);
      ((ControllerModulationSource*)storage.getPatch().scene[1].modsources[ms_modwheel])
          ->set_target(fval);
      break;
   case 6:
      if (channelState[channel].nrpn_last)
      {
         channelState[channel].nrpn_v[1] = value;

         onNRPN(channel, channelState[channel].nrpn[0], channelState[channel].nrpn[1],
                channelState[channel].nrpn_v[0], channelState[channel].nrpn_v[1]);
      }
      else
      {
         channelState[channel].rpn_v[1] = value;

         onRPN(channel, channelState[channel].rpn[0], channelState[channel].rpn[1],
               channelState[channel].rpn_v[0], channelState[channel].rpn_v[1]);
      }
      return;

   case 10:
   {
      if (mpeEnabled)
      {
         channelState[channel].pan = int7ToBipolarFloat(value);
         return;
      }
      break;
   }

   case 32:
      CC32 = value;
      return;

   case 38:
      if (channelState[channel].nrpn_last)
         channelState[channel].nrpn_v[0] = value;
      else
         channelState[channel].rpn_v[0] = value;
      break;

   case 64:
   {
      channelState[channel].hold = value > 63; // check hold pedal

      // OK in single mode, only purge scene 0, but in split or dual purge both, and in chsplit
      // pick based on channel
      switch(storage.getPatch().scenemode.val.i)
      {
      case sm_single:
         purgeHoldbuffer(storage.getPatch().scene_active.val.i);
         break;
      case sm_split:
      case sm_dual:
         purgeHoldbuffer(0);
         purgeHoldbuffer(1);
         break;
      case sm_chsplit:
         if( mpeEnabled && channel == 0 ) // a control channel message
         {
            purgeHoldbuffer(0);
            purgeHoldbuffer(1);
         }
         else
         {
            if( channel < ( (int)( storage.getPatch().splitkey.val.i / 8 ) + 1 ) )
               purgeHoldbuffer(0);
            else
               purgeHoldbuffer(1);
         }
         break;
      }

      return;
   }

   case 74:
   {
      if (mpeEnabled)
      {
         channelState[channel].timbre = int7ToBipolarFloat(value);
         return;
      }
      break;
   }

   case 98: // NRPN LSB
      channelState[channel].nrpn[0] = value;
      channelState[channel].nrpn_last = true;
      return;
   case 99: // NRPN MSB
      channelState[channel].nrpn[1] = value;
      channelState[channel].nrpn_last = true;
      return;
   case 100: // RPN LSB
      channelState[channel].rpn[0] = value;
      channelState[channel].nrpn_last = false;
      return;
   case 101: // RPN MSB
      channelState[channel].rpn[1] = value;
      channelState[channel].nrpn_last = false;
      return;
   case 120: // all sound off
   case 123: // all notes off
      return;
   };

   int cc_encoded = cc;

   if ((cc == 6) || (cc == 38)) // handle RPN/NRPNs (untested)
   {
      int tv, cnum;
      if (channelState[channel].nrpn_last)
      {
         tv = (channelState[channel].nrpn_v[1] << 7) + channelState[channel].nrpn_v[0];
         cnum = (channelState[channel].nrpn[1] << 7) + channelState[channel].nrpn[0];
         cc_encoded = cnum | (1 << 16);
      }
      else
      {
         tv = (channelState[channel].rpn_v[1] << 7) + channelState[channel].rpn_v[0];
         cnum = (channelState[channel].rpn[1] << 7) + channelState[channel].rpn[0];
         cc_encoded = cnum | (2 << 16);
      }

      fval = (float)tv / 16384.0f;
      // int cmode = channelState[channel].nrpn_last;
   }

   for (int i = 0; i < n_customcontrollers; i++)
   {
      if (storage.controllers[i] == cc_encoded)
      {
         ((ControllerModulationSource*)storage.getPatch().scene[0].modsources[ms_ctrl1 + i])
             ->set_target01(fval);
#if !TARGET_LV2
         sendParameterAutomation(i + metaparam_offset, fval);
#else
         // LV2 must not modify its own control input; just set the value.
         setParameter01(i + metaparam_offset, fval);
#endif
      }
   }

   int n = storage.getPatch().param_ptr.size();

   if (learn_param >= 0)
   {
      if (learn_param < n_global_params)
      {
         storage.getPatch().param_ptr[learn_param]->midictrl = cc_encoded;
      }
      else
      {
         int a = learn_param;
         if (learn_param >= (n_global_params + n_scene_params))
            a -= n_scene_params;

         storage.getPatch().param_ptr[a]->midictrl = cc_encoded;
         storage.getPatch().param_ptr[a + n_scene_params]->midictrl = cc_encoded;
      }
      storage.save_midi_controllers();
      learn_param = -1;
   }

   if ((learn_custom >= 0) && (learn_custom < n_customcontrollers))
   {
      storage.controllers[learn_custom] = cc_encoded;
      storage.save_midi_controllers();
      learn_custom = -1;
   }

   // if(storage.getPatch().scene_active.val.i == 1)
   for (int i = 0; i < n_global_params; i++)
   {
      if (storage.getPatch().param_ptr[i]->midictrl == cc_encoded)
      {
         this->setParameterSmoothed(i, fval);
         int j = 0;
         while (j < 7)
         {
            if ((refresh_ctrl_queue[j] > -1) && (refresh_ctrl_queue[j] != i))
               j++;
            else
               break;
         }
         refresh_ctrl_queue[j] = i;
         refresh_ctrl_queue_value[j] = fval;
      }
   }
   int a = n_global_params + storage.getPatch().scene_active.val.i * n_scene_params;
   for (int i = a; i < (a + n_scene_params); i++)
   {
      if (storage.getPatch().param_ptr[i]->midictrl == cc_encoded)
      {
         this->setParameterSmoothed(i, fval);
         int j = 0;
         while (j < 7)
         {
            if ((refresh_ctrl_queue[j] > -1) && (refresh_ctrl_queue[j] != i))
               j++;
            else
               break;
         }
         refresh_ctrl_queue[j] = i;
         refresh_ctrl_queue_value[j] = fval;
      }
   }
}

void SurgeSynthesizer::purgeHoldbuffer(int scene)
{
   std::list<std::pair<int,int>> retainBuffer;
   for( auto hp : holdbuffer[scene] )
   {
      auto channel = hp.first;
      auto key = hp.second;

      if( channel < 0 || key < 0 )
      {
         // std::cout << "Caught tricky double releease condition!" << std::endl;
      }
      else
      {
         if (!channelState[0].hold && ! channelState[channel].hold )
         {
            releaseNotePostHoldCheck(scene, channel, key, 127);
         }
         else
         {
            retainBuffer.push_back(hp);
         }
      }
   }
   holdbuffer[scene] = retainBuffer;
}

void SurgeSynthesizer::allNotesOff()
{
   for (int i = 0; i < 16; i++)
   {
      channelState[i].hold = false;

      for (int k = 0; k < 128; k++)
      {
         channelState[i].keyState[k].keystate = 0;
         channelState[i].keyState[k].lastdetune = 0;
      }
   }

   for (int s = 0; s < 2; s++)
   {
      list<SurgeVoice*>::const_iterator iter;
      for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
      {
         //_aligned_free(*iter);
         freeVoice(*iter);
      }
      voices[s].clear();
   }
   holdbuffer[0].clear();
   holdbuffer[1].clear();
   halfbandA.reset();
   halfbandB.reset();
   halfbandIN.reset();

   hpA.suspend();
   hpB.suspend();

   for (int i = 0; i < 8; i++)
   {
      if (fx[i])
      {
         fx[i]->suspend();
      }
   }

   memset(mControlInterpolatorUsed, 0, sizeof(bool) * num_controlinterpolators);
}

void SurgeSynthesizer::setSamplerate(float sr)
{
   // If I am changing my sample rate I will change my internal tables, so this
   // needs to be tuning aware and reapply tuning if needed
   auto s = storage.currentScale;
   bool wasST = storage.isStandardTuning;

   samplerate = sr;
   dsamplerate = sr;
   samplerate_inv = 1.0 / sr;
   dsamplerate_inv = 1.0 / sr;
   dsamplerate_os = dsamplerate * OSC_OVERSAMPLING;
   dsamplerate_os_inv = 1.0 / dsamplerate_os;
   storage.init_tables();
   sinus.set_rate(1000.0 * dsamplerate_inv);

   if( ! wasST )
   {
       storage.retuneToScale(s);
   }
}

//-------------------------------------------------------------------------------------------------

int SurgeSynthesizer::GetFreeControlInterpolatorIndex()
{
   for (int i = 0; i < num_controlinterpolators; i++)
   {
      if (!mControlInterpolatorUsed[i])
      {
         return i;
      }
   }
   assert(0);
   return -1;
}

//-------------------------------------------------------------------------------------------------

int SurgeSynthesizer::GetControlInterpolatorIndex(int Id)
{
   for (int i = 0; i < num_controlinterpolators; i++)
   {
      if (mControlInterpolatorUsed[i] && mControlInterpolator[i].id == Id)
      {
         return i;
      }
   }
   return -1;
}

//-------------------------------------------------------------------------------------------------

void SurgeSynthesizer::ReleaseControlInterpolator(int Id)
{
   int Index = GetControlInterpolatorIndex(Id);
   if (Index >= 0)
   {
      assert(Index < num_controlinterpolators);
      mControlInterpolatorUsed[Index] = false;
   }
}

//-------------------------------------------------------------------------------------------------

ControllerModulationSource* SurgeSynthesizer::ControlInterpolator(int Id)
{
   int Index = GetControlInterpolatorIndex(Id);

   if (Index < 0)
      return nullptr;

   return &mControlInterpolator[Index];
}

//-------------------------------------------------------------------------------------------------

ControllerModulationSource* SurgeSynthesizer::AddControlInterpolator(int Id, bool& AlreadyExisted)
{
   int Index = GetControlInterpolatorIndex(Id);

   AlreadyExisted = false;

   if (Index >= 0)
   {
      // Already exists, return it
      AlreadyExisted = true;
      return &mControlInterpolator[Index];
   }

   Index = GetFreeControlInterpolatorIndex();

   if (Index >= 0)
   {
      // Add new
      mControlInterpolator[Index].id = Id;
      mControlInterpolatorUsed[Index] = true;
      return &mControlInterpolator[Index];
   }

   return nullptr;
}

//-------------------------------------------------------------------------------------------------

void SurgeSynthesizer::setParameterSmoothed(long index, float value)
{
   float oldval = storage.getPatch().param_ptr[index]->get_value_f01();

   bool AlreadyExisted;
   ControllerModulationSource* mc = AddControlInterpolator(index, AlreadyExisted);

   if (mc)
   {
      if (!AlreadyExisted)
      {
         float oldval = storage.getPatch().param_ptr[index]->get_value_f01();
         mc->init(oldval);
         mc->set_target(value);
      }
      else
      {
         mc->set_target(value);
      }
   }
}

//-------------------------------------------------------------------------------------------------

bool SurgeSynthesizer::setParameter01(long index, float value, bool external, bool force_integer)
{
   // does the parameter exist in the interpolator array? If it does, delete it
   ReleaseControlInterpolator(index);

   bool need_refresh = false;

   if (index >= metaparam_offset)
   {
      ((ControllerModulationSource*)storage.getPatch().scene[0].modsources[ms_ctrl1 + index -
                                                                           metaparam_offset])
          ->set_target01(value, true);
      return false;
   }

   if (index < storage.getPatch().param_ptr.size())
   {
      pdata oldval;
      oldval.i = storage.getPatch().param_ptr[index]->val.i;

      storage.getPatch().param_ptr[index]->set_value_f01(value, force_integer);
      if (storage.getPatch().param_ptr[index]->affect_other_parameters)
      {
         storage.getPatch().update_controls();
         need_refresh = true;
      }

      /*if(storage.getPatch().param_ptr[index]->ctrltype == ct_polymode)
      {
              if (storage.getPatch().param_ptr[index]->val.i == pm_latch)
              {
                      int s = storage.getPatch().param_ptr[index]->scene - 1;
                      release_if_latched[s&1] = true;
                      release_anyway[s&1] = true;
                      // release old notes if previous polymode was latch
              }
      }*/

      switch (storage.getPatch().param_ptr[index]->ctrltype)
      {
      case ct_scenemode:
         release_if_latched[0] = true;
         release_if_latched[1] = true;
         release_anyway[0] = false;
         release_anyway[1] = false;
         break;
      case ct_polymode:
         if ((oldval.i == pm_latch) && (storage.getPatch().param_ptr[index]->val.i != pm_latch))
         {
            int s = storage.getPatch().param_ptr[index]->scene - 1;
            release_if_latched[s & 1] = true;
            release_anyway[s & 1] = true;
            // release old notes if previous polymode was latch
         }
         break;
      case ct_filtertype:
         switch_toggled_queued = true;
         {
            switch (storage.getPatch().param_ptr[index]->val.i)
            {
            case fut_lpmoog:
               storage.getPatch().param_ptr[index + 1]->val.i = 3;
               break;
            case fut_comb:
            default:
               storage.getPatch().param_ptr[index + 1]->val.i = 0;
               storage.getPatch().param_ptr[index + 1]->val.i = 0;
               break;
            }
         }
         break;
      case ct_osctype:
      {
          int s = storage.getPatch().param_ptr[index]->scene - 1;

          if( storage.getPatch().param_ptr[index]->val.i != oldval.i )
          {
              /*
              ** Wish there was a better way to figure out my osc but thsi works
              */
              for( auto oi = 0; s >=0 && s <= 1 && oi < n_oscs; oi++ )
              {
                  if( storage.getPatch().scene[s].osc[oi].type.id == storage.getPatch().param_ptr[index]->id )
                  {
                      storage.getPatch().scene[s].osc[oi].queue_type = storage.getPatch().param_ptr[index]->val.i;
                  }
              }
          }
          switch_toggled_queued = true;
          need_refresh = true;
          refresh_editor = true;
          break;
      }
      case ct_wstype:
      case ct_bool_mute:
      case ct_bool_fm:
      case ct_fbconfig:
      case ct_filtersubtype:
         switch_toggled_queued = true;
         break;
      case ct_fxtype:
         switch_toggled_queued = true;
         load_fx_needed = true;
         break;
      case ct_bool_relative_switch:
      {
         int s = storage.getPatch().param_ptr[index]->scene - 1;
         bool down = storage.getPatch().param_ptr[index]->val.b;
         float polarity = down ? -1.f : 1.f;
         if (oldval.b == down)
            polarity = 0.f;
         if (s >= 0)
         {
            storage.getPatch().scene[s].filterunit[1].cutoff.val.f +=
                polarity * storage.getPatch().scene[s].filterunit[0].cutoff.val.f;
            storage.getPatch().scene[s].filterunit[1].envmod.val.f +=
                polarity * storage.getPatch().scene[s].filterunit[0].envmod.val.f;
            storage.getPatch().scene[s].filterunit[1].keytrack.val.f +=
                polarity * storage.getPatch().scene[s].filterunit[0].keytrack.val.f;
         }
         if (down)
         {
            storage.getPatch().scene[s].filterunit[1].cutoff.set_type(ct_freq_mod);
            storage.getPatch().scene[s].filterunit[1].cutoff.set_name("Offset");
         }
         else
         {
            storage.getPatch().scene[s].filterunit[1].cutoff.set_type(ct_freq_audible);
            storage.getPatch().scene[s].filterunit[1].cutoff.set_name("Cutoff");
         }
         need_refresh = true;
      }
      break;
      case ct_envmode:
         refresh_editor = true;
         need_refresh = true; // See github issue #160
         break;
      case ct_bool_link_switch:
         need_refresh = true;
         break;
      case ct_bool_solo:
         if (storage.getPatch().param_ptr[index]->val.b)
         {
            int s = storage.getPatch().param_ptr[index]->scene - 1;
            if (s >= 0)
            {
               storage.getPatch().scene[s].solo_o1.val.b = false;
               storage.getPatch().scene[s].solo_o2.val.b = false;
               storage.getPatch().scene[s].solo_o3.val.b = false;
               storage.getPatch().scene[s].solo_ring_12.val.b = false;
               storage.getPatch().scene[s].solo_ring_23.val.b = false;
               storage.getPatch().scene[s].solo_noise.val.b = false;
               storage.getPatch().param_ptr[index]->val.b = true;
            }
         }
         switch_toggled_queued = true;
         need_refresh = true; 
         break;
      };
   }
   if (external && !need_refresh)
   {
      for (int i = 0; i < 8; i++)
      {
         if (refresh_parameter_queue[i] < 0)
         {
            refresh_parameter_queue[i] = index;
            break;
         }
      }
   }
   return need_refresh;
}

void SurgeSynthesizer::switch_toggled()
{
   for (int s = 0; s < 2; s++)
   {
      list<SurgeVoice*>::iterator iter;
      for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
      {
         SurgeVoice* v = *iter;
         v->switch_toggled();
      }
   }
}

bool SurgeSynthesizer::loadFx(bool initp, bool force_reload_all)
{
   load_fx_needed = false;
   bool something_changed = false;
   for (int s = 0; s < 8; s++)
   {
      if ((fxsync[s].type.val.i != storage.getPatch().fx[s].type.val.i) || force_reload_all)
      {
         fx_reload[s] = false;

         fx[s].reset();
         /*if (!force_reload_all)*/ storage.getPatch().fx[s].type.val.i = fxsync[s].type.val.i;
         // else fxsync[s].type.val.i = storage.getPatch().fx[s].type.val.i;

         for (int j = 0; j < n_fx_params; j++)
         {
            storage.getPatch().fx[s].p[j].set_type(ct_none);
            storage.getPatch().fx[s].p[j].val.i = 0;
            storage.getPatch().globaldata[storage.getPatch().fx[s].p[j].id].i = 0;
         }

         if (/*!force_reload_all && */ storage.getPatch().fx[s].type.val.i)
            memcpy((void*)&storage.getPatch().fx[s].p, (void*)&fxsync[s].p, sizeof(Parameter) * n_fx_params);

         fx[s].reset(spawn_effect(storage.getPatch().fx[s].type.val.i, &storage,
                              &storage.getPatch().fx[s], storage.getPatch().globaldata));

         if (fx[s])
         {
            fx[s]->init_ctrltypes();
            if (initp)
               fx[s]->init_default_values();
            else
            {
               for(int j=0; j<n_fx_params; j++)
               {
                  auto p = &( storage.getPatch().fx[s].p[j] );
                  if( p->valtype == vt_float )
                  {
                     if( p->val.f < p->val_min.f )
                     {
                        p->val.f = p->val_min.f;
                     }
                     if( p->val.f > p->val_max.f )
                     {
                        p->val.f = p->val_max.f;
                     }
                  }
               }

            }
            /*for(int j=0; j<n_fx_params; j++)
            {
                storage.getPatch().globaldata[storage.getPatch().fx[s].p[j].id].f =
                storage.getPatch().fx[s].p[j].val.f;
            }*/

            fx[s]->init();

            /*
            ** Clear modulation onto FX otherwise it hangs around from old ones, often with
            ** disastrously bad meaning. #2036. But only do this if it is a one FX change
            ** (not a patch load)
            */
            if( ! force_reload_all ) 
               for(int j=0; j<n_fx_params; j++)
               {
                  auto p = &( storage.getPatch().fx[s].p[j] );
                  for( int ms=1; ms<n_modsources; ms++ )
                  {
                     clearModulation(p->id, (modsources)ms, true );
                  }
               }
            
         }
         something_changed = true;
         refresh_editor = true;
      }
      else if (fx_reload[s])
      {
         memcpy((void*)&storage.getPatch().fx[s].p, (void*)&fxsync[s].p, sizeof(Parameter) * n_fx_params);
         if (fx[s])
         {
            fx[s]->suspend();
            fx[s]->init();
         }
         fx_reload[s] = false;
         refresh_editor = true;
      }
   }

   // if (something_changed) storage.getPatch().update_controls(false);
   return true;
}

bool SurgeSynthesizer::loadOscalgos()
{
   for (int s = 0; s < 2; s++)
   {
      for (int i = 0; i < n_oscs; i++)
      {
         bool resend = false;
         if (storage.getPatch().scene[s].osc[i].queue_type > -1)
         {
            storage.getPatch().scene[s].osc[i].type.val.i =
                storage.getPatch().scene[s].osc[i].queue_type;
            storage.getPatch().update_controls(false, &storage.getPatch().scene[s].osc[i]);
            storage.getPatch().scene[s].osc[i].queue_type = -1;
            switch_toggled_queued = true;
            refresh_editor = true;
            resend = true;
         }

         TiXmlElement* e = (TiXmlElement*)storage.getPatch().scene[s].osc[i].queue_xmldata;

         if (e)
         {
            resend = true;
            for (int k = 0; k < n_osc_params; k++)
            {
               double d;
               int j;
               char lbl[256];
               sprintf(lbl, "p%i", k);
               if (storage.getPatch().scene[s].osc[i].p[k].valtype == vt_float)
               {
                  if (e->QueryDoubleAttribute(lbl, &d) == TIXML_SUCCESS)
                     storage.getPatch().scene[s].osc[i].p[k].val.f = (float)d;
               }
               else
               {
                  if (e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS)
                     storage.getPatch().scene[s].osc[i].p[k].val.i = j;
               }
            }
            storage.getPatch().scene[s].osc[i].queue_xmldata = 0;
         }
         if (resend)
         {
#if TARGET_LV2
            auto tp = &(storage.getPatch().scene[s].osc[i].type);
            sendParameterAutomation(tp->id, getParameter01(tp->id) );
            for (int k = 0; k < n_osc_params; k++)
            {
               auto pp = &(storage.getPatch().scene[s].osc[i].p[k]);
               sendParameterAutomation(pp->id, getParameter01(pp->id) );
            }
#endif            
         }
      }
   }
   return true;
}

bool SurgeSynthesizer::isValidModulation(long ptag, modsources modsource)
{
   if (!modsource)
      return false;
   if (!(ptag < storage.getPatch().param_ptr.size()))
      return false;
   Parameter* p = storage.getPatch().param_ptr[ptag];

   if (!p->modulateable)
      return false;
   if (p->valtype != ((valtypes)vt_float))
      return false;
   if (!p->per_voice_processing && !canModulateMonophonicTarget(modsource))
      return false;
   if ((modsource == ms_keytrack) && (p == &storage.getPatch().scene[0].pitch))
      return false;
   if ((modsource == ms_keytrack) && (p == &storage.getPatch().scene[1].pitch))
      return false;

   /*
     canModulateModulators is really a check at this point for "is amp or filter env" but
     amp and filter env can modulate an VLFO so with 1.7 comment this out

   if ((p->ctrlgroup == cg_LFO) && (p->ctrlgroup_entry >= ms_lfo1) && !canModulateModulators(modsource) )
      return false;
   */

   if ((p->ctrlgroup == cg_LFO) && (p->ctrlgroup_entry == modsource))
      return false;
   if ((p->ctrlgroup == cg_LFO) && (p->ctrlgroup_entry >= ms_slfo1) && (!isScenelevel(modsource)))
      return false;
   if ((p->ctrlgroup == cg_ENV) && !canModulateModulators(modsource))
      return false;
   return true;
}

ModulationRouting* SurgeSynthesizer::getModRouting(long ptag, modsources modsource)
{
   if (!isValidModulation(ptag, modsource))
      return 0;

   int scene = storage.getPatch().param_ptr[ptag]->scene;
   vector<ModulationRouting>* modlist = nullptr;

   if (!scene)
   {
      modlist = &storage.getPatch().modulation_global;
   }
   else
   {
      if (isScenelevel(modsource))
         modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
      else
         modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
   }

   int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
   if (!scene)
      id = ptag;
   int n = modlist->size();

   for (int i = 0; i < n; i++)
   {
      if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource))
      {
         return &modlist->at(i);
      }
   }

   return 0;
}

float SurgeSynthesizer::getModDepth(long ptag, modsources modsource)
{
   ModulationRouting* r = getModRouting(ptag, modsource);
   float d = 0.f;
   if (r)
      d = r->depth;
   Parameter* p = storage.getPatch().param_ptr.at(ptag);
   if (p && p->extend_range)
      d = p->get_extended(d);
   return d;
}

bool SurgeSynthesizer::isActiveModulation(long ptag, modsources modsource)
{
   // if(!isValidModulation(ptag,modsource)) return false;
   if (getModRouting(ptag, modsource))
      return true;
   return false;
}

bool SurgeSynthesizer::isBipolarModulation(modsources tms)
{
   // HERE
   int scene_ms = storage.getPatch().scene_active.val.i;
   /* You would think you could just do this nad ask for is_bipolar but remember the LFOs are made at voice time so... */
   // auto ms = storage.getPatch().scene[scene_ms].modsources.at(tms);

   // FIX - this will break in S++
   if( tms >= ms_lfo1 && tms <= ms_slfo6 )
   {
      bool isup = storage.getPatch().scene[scene_ms].lfo[tms-ms_lfo1].unipolar.val.i ||
         storage.getPatch().scene[scene_ms].lfo[tms-ms_lfo1].shape.val.i == ls_constant1;
      
      // For now
      return !isup;
   }
   if( tms >= ms_ctrl1 && tms <= ms_ctrl8 )
   {
      // Controls can also be bipolar
      auto ms = storage.getPatch().scene[scene_ms].modsources[tms];
      if( ms )
         return ms->is_bipolar();
      else
         return false;
   }
   if( tms == ms_keytrack )
   {
      return true;
   }
   else
   {
      return false;
   }
}


bool SurgeSynthesizer::isModDestUsed(long ptag)
{
   int scene_ms = storage.getPatch().scene_active.val.i;
   int scene_p = storage.getPatch().param_ptr[ptag]->scene;

   long md_id = storage.getPatch().param_ptr[ptag]->id;
   if (scene_p)
      md_id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;

   for (int j = 0; j < 3; j++)
   {
      // if((scene && (j>0))||(!scene && (j==0)))
      if ((scene_p && (j > 0)) || (!scene_p && (j == 0)))
      {
         vector<ModulationRouting>* modlist;

         switch (j)
         {
         case 0:
            modlist = &storage.getPatch().modulation_global;
            break;
         case 1:
            modlist = &storage.getPatch().scene[scene_ms].modulation_scene;
            break;
         case 2:
            modlist = &storage.getPatch().scene[scene_ms].modulation_voice;
            break;
         }

         int n = modlist->size();
         for (int i = 0; i < n; i++)
         {
            if (modlist->at(i).destination_id == md_id)
               return true;
         }
      }
   }
   return false;
}

void SurgeSynthesizer::updateUsedState()
{
   // intended for GUI only
   for (int i = 0; i < n_modsources; i++)
      modsourceused[i] = false;

   int scene = storage.getPatch().scene_active.val.i;

   for (int j = 0; j < 3; j++)
   {
      vector<ModulationRouting>* modlist;

      switch (j)
      {
      case 0:
         modlist = &storage.getPatch().modulation_global;
         break;
      case 1:
         modlist = &storage.getPatch().scene[scene].modulation_scene;
         break;
      case 2:
         modlist = &storage.getPatch().scene[scene].modulation_voice;
         break;
      }

      int n = modlist->size();
      for (int i = 0; i < n; i++)
      {
         int id = modlist->at(i).source_id;
         assert((id > 0) && (id < n_modsources));
         modsourceused[id] = true;
      }
   }
}

void SurgeSynthesizer::prepareModsourceDoProcess(int scenemask)
{
   for (int scene = 0; scene < 2; scene++)
   {
      if ((1 << scene) & scenemask)
      {
         for (int i = 0; i < n_modsources; i++)
         {
            storage.getPatch().scene[scene].modsource_doprocess[i] = false;
         }

         for (int j = 0; j < 3; j++)
         {
            vector<ModulationRouting>* modlist;

            switch (j)
            {
            case 0:
               modlist = &storage.getPatch().modulation_global;
               break;
            case 1:
               modlist = &storage.getPatch().scene[scene].modulation_scene;
               break;
            case 2:
               modlist = &storage.getPatch().scene[scene].modulation_voice;
               break;
            }

            int n = modlist->size();
            for (int i = 0; i < n; i++)
            {
               int id = modlist->at(i).source_id;
               assert((id > 0) && (id < n_modsources));
               storage.getPatch().scene[scene].modsource_doprocess[id] = true;
            }
         }
      }
   }
}

bool SurgeSynthesizer::isModsourceUsed(modsources modsource)
{
   updateUsedState();
   return modsourceused[modsource];
}

float SurgeSynthesizer::getModulation(long ptag, modsources modsource)
{
   if (!isValidModulation(ptag, modsource))
      return 0.0f;

   ModulationRouting* r = getModRouting(ptag, modsource);
   if (r)
      return storage.getPatch().param_ptr[ptag]->get_modulation_f01(r->depth);

   return storage.getPatch().param_ptr[ptag]->get_modulation_f01(0);
}

void SurgeSynthesizer::clear_osc_modulation(int scene, int entry)
{
   storage.CS_ModRouting.enter();
   vector<ModulationRouting>::iterator iter;

   int pid = storage.getPatch().scene[scene].osc[entry].p[0].param_id_in_scene;
   iter = storage.getPatch().scene[scene].modulation_scene.begin();
   while (iter != storage.getPatch().scene[scene].modulation_scene.end())
   {
      if ((iter->destination_id >= pid) && (iter->destination_id < (pid + n_osc_params)))
      {
         iter = storage.getPatch().scene[scene].modulation_scene.erase(iter);
      }
      else
         iter++;
   }
   iter = storage.getPatch().scene[scene].modulation_voice.begin();
   while (iter != storage.getPatch().scene[scene].modulation_voice.end())
   {
      if ((iter->destination_id >= pid) && (iter->destination_id < (pid + n_osc_params)))
      {
         iter = storage.getPatch().scene[scene].modulation_voice.erase(iter);
      }
      else
         iter++;
   }
   storage.CS_ModRouting.leave();
}

void SurgeSynthesizer::clearModulation(long ptag, modsources modsource, bool clearEvenIfInvalid)
{
   if (!isValidModulation(ptag, modsource) && ! clearEvenIfInvalid )
      return;
   
   int scene = storage.getPatch().param_ptr[ptag]->scene;

   vector<ModulationRouting>* modlist;

   if (!scene)
   {
      modlist = &storage.getPatch().modulation_global;
   }
   else
   {
      if (isScenelevel(modsource))
         modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
      else
         modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
   }

   int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
   if (!scene)
      id = ptag;
   int n = modlist->size();

   for (int i = 0; i < n; i++)
   {
      if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource))
      {
         storage.CS_ModRouting.enter();
         modlist->erase(modlist->begin() + i);
         storage.CS_ModRouting.leave();
         return;
      }
   }
}

bool SurgeSynthesizer::setModulation(long ptag, modsources modsource, float val)
{
   if (!isValidModulation(ptag, modsource))
      return false;
   float value = storage.getPatch().param_ptr[ptag]->set_modulation_f01(val);
   int scene = storage.getPatch().param_ptr[ptag]->scene;

   vector<ModulationRouting>* modlist;

   if (!scene)
   {
      modlist = &storage.getPatch().modulation_global;
   }
   else
   {
      if (isScenelevel(modsource))
         modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
      else
         modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
   }

   storage.CS_ModRouting.enter();

   int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
   if (!scene)
      id = ptag;
   int n = modlist->size();
   int found_id = -1;
   for (int i = 0; i < n; i++)
   {
      if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource))
      {
         found_id = i;
         break;
      }
   }

   if (value == 0)
   {
      if (found_id >= 0)
      {
         modlist->erase(modlist->begin() + found_id);
      }
   }
   else
   {
      if (found_id < 0)
      {
         ModulationRouting t;
         t.depth = value;
         t.source_id = modsource;
         t.destination_id = id;
         modlist->push_back(t);
      }
      else
      {
         modlist->at(found_id).depth = value;
      }
   }
   storage.CS_ModRouting.leave();

   return true;
}

int SurgeSynthesizer::remapExternalApiToInternalId(unsigned int x)
{
   if (x < n_customcontrollers)
      return metaparam_offset + x;
   else if (x >= n_total_params)
      return x - n_total_params;
   return x;
}

int SurgeSynthesizer::remapInternalToExternalApiId(unsigned int x)
{
   if (x >= metaparam_offset)
      return (x - metaparam_offset);
   else if (x < n_customcontrollers)
      return x + n_total_params;
   return x;
}

float SurgeSynthesizer::getParameter01(long index)
{
   if (index < 0)
      return 0.f;
   if (index >= metaparam_offset)
      return storage.getPatch()
          .scene[0]
          .modsources[ms_ctrl1 + index - metaparam_offset]
          ->get_output01();
   if (index < storage.getPatch().param_ptr.size())
      return storage.getPatch().param_ptr[index]->get_value_f01();
   return 0.f;
}

void SurgeSynthesizer::getParameterDisplay(long index, char* text)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      storage.getPatch().param_ptr[index]->get_display(text);
   }
   else if (index >= metaparam_offset)
   {
      sprintf(text, "%.2f %%",
              100.f * storage.getPatch()
                          .scene[0]
                          .modsources[ms_ctrl1 + index - metaparam_offset]
                          ->get_output());
   }
   else
      sprintf(text, "-");
}

void SurgeSynthesizer::getParameterDisplayAlt(long index, char* text)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      storage.getPatch().param_ptr[index]->get_display_alt(text);
   }
   else 
   {
      text[0] = 0;
   }
}

void SurgeSynthesizer::getParameterDisplay(long index, char* text, float x)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      storage.getPatch().param_ptr[index]->get_display(text, true, x);
   }
   else if (index >= metaparam_offset)
   {
      sprintf(text, "%.2f %%",
              100.f * storage.getPatch()
                          .scene[0]
                          .modsources[ms_ctrl1 + index - metaparam_offset]
                          ->get_output());
   }
   else
      sprintf(text, "-");
}

void SurgeSynthesizer::getParameterName(long index, char* text)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      int scn = storage.getPatch().param_ptr[index]->scene;
      string sn[3] = {"", "A ", "B "};

      sprintf(text, "%s%s", sn[scn].c_str(), storage.getPatch().param_ptr[index]->get_full_name());
   }
   else if (index >= metaparam_offset)
   {
      int c = index - metaparam_offset;
      sprintf(text, "Macro %i: %s", c + 1, storage.getPatch().CustomControllerLabel[c]);
   }
   else
      sprintf(text, "-");
}

void SurgeSynthesizer::getParameterNameW(long index, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      int scn = storage.getPatch().param_ptr[index]->scene;
      char sn[3][3] = {"", "A ", "B "};
      char pname[256];
      
      snprintf(pname, 255, "%s%s", sn[scn], storage.getPatch().param_ptr[index]->get_full_name());

      // the input is not wide so don't use %S
      swprintf(ptr, 128, L"%s", pname);
   }
   else if (index >= metaparam_offset)
   {
      int c = index - metaparam_offset;
      // For a reason I don't understand, on windows, we need to sprintf then swprinf just the short char
      // to make just these names work. :shrug:
      char wideHack[256];
       
      if (c >= num_metaparameters)
      {
         snprintf(wideHack, 255, "Macro: ERROR");
      }
      else
      {
         snprintf(wideHack, 255, "Macro %d: %s", c+1, storage.getPatch().CustomControllerLabel[c]);
      }
      swprintf(ptr, 128, L"%s", wideHack);
   }
   else
   {
      swprintf(ptr, 128, L"-");
   }
}

void SurgeSynthesizer::getParameterShortNameW(long index, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      int scn = storage.getPatch().param_ptr[index]->scene;
      string sn[3] = {"", "A ", "B "};

      swprintf(ptr, 128, L"%s%s", sn[scn].c_str(), storage.getPatch().param_ptr[index]->get_name());
   }
   else if (index >= metaparam_offset)
   {
       getParameterNameW( index, ptr );
   }
   else
   {
      swprintf(ptr, 128, L"-");
   }
}

void SurgeSynthesizer::getParameterUnitW(long index, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      swprintf(ptr, 128, L"%s", storage.getPatch().param_ptr[index]->getUnit());
   }
   else
   {
      swprintf(ptr, 128, L"");
   }
}

void SurgeSynthesizer::getParameterStringW(long index, float value, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      char text[128];
      storage.getPatch().param_ptr[index]->get_display(text);

      swprintf(ptr, 128, L"%s", text);
   }
   else if (index >= metaparam_offset)
   {
      // For a reason I don't understand, on windows, we need to sprintf then swprinf just the short char
      // to make just these names work. :shrug:
      char wideHack[256];
      snprintf(wideHack, 256, "%.2f %%", 100.f * value ); 
      swprintf(ptr, 128, L"%s", wideHack);
   }
   else
   {
      swprintf(ptr, 128, L"-");
   }
}

void SurgeSynthesizer::getParameterMeta(long index, parametermeta& pm)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      pm.flags = storage.getPatch().param_ptr[index]->ctrlstyle;
      pm.fmin = 0.f; // storage.getPatch().param_ptr[index]->val_min.f;
      pm.fmax = 1.f; // storage.getPatch().param_ptr[index]->val_max.f;
      pm.fdefault =
          storage.getPatch()
              .param_ptr[index]
              ->get_default_value_f01(); // storage.getPatch().param_ptr[index]->val_default.f;
      pm.hide = (pm.flags & kHide) != 0;
      pm.meta = (pm.flags & kMeta) != 0;
      pm.expert = !(pm.flags & kEasy);
      pm.clump = 2;
      if (storage.getPatch().param_ptr[index]->scene)
      {
         pm.clump += storage.getPatch().param_ptr[index]->ctrlgroup +
                     (storage.getPatch().param_ptr[index]->scene - 1) * 6;
         if (storage.getPatch().param_ptr[index]->ctrlgroup == 0)
            pm.clump++;
      }
   }
   else if (index >= metaparam_offset)
   {
      int c = index - metaparam_offset;

      pm.flags = 0;
      pm.fmin = 0.f;
      pm.fmax = 1.f;
      pm.fdefault = 0.5f;
      pm.hide = false;
      pm.meta = false; // ironic as they are metaparameters, but they don't affect any other sliders
      pm.expert = false;
      pm.clump = 1;
   }
}
/*unsigned int sub3_synth::getParameterFlags (long index)
{
        if (index<storage.getPatch().param_ptr.size())
        {
                return storage.getPatch().param_ptr[index]->ctrlstyle;
        }
        return 0;
}*/

float SurgeSynthesizer::getParameter(long index)
{
   if (index < 0)
      return 0.f;
   if (index >= metaparam_offset)
      return storage.getPatch()
          .scene[0]
          .modsources[ms_ctrl1 + index - metaparam_offset]
          ->get_output();
   if (index < storage.getPatch().param_ptr.size())
      return storage.getPatch().param_ptr[index]->get_value_f01();
   return 0.f;
}

float SurgeSynthesizer::normalizedToValue(long index, float value)
{
   if (index < 0)
      return 0.f;
   if (index >= metaparam_offset)
      return value;
   if (index < storage.getPatch().param_ptr.size())
      return storage.getPatch().param_ptr[index]->normalized_to_value(value);
   return 0.f;
}

float SurgeSynthesizer::valueToNormalized(long index, float value)
{
   if (index < 0)
      return 0.f;
   if (index >= metaparam_offset)
      return value;
   if (index < storage.getPatch().param_ptr.size())
      return storage.getPatch().param_ptr[index]->value_to_normalized(value);
   return 0.f;
}

#if MAC || LINUX
void* loadPatchInBackgroundThread(void* sy)
{
#else
DWORD WINAPI loadPatchInBackgroundThread(LPVOID lpParam)
{
   void* sy = lpParam;
#endif
   SurgeSynthesizer* synth = (SurgeSynthesizer*)sy;
   int patchid = synth->patchid_queue;
   synth->patchid_queue = -1;
   synth->allNotesOff();
   synth->loadPatch(patchid);
#if TARGET_LV2
   synth->getParent()->patchChanged();
#endif
   synth->halt_engine = false;
   return 0;
}

void SurgeSynthesizer::processThreadunsafeOperations()
{
   if (!audio_processing_active)
   {
      // if the audio processing is inactive, patchloading should occur anyway
      if (patchid_queue >= 0)
      {
         loadPatch(patchid_queue);
#if TARGET_LV2
         getParent()->patchChanged();
#endif
         patchid_queue = -1;
      }

      if (load_fx_needed)
         loadFx(false, false);

      loadOscalgos();
   }
}

void SurgeSynthesizer::processControl()
{
   storage.perform_queued_wtloads();
   int sm = storage.getPatch().scenemode.val.i;
   bool playA = (sm == sm_split) || (sm == sm_dual) || (sm == sm_chsplit) || (storage.getPatch().scene_active.val.i == 0);
   bool playB = (sm == sm_split) || (sm == sm_dual) || (sm == sm_chsplit) || (storage.getPatch().scene_active.val.i == 1);
   storage.songpos = time_data.ppqPos;
   storage.temposyncratio = time_data.tempo / 120.f;
   storage.temposyncratio_inv = 1.f / storage.temposyncratio;

   if (release_if_latched[0])
   {
      if (!playA || release_anyway[0])
         releaseScene(0);
      release_if_latched[0] = false;
      release_anyway[0] = false;
   }
   if (release_if_latched[1])
   {
      if (!playB || release_anyway[1])
         releaseScene(1);
      release_if_latched[1] = false;
      release_anyway[1] = false;
   }

   /*if(playA && (storage.getPatch().scene[0].polymode.val.i == pm_latch))
   {
           if(voices[0].empty())
           {
                   play_note(0,60,100,0);
                   release_note(0,60,100);
           }
   }
   else if(!hold[0]) purge_holdbuffer();*/

   if (playA && (storage.getPatch().scene[0].polymode.val.i == pm_latch) && voices[0].empty())
      playNote(1, 60, 100, 0);
   if (playB && (storage.getPatch().scene[1].polymode.val.i == pm_latch) && voices[1].empty())
      playNote(2, 60, 100, 0);

   // interpolate midi controllers
   for (int i = 0; i < num_controlinterpolators; i++)
   {
      if (mControlInterpolatorUsed[i])
      {
         ControllerModulationSource* mc = &mControlInterpolator[i];
         bool cont = mc->process_block_until_close(0.001f);
         int id = mc->id;
         storage.getPatch().param_ptr[id]->set_value_f01(mc->output);
         if (!cont)
         {
            mControlInterpolatorUsed[i] = false;
         }
      }
   }

   storage.getPatch().copy_globaldata(
       storage.getPatch().globaldata); // Drains a great deal of CPU while in Debug mode.. optimize?
   if (playA)
      storage.getPatch().copy_scenedata(storage.getPatch().scenedata[0], 0); // -""-
   if (playB)
      storage.getPatch().copy_scenedata(storage.getPatch().scenedata[1], 1);

   //	if(sm == sm_morph) storage.getPatch().do_morph();

   prepareModsourceDoProcess((playA ? 1 : 0) | (playB ? 2 : 0));

   for (int s = 0; s < 2; s++)
   {
      if (((s == 0) && playA) || ((s == 1) && playB))
      {
         if (storage.getPatch().scene[s].modsource_doprocess[ms_modwheel])
            storage.getPatch().scene[s].modsources[ms_modwheel]->process_block();
         if (storage.getPatch().scene[s].modsource_doprocess[ms_aftertouch])
            storage.getPatch().scene[s].modsources[ms_aftertouch]->process_block();
         storage.getPatch().scene[s].modsources[ms_pitchbend]->process_block();
         for (int i = 0; i < n_customcontrollers; i++)
         {
            storage.getPatch().scene[s].modsources[ms_ctrl1 + i]->process_block();
         }
         // for(int i=0; i<n_lfos_scene; i++)
         // storage.getPatch().scene[s].modsources[ms_slfo1+i]->process_block();

         int n = storage.getPatch().scene[s].modulation_scene.size();
         for (int i = 0; i < n; i++)
         {
            int src_id = storage.getPatch().scene[s].modulation_scene[i].source_id;
            if (storage.getPatch().scene[s].modsources[src_id])
            {
               int dst_id = storage.getPatch().scene[s].modulation_scene[i].destination_id;
               float depth = storage.getPatch().scene[s].modulation_scene[i].depth;
               storage.getPatch().scenedata[s][dst_id].f +=
                   depth * storage.getPatch().scene[s].modsources[src_id]->output;
            }
         }

         for (int i = 0; i < n_lfos_scene; i++)
            storage.getPatch().scene[s].modsources[ms_slfo1 + i]->process_block();
      }
   }

   loadOscalgos();

   int n = storage.getPatch().modulation_global.size();
   for (int i = 0; i < n; i++)
   {
      int src_id = storage.getPatch().modulation_global[i].source_id;
      int dst_id = storage.getPatch().modulation_global[i].destination_id;
      float depth = storage.getPatch().modulation_global[i].depth;
      storage.getPatch().globaldata[dst_id].f +=
          depth * storage.getPatch().scene[0].modsources[src_id]->output;
   }

   if (switch_toggled_queued)
   {
      switch_toggled();
      switch_toggled_queued = false;
   }
   if (load_fx_needed)
      loadFx(false, false);

   if (fx_suspend_bitmask)
   {
      for (int i = 0; i < 8; i++)
      {
         if (((1 << i) & fx_suspend_bitmask) && fx[i])
            fx[i]->suspend();
      }
      fx_suspend_bitmask = 0;
   }
}

void SurgeSynthesizer::process()
{
   float mfade = 1.f;

   if (halt_engine)
   {
      /*for(int k=0; k<BLOCK_SIZE; k++)
      {
              output[0][k] = 0;
              output[1][k] = 0;
      }*/
      clear_block(output[0], BLOCK_SIZE_QUAD);
      clear_block(output[1], BLOCK_SIZE_QUAD);
      return;
   }
   else if (patchid_queue >= 0)
   {
      masterfade = max(0.f, masterfade - 0.025f);
      mfade = masterfade * masterfade;
      if (masterfade < 0.0001f)
      {
         // spawn patch-loading thread
         halt_engine = true;

#if MAC || LINUX
         pthread_t thread;
         pthread_attr_t attributes;
         int ret;
         sched_param params;

         /* initialized with default attributes */
         ret = pthread_attr_init(&attributes);

         /* safe to get existing scheduling param */
         ret = pthread_attr_getschedparam(&attributes, &params);

         /* set the priority; others are unchanged */
         params.sched_priority = sched_get_priority_min(SCHED_OTHER);

         /* setting the new scheduling param */
         ret = pthread_attr_setschedparam(&attributes, &params);

         ret = pthread_create(&thread, &attributes, loadPatchInBackgroundThread, this);
#else

         DWORD dwThreadId;
         HANDLE hThread = CreateThread(NULL,                        // default security attributes
                                       0,                           // use default stack size
                                       loadPatchInBackgroundThread, // thread function
                                       this,                        // argument to thread function
                                       0,                           // use default creation flags
                                       &dwThreadId);                // returns the thread identifier
         SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
#endif

         clear_block(output[0], BLOCK_SIZE_QUAD);
         clear_block(output[1], BLOCK_SIZE_QUAD);
         return;
      }
   }

   // process inputs (upsample & halfrate)
   if (process_input)
   {
      hardclip_block8(input[0], BLOCK_SIZE_QUAD);
      hardclip_block8(input[1], BLOCK_SIZE_QUAD);
      copy_block(input[0], storage.audio_in_nonOS[0], BLOCK_SIZE_QUAD);
      copy_block(input[1], storage.audio_in_nonOS[1], BLOCK_SIZE_QUAD);
      halfbandIN.process_block_U2(input[0], input[1], storage.audio_in[0], storage.audio_in[1]);
   }
   else
   {
      clear_block_antidenormalnoise(storage.audio_in[0], BLOCK_SIZE_OS_QUAD);
      clear_block_antidenormalnoise(storage.audio_in[1], BLOCK_SIZE_OS_QUAD);
      clear_block_antidenormalnoise(storage.audio_in_nonOS[1], BLOCK_SIZE_QUAD);
      clear_block_antidenormalnoise(storage.audio_in_nonOS[1], BLOCK_SIZE_QUAD);
   }

   float fxsendout alignas(16)[2][2][BLOCK_SIZE];
   bool play_scene[2];

   {
      clear_block_antidenormalnoise(sceneout[0][0], BLOCK_SIZE_OS_QUAD);
      clear_block_antidenormalnoise(sceneout[0][1], BLOCK_SIZE_OS_QUAD);
      clear_block_antidenormalnoise(sceneout[1][0], BLOCK_SIZE_OS_QUAD);
      clear_block_antidenormalnoise(sceneout[1][1], BLOCK_SIZE_OS_QUAD);

      clear_block_antidenormalnoise(fxsendout[0][0], BLOCK_SIZE_QUAD);
      clear_block_antidenormalnoise(fxsendout[0][1], BLOCK_SIZE_QUAD);
      clear_block_antidenormalnoise(fxsendout[1][0], BLOCK_SIZE_QUAD);
      clear_block_antidenormalnoise(fxsendout[1][1], BLOCK_SIZE_QUAD);
   }

   // CS ENTER
   storage.CS_ModRouting.enter();
   processControl();

   amp.set_target_smoothed(db_to_linear(storage.getPatch().volume.val.f));
   amp_mute.set_target(mfade);

   int fx_bypass = storage.getPatch().fx_bypass.val.i;

   if (fx_bypass == fxb_all_fx)
   {
      if (fx[4])
      {
         FX1.set_target_smoothed(amp_to_linear(
             storage.getPatch().globaldata[storage.getPatch().fx[4].return_level.id].f));
         send[0][0].set_target_smoothed(amp_to_linear(
             storage.getPatch()
                 .scenedata[0][storage.getPatch().scene[0].send_level[0].param_id_in_scene]
                 .f));
         send[0][1].set_target_smoothed(amp_to_linear(
             storage.getPatch()
                 .scenedata[1][storage.getPatch().scene[1].send_level[0].param_id_in_scene]
                 .f));
      }
      if (fx[5])
      {
         FX2.set_target_smoothed(amp_to_linear(
             storage.getPatch().globaldata[storage.getPatch().fx[5].return_level.id].f));
         send[1][0].set_target_smoothed(amp_to_linear(
             storage.getPatch()
                 .scenedata[0][storage.getPatch().scene[0].send_level[1].param_id_in_scene]
                 .f));
         send[1][1].set_target_smoothed(amp_to_linear(
             storage.getPatch()
                 .scenedata[1][storage.getPatch().scene[1].send_level[1].param_id_in_scene]
                 .f));
      }
   }

   list<SurgeVoice*>::iterator iter;
   play_scene[0] = (!voices[0].empty());
   play_scene[1] = (!voices[1].empty());

   int FBentry[2];
   int vcount = 0;
   for (int s = 0; s < 2; s++)
   {
      FBentry[s] = 0;
      iter = voices[s].begin();
      while (iter != voices[s].end())
      {
         SurgeVoice* v = *iter;
         assert(v);
         bool resume = v->process_block(FBQ[s][FBentry[s] >> 2], FBentry[s] & 3);
         FBentry[s]++;

         vcount ++;

         if (!resume)
         {
            //_aligned_free(v);
            freeVoice(v);
            iter = voices[s].erase(iter);
         }
         else
            iter++;
      }

      storage.CS_ModRouting.leave();

      fbq_global g;
      g.FU1ptr = GetQFPtrFilterUnit(storage.getPatch().scene[s].filterunit[0].type.val.i,
                                    storage.getPatch().scene[s].filterunit[0].subtype.val.i);
      g.FU2ptr = GetQFPtrFilterUnit(storage.getPatch().scene[s].filterunit[1].type.val.i,
                                    storage.getPatch().scene[s].filterunit[1].subtype.val.i);
      g.WSptr = GetQFPtrWaveshaper(storage.getPatch().scene[s].wsunit.type.val.i);

      FBQFPtr ProcessQuadFB =
          GetFBQPointer(storage.getPatch().scene[s].filterblock_configuration.val.i, g.FU1ptr != 0,
                        g.WSptr != 0, g.FU2ptr != 0);

      for (int e = 0; e < FBentry[s]; e += 4)
      {
         int units = FBentry[s] - e;
         for (int i = units; i < 4; i++)
         {
            FBQ[s][e >> 2].FU[0].active[i] = 0;
            FBQ[s][e >> 2].FU[1].active[i] = 0;
            FBQ[s][e >> 2].FU[2].active[i] = 0;
            FBQ[s][e >> 2].FU[3].active[i] = 0;
         }
         ProcessQuadFB(FBQ[s][e >> 2], g, sceneout[s][0], sceneout[s][1]);
      }

      if( s == 0 && storage.otherscene_clients > 0 )
      {
         // Make available for scene b 
         copy_block(sceneout[0][0], storage.audio_otherscene[0], BLOCK_SIZE_OS_QUAD);
         copy_block(sceneout[0][1], storage.audio_otherscene[1], BLOCK_SIZE_OS_QUAD);
      }

      
      iter = voices[s].begin();
      while (iter != voices[s].end())
      {
         SurgeVoice* v = *iter;
         assert(v);
         v->GetQFB(); // save filter state in voices after quad processing is done
         iter++;
      }
      storage.CS_ModRouting.enter();
   }

   // CS LEAVE
   storage.CS_ModRouting.leave();
   polydisplay = vcount;

   if (play_scene[0])
   {
      hardclip_block8(sceneout[0][0], BLOCK_SIZE_OS_QUAD);
      hardclip_block8(sceneout[0][1], BLOCK_SIZE_OS_QUAD);
      halfbandA.process_block_D2(sceneout[0][0], sceneout[0][1]);
   }

   if (play_scene[1])
   {
      hardclip_block8(sceneout[1][0], BLOCK_SIZE_OS_QUAD);
      hardclip_block8(sceneout[1][1], BLOCK_SIZE_OS_QUAD);
      halfbandB.process_block_D2(sceneout[1][0], sceneout[1][1]);
   }

   hpA.coeff_HP(
       hpA.calc_omega(
           storage.getPatch().scenedata[0][storage.getPatch().scene[0].lowcut.param_id_in_scene].f /
           12.0),
       0.4); // var 0.707
   hpB.coeff_HP(
       hpB.calc_omega(
           storage.getPatch().scenedata[1][storage.getPatch().scene[1].lowcut.param_id_in_scene].f /
           12.0),
       0.4);
   hpA.process_block(sceneout[0][0], sceneout[0][1]); // TODO: quadify
   hpB.process_block(sceneout[1][0], sceneout[1][1]);

   bool sc_a = play_scene[0];
   bool sc_b = play_scene[1];

   // apply insert effects
   if (fx_bypass != fxb_no_fx)
   {
      if (fx[0] && !(storage.getPatch().fx_disable.val.i & (1 << 0)))
         sc_a = fx[0]->process_ringout(sceneout[0][0], sceneout[0][1], sc_a);
      if (fx[1] && !(storage.getPatch().fx_disable.val.i & (1 << 1)))
         sc_a = fx[1]->process_ringout(sceneout[0][0], sceneout[0][1], sc_a);
      if (fx[2] && !(storage.getPatch().fx_disable.val.i & (1 << 2)))
         sc_b = fx[2]->process_ringout(sceneout[1][0], sceneout[1][1], sc_b);
      if (fx[3] && !(storage.getPatch().fx_disable.val.i & (1 << 3)))
         sc_b = fx[3]->process_ringout(sceneout[1][0], sceneout[1][1], sc_b);
   }

   // sum scenes
   copy_block(sceneout[0][0], output[0], BLOCK_SIZE_QUAD);
   copy_block(sceneout[0][1], output[1], BLOCK_SIZE_QUAD);
   accumulate_block(sceneout[1][0], output[0], BLOCK_SIZE_QUAD);
   accumulate_block(sceneout[1][1], output[1], BLOCK_SIZE_QUAD);

   bool send1 = false, send2 = false;
   // add send effects
   if (fx_bypass == fxb_all_fx)
   {
      if (fx[4] && !(storage.getPatch().fx_disable.val.i & (1 << 4)))
      {
         send[0][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[0][0],
                                    fxsendout[0][1], BLOCK_SIZE_QUAD);
         send[0][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[0][0],
                                    fxsendout[0][1], BLOCK_SIZE_QUAD);
         send1 = fx[4]->process_ringout(fxsendout[0][0], fxsendout[0][1], sc_a || sc_b);
         FX1.MAC_2_blocks_to(fxsendout[0][0], fxsendout[0][1], output[0], output[1],
                             BLOCK_SIZE_QUAD);
      }
      if (fx[5] && !(storage.getPatch().fx_disable.val.i & (1 << 5)))
      {
         send[1][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[1][0],
                                    fxsendout[1][1], BLOCK_SIZE_QUAD);
         send[1][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[1][0],
                                    fxsendout[1][1], BLOCK_SIZE_QUAD);
         send2 = fx[5]->process_ringout(fxsendout[1][0], fxsendout[1][1], sc_a || sc_b);
         FX2.MAC_2_blocks_to(fxsendout[1][0], fxsendout[1][1], output[0], output[1],
                             BLOCK_SIZE_QUAD);
      }
   }

   // apply global effects
   if ((fx_bypass == fxb_all_fx) || (fx_bypass == fxb_no_sends))
   {
      bool glob = sc_a || sc_b || send1 || send2;
      if (fx[6] && !(storage.getPatch().fx_disable.val.i & (1 << 6)))
         glob = fx[6]->process_ringout(output[0], output[1], glob);
      if (fx[7] && !(storage.getPatch().fx_disable.val.i & (1 << 7)))
         glob = fx[7]->process_ringout(output[0], output[1], glob);
   }

   amp.multiply_2_blocks(output[0], output[1], BLOCK_SIZE_QUAD);
   amp_mute.multiply_2_blocks(output[0], output[1], BLOCK_SIZE_QUAD);

   // VU
   // falloff
   float a = storage.vu_falloff;
   vu_peak[0] = min(2.f, a * vu_peak[0]);
   vu_peak[1] = min(2.f, a * vu_peak[1]);
   /*for(int i=0; i<BLOCK_SIZE; i++)
   {
           vu_peak[0] = max(vu_peak[0], output[0][i]);
           vu_peak[1] = max(vu_peak[1], output[1][i]);
   }*/
   vu_peak[0] = max(vu_peak[0], get_absmax(output[0], BLOCK_SIZE_QUAD));
   vu_peak[1] = max(vu_peak[1], get_absmax(output[1], BLOCK_SIZE_QUAD));

   hardclip_block8(output[0], BLOCK_SIZE_QUAD);
   hardclip_block8(output[1], BLOCK_SIZE_QUAD);

   // since the sceneout is now routable we also need to mute and clip it
   for( int s=0; s<2; ++s )
   {
      amp.multiply_2_blocks(sceneout[s][0], sceneout[s][1], BLOCK_SIZE_QUAD);
      amp_mute.multiply_2_blocks(sceneout[s][0], sceneout[s][1], BLOCK_SIZE_QUAD);
      hardclip_block8(sceneout[s][0], BLOCK_SIZE_QUAD);
      hardclip_block8(sceneout[s][1], BLOCK_SIZE_QUAD);
   }
}

PluginLayer* SurgeSynthesizer::getParent()
{
   assert(_parent != nullptr);
   return _parent;
}

void SurgeSynthesizer::populateDawExtraState() {
   storage.getPatch().dawExtraState.isPopulated = true;
   storage.getPatch().dawExtraState.mpeEnabled = mpeEnabled;
   storage.getPatch().dawExtraState.mpePitchBendRange = mpePitchBendRange;
   
   storage.getPatch().dawExtraState.hasTuning = !storage.isStandardTuning;
   if( ! storage.isStandardTuning )
      storage.getPatch().dawExtraState.tuningContents = storage.currentScale.rawText;
   else
      storage.getPatch().dawExtraState.tuningContents = "";
   
   storage.getPatch().dawExtraState.hasMapping = !storage.isStandardMapping;
   if( ! storage.isStandardMapping )
      storage.getPatch().dawExtraState.mappingContents = storage.currentMapping.rawText;
   else
      storage.getPatch().dawExtraState.mappingContents = "";

   int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                             // B will be duplicated on load)
   for (int i = 0; i < n; i++)
   {
      if (storage.getPatch().param_ptr[i]->midictrl >= 0)
      {
         storage.getPatch().dawExtraState.midictrl_map[i] = storage.getPatch().param_ptr[i]->midictrl;
      }
   }

   for (int i=0; i<n_customcontrollers; ++i )
   {
      storage.getPatch().dawExtraState.customcontrol_map[i] = storage.controllers[i];
   }

}

void SurgeSynthesizer::loadFromDawExtraState() {
   if( ! storage.getPatch().dawExtraState.isPopulated )
      return;
   mpeEnabled = storage.getPatch().dawExtraState.mpeEnabled;
   if( storage.getPatch().dawExtraState.mpePitchBendRange > 0 )
      mpePitchBendRange = storage.getPatch().dawExtraState.mpePitchBendRange;
   
   if( storage.getPatch().dawExtraState.hasTuning )
   {
      try {
         auto sc = Tunings::parseSCLData(storage.getPatch().dawExtraState.tuningContents );
         storage.retuneToScale(sc);
      }
      catch( Tunings::TuningError &e )
      {
         Surge::UserInteractions::promptError( e.what(), "Unable to restore tuning" );
         storage.retuneToStandardTuning();
      }
   }
   else
   {
      storage.retuneToStandardTuning();
   }
   
   if( storage.getPatch().dawExtraState.hasMapping )
   {
      try
      {
         auto kb = Tunings::parseKBMData(storage.getPatch().dawExtraState.mappingContents );
         storage.remapToKeyboard(kb);
      }
      catch( Tunings::TuningError &e )
      {
         Surge::UserInteractions::promptError( e.what(), "Unable to restore mapping" );
         storage.retuneToStandardTuning();
      }
      
   }
   else
   {
      storage.remapToStandardKeyboard();
   }

   int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                             // B will be duplicated on load)
   for (int i = 0; i < n; i++)
   {
      if (storage.getPatch().dawExtraState.midictrl_map.find(i) != storage.getPatch().dawExtraState.midictrl_map.end() )
      {
         storage.getPatch().param_ptr[i]->midictrl =  storage.getPatch().dawExtraState.midictrl_map[i];
         if( i >= n_global_params )
         {
            storage.getPatch().param_ptr[i + n_scene_params]->midictrl =  storage.getPatch().dawExtraState.midictrl_map[i];
         }
      }
   }

   for (int i=0; i<n_customcontrollers; ++i )
   {
      if( storage.getPatch().dawExtraState.customcontrol_map.find(i) != storage.getPatch().dawExtraState.midictrl_map.end() )
         storage.controllers[i] = storage.getPatch().dawExtraState.customcontrol_map[i];
   }

}
