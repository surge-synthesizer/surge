//-------------------------------------------------------------------------------------------------------
//		Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "SurgeVoice.h"
#include "DspUtilities.h"
#include "QuadFilterChain.h"

using namespace std;

enum lag_entries
{
   le_osc1,
   le_osc2,
   le_osc3,
   le_noise,
   le_ring12,
   le_ring23,
   le_pfg,
};

inline void set1f(__m128& m, int i, float f)
{
   *((float*)&m + i) = f;
}

inline void set1i(__m128& m, int e, int i)
{
   *((int*)&m + e) = i;
}

inline float get1f(__m128 m, int i)
{
   return *((float*)&m + i);
}

float SurgeVoiceState::getPitch()
{
   return key + mainChannelState->pitchBendInSemitones + voiceChannelState->pitchBendInSemitones +
          detune;
}

SurgeVoice::SurgeVoice()
{
}

SurgeVoice::SurgeVoice(SurgeStorage* storage,
                       SurgeSceneStorage* oscene,
                       pdata* params,
                       int key,
                       int velocity,
                       int channel,
                       int scene_id,
                       float detune,
                       MidiKeyState* keyState,
                       MidiChannelState* mainChannelState,
                       MidiChannelState* voiceChannelState,
                       bool mpeEnabled
    )
//: fb(storage,oscene)
{
   // assign pointers
   this->storage = storage;
   this->scene = oscene;
   this->paramptr = params;
   this->mpeEnabled = mpeEnabled;
   assert(storage);
   assert(oscene);

   memcpy(localcopy, paramptr, sizeof(localcopy));

   age = 0;
   age_release = 0;
   state.key = key;
   state.channel = channel;

   state.velocity = velocity;
   state.fvel = velocity / 127.f;

   state.releasevelocity = 0;
   state.freleasevel = 0;
   
   state.scene_id = scene_id;
   state.detune = detune;
   state.uberrelease = false;
   state.keyState = keyState;
   state.mainChannelState = mainChannelState;
   state.voiceChannelState = voiceChannelState;
   if ((scene->polymode.val.i == pm_mono_st_fp) ||
       (scene->portamento.val.f == scene->portamento.val_min.f))
      state.portasrc_key = state.getPitch();
   else
      state.portasrc_key = storage->last_key[scene_id];

   storage->last_key[scene_id] = key;
   state.portaphase = 0;
   noisegenL[0] = 0.f;
   noisegenR[0] = 0.f;
   noisegenL[1] = 0.f;
   noisegenR[1] = 0.f;

   // set states & variables
   state.gate = true;
   state.keep_playing = true;

   // init subcomponents
   for (int i = 0; i < n_oscs; i++)
   {
      osctype[i] = -1;
   }
   memset(&FBP, 0, sizeof(FBP));

   lag_id[le_osc1] = scene->level_o1.param_id_in_scene;
   lag_id[le_osc2] = scene->level_o2.param_id_in_scene;
   lag_id[le_osc3] = scene->level_o3.param_id_in_scene;
   lag_id[le_noise] = scene->level_noise.param_id_in_scene;
   lag_id[le_ring12] = scene->level_ring_12.param_id_in_scene;
   lag_id[le_ring23] = scene->level_ring_23.param_id_in_scene;
   lag_id[le_pfg] = scene->level_pfg.param_id_in_scene;

   volume_id = scene->volume.param_id_in_scene;
   pan_id = scene->pan.param_id_in_scene;
   width_id = scene->width.param_id_in_scene;
   pitch_id = scene->pitch.param_id_in_scene;
   octave_id = scene->octave.param_id_in_scene;

   for (int i = 0; i < 6; i++)
   {
      lfo[i].assign(storage, &scene->lfo[i], localcopy, &state,
                    &storage->getPatch().stepsequences[state.scene_id][i]);
      modsources[ms_lfo1 + i] = &lfo[i];
   }
   modsources[ms_velocity] = &velocitySource;
   modsources[ms_releasevelocity] = &releaseVelocitySource;
   modsources[ms_keytrack] = &keytrackSource;
   modsources[ms_polyaftertouch] = &polyAftertouchSource;
   polyAftertouchSource.init(storage->poly_aftertouch[state.scene_id & 1][state.key & 127]);

   velocitySource.output = state.fvel;
   releaseVelocitySource.output = state.freleasevel;
   keytrackSource.output = 0;
   
   ampEGSource.init(storage, &scene->adsr[0], localcopy, &state);
   filterEGSource.init(storage, &scene->adsr[1], localcopy, &state);
   modsources[ms_ampeg] = &ampEGSource;
   modsources[ms_filtereg] = &filterEGSource;

   modsources[ms_modwheel] = oscene->modsources[ms_modwheel];
   modsources[ms_aftertouch] = &monoAftertouchSource;
   monoAftertouchSource.output = state.voiceChannelState->pressure;
   modsources[ms_timbre] = &timbreSource;
   timbreSource.output = state.voiceChannelState->timbre;
   modsources[ms_pitchbend] = oscene->modsources[ms_pitchbend];
   for (int i = 0; i < n_customcontrollers; i++)
      modsources[ms_ctrl1 + i] = oscene->modsources[ms_ctrl1 + i];
   modsources[ms_slfo1] = oscene->modsources[ms_slfo1];
   modsources[ms_slfo2] = oscene->modsources[ms_slfo2];
   modsources[ms_slfo3] = oscene->modsources[ms_slfo3];
   modsources[ms_slfo4] = oscene->modsources[ms_slfo4];
   modsources[ms_slfo5] = oscene->modsources[ms_slfo5];
   modsources[ms_slfo6] = oscene->modsources[ms_slfo6];

   id_cfa = scene->filterunit[0].cutoff.param_id_in_scene;
   id_cfb = scene->filterunit[1].cutoff.param_id_in_scene;
   id_kta = scene->filterunit[0].keytrack.param_id_in_scene;
   id_ktb = scene->filterunit[1].keytrack.param_id_in_scene;
   id_emoda = scene->filterunit[0].envmod.param_id_in_scene;
   id_emodb = scene->filterunit[1].envmod.param_id_in_scene;
   id_resoa = scene->filterunit[0].resonance.param_id_in_scene;
   id_resob = scene->filterunit[1].resonance.param_id_in_scene;
   id_drive = scene->wsunit.drive.param_id_in_scene;
   id_vca = scene->vca_level.param_id_in_scene;
   id_vcavel = scene->vca_velsense.param_id_in_scene;
   id_fbalance = scene->filter_balance.param_id_in_scene;
   id_feedback = scene->feedback.param_id_in_scene;

   switch_toggled();
   ampEGSource.attack();
   filterEGSource.attack();

   calc_ctrldata<true>(0, 0); // init interpolators

   SetQFB(0, 0); // init Quad-Filterblock parameter interpolators

   for (int i = 0; i < 6; i++)
      lfo[i].attack();
}

