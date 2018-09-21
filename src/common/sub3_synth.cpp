//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "sub3_synth.h"
#include "dsputils.h"
#include <time.h>
#if MAC
#include <pthread.h>
#else
#include <windows.h>
#include <process.h>
#endif
#if TARGET_AUDIOUNIT
#include "aulayer.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
#elif TARGET_VST3
#include "surgeprocessor.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
#else
#include "vstlayer.h"
#include "vstgui/plugin-bindings/aeffguieditor.h"
#endif
#include "gui_slider.h"

sub3_synth::sub3_synth(PluginLayer* parent)
    //: halfband_AL(false),halfband_AR(false),halfband_BL(false),halfband_BR(false),
    : hpA(&storage), hpB(&storage), _parent(parent)
{
   halfbandA = (halfrate_stereo*)_aligned_malloc(sizeof(halfrate_stereo), 16);
   halfbandB = (halfrate_stereo*)_aligned_malloc(sizeof(halfrate_stereo), 16);
   halfbandIN = (halfrate_stereo*)_aligned_malloc(sizeof(halfrate_stereo), 16);
   new (halfbandA) halfrate_stereo(6, true);
   new (halfbandB) halfrate_stereo(6, true);
   new (halfbandIN) halfrate_stereo(6, true);

   switch_toggled_queued = false;
   halt_engine = false;

   fx_suspend_bitmask = 0;

   demo_counter = 10;

   memset(fx, 0, sizeof(void*) * 8);
   srand((unsigned)time(NULL));
   memset(storage.getPatch().scenedata[0], 0, sizeof(pdata) * n_scene_params);
   memset(storage.getPatch().scenedata[1], 0, sizeof(pdata) * n_scene_params);
   memset(storage.getPatch().globaldata, 0, sizeof(pdata) * n_global_params);
   memset(mControlInterpolatorUsed, 0, sizeof(bool) * num_controlinterpolators);
   memset(fxsync, 0, sizeof(sub3_fx) * 8);
   for (int i = 0; i < 8; i++)
   {
      memcpy(&fxsync[i], &storage.getPatch().fx[i], sizeof(sub3_fx));
      fx_reload[i] = false;
   }

   all_notes_off();

   for (int i = 0; i < max_voices; i++)
   {
      voices_usedby[0][i] = 0;
      voices_usedby[1][i] = 0;
      voices_array[0][i] = (sub3_voice*)_aligned_malloc(sizeof(sub3_voice), 16);
      voices_array[1][i] = (sub3_voice*)_aligned_malloc(sizeof(sub3_voice), 16);
   }

   FBQ[0] = (fbq_store*)_aligned_malloc((max_voices >> 2) * sizeof(fbq_store), 16);
   FBQ[1] = (fbq_store*)_aligned_malloc((max_voices >> 2) * sizeof(fbq_store), 16);

   storage.getPatch().polylimit.val.i = 8;
   for (int sc = 0; sc < 2; sc++)
   {

      storage.getPatch().scene[sc].modsources.resize(n_modsources);
      for (int i = 0; i < n_modsources; i++)
         storage.getPatch().scene[sc].modsources[i] = 0;
      storage.getPatch().scene[sc].modsources[ms_modwheel] = new modulation_controller();
      storage.getPatch().scene[sc].modsources[ms_aftertouch] = new modulation_controller();
      storage.getPatch().scene[sc].modsources[ms_pitchbend] = new modulation_controller();

      for (int l = 0; l < n_lfos_scene; l++)
      {
         storage.getPatch().scene[sc].modsources[ms_slfo1 + l] = new modulation_lfo();
         ((modulation_lfo*)storage.getPatch().scene[sc].modsources[ms_slfo1 + l])
             ->assign(&storage, &storage.getPatch().scene[sc].lfo[n_lfos_voice + l],
                      storage.getPatch().scenedata[sc], 0,
                      &storage.getPatch().stepsequences[sc][n_lfos_voice + l]);
      }
   }
   for (int i = 0; i < n_customcontrollers; i++)
   {
      storage.getPatch().scene[0].modsources[ms_ctrl1 + i] = new modulation_controller();
      storage.getPatch().scene[1].modsources[ms_ctrl1 + i] =
          storage.getPatch().scene[0].modsources[ms_ctrl1 + i];
   }

   amp.set_blocksize(block_size);
   FX1.set_blocksize(block_size);
   FX2.set_blocksize(block_size);
   send[0][0].set_blocksize(block_size);
   send[0][1].set_blocksize(block_size);
   send[1][0].set_blocksize(block_size);
   send[1][1].set_blocksize(block_size);

   polydisplay = 0;
   refresh_editor = false;
   patch_loaded = false;
   storage.getPatch().category = "init";
   storage.getPatch().name = "init";
   storage.getPatch().comment = "";
   storage.getPatch().author = "";
   midiprogramshavechanged = false;

   for (int i = 0; i < block_size; i++)
   {
      input[0][i] = 0.f;
      input[1][i] = 0.f;
   }

   masterfade = 1.f;
   current_category_id = -1;
   patchid_queue = -1;
   patchid = -1;
   CC0 = 0;
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
   mpePitchBendRange = 48;
   mpeGlobalPitchBendRange = 2;

   //	load_patch(0);
}

sub3_synth::~sub3_synth()
{
   all_notes_off();

   halfbandA->~halfrate_stereo();
   _aligned_free(halfbandA);
   halfbandB->~halfrate_stereo();
   _aligned_free(halfbandB);
   halfbandIN->~halfrate_stereo();
   _aligned_free(halfbandIN);

   _aligned_free(FBQ[0]);
   _aligned_free(FBQ[1]);

   for (int i = 0; i < max_voices; i++)
   {
      _aligned_free(voices_array[0][i]);
      _aligned_free(voices_array[1][i]);
   }

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
   for (int i = 0; i < 8; i++)
      _aligned_free(fx[i]);
}