SurgeVoice::~SurgeVoice()
{
}

void SurgeVoice::legato(int key, int velocity, char detune)
{
   if (state.portaphase > 1)
      state.portasrc_key = state.getPitch();
   else
   {
      state.portasrc_key =
          ((1 - state.portaphase) * state.portasrc_key + state.portaphase * state.getPitch());
   }

   state.key = key;
   storage->last_key[state.scene_id] = key;
   state.portaphase = 0;

   /*state.velocity = velocity;
   state.fvel = velocity/127.f;*/
}

void SurgeVoice::switch_toggled()
{
   update_portamento();
   float pb = modsources[ms_pitchbend]->output;
   if (pb > 0)
      pb *= (float)scene->pbrange_up.val.i;
   else
      pb *= (float)scene->pbrange_dn.val.i;
   state.pitch = state.pkey + pb;

   modsources[ms_keytrack]->output =
       (state.pitch - (float)scene->keytrack_root.val.i) * (1.0f / 12.0f);

   for (int i = 0; i < n_oscs; i++)
   {
      if (osctype[i] != scene->osc[i].type.val.i)
      {
         osc[i].reset(spawn_osc(scene->osc[i].type.val.i, storage, &scene->osc[i], localcopy));
         if (osc[i])
            osc[i]->init(state.pitch);
         osctype[i] = scene->osc[i].type.val.i;
      }
   }

   int FM = scene->fm_switch.val.i;
   switch (FM)
   {
   case fm_off:
      break;
   case fm_2to1:
      if (osc[1] && osc[0])
         osc[0]->assign_fm(osc[1]->output);
      break;
   case fm_3to2to1:
      if (osc[1] && osc[0])
         osc[0]->assign_fm(osc[1]->output);
      if (osc[2] && osc[1])
         osc[1]->assign_fm(osc[2]->output);
      break;
   case fm_2and3to1:
      if (osc[0])
         osc[0]->assign_fm(fmbuffer);
      break;
   }

   bool solo = (scene->solo_o1.val.b || scene->solo_o2.val.b || scene->solo_o3.val.b ||
                scene->solo_noise.val.b || scene->solo_ring_12.val.b || scene->solo_ring_23.val.b);
   if (solo)
   {
      if (scene->solo_o1.val.b)
         set_path(true, false, false, false, false, false, false);
      else if (scene->solo_o2.val.b)
         set_path(false, true, false, FM, false, false, false);
      else if (scene->solo_o3.val.b)
         set_path(false, false, true, false, false, false, false);
      else if (scene->solo_noise.val.b)
         set_path(false, false, false, false, false, false, true);
      else if (scene->solo_ring_12.val.b)
         set_path(false, false, false, false, true, false, false);
      else if (scene->solo_ring_23.val.b)
         set_path(false, false, false, false, false, true, false);
   }
   else
   {
      bool use_osc1 = (!scene->mute_o1.val.b);
      bool use_osc2 = (!scene->mute_o2.val.b);
      bool use_osc3 = (!scene->mute_o3.val.b);
      bool use_ring12 = (!scene->mute_ring_12.val.b);
      bool use_ring23 = (!scene->mute_ring_23.val.b);
      bool use_noise = (!scene->mute_noise.val.b);
      set_path(use_osc1, use_osc2, use_osc3, FM, use_ring12, use_ring23, use_noise);
   }

   // Check the filtertype (kolla = check?)
   for (int u = 0; u < 2; u++)
   {
      if ((scene->filterunit[u].type.val.i != FBP.FU[u].type) ||
          (scene->filterunit[u].subtype.val.i != FBP.FU[u].subtype))
      {
         memset(&FBP.FU[u], 0, sizeof(FBP.FU[u]));
         FBP.FU[u].type = scene->filterunit[u].type.val.i;
         FBP.FU[u].subtype = scene->filterunit[u].subtype.val.i;

         if (scene->filterblock_configuration.val.i == fb_wide)
         {
            memset(&FBP.FU[u + 2], 0, sizeof(FBP.FU[u + 2]));
            FBP.FU[u + 2].type = scene->filterunit[u].type.val.i;
            FBP.FU[u + 2].subtype = scene->filterunit[u].subtype.val.i;
         }

         CM[u].Reset();
      }
   }
}