int sub3_synth::calc_channelmask(int channel, int key)
{

   int channelmask = channel;

   if ((channel == 0) || (channel > 2) || mpeEnabled)
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
      }
   }
   return channelmask;
}

void sub3_synth::play_note(char channel, char key, char velocity, char detune)
{
   if (halt_engine)
      return;

   // For split/dual
   // MIDI Channel 1 plays the split/dual
   // MIDI Channel 2 plays A
   // MIDI Channel 3 plays B

   int channelmask = calc_channelmask(channel, key);

   if (channelmask & 1)
      play_voice(0, channel, key, velocity, detune);
   if (channelmask & 2)
      play_voice(1, channel, key, velocity, detune);

   channelState[channel].keyState[key].keystate = velocity;
   channelState[channel].keyState[key].lastdetune = detune;
}

void sub3_synth::softkill_voice(int s)
{
   list<sub3_voice*>::iterator iter, max_playing, max_released;
   int max_age = 0, max_age_release = 0;
   iter = voices[s].begin();

   while (iter != voices[s].end())
   {
      sub3_voice* v = *iter;
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

// only allow 'margin' number of voices to be softkilled simultainously
void sub3_synth::enforce_polylimit(int s, int margin)
{
   list<sub3_voice*>::iterator iter;
   if (voices[s].size() > (storage.getPatch().polylimit.val.i + margin))
   {
      int excess_voices =
          max(0, (int)voices[s].size() - (storage.getPatch().polylimit.val.i + margin));
      iter = voices[s].begin();

      while (iter != voices[s].end())
      {
         if (excess_voices < 1)
            break;

         sub3_voice* v = *iter;
         if (v->state.uberrelease)
         {
            excess_voices--;
            free_voice(v);
            iter = voices[s].erase(iter);
         }
         else
            iter++;
      }
   }
}

int sub3_synth::get_non_ur_voices(int s)
{
   int count = 0;

   list<sub3_voice*>::iterator iter;
   iter = voices[s].begin();
   while (iter != voices[s].end())
   {
      sub3_voice* v = *iter;
      assert(v);
      if (!v->state.uberrelease)
         count++;
      iter++;
   }

   return count;
}

int sub3_synth::get_non_released_voices(int s)
{
   int count = 0;

   list<sub3_voice*>::iterator iter;
   iter = voices[s].begin();
   while (iter != voices[s].end())
   {
      sub3_voice* v = *iter;
      assert(v);
      if (v->state.gate)
         count++;
      iter++;
   }

   return count;
}

sub3_voice* sub3_synth::get_unused_voice(int scene)
{
   for (int i = 0; i < max_voices; i++)
   {
      if (!voices_usedby[scene][i])
      {
         voices_usedby[scene][i] = scene + 1;
         return voices_array[scene][i];
      }
   }
   return 0;
}

void sub3_synth::free_voice(sub3_voice* v)
{
   for (int i = 0; i < max_voices; i++)
   {
      if (voices_usedby[0][i] && (v == voices_array[0][i]))
      {
         voices_usedby[0][i] = 0;
      }
      if (voices_usedby[1][i] && (v == voices_array[1][i]))
      {
         voices_usedby[1][i] = 0;
      }
   }
}

int sub3_synth::getMpeMainChannel(int voiceChannel, int key)
{
   if (mpeEnabled)
   {
      return 0;
   }

   return voiceChannel;
}

void sub3_synth::play_voice(int scene, char channel, char key, char velocity, char detune)
{
   if (get_non_released_voices(scene) == 0)
   {
      for (int l = 0; l < n_lfos_scene; l++)
         storage.getPatch().scene[scene].modsources[ms_slfo1 + l]->attack();
   }

   // int excess_voices = max(0,(int)get_non_ur_voices(scene) - storage.getPatch().polylimit.val.i +
   // 1);	// +1 för det kommer ju en ny voice här
   int excess_voices = max(0, (int)get_non_ur_voices(scene) - storage.getPatch().polylimit.val.i +
                                  1); // +1 för det kommer ju en ny voice här
   for (int i = 0; i < excess_voices; i++)
   {
      softkill_voice(scene);
   }
   enforce_polylimit(scene, 3);

   switch (storage.getPatch().scene[scene].polymode.val.i)
   {
   case pm_poly:
   {
      // sub3_voice *nvoice = (sub3_voice*) _aligned_malloc(sizeof(sub3_voice),16);
      sub3_voice* nvoice = get_unused_voice(scene);
      if (nvoice)
      {
         int mpeMainChannel = getMpeMainChannel(channel, key);

         voices[scene].push_back(nvoice);
         new (nvoice) sub3_voice(&storage, &storage.getPatch().scene[scene],
                                 storage.getPatch().scenedata[scene], key, velocity, channel, scene,
                                 detune, &channelState[channel].keyState[key],
                                 &channelState[mpeMainChannel], &channelState[channel]);
      }
      break;
   }
   case pm_mono:
   case pm_mono_fp:
   case pm_latch:
   {
      list<sub3_voice*>::const_iterator iter;
      bool glide = false;

      for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
      {
         sub3_voice* v = *iter;
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
      sub3_voice* nvoice = get_unused_voice(scene);
      if (nvoice)
      {
         int mpeMainChannel = getMpeMainChannel(channel, key);

         voices[scene].push_back(nvoice);
         if ((storage.getPatch().scene[scene].polymode.val.i == pm_mono_fp) && !glide)
            storage.last_key[scene] = key;
         new (nvoice) sub3_voice(&storage, &storage.getPatch().scene[scene],
                                 storage.getPatch().scenedata[scene], key, velocity, channel, scene,
                                 detune, &channelState[channel].keyState[key],
                                 &channelState[mpeMainChannel], &channelState[channel]);
      }
   }
   break;
   case pm_mono_st:
   case pm_mono_st_fp:
   {
      bool found_one = false;
      list<sub3_voice*>::const_iterator iter;
      for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
      {
         sub3_voice* v = *iter;
         if ((v->state.scene_id == scene) && (v->state.gate))
         {
            v->legato(key, velocity, detune);
            found_one = true;
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
         sub3_voice* nvoice = get_unused_voice(scene);
         if (nvoice)
         {
            voices[scene].push_back(nvoice);
            new (nvoice) sub3_voice(&storage, &storage.getPatch().scene[scene],
                                    storage.getPatch().scenedata[scene], key, velocity, channel,
                                    scene, detune, &channelState[channel].keyState[key],
                                    &channelState[mpeMainChannel], &channelState[channel]);
         }
      }
   }
   break;
   }
}

void sub3_synth::release_scene(int s)
{
   list<sub3_voice*>::const_iterator iter;
   for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
   {
      (*iter)->~sub3_voice();
      //_aligned_free(*iter);
      free_voice(*iter);
   }
   voices[s].clear();
}

void sub3_synth::release_note(char channel, char key, char velocity)
{
   int channelmask =
       ((channel == 0) ? 3 : 0) || ((channel == 1) ? 1 : 0) || ((channel == 2) ? 2 : 0);

   // if(channelmask&1)
   {
      if (!channelState[channel].hold)
         release_note_postholdcheck(0, channel, key, velocity);
      else
         holdbuffer[0].push_back(key); // hold pedal is down, add to bufffer
   }
   // if(channelmask&2)
   {
      if (!channelState[channel].hold)
         release_note_postholdcheck(1, channel, key, velocity);
      else
         holdbuffer[1].push_back(key); // hold pedal is down, add to bufffer
   }
}

void sub3_synth::release_note_postholdcheck(int scene, char channel, char key, char velocity)
{
   channelState[channel].keyState[key].keystate = 0;
   list<sub3_voice*>::const_iterator iter;
   for (int s = 0; s < 2; s++)
   {
      bool do_switch = false;
      int k = 0;

      for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
      {
         sub3_voice* v = *iter;
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
            if ((v->state.key == key) && (v->state.channel == channel))
            {
               // v->release();
               for (k = hikey; k >= lowkey; k--) // search downwards
               {
                  if (channelState[channel].keyState[k].keystate)
                  {
                     do_switch = true;
                     break;
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
                  if (get_non_ur_voices(scene) == 0)
                     play_voice(scene, channel, k, velocity,
                                channelState[channel].keyState[k].lastdetune);
               }
            }
         }
         break;
         case pm_mono_st:
         case pm_mono_st_fp:
         {
            if ((v->state.key == key) && (v->state.channel == channel))
            {
               bool do_release = true;
               for (k = hikey; k >= lowkey; k--) // search downwards
               {
                  if (channelState[channel].keyState[k].keystate)
                  {
                     v->legato(k, velocity, channelState[channel].keyState[k].lastdetune);
                     do_release = false;
                     break;
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
      if (get_non_released_voices(scene) == 0)
      {
         for (int l = 0; l < n_lfos_scene; l++)
            storage.getPatch().scene[scene].modsources[ms_slfo1 + l]->release();
      }
   }
}

void sub3_synth::pitch_bend(char channel, int value)
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
   else
   {
      storage.pitch_bend = value / 8192.f;
      ((modulation_controller*)storage.getPatch().scene[0].modsources[ms_pitchbend])
          ->set_target(storage.pitch_bend);
      ((modulation_controller*)storage.getPatch().scene[1].modsources[ms_pitchbend])
          ->set_target(storage.pitch_bend);
   }
}
void sub3_synth::channel_aftertouch(char channel, int value)
{
   float fval = (float)value / 127.f;

   if (mpeEnabled)
   {
      channelState[channel].pressure = fval;
   }
   else
   {
      ((modulation_controller*)storage.getPatch().scene[0].modsources[ms_aftertouch])
          ->set_target(fval);
      ((modulation_controller*)storage.getPatch().scene[1].modsources[ms_aftertouch])
          ->set_target(fval);
   }
}

void sub3_synth::poly_aftertouch(char channel, int key, int value)
{
   float fval = (float)value / 127.f;
   storage.poly_aftertouch[0][key & 127] = fval;
   storage.poly_aftertouch[1][key & 127] = fval;
}

void sub3_synth::program_change(char channel, int value)
{
   PCH = value;
   // load_patch((CC0<<7) + PCH);
   patchid_queue = (CC0 << 7) + PCH;
}

void sub3_synth::sysex(size_t size, unsigned char* data)
{
   if (size < 5)
      return;

   /*if (data[0] == 0xF0 && data[1] == 0x7F)
   {
      // Universal Real Time SysEx header
      
      int deviceId = data[2]; // ignore
      
      int subId1 = data[3];
      int subId2 = data[4];
      
      if (subId1 == 0x08 && subId2 == 0x02)
      {
         // [SINGLE NOTE TUNING CHANGE (REAL-TIME)]
         
         if (size < 12) return;  // message shorter than this would either be invalid or useless
         
         int tuningProgram = data[5];
         
         int numChanges = data[6];
            
         int bytesLeft = size - 7;
         
         for(int k=0; k<numChanges; k++)
         {
            if (bytesLeft > 4)
            {
               int key = data[7 + 4*k];
               int semitone = data[8 + 4*k];
               int sub1 = data[9 + 4*k];
               int sub2 = data[10 + 4*k];
               
               float pitch = semitone + (float)((sub1 << 7) + sub2) * (1.f / 16384.f);
               
               setTuning(tuningProgram, key, pitch);
               
               bytesLeft -= 4;
            }
         }
      }
      else if (subId1 == 0x0A && subId2 == 0x01)
      {
         // [UNIVERSAL REAL TIME SYSTEM EXCLUSIVE]
         // KEY-BASED INSTRUMENT CONTROL
         
         if (size < 10) return;  // message shorter than this would either be invalid or useless
         
         int channel = data[5];
         int key = data[6];
         
         int bytesLeft = size - 7;
         int numControllers = (bytesLeft - 1) / 2;
         
         for(int k=0; k<numControllers; k++)
         {
            int controller = data[7 + 2*k];
            int value = data[8 + 2*k];
            
            setKeyBasedController(channel, key, controller, value);
         }
      }
   }*/
}

void sub3_synth::updateDisplay()
{
#if PLUGGUI
#else
   getParent()->updateDisplay();
#endif
}

void sub3_synth::sendParameterAutomation(long index, float value)
{
   int externalparam = remapInternalToExternalApiId(index);

   if (externalparam >= 0)
   {
#if TARGET_AU
      getParent()->ParameterUpdate(externalparam);
#elif TARGET_VST3
      getParent()->setParameterAutomated(externalparam, value);
#else
      getParent()->setParameterAutomated(externalparam, value);
#endif
   }
}

void sub3_synth::onRPN(int channel, int lsbRPN, int msbRPN, int lsbValue, int msbValue)
{
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
      mpePitchBendRange = 48;
      mpeGlobalPitchBendRange = 2;
      return;
   }
}

void sub3_synth::onNRPN(int channel, int lsbNRPN, int msbNRPN, int lsbValue, int msbValue)
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

void sub3_synth::channel_controller(char channel, int cc, int value)
{
   int channelmask = ((channel == 0) ? 3 : 0) | ((channel == 1) ? 1 : 0) | ((channel == 2) ? 2 : 0);

   float fval = (float)value * (1.f / 127.f);
   // spara all möjliga NRPN & RPN's i ett short-array.. blir endast 128kb eller nåt av av det
   // ändå..
   switch (cc)
   {
   case 0:
      CC0 = value;
      return;
   case 1:
      ((modulation_controller*)storage.getPatch().scene[0].modsources[ms_modwheel])
          ->set_target(fval);
      ((modulation_controller*)storage.getPatch().scene[1].modsources[ms_modwheel])
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
      break;
   case 38:
      if (channelState[channel].nrpn_last)
         channelState[channel].nrpn_v[0] = value;
      else
         channelState[channel].rpn_v[0] = value;
      break;
   case 64:
   {
      channelState[channel].hold = value > 63; // check hold pedal
      if (channelmask & 1)
         purge_holdbuffer(0);
      if (channelmask & 2)
         purge_holdbuffer(1);
      return;
   }

   case 10:
   {
      if (mpeEnabled)
      {
         channelState[channel].pan = int7ToBipolarFloat(value);
         return;
      }
      break;
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
      int cmode = channelState[channel].nrpn_last;
   }

   for (int i = 0; i < n_customcontrollers; i++)
   {
      if (storage.controllers[i] == cc_encoded)
      {
         ((modulation_controller*)storage.getPatch().scene[0].modsources[ms_ctrl1 + i])
             ->set_target01(fval);
         sendParameterAutomation(i + metaparam_offset, fval);
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

void sub3_synth::purge_holdbuffer(int scene)
{
   int z;
   list<int>::iterator iter = holdbuffer[scene].begin();
   while (1)
   {
      if (iter == holdbuffer[scene].end())
         break;
      z = *iter;
      if (/*voice_state[z].active && */ !channelState[0].hold)
      {
         // voices[z]->release(127);
         release_note_postholdcheck(scene, 0, z, 127);
         list<int>::iterator del = iter;
         iter++;
         holdbuffer[scene].erase(del);
      }
      else
         iter++;
   }

   // note: måste remova entries när noter dödar sig själv auch
}

void sub3_synth::all_notes_off()
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
      list<sub3_voice*>::const_iterator iter;
      for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
      {
         (*iter)->~sub3_voice();
         //_aligned_free(*iter);
         free_voice(*iter);
      }
      voices[s].clear();
   }
   holdbuffer[0].clear();
   holdbuffer[1].clear();
   halfbandA->reset();
   halfbandB->reset();
   halfbandIN->reset();

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

void sub3_synth::set_samplerate(float sr)
{
   samplerate = sr;
   dsamplerate = sr;
   samplerate_inv = 1.0 / sr;
   dsamplerate_inv = 1.0 / sr;
   dsamplerate_os = dsamplerate * osc_oversampling;
   dsamplerate_os_inv = 1.0 / dsamplerate_os;
   storage.init_tables();
   sinus.set_rate(1000.0 * dsamplerate_inv);
}

//-------------------------------------------------------------------------------------------------

int sub3_synth::GetFreeControlInterpolatorIndex()
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

int sub3_synth::GetControlInterpolatorIndex(int Id)
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

void sub3_synth::ReleaseControlInterpolator(int Id)
{
   int Index = GetControlInterpolatorIndex(Id);
   if (Index >= 0)
   {
      assert(Index < num_controlinterpolators);
      mControlInterpolatorUsed[Index] = false;
   }
}

//-------------------------------------------------------------------------------------------------

modulation_controller* sub3_synth::ControlInterpolator(int Id)
{
   int Index = GetControlInterpolatorIndex(Id);

   if (Index < 0)
      return NULL;

   return &mControlInterpolator[Index];
}

//-------------------------------------------------------------------------------------------------

modulation_controller* sub3_synth::AddControlInterpolator(int Id, bool& AlreadyExisted)
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

   return NULL;
}

//-------------------------------------------------------------------------------------------------

void sub3_synth::setParameterSmoothed(long index, float value)
{
   float oldval = storage.getPatch().param_ptr[index]->get_value_f01();

   bool AlreadyExisted;
   modulation_controller* mc = AddControlInterpolator(index, AlreadyExisted);

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

bool sub3_synth::setParameter01(long index, float value, bool external, bool force_integer)
{
   // does the parameter exist in the interpolator array? If it does, delete it
   ReleaseControlInterpolator(index);

   bool need_refresh = false;

   if (index >= metaparam_offset)
   {
      ((modulation_controller*)storage.getPatch().scene[0].modsources[ms_ctrl1 + index -
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
      case ct_wstype:
      case ct_osctype:
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
         need_refresh = true;
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

void sub3_synth::switch_toggled()
{
   for (int s = 0; s < 2; s++)
   {
      list<sub3_voice*>::iterator iter;
      for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
      {
         sub3_voice* v = *iter;
         v->switch_toggled();
      }
   }
}

bool sub3_synth::load_fx(bool initp, bool force_reload_all)
{
   load_fx_needed = false;
   bool something_changed = false;
   for (int s = 0; s < 8; s++)
   {
      if ((fxsync[s].type.val.i != storage.getPatch().fx[s].type.val.i) || force_reload_all)
      {
         fx_reload[s] = false;

         _aligned_free(fx[s]);
         /*if (!force_reload_all)*/ storage.getPatch().fx[s].type.val.i = fxsync[s].type.val.i;
         // else fxsync[s].type.val.i = storage.getPatch().fx[s].type.val.i;

         for (int j = 0; j < n_fx_params; j++)
         {
            storage.getPatch().fx[s].p[j].set_type(ct_none);
            storage.getPatch().fx[s].p[j].val.i = 0;
            storage.getPatch().globaldata[storage.getPatch().fx[s].p[j].id].i = 0;
         }

         if (/*!force_reload_all && */ storage.getPatch().fx[s].type.val.i)
            memcpy(&storage.getPatch().fx[s].p, &fxsync[s].p, sizeof(parameter) * n_fx_params);

         fx[s] = spawn_effect(storage.getPatch().fx[s].type.val.i, &storage,
                              &storage.getPatch().fx[s], storage.getPatch().globaldata);

         if (fx[s])
         {
            fx[s]->init_ctrltypes();
            if (initp)
               fx[s]->init_default_values();
            /*for(int j=0; j<n_fx_params; j++)
            {
                    storage.getPatch().globaldata[storage.getPatch().fx[s].p[j].id].f =
            storage.getPatch().fx[s].p[j].val.f;
            }*/

            fx[s]->init();
         }
         something_changed = true;
         refresh_editor = true;
      }
      else if (fx_reload[s])
      {
         memcpy(&storage.getPatch().fx[s].p, &fxsync[s].p, sizeof(parameter) * n_fx_params);
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

bool sub3_synth::load_oscalgos()
{
   for (int s = 0; s < 2; s++)
   {
      for (int i = 0; i < n_oscs; i++)
      {

         if (storage.getPatch().scene[s].osc[i].queue_type > -1)
         {
            storage.getPatch().scene[s].osc[i].type.val.i =
                storage.getPatch().scene[s].osc[i].queue_type;
            storage.getPatch().update_controls(false, &storage.getPatch().scene[s].osc[i]);
            storage.getPatch().scene[s].osc[i].queue_type = -1;
            switch_toggled_queued = true;
            refresh_editor = true;
         }

         TiXmlElement* e = (TiXmlElement*)storage.getPatch().scene[s].osc[i].queue_xmldata;

         if (e)
         {
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
      }
   }
   return true;
}

bool sub3_synth::isValidModulation(long ptag, long modsource)
{
   if (!modsource)
      return false;
   if (!(ptag < storage.getPatch().param_ptr.size()))
      return false;
   parameter* p = storage.getPatch().param_ptr[ptag];

   if (!p->modulateable)
      return false;
   if (p->valtype != ((valtypes)vt_float))
      return false;
   if (!p->per_voice_processing && !is_scenelevel(modsource))
      return false;
   if ((modsource == ms_keytrack) && (p == &storage.getPatch().scene[0].pitch))
      return false;
   if ((modsource == ms_keytrack) && (p == &storage.getPatch().scene[1].pitch))
      return false;
   if ((p->ctrlgroup == 6) && (p->ctrlgroup_entry >= ms_lfo1) &&
       !can_modulate_modulators(modsource))
      return false;
   if ((p->ctrlgroup == 6) && (p->ctrlgroup_entry == modsource))
      return false;
   if ((p->ctrlgroup == 6) && (p->ctrlgroup_entry >= ms_slfo1) && (!is_scenelevel(modsource)))
      return false;
   if ((p->ctrlgroup == 5) && !can_modulate_modulators(modsource))
      return false;
   return true;
}

modulation_routing* sub3_synth::getModRouting(long ptag, long modsource)
{
   if (!isValidModulation(ptag, modsource))
      return 0;

   int scene = storage.getPatch().param_ptr[ptag]->scene;
   vector<modulation_routing>* modlist;

   if (!scene)
   {
      if (is_scenelevel(modsource))
         modlist = &storage.getPatch().modulation_global;
      else
         return 0;
   }
   else
   {
      if (is_scenelevel(modsource))
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

float sub3_synth::getModDepth(long ptag, long modsource)
{
   modulation_routing* r = getModRouting(ptag, modsource);
   float d = 0.f;
   if (r)
      d = r->depth;
   parameter* p = storage.getPatch().param_ptr.at(ptag);
   if (p && p->extend_range)
      d = p->get_extended(d);
   return d;
}

bool sub3_synth::isActiveModulation(long ptag, long modsource)
{
   // if(!isValidModulation(ptag,modsource)) return false;
   if (getModRouting(ptag, modsource))
      return true;
   return false;
}

bool sub3_synth::isModDestUsed(long ptag)
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
         vector<modulation_routing>* modlist;

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

void sub3_synth::update_usedstate()
{
   // intended for gui only
   for (int i = 0; i < n_modsources; i++)
      modsourceused[i] = false;

   int scene = storage.getPatch().scene_active.val.i;

   for (int j = 0; j < 3; j++)
   {
      vector<modulation_routing>* modlist;

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

void sub3_synth::prepare_modsource_doprocess(int scenemask)
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
            vector<modulation_routing>* modlist;

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

bool sub3_synth::isModsourceUsed(long modsource)
{
   update_usedstate();
   return modsourceused[modsource];
}

float sub3_synth::getModulation(long ptag, long modsource)
{
   if (!isValidModulation(ptag, modsource))
      return 0.0f;

   modulation_routing* r = getModRouting(ptag, modsource);
   if (r)
      return storage.getPatch().param_ptr[ptag]->get_modulation_f01(r->depth);

   return storage.getPatch().param_ptr[ptag]->get_modulation_f01(0);
}

void sub3_synth::clear_osc_modulation(int scene, int entry)
{
   storage.CS_ModRouting.enter();
   vector<modulation_routing>::iterator iter;

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

void sub3_synth::clearModulation(long ptag, long modsource)
{
   if (!isValidModulation(ptag, modsource))
      return;
   int scene = storage.getPatch().param_ptr[ptag]->scene;

   vector<modulation_routing>* modlist;

   if (!scene)
   {
      if (is_scenelevel(modsource))
         modlist = &storage.getPatch().modulation_global;
      else
         return;
   }
   else
   {
      if (is_scenelevel(modsource))
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

bool sub3_synth::setModulation(long ptag, long modsource, float val)
{
   if (!isValidModulation(ptag, modsource))
      return false;
   float value = storage.getPatch().param_ptr[ptag]->set_modulation_f01(val);
   int scene = storage.getPatch().param_ptr[ptag]->scene;

   vector<modulation_routing>* modlist;

   if (!scene)
   {
      if (is_scenelevel(modsource))
         modlist = &storage.getPatch().modulation_global;
      else
         return false;
   }
   else
   {
      if (is_scenelevel(modsource))
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
         modulation_routing t;
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

int sub3_synth::remapExternalApiToInternalId(unsigned int x)
{
   if (x < n_customcontrollers)
      return metaparam_offset + x;
   else if (x >= n_total_params)
      return x - n_total_params;
   return x;
}

int sub3_synth::remapInternalToExternalApiId(unsigned int x)
{
   if (x >= metaparam_offset)
      return (x - metaparam_offset);
   else if (x < n_customcontrollers)
      return x + n_total_params;
   return x;
}

float sub3_synth::getParameter01(long index)
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

void sub3_synth::getParameterDisplay(long index, char* text)
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

void sub3_synth::getParameterDisplay(long index, char* text, float x)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      storage.getPatch().param_ptr[index]->get_display(text, true, x);
   }
   else
      sprintf(text, "-");
}

void sub3_synth::getParameterName(long index, char* text)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      // strncpy(text,storage.getPatch().param_ptr[index]->get_name(),32);
      strncpy(text, storage.getPatch().param_ptr[index]->get_full_name(), 32);
      // strncpy(text,storage.getPatch().param_ptr[index]->get_storage_name(),32);
   }
   else if (index >= metaparam_offset)
   {
      int c = index - metaparam_offset;
      sprintf(text, "C%i:%s", c + 1, storage.getPatch().CustomControllerLabel[c]);
   }
   else
      sprintf(text, "-");
}

void sub3_synth::getParameterNameW(long index, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      swprintf(ptr, 128, L"%S", storage.getPatch().param_ptr[index]->get_full_name());
   }
   else if (index >= metaparam_offset)
   {
      int c = index - metaparam_offset;
      if (c >= num_metaparameters)
      {
         swprintf(ptr, 128, L"C%i:ERROR");
      }
      else
      {
         swprintf(ptr, 128, L"C%i:%S", c + 1, storage.getPatch().CustomControllerLabel[c]);
      }
   }
   else
   {
      swprintf(ptr, 128, L"-");
   }
}

void sub3_synth::getParameterShortNameW(long index, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      swprintf(ptr, 128, L"%S", storage.getPatch().param_ptr[index]->get_name());
   }
   else if (index >= metaparam_offset)
   {
      int c = index - metaparam_offset;
      if (c >= num_metaparameters)
      {
         swprintf(ptr, 128, L"C%i:ERROR");
      }
      else
      {
         swprintf(ptr, 128, L"C%i:%S", c + 1, storage.getPatch().CustomControllerLabel[c]);
      }
   }
   else
   {
      swprintf(ptr, 128, L"-");
   }
}

void sub3_synth::getParameterUnitW(long index, wchar_t* ptr)
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

void sub3_synth::getParameterStringW(long index, float value, wchar_t* ptr)
{
   if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
   {
      char text[128];
      storage.getPatch().param_ptr[index]->get_display(text);

      swprintf(ptr, 128, L"%S", text);
   }
   else if (index >= metaparam_offset)
   {
      swprintf(ptr, 128, L"%.2f %%", 100.f * value);
   }
   else
   {
      swprintf(ptr, 128, L"-");
   }
}

void sub3_synth::getParameterMeta(long index, parametermeta& pm)
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
      pm.meta =
          false; // ironiskt eftersom det Šr metaparameters, men dom pŒverkar inga andra sliders
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

float sub3_synth::getParameter(long index)
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

float sub3_synth::normalizedToValue(long index, float value)
{
   if (index < 0)
      return 0.f;
   if (index < storage.getPatch().param_ptr.size())
      return storage.getPatch().param_ptr[index]->normalized_to_value(value);
   return 0.f;
}

float sub3_synth::valueToNormalized(long index, float value)
{
   if (index < 0)
      return 0.f;
   if (index < storage.getPatch().param_ptr.size())
      return storage.getPatch().param_ptr[index]->value_to_normalized(value);
   return 0.f;
}

#if MAC
void* loadPatchInBackgroundThread(void* sy)
{
#else
DWORD WINAPI loadPatchInBackgroundThread(LPVOID lpParam)
{
   void* sy = lpParam;
#endif
   sub3_synth* synth = (sub3_synth*)sy;
   int patchid = synth->patchid_queue;
   synth->patchid_queue = -1;
   synth->all_notes_off();
   synth->load_patch(patchid);
   synth->halt_engine = false;
   return 0;
}

void sub3_synth::process_threadunsafe_operations()
{
   if (!audio_processing_active)
   {
      // if the audio processing is inactive, patchloading should occur anyway
      if (patchid_queue >= 0)
      {
         load_patch(patchid_queue);
         patchid_queue = -1;
      }

      if (load_fx_needed)
         load_fx(false, false);

      load_oscalgos();
   }
}

void sub3_synth::process_control()
{
   storage.perform_queued_wtloads();
   int sm = storage.getPatch().scenemode.val.i;
   bool playA = (sm == sm_split) || (sm == sm_dual) || (storage.getPatch().scene_active.val.i == 0);
   bool playB = (sm == sm_split) || (sm == sm_dual) || (storage.getPatch().scene_active.val.i == 1);
   storage.songpos = time_data.ppqPos;
   storage.temposyncratio = time_data.tempo / 120.f;
   storage.temposyncratio_inv = 1.f / storage.temposyncratio;

   if (release_if_latched[0])
   {
      if (!playA || release_anyway[0])
         release_scene(0);
      release_if_latched[0] = false;
      release_anyway[0] = false;
   }
   if (release_if_latched[1])
   {
      if (!playB || release_anyway[1])
         release_scene(1);
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
      play_note(1, 60, 100, 0);
   if (playB && (storage.getPatch().scene[1].polymode.val.i == pm_latch) && voices[1].empty())
      play_note(2, 60, 100, 0);

   // interpolate midi controllers
   for (int i = 0; i < num_controlinterpolators; i++)
   {
      if (mControlInterpolatorUsed[i])
      {
         modulation_controller* mc = &mControlInterpolator[i];
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
       storage.getPatch().globaldata); // suger ganska mkt cpu i debug mode
   if (playA)
      storage.getPatch().copy_scenedata(storage.getPatch().scenedata[0], 0); // -""-
   if (playB)
      storage.getPatch().copy_scenedata(storage.getPatch().scenedata[1], 1);

   //	if(sm == sm_morph) storage.getPatch().do_morph();

   prepare_modsource_doprocess((playA ? 1 : 0) | (playB ? 2 : 0));

   for (int s = 0; s < 2; s++)
   {
      if (((s == 0) && playA) || ((s == 1) && playB))
      {
         if (storage.getPatch().scene[s].modsource_doprocess[ms_modwheel])
            storage.getPatch().scene[s].modsources[ms_modwheel]->process_block();
         /*if(storage.getPatch().scene[s].modsource_doprocess[ms_aftertouch])
                 storage.getPatch().scene[s].modsources[ms_aftertouch]->process_block();*/
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

   load_oscalgos();

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
      load_fx(false, false);

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

void sub3_synth::process()
{
   float mfade = 1.f;

   if (halt_engine)
   {
      /*for(int k=0; k<block_size; k++)
      {
              output[0][k] = 0;
              output[1][k] = 0;
      }*/
      clear_block(output[0], block_size_quad);
      clear_block(output[1], block_size_quad);
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

#if MAC
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

         clear_block(output[0], block_size_quad);
         clear_block(output[1], block_size_quad);
         return;
      }
   }

   // process inputs (upsample & halfrate)
   if (process_input)
   {
      hardclip_block8(input[0], block_size_quad);
      hardclip_block8(input[1], block_size_quad);
      copy_block(input[0], storage.audio_in_nonOS[0], block_size_quad);
      copy_block(input[1], storage.audio_in_nonOS[1], block_size_quad);
      halfbandIN->process_block_U2(input[0], input[1], storage.audio_in[0], storage.audio_in[1]);
   }
   else
   {
      clear_block_antidenormalnoise(storage.audio_in[0], block_size_os_quad);
      clear_block_antidenormalnoise(storage.audio_in[1], block_size_os_quad);
      clear_block_antidenormalnoise(storage.audio_in_nonOS[1], block_size_quad);
      clear_block_antidenormalnoise(storage.audio_in_nonOS[1], block_size_quad);
   }

   _MM_ALIGN16 float sceneout[2][2][block_size_os];
   _MM_ALIGN16 float fxsendout[2][2][block_size];
   bool play_scene[2];
   polydisplay = 0;

   {
      clear_block_antidenormalnoise(sceneout[0][0], block_size_os_quad);
      clear_block_antidenormalnoise(sceneout[0][1], block_size_os_quad);
      clear_block_antidenormalnoise(sceneout[1][0], block_size_os_quad);
      clear_block_antidenormalnoise(sceneout[1][1], block_size_os_quad);

      clear_block_antidenormalnoise(fxsendout[0][0], block_size_quad);
      clear_block_antidenormalnoise(fxsendout[0][1], block_size_quad);
      clear_block_antidenormalnoise(fxsendout[1][0], block_size_quad);
      clear_block_antidenormalnoise(fxsendout[1][1], block_size_quad);
   }

   // CS ENTER
   storage.CS_ModRouting.enter();
   process_control();

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

   list<sub3_voice*>::iterator iter;
   play_scene[0] = (voices[0].size() > 0);
   play_scene[1] = (voices[1].size() > 0);

   int FBentry[2];
   for (int s = 0; s < 2; s++)
   {
      FBentry[s] = 0;
      iter = voices[s].begin();
      while (iter != voices[s].end())
      {
         sub3_voice* v = *iter;
         assert(v);
         bool resume = v->process_block(FBQ[s][FBentry[s] >> 2], FBentry[s] & 3);
         FBentry[s]++;

         polydisplay++;

         if (!resume)
         {
            v->~sub3_voice();
            //_aligned_free(v);
            free_voice(v);
            iter = voices[s].erase(iter);
         }
         else
            iter++;
      }
   }

   // CS LEAVE
   storage.CS_ModRouting.leave();

   for (int s = 0; s < 2; s++)
   {
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

      iter = voices[s].begin();
      while (iter != voices[s].end())
      {
         sub3_voice* v = *iter;
         assert(v);
         v->GetQFB(); // save filter state in voices after quad processing is done
         iter++;
      }
   }

   if (play_scene[0])
   {
      hardclip_block8(sceneout[0][0], block_size_os_quad);
      hardclip_block8(sceneout[0][1], block_size_os_quad);
      halfbandA->process_block_D2(sceneout[0][0], sceneout[0][1]);
   }

   if (play_scene[1])
   {
      hardclip_block8(sceneout[1][0], block_size_os_quad);
      hardclip_block8(sceneout[1][1], block_size_os_quad);
      halfbandB->process_block_D2(sceneout[1][0], sceneout[1][1]);
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
   copy_block(sceneout[0][0], output[0], block_size_quad);
   copy_block(sceneout[0][1], output[1], block_size_quad);
   accumulate_block(sceneout[1][0], output[0], block_size_quad);
   accumulate_block(sceneout[1][1], output[1], block_size_quad);

   bool send1 = false, send2 = false;
   // add send effects
   if (fx_bypass == fxb_all_fx)
   {
      if (fx[4] && !(storage.getPatch().fx_disable.val.i & (1 << 4)))
      {
         send[0][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[0][0],
                                    fxsendout[0][1], block_size_quad);
         send[0][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[0][0],
                                    fxsendout[0][1], block_size_quad);
         send1 = fx[4]->process_ringout(fxsendout[0][0], fxsendout[0][1], sc_a || sc_b);
         FX1.MAC_2_blocks_to(fxsendout[0][0], fxsendout[0][1], output[0], output[1],
                             block_size_quad);
      }
      if (fx[5] && !(storage.getPatch().fx_disable.val.i & (1 << 5)))
      {
         send[1][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[1][0],
                                    fxsendout[1][1], block_size_quad);
         send[1][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[1][0],
                                    fxsendout[1][1], block_size_quad);
         send2 = fx[5]->process_ringout(fxsendout[1][0], fxsendout[1][1], sc_a || sc_b);
         FX2.MAC_2_blocks_to(fxsendout[1][0], fxsendout[1][1], output[0], output[1],
                             block_size_quad);
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

   amp.multiply_2_blocks(output[0], output[1], block_size_quad);
   amp_mute.multiply_2_blocks(output[0], output[1], block_size_quad);

#ifdef ISDEMO
   demo_counter--;
   if (demo_counter < 2000)
   {
      for (int i = 0; i < block_size; i++)
      {
         sinus.process();
         output[0][i] *= sinus.r;
         output[1][i] *= sinus.r;
      }
   }
   if (demo_counter < 0)
   {
      float r = (float)rand() / RAND_MAX;
      demo_counter = (int)(float)(samplerate * block_size_inv * (20.f + 20.f * r));
   }
#endif

   // VU
   // falloff
   float a = storage.vu_falloff;
   vu_peak[0] = min(2.f, a * vu_peak[0]);
   vu_peak[1] = min(2.f, a * vu_peak[1]);
   /*for(int i=0; i<block_size; i++)
   {
           vu_peak[0] = max(vu_peak[0], output[0][i]);
           vu_peak[1] = max(vu_peak[1], output[1][i]);
   }*/
   vu_peak[0] = max(vu_peak[0], get_absmax(output[0], block_size_quad));
   vu_peak[1] = max(vu_peak[1], get_absmax(output[1], block_size_quad));

   hardclip_block8(output[0], block_size_quad);
   hardclip_block8(output[1], block_size_quad);
}

PluginLayer* sub3_synth::getParent()
{
   assert(_parent != nullptr);
   return _parent;
}