void SurgeVoice::release()
{
   ampEGSource.release();
   filterEGSource.release();

   for (int i = 0; i < 6; i++)
      lfo[i].release();

   state.gate = false;
   releaseVelocitySource.output = state.releasevelocity / 127.0f;
}

void SurgeVoice::uber_release()
{
   ampEGSource.uber_release();
   state.gate = false;
   state.uberrelease = true;
}

void SurgeVoice::update_portamento()
{
   // float ratemult=1;
   // if(zone->portamento_mode) ratemult = 12/(0.00001+fabs(key-portasrc_key));
   state.portaphase += envelope_rate_linear(localcopy[scene->portamento.param_id_in_scene].f) *
                       (scene->portamento.temposync ? storage->temposyncratio : 1.f);
   // powf(2,-zone->portamento)
   if (state.portaphase < 1)
   {
      state.pkey = (1.f - state.portaphase) * state.portasrc_key +
                   (float)state.portaphase * state.getPitch();
   }
   else
      state.pkey = state.getPitch();
}

int SurgeVoice::routefilter(int r)
{
   switch (scene->filterblock_configuration.val.i)
   {
   case fb_serial:
   case fb_serial2:
   case fb_serial3:
      if (r == 1)
         r = 0;
      break;
   }
   return r;
}

template <bool first> void SurgeVoice::calc_ctrldata(QuadFilterChainState* Q, int e)
{
   // Always process LFO1 so the gate retrigger always work
   lfo[0].process_block();

   for (int i = 1; i < 6; i++)
   {
      if (scene->modsource_doprocess[ms_lfo1 + i])
         lfo[i].process_block();
   }

   if (lfo[0].retrigger_AEG)
   {
      ((AdsrEnvelope*)modsources[ms_ampeg])->retrigger();
   }
   if (lfo[0].retrigger_FEG)
   {
      ((AdsrEnvelope*)modsources[ms_filtereg])->retrigger();
   }

   modsources[ms_aftertouch]->output = state.voiceChannelState->pressure;
   modsources[ms_timbre]->output = state.voiceChannelState->timbre;

   modsources[ms_ampeg]->process_block();
   modsources[ms_filtereg]->process_block();
   if (((AdsrEnvelope*)modsources[ms_ampeg])->is_idle())
      state.keep_playing = false;

   // TODO memcpy is bottleneck
   memcpy(localcopy, paramptr, sizeof(localcopy));
   // don't actually need to copy everything
   // LFOs could be ignore when unused
   // same for FX & OSCs
   // also ignore int-parameters

   vector<ModulationRouting>::iterator iter;
   iter = scene->modulation_voice.begin();
   while (iter != scene->modulation_voice.end())
   {
      int src_id = iter->source_id;
      int dst_id = iter->destination_id;
      float depth = iter->depth;

      if (modsources[src_id])
      {
         localcopy[dst_id].f += depth * modsources[src_id]->output;
      }
      iter++;
   }

   if( mpeEnabled )
   {
       // See github issue 1214. This basically compensates for
       // channel AT being per-voice in MPE mode (since it is per channel)
       // vs per-scene (since it is per keyboard in non MPE mode).
       iter = scene->modulation_scene.begin();
       while( iter != scene->modulation_scene.end() )
       {
           int src_id = iter->source_id;
           if( src_id == ms_aftertouch && modsources[src_id] )
           {
               int dst_id = iter->destination_id;
               // I don't THINK we need this but am not sure the global params are in my localcopy span
               if( dst_id >= 0 && dst_id < n_scene_params )
               {
                   float depth = iter->depth;
                   localcopy[dst_id].f += depth * modsources[src_id]->output;
               }
           }
           iter++;
       }
   }

   update_portamento();
   float pb = modsources[ms_pitchbend]->output;
   if (pb > 0)
      pb *= (float)scene->pbrange_up.val.i;
   else
      pb *= (float)scene->pbrange_dn.val.i;

   state.pitch = state.pkey + pb +
                 localcopy[pitch_id].f * (scene->pitch.extend_range ? 12.f : 1.f) +
                 (12.0f * localcopy[octave_id].i);
   modsources[ms_keytrack]->output =
       (state.pitch - (float)scene->keytrack_root.val.i) * (1.0f / 12.0f);

   if (scene->modsource_doprocess[ms_polyaftertouch])
   {
      ((ControllerModulationSource*)modsources[ms_polyaftertouch])
          ->set_target(storage->poly_aftertouch[state.scene_id & 1][state.key & 127]);
      ((ControllerModulationSource*)modsources[ms_polyaftertouch])->process_block();
   }

   float o1 = amp_to_linear(localcopy[lag_id[le_osc1]].f);
   float o2 = amp_to_linear(localcopy[lag_id[le_osc2]].f);
   float o3 = amp_to_linear(localcopy[lag_id[le_osc3]].f);
   float on = amp_to_linear(localcopy[lag_id[le_noise]].f);
   float r12 = amp_to_linear(localcopy[lag_id[le_ring12]].f);
   float r23 = amp_to_linear(localcopy[lag_id[le_ring23]].f);
   float pfg = db_to_linear(localcopy[lag_id[le_pfg]].f);

   osclevels[le_osc1].set_target(o1);
   osclevels[le_osc2].set_target(o2);
   osclevels[le_osc3].set_target(o3);
   osclevels[le_noise].set_target(on);
   osclevels[le_ring12].set_target(r12);
   osclevels[le_ring23].set_target(r23);
   osclevels[le_pfg].set_target(pfg);

   route[0] = routefilter(scene->route_o1.val.i);
   route[1] = routefilter(scene->route_o2.val.i);
   route[2] = routefilter(scene->route_o3.val.i);
   route[3] = routefilter(scene->route_ring_12.val.i);
   route[4] = routefilter(scene->route_ring_23.val.i);
   route[5] = routefilter(scene->route_noise.val.i);

   float pan1 = limit_range(
       localcopy[pan_id].f + state.voiceChannelState->pan + state.mainChannelState->pan, -1.f, 1.f);
   float amp =
       0.5f * amp_to_linear(localcopy[volume_id].f); // the *0.5 multiplication will be eliminated
                                                     // by the 2x gain of the halfband filter

   // Volume correcting/correction (fb_stereo updated since v1.2.2)
   if (scene->filterblock_configuration.val.i == fb_wide)
      amp *= 0.6666666f;
   else if (scene->filterblock_configuration.val.i == fb_stereo)
      amp *= 1.3333333f;

   if ((scene->filterblock_configuration.val.i == fb_stereo) ||
       (scene->filterblock_configuration.val.i == fb_wide))
   {
      pan1 -= localcopy[width_id].f;
      float pan2 = localcopy[pan_id].f + localcopy[width_id].f;
      float amp2L = amp * megapanL(pan2);
      float amp2R = amp * megapanR(pan2);
      if (Q)
      {
         set1f(Q->Out2L, e, FBP.Out2L);
         set1f(Q->dOut2L, e, (amp2L - FBP.Out2L) * BLOCK_SIZE_OS_INV);
         set1f(Q->Out2R, e, FBP.Out2R);
         set1f(Q->dOut2R, e, (amp2R - FBP.Out2R) * BLOCK_SIZE_OS_INV);
      }
      FBP.Out2L = amp2L;
      FBP.Out2R = amp2R;
   }

   float ampL = amp * megapanL(pan1);
   float ampR = amp * megapanR(pan1);

   if (Q)
   {
      set1f(Q->OutL, e, FBP.OutL);
      set1f(Q->dOutL, e, (ampL - FBP.OutL) * BLOCK_SIZE_OS_INV);
      set1f(Q->OutR, e, FBP.OutR);
      set1f(Q->dOutR, e, (ampR - FBP.OutR) * BLOCK_SIZE_OS_INV);
   }

   FBP.OutL = ampL;
   FBP.OutR = ampR;
}

bool SurgeVoice::process_block(QuadFilterChainState& Q, int Qe)
{
   calc_ctrldata<0>(&Q, Qe);

   bool is_wide = scene->filterblock_configuration.val.i == fb_wide;
   float tblock alignas(16)[BLOCK_SIZE_OS],
         tblock2 alignas(16)[BLOCK_SIZE_OS];
   float* tblockR = is_wide ? tblock2 : tblock;

   // float ktrkroot = (float)scene->keytrack_root.val.i;
   float ktrkroot = 60;
   float drift = localcopy[scene->drift.param_id_in_scene].f;

   // clear output
   clear_block(output[0], BLOCK_SIZE_OS_QUAD);
   clear_block(output[1], BLOCK_SIZE_OS_QUAD);

   if (osc3 || ring23 || ((osc1 || osc2) && (FMmode == fm_3to2to1)) ||
       (osc1 && (FMmode == fm_2and3to1)))
   {
       osc[2]->process_block(noteShiftFromPitchParam( (scene->osc[2].keytrack.val.b ? state.pitch : ktrkroot) +
                                                      12 * scene->osc[2].octave.val.i,
                                                      2 ),
                            drift, is_wide);

      if (osc3)
      {
         if (is_wide)
         {
            osclevels[le_osc3].multiply_2_blocks_to(osc[2]->output, osc[2]->outputR, tblock,
                                                    tblockR, BLOCK_SIZE_OS_QUAD);
         }
         else
         {
            osclevels[le_osc3].multiply_block_to(osc[2]->output, tblock, BLOCK_SIZE_OS_QUAD);
         }
         if (route[2] < 2)
            accumulate_block(tblock, output[0], BLOCK_SIZE_OS_QUAD);
         if (route[2] > 0)
            accumulate_block(tblockR, output[1], BLOCK_SIZE_OS_QUAD);
      }
   }

   if (osc2 || ring12 || ring23 || (FMmode && osc1))
   {
      if (FMmode == fm_3to2to1)
      {
          osc[1]->process_block(noteShiftFromPitchParam((scene->osc[1].keytrack.val.b ? state.pitch : ktrkroot) +
                                                        12 * scene->osc[1].octave.val.i,
                                                        1 ),
                               
                               drift, is_wide, true,
                               db_to_linear(localcopy[scene->fm_depth.param_id_in_scene].f));
      }
      else
      {
          osc[1]->process_block(noteShiftFromPitchParam((scene->osc[1].keytrack.val.b ? state.pitch : ktrkroot) +
                                                        12 * scene->osc[1].octave.val.i,
                                                        1),
                               drift, is_wide);
      }
      if (osc2)
      {
         if (is_wide)
         {
            osclevels[le_osc2].multiply_2_blocks_to(osc[1]->output, osc[1]->outputR, tblock,
                                                    tblockR, BLOCK_SIZE_OS_QUAD);
         }
         else
         {
            osclevels[le_osc2].multiply_block_to(osc[1]->output, tblock, BLOCK_SIZE_OS_QUAD);
         }

         if (route[1] < 2)
            accumulate_block(tblock, output[0], BLOCK_SIZE_OS_QUAD);
         if (route[1] > 0)
            accumulate_block(tblockR, output[1], BLOCK_SIZE_OS_QUAD);
      }
   }

   if (osc1 || ring12)
   {
      if (FMmode == fm_2and3to1)
      {
         add_block(osc[1]->output, osc[2]->output, fmbuffer, BLOCK_SIZE_OS_QUAD);
         osc[0]->process_block(noteShiftFromPitchParam((scene->osc[0].keytrack.val.b ? state.pitch : ktrkroot) +
                                                       12 * scene->osc[0].octave.val.i, 0 ),
                               drift, is_wide, true,
                               db_to_linear(localcopy[scene->fm_depth.param_id_in_scene].f));
      }
      else if (FMmode)
      {
          osc[0]->process_block(noteShiftFromPitchParam((scene->osc[0].keytrack.val.b ? state.pitch : 60) +
                                                        12 * scene->osc[0].octave.val.i,
                                                        0),

                               drift, is_wide, true,
                               db_to_linear(localcopy[scene->fm_depth.param_id_in_scene].f));
      }
      else
      {
         osc[0]->process_block(noteShiftFromPitchParam((scene->osc[0].keytrack.val.b ? state.pitch : ktrkroot) +
                                                       12 * scene->osc[0].octave.val.i,
                                                       0),
                               drift, is_wide);
      }
      if (osc1)
      {
         if (is_wide)
         {
            osclevels[le_osc1].multiply_2_blocks_to(osc[0]->output, osc[0]->outputR, tblock,
                                                    tblockR, BLOCK_SIZE_OS_QUAD);
         }
         else
         {
            osclevels[le_osc1].multiply_block_to(osc[0]->output, tblock, BLOCK_SIZE_OS_QUAD);
         }
         if (route[0] < 2)
            accumulate_block(tblock, output[0], BLOCK_SIZE_OS_QUAD);
         if (route[0] > 0)
            accumulate_block(tblockR, output[1], BLOCK_SIZE_OS_QUAD);
      }
   }

   if (ring12)
   {
      if (is_wide)
      {
         mul_block(osc[0]->output, osc[1]->output, tblock, BLOCK_SIZE_OS_QUAD);
         mul_block(osc[0]->outputR, osc[1]->outputR, tblockR, BLOCK_SIZE_OS_QUAD);
         osclevels[le_ring12].multiply_2_blocks(tblock, tblockR, BLOCK_SIZE_OS_QUAD);
      }
      else
      {
         mul_block(osc[0]->output, osc[1]->output, tblock, BLOCK_SIZE_OS_QUAD);
         osclevels[le_ring12].multiply_block(tblock, BLOCK_SIZE_OS_QUAD);
      }
      if (route[3] < 2)
         accumulate_block(tblock, output[0], BLOCK_SIZE_OS_QUAD);
      if (route[3] > 0)
         accumulate_block(tblockR, output[1], BLOCK_SIZE_OS_QUAD);
   }

   if (ring23)
   {
      if (is_wide)
      {
         mul_block(osc[1]->output, osc[2]->output, tblock, BLOCK_SIZE_OS_QUAD);
         mul_block(osc[1]->outputR, osc[2]->outputR, tblockR, BLOCK_SIZE_OS_QUAD);
         osclevels[le_ring23].multiply_2_blocks(tblock, tblockR, BLOCK_SIZE_OS_QUAD);
      }
      else
      {
         mul_block(osc[1]->output, osc[2]->output, tblock, BLOCK_SIZE_OS_QUAD);
         osclevels[le_ring23].multiply_block(tblock, BLOCK_SIZE_OS_QUAD);
      }

      if (route[4] < 2)
         accumulate_block(tblock, output[0], BLOCK_SIZE_OS_QUAD);
      if (route[4] > 0)
         accumulate_block(tblockR, output[1], BLOCK_SIZE_OS_QUAD);
   }

   if (noise)
   {
      float noisecol = limit_range(localcopy[scene->noise_colour.param_id_in_scene].f, -1.f, 1.f);
      for (int i = 0; i < BLOCK_SIZE_OS; i += 2)
      {
         ((float*)tblock)[i] = correlated_noise_o2mk2(noisegenL[0], noisegenL[1], noisecol);
         ((float*)tblock)[i + 1] = ((float*)tblock)[i];
         if (is_wide)
         {
            ((float*)tblockR)[i] = correlated_noise_o2mk2(noisegenR[0], noisegenR[1], noisecol);
            ((float*)tblockR)[i + 1] = ((float*)tblockR)[i];
         }
      }
      if (is_wide)
      {
         osclevels[le_noise].multiply_2_blocks(tblock, tblockR, BLOCK_SIZE_OS_QUAD);
      }
      else
      {
         osclevels[le_noise].multiply_block(tblock, BLOCK_SIZE_OS_QUAD);
      }
      if (route[5] < 2)
         accumulate_block(tblock, output[0], BLOCK_SIZE_OS_QUAD);
      if (route[5] > 0)
         accumulate_block(tblockR, output[1], BLOCK_SIZE_OS_QUAD);
   }

   // PFG
   osclevels[le_pfg].multiply_2_blocks(output[0], output[1], BLOCK_SIZE_OS_QUAD);

   for (int i = 0; i < BLOCK_SIZE_OS; i++)
   {
      _mm_store_ss(((float*)&Q.DL[i] + Qe), _mm_load_ss(&output[0][i]));
      _mm_store_ss(((float*)&Q.DR[i] + Qe), _mm_load_ss(&output[1][i]));
   }
   SetQFB(&Q, Qe);

   age++;
   if (!state.gate)
      age_release++;

   return state.keep_playing;
}

void SurgeVoice::set_path(
    bool osc1, bool osc2, bool osc3, int FMmode, bool ring12, bool ring23, bool noise)
{
   this->osc1 = osc1;
   this->osc2 = osc2;
   this->osc3 = osc3;
   this->FMmode = FMmode;
   this->ring12 = ring12;
   this->ring23 = ring23;
   this->noise = noise;
}

void SurgeVoice::SetQFB(QuadFilterChainState* Q, int e) // Q == 0 means init(ialise)
{
   fbq = Q;
   fbqi = e;

   float FMix1, FMix2;
   switch (scene->filterblock_configuration.val.i)
   {
   case fb_serial:
   case fb_serial2:
   case fb_serial3:
   case fb_ring:
   case fb_wide:
      FMix1 = min(1.f, 1.f - localcopy[id_fbalance].f);
      FMix2 = min(1.f, 1.f + localcopy[id_fbalance].f);
      break;
   default:
      FMix1 = 0.5f - 0.5f * localcopy[id_fbalance].f;
      FMix2 = 0.5f + 0.5f * localcopy[id_fbalance].f;
      break;
   }

   float Drive = db_to_linear(localcopy[id_drive].f);
   float Gain = db_to_linear(localcopy[id_vca].f + localcopy[id_vcavel].f * (1.f - state.fvel)) *
                modsources[ms_ampeg]->output;
   float FB = localcopy[id_feedback].f;

   if (Q)
   {
      set1f(Q->Gain, e, FBP.Gain);
      set1f(Q->dGain, e, (Gain - FBP.Gain) * BLOCK_SIZE_OS_INV);
      set1f(Q->Drive, e, FBP.Drive);
      set1f(Q->dDrive, e, (Drive - FBP.Drive) * BLOCK_SIZE_OS_INV);
      set1f(Q->FB, e, FBP.FB);
      set1f(Q->dFB, e, (FB - FBP.FB) * BLOCK_SIZE_OS_INV);
      set1f(Q->Mix1, e, FBP.Mix1);
      set1f(Q->dMix1, e, (FMix1 - FBP.Mix1) * BLOCK_SIZE_OS_INV);
      set1f(Q->Mix2, e, FBP.Mix2);
      set1f(Q->dMix2, e, (FMix2 - FBP.Mix2) * BLOCK_SIZE_OS_INV);
   }

   FBP.Gain = Gain;
   FBP.Drive = Drive;
   FBP.FB = FB;
   FBP.Mix1 = FMix1;
   FBP.Mix2 = FMix2;

   // filterunits
   if (Q)
   {
      set1f(Q->wsLPF, e, FBP.wsLPF); // remember state
      set1f(Q->FBlineL, e, FBP.FBlineL);
      set1f(Q->FBlineR, e, FBP.FBlineR);
      Q->FU[0].active[e] = 0xffffffff;
      Q->FU[1].active[e] = 0xffffffff;
      Q->FU[2].active[e] = 0xffffffff;
      Q->FU[3].active[e] = 0xffffffff;

      float keytrack = state.pitch - (float)scene->keytrack_root.val.i;
      float fenv = modsources[ms_filtereg]->output;
      float cutoffA =
          localcopy[id_cfa].f + localcopy[id_kta].f * keytrack + localcopy[id_emoda].f * fenv;
      float cutoffB =
          localcopy[id_cfb].f + localcopy[id_ktb].f * keytrack + localcopy[id_emodb].f * fenv;

      if (scene->f2_cutoff_is_offset.val.b)
         cutoffB += cutoffA;

      CM[0].MakeCoeffs(cutoffA, localcopy[id_resoa].f, scene->filterunit[0].type.val.i,
                       scene->filterunit[0].subtype.val.i, storage);
      CM[1].MakeCoeffs(
          cutoffB, scene->f2_link_resonance.val.b ? localcopy[id_resoa].f : localcopy[id_resob].f,
          scene->filterunit[1].type.val.i, scene->filterunit[1].subtype.val.i, storage);

      for (int u = 0; u < 2; u++)
      {
         if (scene->filterunit[u].type.val.i != 0)
         {
            for (int i = 0; i < n_cm_coeffs; i++)
            {
               set1f(Q->FU[u].C[i], e, CM[u].C[i]);
               set1f(Q->FU[u].dC[i], e, CM[u].dC[i]);
            }

            for (int i = 0; i < n_filter_registers; i++)
            {
               set1f(Q->FU[u].R[i], e, FBP.FU[u].R[i]);
            }

            Q->FU[u].DB[e] = FBP.Delay[u];
            Q->FU[u].WP[e] = FBP.FU[u].WP;
            if (scene->filterunit[u].type.val.i == fut_lpmoog)
               Q->FU[u].WP[0] =
                   scene->filterunit[u].subtype.val.i; // LPMoog's output stage index is stored in
                                                       // WP[0] for the entire quad

            if (scene->filterblock_configuration.val.i == fb_wide)
            {
               for (int i = 0; i < n_cm_coeffs; i++)
               {
                  set1f(Q->FU[u + 2].C[i], e, CM[u].C[i]);
                  set1f(Q->FU[u + 2].dC[i], e, CM[u].dC[i]);
               }

               for (int i = 0; i < n_filter_registers; i++)
               {
                  set1f(Q->FU[u + 2].R[i], e, FBP.FU[u + 2].R[i]);
               }

               Q->FU[u + 2].DB[e] = FBP.Delay[u + 2];
               Q->FU[u + 2].WP[e] = FBP.FU[u].WP;
               if (scene->filterunit[u].type.val.i == fut_lpmoog)
                  Q->FU[u + 2].WP[0] = scene->filterunit[u].subtype.val.i;
            }
         }
      }
   }
}

void SurgeVoice::GetQFB()
{
   for (int u = 0; u < 2; u++)
   {
      if (scene->filterunit[u].type.val.i != 0)
      {
         for (int i = 0; i < n_filter_registers; i++)
         {
            FBP.FU[u].R[i] = get1f(fbq->FU[u].R[i], fbqi);
         }

         for (int i = 0; i < n_cm_coeffs; i++)
         {
            CM[u].C[i] = get1f(fbq->FU[u].C[i], fbqi);
         }
         FBP.FU[u].WP = fbq->FU[u].WP[fbqi];

         if (scene->filterblock_configuration.val.i == fb_wide)
         {
            for (int i = 0; i < n_filter_registers; i++)
            {
               FBP.FU[u + 2].R[i] = get1f(fbq->FU[u + 2].R[i], fbqi);
            }
         }
      }
   }
   FBP.FBlineL = get1f(fbq->FBlineL, fbqi);
   FBP.FBlineR = get1f(fbq->FBlineR, fbqi);
   FBP.wsLPF = get1f(fbq->wsLPF, fbqi);
}

void SurgeVoice::freeAllocatedElements()
{
   for(int i=0;i<3;++i)
   {
      osc[i].reset(nullptr);
      osctype[i] = -1;
   }
}
