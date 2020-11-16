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

#include "SurgeStorage.h"
#include "Oscillator.h"
#include "SurgeParamConfig.h"
#include "effect/Effect.h"
#include <list>
#include <vt_dsp/vt_dsp_endian.h>
#include "MSEGModulationHelper.h"
#include "DebugHelpers.h"
#include "SkinModel.h"
#include "UserInteractions.h"
#include "version.h"

using namespace std;

SurgePatch::SurgePatch(SurgeStorage* storage)
{
   this->storage = storage;
   patchptr = nullptr;

   ParameterIDCounter p_id;
   {
      param_ptr.push_back(fx[4].return_level.assign(p_id.next(), 0, "volume_FX1", "Send FX 1 Return",
                                                    ct_amplitude,
                                                    Surge::Skin::Global::fx1_return,
                                                    0, cg_GLOBAL, 0, true,
                                                    Surge::ParamConfig::kHorizontal));
      param_ptr.push_back(fx[5].return_level.assign(p_id.next(), 0, "volume_FX2", "Send FX 2 Return",
                                                    ct_amplitude,
                                                     Surge::Skin::Global::fx2_return,
                                                    0, cg_GLOBAL, 0, true,
                                                    Surge::ParamConfig::kHorizontal));
      // TODO don't store in the patch ?
      param_ptr.push_back(volume.assign(
          p_id.next(), 0, "volume", "Global Volume", ct_decibel_attenuation,
          Surge::Skin::Global::master_volume,
          0, cg_GLOBAL, 0, true, Surge::ParamConfig::kHorizontal | kEasy));
   }
   param_ptr.push_back(scene_active.assign(p_id.next(), 0, "scene_active", "Active Scene", ct_scenesel,
                                           Surge::Skin::Global::active_scene,
                                           0, cg_GLOBAL, 0, false,
                                           Surge::ParamConfig::kHorizontal));
   param_ptr.push_back(scenemode.assign(p_id.next(), 0, "scenemode", "Scene Mode", ct_scenemode,
                                        Surge::Skin::Global::scene_mode,
                                        0, cg_GLOBAL, 0, false,
                                        Surge::ParamConfig::kHorizontal | kNoPopup));
   //param_ptr.push_back(scenemorph.assign(p_id.next(),0,"scenemorph","scenemorph",ct_percent,hmargin+gui_sec_width,gui_mid_topbar_y,0,0,0,false,Surge::ParamConfig::kHorizontal));

   param_ptr.push_back(splitpoint.assign(p_id.next(), 0, "splitkey", "Split Point", ct_midikey_or_channel,
                                       Surge::Skin::Scene::splitpoint,
                                        0, cg_GLOBAL, 0, false,
                                       Surge::ParamConfig::kHorizontal | kNoPopup));
   
   param_ptr.push_back(fx_disable.assign(p_id.next(), 0, "fx_disable", "FX Disable", ct_none,
                                          Surge::Skin::Global::fx_disable,
                                         0, cg_GLOBAL, 0, false));

   // shouldnt't be stored in the patch
   param_ptr.push_back(polylimit.assign(p_id.next(), 0, "polylimit", "Polyphony Limit", ct_polylimit,
                                        Surge::Skin::Scene::polylimit,
                                        0, cg_GLOBAL, 0, false,
                                        Surge::ParamConfig::kHorizontal | kNoPopup));
   param_ptr.push_back(fx_bypass.assign(p_id.next(), 0, "fx_bypass", "FX Bypass", ct_fxbypass,
                                        Surge::Skin::Global::fx_bypass,
                                        0, cg_GLOBAL, 0, false,
                                        Surge::ParamConfig::kHorizontal | kNoPopup));

   polylimit.val.i = 16;
   splitpoint.val.i = 60;
   volume.val.f = 0;

   for (int fx = 0; fx < 8; fx++)
   {
      param_ptr.push_back(this->fx[fx].type.assign(p_id.next(), 0, "type", "FX Type", ct_fxtype,
                                                   Surge::Skin::FX::fx_type,
                                                   0, cg_FX, fx, false, Surge::ParamConfig::kHorizontal));
      for (int p = 0; p < n_fx_params; p++)
      {
         auto conn = Surge::Skin::Connector::connectorByID("fx.param_" + std::to_string(p+1) );

         char label[16];
         sprintf(label, "p%i", p);
         char dawlabel[32];
         sprintf(dawlabel, "Param %i", p + 1);
         param_ptr.push_back(
             this->fx[fx].p[p].assign(p_id.next(), 0, label, dawlabel, ct_none,
                                      conn,
                                      0, cg_FX, fx,
                                      true, Surge::ParamConfig::kHorizontal | kHide | ((fx == 0) ? kEasy : 0)));
      }
   }

   ParameterIDCounter::promise_t globparams_promise = p_id.tail;
   ParameterIDCounter::promise_t scene_start_promise[n_scenes];

   for (int sc = 0; sc < n_scenes; sc++)
   {
      int sceasy =
          (sc == 0) ? kEasy
                    : 0; // only consider parameters in the first scene as non-expertmode parameters
      int id_s = 0;
      vector<Parameter*>* a;
      a = &param_ptr;

      int sc_id = (sc & 1) + 1; // probably needs to change in case we want to add more scenes?

      scene_start_promise[sc] = p_id.tail;

      a->push_back(scene[sc].octave.assign(p_id.next(), id_s++, "octave", "Octave", ct_pitch_octave,
                                           Surge::Skin::Scene::octave,
                                           sc_id, cg_GLOBAL, 0, false,
                                           Surge::ParamConfig::kHorizontal | kNoPopup));
      a->push_back(scene[sc].pitch.assign(p_id.next(), id_s++, "pitch", "Pitch", ct_pitch_semi7bp,
                                          Surge::Skin::Scene::pitch,
                                          sc_id, cg_GLOBAL, 0, true,
                                          kSemitone | sceasy));
      a->push_back(scene[sc].portamento.assign(p_id.next(), id_s++, "portamento", "Portamento",
                                               ct_portatime,
                                               Surge::Skin::Scene::portatime,
                                               sc_id, cg_GLOBAL, 0, true,
                                                sceasy));
      for (int osc = 0; osc < n_oscs; osc++)
      {
         // Initialize the display name here
         scene[sc].osc[osc].wavetable_display_name[0] = '\0';
         
         a->push_back(scene[sc].osc[osc].type.assign(p_id.next(), id_s++, "type", "Type", ct_osctype,
                                                     Surge::Skin::Osc::osc_type,
                                                     sc_id, cg_OSC, osc, false));
         a->push_back(scene[sc].osc[osc].octave.assign(p_id.next(), id_s++, "octave", "Octave",
                                                       ct_pitch_octave,
                                                       Surge::Skin::Osc::octave,
                                                       sc_id, cg_OSC, osc, false,  kNoPopup));
         a->push_back(scene[sc].osc[osc].pitch.assign(p_id.next(), id_s++, "pitch", "Pitch",
                                                      ct_pitch_semi7bp_absolutable,
                                                      Surge::Skin::Osc::pitch,
                                                      sc_id, cg_OSC, osc,
                                                      true,  kSemitone | sceasy));
         for (int i = 0; i < n_osc_params; i++)
         {
            char label[16];
            sprintf(label, "param%i", i);
            a->push_back(scene[sc].osc[osc].p[i].assign(p_id.next(), id_s++, label, "-", ct_none,
                                                        Surge::Skin::Connector::connectorByID("osc.param_" + std::to_string(i+1)),
                                                        sc_id, cg_OSC, osc, true,
                                                         ((i < 6) ? sceasy : 0)));
         }
         a->push_back(scene[sc].osc[osc].keytrack.assign(p_id.next(), id_s++, "keytrack", "Keytrack",
                                                         ct_bool_keytrack,
                                                         Surge::Skin::Osc::keytrack,
                                                         sc_id,
                                                         cg_OSC, osc, false));
         a->push_back(scene[sc].osc[osc].retrigger.assign(p_id.next(), id_s++, "retrigger", "Retrigger",
                                                          ct_bool_retrigger,
                                                          Surge::Skin::Osc::retrigger,
                                                          sc_id,
                                                          cg_OSC, osc, false));
      }
      a->push_back(scene[sc].polymode.assign(p_id.next(), id_s++, "polymode", "Play Mode", ct_polymode,
                                             Surge::Skin::Scene::playmode,
                                             sc_id, cg_GLOBAL,
                                             0, false, kNoPopup));
      a->push_back(scene[sc].fm_switch.assign(p_id.next(), id_s++, "fm_switch", "FM Routing",
                                              ct_fmconfig,
                                              Surge::Skin::Scene::fmrouting,
                                              sc_id,
                                              cg_GLOBAL, 0, false));
      a->push_back(scene[sc].fm_depth.assign(p_id.next(), id_s++, "fm_depth", "FM Depth",
                                             ct_decibel_fmdepth,
                                             Surge::Skin::Scene::fmdepth,
                                             sc_id, cg_GLOBAL,
                                             0, true, sceasy));

      a->push_back(scene[sc].drift.assign(p_id.next(), id_s++, "drift", "Osc Drift", ct_percent,
                                          Surge::Skin::Scene::drift,
                                          sc_id,
                                          cg_GLOBAL, 0, true));
      a->push_back(scene[sc].noise_colour.assign(
          p_id.next(), id_s++, "noisecol", "Noise Color", ct_percent_bidirectional,
          Surge::Skin::Scene::noise_color, sc_id, cg_GLOBAL, 0, true,
          sceasy));
      a->push_back(scene[sc].keytrack_root.assign(p_id.next(), id_s++, "ktrkroot", "Keytrack Root Key",
                                                  ct_midikey,
                                                  Surge::Skin::Scene::keytrack_root,
                                                  sc_id, cg_GLOBAL, 0, false));
      // ct_midikey
      // drift,keytrack_root

      a->push_back(scene[sc].volume.assign(p_id.next(), id_s++, "volume", "Volume", ct_amplitude_clipper,
                                           Surge::Skin::Scene::volume,
                                           sc_id, cg_GLOBAL, 0, true,
                                            sceasy));
      a->push_back(scene[sc].pan.assign(p_id.next(), id_s++, "pan", "Pan", ct_percent_bidirectional,
                                        Surge::Skin::Scene::pan,
                                        sc_id, cg_GLOBAL, 0, true,
                                         sceasy));
      a->push_back(scene[sc].width.assign(p_id.next(), id_s++, "pan2", "Width", ct_percent_bidirectional,
                                          Surge::Skin::Scene::width,
                                          sc_id, cg_GLOBAL, 0, true,
                                          sceasy));
      a->push_back(scene[sc].send_level[0].assign(p_id.next(), id_s++, "send_fx_1", "Send FX 1 Level",
                                                  ct_sendlevel,
                                                  Surge::Skin::Scene::send_fx_1,
                                                  sc_id, cg_GLOBAL, 0, true,
                                                   sceasy));
      a->push_back(scene[sc].send_level[1].assign(p_id.next(), id_s++, "send_fx_2", "Send FX 2 Level",
                                                  ct_sendlevel,
                                                  Surge::Skin::Scene::send_fx_2,
                                                  sc_id, cg_GLOBAL, 0, true,
                                                   sceasy));
      a->push_back(scene[sc].level_o1.assign(p_id.next(), id_s++, "level_o1", "Osc 1 Level", ct_amplitude,
                                             Surge::Skin::Mixer::level_o1,
                                             sc_id, cg_MIX, 0, true,
                                             sceasy));
      a->push_back(scene[sc].mute_o1.assign(p_id.next(), id_s++, "mute_o1", "Osc 1 Mute", ct_bool_mute,
                                            Surge::Skin::Mixer::mute_o1,
                                            sc_id, cg_MIX, 0, false));
      a->push_back(scene[sc].solo_o1.assign(p_id.next(), id_s++, "solo_o1", "Osc 1 Solo", ct_bool_solo,
                                            Surge::Skin::Mixer::solo_o1,
                                            sc_id, cg_MIX, 0, false, kMeta));
      a->push_back(scene[sc].route_o1.assign(p_id.next(), id_s++, "route_o1", "Osc 1 Route", ct_oscroute,
                                             Surge::Skin::Mixer::route_o1,
                                             sc_id, cg_MIX, 0, false));
      a->push_back(scene[sc].level_o2.assign(p_id.next(), id_s++, "level_o2", "Osc 2 Level", ct_amplitude,
                                             Surge::Skin::Mixer::level_o2,
                                             sc_id, cg_MIX, 0, true,
                                             sceasy));
      a->push_back(scene[sc].mute_o2.assign(p_id.next(), id_s++, "mute_o2", "Osc 2 Mute", ct_bool_mute,
                                            Surge::Skin::Mixer::mute_o2,
                                            sc_id, cg_MIX, 0, false));
      a->push_back(scene[sc].solo_o2.assign(p_id.next(), id_s++, "solo_o2", "Osc 2 Solo", ct_bool_solo,
                                            Surge::Skin::Mixer::solo_o2,
                                            sc_id, cg_MIX, 0, false, kMeta));
      a->push_back(scene[sc].route_o2.assign(p_id.next(), id_s++, "route_o2", "Osc 2 Route", ct_oscroute,
                                            Surge::Skin::Mixer::route_o2,
                                             sc_id, cg_MIX, 0, false));
      a->push_back(scene[sc].level_o3.assign(p_id.next(), id_s++, "level_o3", "Osc 3 Level", ct_amplitude,
                                             Surge::Skin::Mixer::level_o3,
                                             sc_id, cg_MIX, 0, true,
                                             sceasy));
      a->push_back(scene[sc].mute_o3.assign(p_id.next(), id_s++, "mute_o3", "Osc 3 Mute", ct_bool_mute,
                                            Surge::Skin::Mixer::mute_o3,
                                              sc_id, cg_MIX, 0, false));
      a->push_back(scene[sc].solo_o3.assign(p_id.next(), id_s++, "solo_o3", "Osc 3 Solo", ct_bool_solo,
                                            Surge::Skin::Mixer::solo_o3,
                                            sc_id, cg_MIX, 0, false, kMeta));
      a->push_back(scene[sc].route_o3.assign(p_id.next(), id_s++, "route_o3", "Osc 3 Route", ct_oscroute,
                                             Surge::Skin::Mixer::route_o3,
                                             sc_id, cg_MIX, 0, false));
      a->push_back(scene[sc].level_ring_12.assign(p_id.next(), id_s++, "level_ring12", "Ring Modulation 1x2 Level",
                                                  ct_amplitude,
                                                  Surge::Skin::Mixer::level_ring12,
                                                  sc_id, cg_MIX, 0, true,
                                                   sceasy));
      a->push_back(scene[sc].mute_ring_12.assign(p_id.next(), id_s++, "mute_ring12", "Ring Modulation 1x2 Mute",
                                                 ct_bool_mute,
                                                 Surge::Skin::Mixer::mute_ring12,
                                                 sc_id, cg_MIX, 0,
                                                 false));
      a->push_back(scene[sc].solo_ring_12.assign(p_id.next(), id_s++, "solo_ring12", "Ring Modulation 1x2 Solo",
                                                 ct_bool_solo,
                                                 Surge::Skin::Mixer::solo_ring12,
                                                 sc_id, cg_MIX, 0,
                                                 false, kMeta));
      a->push_back(scene[sc].route_ring_12.assign(p_id.next(), id_s++, "route_ring12", "Ring Modulation 1x2 Route",
                                                  ct_oscroute,
                                                  Surge::Skin::Mixer::route_ring12, sc_id, cg_MIX, 0,
                                                  false));
      a->push_back(scene[sc].level_ring_23.assign(p_id.next(), id_s++, "level_ring23", "Ring Modulation 2x3 Level",
                                                  ct_amplitude,
                                                  Surge::Skin::Mixer::level_ring23,
                                                  sc_id, cg_MIX, 0, true,
                                                  sceasy));
      a->push_back(scene[sc].mute_ring_23.assign(p_id.next(), id_s++, "mute_ring23", "Ring Modulation 2x3 Mute",
                                                 ct_bool_mute,
                                                 Surge::Skin::Mixer::mute_ring23,
                                                 sc_id, cg_MIX, 0,
                                                 false));
      a->push_back(scene[sc].solo_ring_23.assign(p_id.next(), id_s++, "solo_ring23", "Ring Modulation 2x3 Solo",
                                                 ct_bool_solo,
                                                 Surge::Skin::Mixer::solo_ring23,
                                                 sc_id, cg_MIX, 0,
                                                 false, kMeta));
      a->push_back(scene[sc].route_ring_23.assign(p_id.next(), id_s++, "route_ring23", "Ring Modulation 2x3 Route",
                                                  ct_oscroute,
                                                  Surge::Skin::Mixer::route_ring23, sc_id, cg_MIX, 0,
                                                  false));
      a->push_back(scene[sc].level_noise.assign(p_id.next(), id_s++, "level_noise", "Noise Level",
                                                ct_amplitude,
                                                Surge::Skin::Mixer::level_noise,
                                                sc_id, cg_MIX, 0, true,
                                                 sceasy));
      a->push_back(scene[sc].mute_noise.assign(p_id.next(), id_s++, "mute_noise", "Noise Mute",
                                               ct_bool_mute,
                                               Surge::Skin::Mixer::mute_noise, sc_id, cg_MIX, 0,
                                               false));
      a->push_back(scene[sc].solo_noise.assign(p_id.next(), id_s++, "solo_noise", "Noise Solo",
                                               ct_bool_solo,
                                               Surge::Skin::Mixer::solo_noise, sc_id, cg_MIX, 0, false,
                                               kMeta));
      a->push_back(scene[sc].route_noise.assign(p_id.next(), id_s++, "route_noise", "Noise Route",
                                                ct_oscroute,
                                                Surge::Skin::Mixer::route_noise, sc_id, cg_MIX, 0,
                                                false));
      a->push_back(scene[sc].level_pfg.assign(p_id.next(), id_s++, "level_pfg", "Pre-Filter Gain",
                                              ct_decibel,
                                              Surge::Skin::Mixer::level_prefiltergain, sc_id, cg_MIX, 0, true,
                                              sceasy));

      int pbx = 164, pby = 112;
      a->push_back(scene[sc].pbrange_up.assign(p_id.next(), id_s++, "pbrange_up",
                                               "Pitch Bend Up Range", ct_pbdepth,
                                               Surge::Skin::Scene::pbrange_up,
                                               sc_id, cg_GLOBAL, 0, true, kNoPopup));
      a->push_back(scene[sc].pbrange_dn.assign(p_id.next(), id_s++, "pbrange_dn",
                                               "Pitch Bend Down Range", ct_pbdepth,
                                               Surge::Skin::Scene::pbrange_dn,
                                               sc_id, cg_GLOBAL, 0, true, kNoPopup));

      a->push_back(scene[sc].vca_level.assign(p_id.next(), id_s++, "vca_level", "VCA Gain", ct_decibel,
                                              Surge::Skin::Scene::gain, sc_id, cg_GLOBAL, 0, true,
                                              sceasy));
      a->push_back(scene[sc].vca_velsense.assign(p_id.next(), id_s++, "vca_velsense", "Velocity > VCA Gain",
                                                 ct_decibel_attenuation,
                                                 Surge::Skin::Scene::vel_sensitivity, sc_id, cg_GLOBAL,
                                                 0, false));

      a->push_back(scene[sc].feedback.assign(p_id.next(), id_s++, "feedback", "Feedback",
                                             ct_percent_bidirectional,
                                             Surge::Skin::Filter::feedback, sc_id, cg_GLOBAL, 0,
                                             true, Surge::ParamConfig::kHorizontal | kWhite | sceasy));
      a->push_back(scene[sc].filterblock_configuration.assign(
          p_id.next(), id_s++, "fb_config", "Filter Configuration", ct_fbconfig,
          Surge::Skin::Filter::config,
          sc_id, cg_GLOBAL, 0, false));
      a->push_back(scene[sc].filter_balance.assign(
          p_id.next(), id_s++, "f_balance", "Filter Balance", ct_percent_bidirectional,
          Surge::Skin::Filter::balance, sc_id, cg_GLOBAL, 0, true, sceasy));

      a->push_back(scene[sc].lowcut.assign(p_id.next(), id_s++, "lowcut", "Highpass", ct_freq_hpf,
                                           Surge::Skin::Filter::highpass,
                                           sc_id, cg_GLOBAL, 0, true,
                                           sceasy));

      a->push_back(scene[sc].wsunit.type.assign(p_id.next(), id_s++, "ws_type", "Waveshaper Type",
                                                ct_wstype,
                                                Surge::Skin::Filter::waveshaper_type,
                                                sc_id, cg_GLOBAL, 0, false,
                                                kNoPopup));
      a->push_back(scene[sc].wsunit.drive.assign(
          p_id.next(), id_s++, "ws_drive", "Waveshaper Drive", ct_decibel_narrow_short_extendable,
          Surge::Skin::Filter::waveshaper_drive, sc_id, cg_GLOBAL, 0, true,
          sceasy));

      for (int f = 0; f < n_filterunits_per_scene; f++)
      {
         a->push_back(scene[sc].filterunit[f].type.assign(p_id.next(), id_s++, "type", "Type",
                                                          ct_filtertype,
                                                           Surge::Skin::Connector::connectorByID("filter.type_" + std::to_string(f+1) ), sc_id,
                                                          cg_FILTER, f, false
                                                          ));
         a->push_back(scene[sc].filterunit[f].subtype.assign(
             p_id.next(), id_s++, "subtype", "Subtype", ct_filtersubtype,
             Surge::Skin::Connector::connectorByID( "filter.subtype_" + std::to_string(f+1) ),  sc_id,
             cg_FILTER, f, false));
         a->push_back(scene[sc].filterunit[f].cutoff.assign(
             p_id.next(), id_s++, "cutoff", "Cutoff", ct_freq_audible,
             Surge::Skin::Connector::connectorByID( "filter.cutoff_" + std::to_string(f+1) ), sc_id, cg_FILTER, f, true,
             sceasy));
         if (f == 1)
            a->push_back(scene[sc].f2_cutoff_is_offset.assign(
                p_id.next(), id_s++, "f2_cf_is_offset", "Filter 2 Offset Mode", ct_bool_relative_switch,
                Surge::Skin::Filter::f2_offset_mode, sc_id, cg_GLOBAL, 0, false, kMeta));
         a->push_back(scene[sc].filterunit[f].resonance.assign(
             p_id.next(), id_s++, "resonance", "Resonance", ct_percent,
             Surge::Skin::Connector::connectorByID( "filter.resonance_" + std::to_string(f+1) ),
               sc_id, cg_FILTER, f,
             true, Surge::ParamConfig::kHorizontal | sceasy));
         if (f == 1)
            a->push_back(scene[sc].f2_link_resonance.assign(
                p_id.next(), id_s++, "f2_link_resonance", "Link Resonance", ct_bool_link_switch,
                Surge::Skin::Filter::f2_link_resonance,
                sc_id, cg_GLOBAL, 0, false, kMeta));

         a->push_back(scene[sc].filterunit[f].envmod.assign(
             p_id.next(), id_s++, "envmod", "FEG Mod Amount", ct_freq_mod,
             Surge::Skin::Connector::connectorByID( "filter.envmod_" + std::to_string(f+1) ), sc_id,
             cg_FILTER, f, true, sceasy));
         a->push_back(scene[sc].filterunit[f].keytrack.assign(
             p_id.next(), id_s++, "keytrack", "Keytrack", ct_percent_bidirectional,
             Surge::Skin::Connector::connectorByID( "filter.keytrack_" + std::to_string(f+1)), sc_id,
             cg_FILTER, f, true));
      }

      // scene[sc].filterunit[0].type.val.i = 1;
      for (int e = 0; e < 2; e++)   // 2 = we have two envelopes, filter and amplifier
      {
         std::string envs = "aeg.";
         if( e == 1 )
            envs = "feg.";

         /*
          * Since the connectors are in a namespace and here we have a loop over two
          * otherwise identical types, we make the choice to look up the connector by the
          * ID rather than put the direct external reference in here
          */
         auto getCon = [this, envs](std::string sub)
         {
            return Surge::Skin::Connector::connectorByID(envs + sub);
         };

         a->push_back(scene[sc].adsr[e].a.assign(p_id.next(), id_s++, "attack", "Attack", ct_envtime,
                                                 getCon( "attack" ), sc_id, cg_ENV, e, true,
                                                 Surge::ParamConfig::kVertical | kWhite | sceasy));
         a->push_back(scene[sc].adsr[e].a_s.assign(p_id.next(), id_s++, "attack_shape", "Attack Shape",
                                                   ct_envshape_attack,
                                                   getCon( "attack_shape" ), sc_id, cg_ENV, e,
                                                   false, kNoPopup));
         a->push_back(scene[sc].adsr[e].d.assign(p_id.next(), id_s++, "decay", "Decay", ct_envtime,
                                                 getCon( "decay" ), sc_id, cg_ENV, e, true,
                                                 Surge::ParamConfig::kVertical | kWhite | sceasy));
         a->push_back(scene[sc].adsr[e].d_s.assign(p_id.next(), id_s++, "decay_shape", "Decay Shape",
                                                   ct_envshape, getCon( "decay_shape" ), sc_id, cg_ENV, e,
                                                   false, kNoPopup));
         a->push_back(scene[sc].adsr[e].s.assign(p_id.next(), id_s++, "sustain", "Sustain", ct_percent,
                                                 getCon( "sustain" ), sc_id, cg_ENV, e, true,
                                                 Surge::ParamConfig::kVertical | kWhite | sceasy));
         a->push_back(scene[sc].adsr[e].r.assign(p_id.next(), id_s++, "release", "Release", ct_envtime,
                                                 getCon( "release" ), sc_id, cg_ENV, e, true,
                                                 Surge::ParamConfig::kVertical | kWhite | sceasy));
         a->push_back(scene[sc].adsr[e].r_s.assign(p_id.next(), id_s++, "release_shape", "Release Shape",
                                                   ct_envshape, getCon( "release_shape" ), sc_id, cg_ENV, e,
                                                   false, kNoPopup));
         a->push_back(scene[sc].adsr[e].mode.assign(p_id.next(), id_s++, "mode", "Envelope Mode", ct_envmode,
                                                    getCon( "mode" ),  sc_id, cg_ENV, e, false,
                                                    kNoPopup));
      }

      for (int l = 0; l < n_lfos; l++)
      {
         char label[32];

         sprintf(label, "lfo%i_shape", l);
         a->push_back(scene[sc].lfo[l].shape.assign(p_id.next(), id_s++, label, "Type", ct_lfotype,
                                                    Surge::Skin::LFO::shape, sc_id, cg_LFO, ms_lfo1 + l));

         sprintf(label, "lfo%i_rate", l);
         a->push_back(scene[sc].lfo[l].rate.assign(p_id.next(), id_s++, label, "Rate", ct_lforate_deactivatable,
                                                   Surge::Skin::LFO::rate, sc_id, cg_LFO, ms_lfo1 + l, true,
                                                   sceasy, false));
         sprintf(label, "lfo%i_phase", l);
         a->push_back(scene[sc].lfo[l].start_phase.assign(p_id.next(), id_s++, label, "Phase / Shuffle",
                                                          ct_percent,
                                                          Surge::Skin::LFO::phase,
                                                          // Surge::Skin::LfoSection::phase,
                                                          sc_id, cg_LFO,
                                                          ms_lfo1 + l, true));
         sprintf(label, "lfo%i_magnitude", l);
         a->push_back(scene[sc].lfo[l].magnitude.assign(p_id.next(), id_s++, label, "Amplitude",
                                                        ct_lfoamplitude,
                                                        Surge::Skin::LFO::amplitude, sc_id, cg_LFO,
                                                        ms_lfo1 + l, true,
                                                        sceasy));
         sprintf(label, "lfo%i_deform", l);
         a->push_back(scene[sc].lfo[l].deform.assign(p_id.next(), id_s++, label, "Deform",
                                                     ct_lfodeform,
                                                     Surge::Skin::LFO::deform, sc_id,
                                                     cg_LFO, ms_lfo1 + l, true));

         sprintf(label, "lfo%i_trigmode", l);
         a->push_back(scene[sc].lfo[l].trigmode.assign(p_id.next(), id_s++, label, "Trigger Mode",
                                                       ct_lfotrigmode,
                                                       Surge::Skin::LFO::trigger_mode, sc_id,
                                                       cg_LFO, ms_lfo1 + l, false,
                                                       kNoPopup));
         sprintf(label, "lfo%i_unipolar", l);
         a->push_back(scene[sc].lfo[l].unipolar.assign(p_id.next(), id_s++, label, "Unipolar",
                                                       ct_bool_unipolar,
                                                       Surge::Skin::LFO::unipolar, sc_id,
                                                       cg_LFO, ms_lfo1 + l, false));

         sprintf(label, "lfo%i_delay", l);
         a->push_back(scene[sc].lfo[l].delay.assign(p_id.next(), id_s++, label, "Delay", ct_envtime,
                                                    Surge::Skin::LFO::delay, sc_id, cg_LFO, ms_lfo1 + l, true,
                                                    kMini));
         sprintf(label, "lfo%i_attack", l);
         a->push_back(scene[sc].lfo[l].attack.assign(p_id.next(), id_s++, label, "Attack", ct_envtime,
                                                     Surge::Skin::LFO::attack, sc_id, cg_LFO, ms_lfo1 + l, true,
                                                     kMini));
         sprintf(label, "lfo%i_hold", l);
         a->push_back(scene[sc].lfo[l].hold.assign(p_id.next(), id_s++, label, "Hold", ct_envtime,
                                                   Surge::Skin::LFO::hold, sc_id, cg_LFO, ms_lfo1 + l, true,
                                                   kMini));
         sprintf(label, "lfo%i_decay", l);
         a->push_back(scene[sc].lfo[l].decay.assign(p_id.next(), id_s++, label, "Decay", ct_envtime,
                                                    Surge::Skin::LFO::decay, sc_id, cg_LFO, ms_lfo1 + l, true,
                                                    kMini));
         sprintf(label, "lfo%i_sustain", l);
         a->push_back(scene[sc].lfo[l].sustain.assign(p_id.next(), id_s++, label, "Sustain", ct_percent,
                                                      Surge::Skin::LFO::sustain, sc_id, cg_LFO, ms_lfo1 + l, true,
                                                      kMini));
         sprintf(label, "lfo%i_release", l);
         a->push_back(scene[sc].lfo[l].release.assign(p_id.next(), id_s++, label, "Release",
                                                      ct_envtime_lfodecay,
                                                      Surge::Skin::LFO::release, sc_id, cg_LFO,
                                                      ms_lfo1 + l, true,
                                                      kMini));
      }
   }

   param_ptr.push_back(character.assign(p_id.next(), 0, "character", "Character", ct_character,
                                        Surge::Skin::Global::character, 0, cg_GLOBAL, 0, false, kNoPopup));

   // Resolve the promise chain
   p_id.resolve();
   for (unsigned int i = 0; i < param_ptr.size(); i++)
   {
      param_ptr[i]->id = param_ptr[i]->id_promise->value;
      param_ptr[i]->id_promise = nullptr; // we only hold it transiently since we have a direct pointer copy to keep the class size fixed

      /*
      ** Give the param a weak pointer to the storage, since we know this patch will last the
      ** lifetime of the storage which created it. See comment in Parameter.h
      */
      param_ptr[i]->storage = storage;
   }

   scene_start[0] = scene_start_promise[0]->value;
   scene_start[1] = scene_start_promise[1]->value;

   scene_size = scene_start[1] - scene_start[0];
   assert(scene_size == n_scene_params);
   assert(globparams_promise->value == n_global_params);
   init_default_values();
   update_controls(true);

   // Build indexed list for the non-expert parameters
   for (unsigned int i = 0; i < param_ptr.size(); i++)
   {
      if (param_ptr[i]->ctrlstyle & kEasy)
         easy_params_id.push_back(i);
   }
#if 0
   // DEBUG CODE WHICH WILL DIE
   std::map<std::string, int> idToParam;
   for( auto p : param_ptr )
   {
      auto uiid = p->ui_identifier;
      if( idToParam.find( uiid ) == idToParam.end() )
      {
         idToParam[uiid] = p->id;
      }
   }
   auto pns = std::string();
   std::ostringstream hdr, body;
   auto hdrpre = std::string();
   for( auto ni : idToParam )
   {
      auto n = ni.first;
      auto dp = n.find( "." );
      auto ns = n.substr( 0, dp );
      auto la = n.substr( dp + 1 );
      auto p = param_ptr[ni.second];
      if( ns != pns )
      {
         if( pns != "" )
         {
            hdr << ";\n   }\n"; body << "   }\n";
         }
         pns = ns;
         hdr << "   namespace " << ns << " {\n";
         body << "  namespace " << ns << " {\n";
         hdrpre = "      extern Surge::Skin::Connector ";
      }
      hdr << hdrpre << la;
      hdrpre = ", ";
      body << "      Connector " << la << "( \"" << p->ui_identifier << "\", " << p->posx << ", " << p->posy << ", Connector::SLIDER );\n";
      //std::cout << ns << "/" << la << " " << p->posx << " " << p->posy  << std::endl;
   }
   hdr << ";\n   }\n"; body << "   }\n";

   std::cout << hdr.str() << "\n" << body.str() << std::endl;
#endif
}

void SurgePatch::init_default_values()
{
   // reset all parameters to their default value (could be zero, then zeroes never would have to be
   // saved in xml) will make it all more predictable
   int n = param_ptr.size();
   for (int i = 0; i < n; i++)
   {
      if ((i != volume.id) && (i != fx_bypass.id) && (i != polylimit.id))
      {
         param_ptr[i]->val.i = param_ptr[i]->val_default.i;
         param_ptr[i]->clear_flags();
      }
   }

   character.val.i = 1;

   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (auto& osc : scene[sc].osc)
      {
         osc.type.val.i = 0;
         osc.queue_xmldata = 0;
         osc.queue_type = -1;
         osc.keytrack.val.b = true;
         osc.retrigger.val.b = false;
         osc.wt.queue_id = 0;
         osc.wt.queue_filename[0] = 0;
      }
      scene[sc].fm_depth.val.f = -24.f;
      scene[sc].portamento.val.f = scene[sc].portamento.val_min.f;
      scene[sc].keytrack_root.val.i = 60;
      scene[sc].send_level[0].val.f = scene[sc].send_level[0].val_min.f;
      scene[sc].send_level[1].val.f = scene[sc].send_level[1].val_min.f;
      scene[sc].send_level[0].per_voice_processing = false;
      scene[sc].send_level[1].per_voice_processing = false;
      scene[sc].volume.val.f = 0.890899f;
      scene[sc].width.val.f = 1.f; // width

      scene[sc].mute_o2.val.b = true;
      scene[sc].mute_o3.val.b = true;
      scene[sc].mute_noise.val.b = true;
      scene[sc].mute_ring_12.val.b = true;
      scene[sc].mute_ring_23.val.b = true;
      scene[sc].route_o1.val.i = 1;
      scene[sc].route_o2.val.i = 1;
      scene[sc].route_o3.val.i = 1;
      scene[sc].route_ring_12.val.i = 1;
      scene[sc].route_ring_23.val.i = 1;
      scene[sc].route_noise.val.i = 1;
      scene[sc].pbrange_up.val.i = 2.f;
      scene[sc].pbrange_dn.val.i = 2.f;
      scene[sc].lowcut.val.f = scene[sc].lowcut.val_min.f;
      scene[sc].lowcut.deactivated = false;
      scene[sc].lowcut.per_voice_processing = false;

      scene[sc].adsr[0].a.val.f = scene[sc].adsr[0].a.val_min.f;
      scene[sc].adsr[0].a.val_default.f = scene[sc].adsr[0].a.val_min.f;
      scene[sc].adsr[0].d.val.f = -2;
      scene[sc].adsr[0].d.val_default.f = -2;
      scene[sc].adsr[0].r.val.f = -5;
      scene[sc].adsr[0].r.val_default.f = -5;
      scene[sc].adsr[0].s.val.f = 1;
      scene[sc].adsr[0].s.val_default.f = 1;
      scene[sc].adsr[0].a_s.val.i = 1;
      scene[sc].adsr[0].d_s.val.i = 1;
      scene[sc].adsr[0].r_s.val.i = 2;
      scene[sc].adsr[1].a.val.f = scene[sc].adsr[1].a.val_min.f;
      scene[sc].adsr[1].a.val_default.f = scene[sc].adsr[1].a.val_min.f;
      scene[sc].adsr[1].d.val.f = -2;
      scene[sc].adsr[1].d.val_default.f = -2;
      scene[sc].adsr[1].s.val.f = 0;
      scene[sc].adsr[1].s.val_default.f = 0;
      scene[sc].adsr[1].r.val.f = -2;
      scene[sc].adsr[1].r.val_default.f = -2;
      scene[sc].adsr[1].a_s.val.i = 1;
      scene[sc].adsr[1].d_s.val.i = 1;
      scene[sc].adsr[1].r_s.val.i = 1;

      for (int l = 0; l < n_lfos; l++)
      {
         scene[sc].lfo[l].rate.deactivated = false;
         scene[sc].lfo[l].magnitude.val.f = 1.f;
         scene[sc].lfo[l].magnitude.val_default.f = 1.f;
         scene[sc].lfo[l].trigmode.val.i = 1;
         scene[sc].lfo[l].delay.val.f = scene[sc].lfo[l].delay.val_min.f;
         scene[sc].lfo[l].delay.val_default.f = scene[sc].lfo[l].delay.val_min.f;
         scene[sc].lfo[l].attack.val.f = scene[sc].lfo[l].attack.val_min.f;
         scene[sc].lfo[l].attack.val_default.f = scene[sc].lfo[l].attack.val_min.f;
         scene[sc].lfo[l].hold.val.f = scene[sc].lfo[l].hold.val_min.f;
         scene[sc].lfo[l].decay.val.f = 0.f;
         scene[sc].lfo[l].sustain.val.f = scene[sc].lfo[l].sustain.val_max.f;
         scene[sc].lfo[l].sustain.val_default.f = scene[sc].lfo[l].sustain.val_max.f;
         scene[sc].lfo[l].release.val.f = scene[sc].lfo[l].release.val_max.f;
         scene[sc].lfo[l].release.val_default.f = scene[sc].lfo[l].release.val_max.f;

         for (int i = 0; i < n_stepseqsteps; i++)
         {
            stepsequences[sc][l].steps[i] = 0.f;
         }
         stepsequences[sc][l].trigmask = 0;
         stepsequences[sc][l].loop_start = 0;
         stepsequences[sc][l].loop_end = 15;
         stepsequences[sc][l].shuffle = 0.f;
      }
   }

   for (int i = 0; i < n_customcontrollers; i++)
   {
      strcpy(CustomControllerLabel[i], "-");
   }
}

SurgePatch::~SurgePatch()
{
   free(patchptr);
}

void SurgePatch::copy_scenedata(pdata* d, int scene)
{
   int s = scene_start[scene];
   for (int i = 0; i < n_scene_params; i++)
   {
      // if (param_ptr[i+s]->valtype == vt_float)
      // d[i].f = param_ptr[i+s]->val.f;
      d[i].i = param_ptr[i + s]->val.i;
   }

   /*for(int i=0; i<(n_scene_params & 0xfff8); i+=8)
   {
           d[i].i = param_ptr[i+s]->val.i;
           d[i+1].i = param_ptr[i+s+1]->val.i;
           d[i+2].i = param_ptr[i+s+2]->val.i;
           d[i+3].i = param_ptr[i+s+3]->val.i;
           d[i+4].i = param_ptr[i+s+4]->val.i;
           d[i+5].i = param_ptr[i+s+5]->val.i;
           d[i+6].i = param_ptr[i+s+6]->val.i;
           d[i+7].i = param_ptr[i+s+7]->val.i;
   }
   for(int i=(n_scene_params & 0xfff8); i<n_scene_params; i++)
   {
           d[i].i = param_ptr[i+s]->val.i;
   }*/
}

void SurgePatch::copy_globaldata(pdata* d)
{
   for (int i = 0; i < n_global_params; i++)
   {
      // if (param_ptr[i]->valtype == vt_float)
      d[i].i = param_ptr[i]->val.i; // int is safer (no exceptions or anything)
   }
}
// pdata scenedata[n_scenes][n_scene_params];

void SurgePatch::update_controls(bool init,
                                 void* init_osc, // init_osc is the pointer to the data structure of a particular osc to init
                                 bool from_streaming // we are loading from a patch
    ) 
{
   int sn = 0;
   for (auto& sc : scene)
   {
      for (int osc = 0; osc < n_oscs; osc++)
      {
         for (int i = 0; i < n_osc_params; i++)
            sc.osc[osc].p[i].set_type(ct_none);

         Oscillator* t_osc = spawn_osc(sc.osc[osc].type.val.i, nullptr, &sc.osc[osc], nullptr);
         if (t_osc)
         {
            t_osc->init_ctrltypes(sn,osc);
            if (from_streaming)
                t_osc->handleStreamingMismatches( streamingRevision, currentSynthStreamingRevision );
            if (init || (init_osc == &sc.osc[osc]))
            {
                t_osc->init_default_values();
            }
            delete t_osc;
         }
      }
      sn++;
   }

   if( from_streaming )
   {
       for( int i=0; i<8; ++i )
       {
           if(fx[i].type.val.i != fxt_off)
           {
               Effect *t_fx = spawn_effect(fx[i].type.val.i, nullptr, &(fx[i]), nullptr );
               t_fx->init_ctrltypes();
               t_fx->handleStreamingMismatches(streamingRevision, currentSynthStreamingRevision );
               delete t_fx;
           }
       }
   }
}

void SurgePatch::do_morph()
{
   int s = scene_start[0];
   int n = scene_start[1] - scene_start[0];
   for (int i = 0; i < n; i++)
   {
      //		copy_ptr[i]->morph(copy_ptr[i+n],scenemorph.val.f);
      scenedata[0][i] = param_ptr[s + i]->morph(param_ptr[s + i + n], scenemorph.val.f);
   }
}

#pragma pack(push, 1)
struct patch_header
{
   char tag[4];
   unsigned int xmlsize, wtsize[2][3];  // TODO: FIX SCENE AND OSC COUNT ASSUMPTION (but also since it's used in streaming, do it with care!)
};
#pragma pack(pop)


// BASE 64 SUPPORT. THANKS https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";


static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i)
    {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = ( char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
        
    }
    
    return ret;
}

std::string base64_decode(std::string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    
    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            
            char_array_3[0] = ( char_array_4[0] << 2       ) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) +   char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) {
        for (j = 0; j < i; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    
    return ret;
}

void SurgePatch::load_patch(const void* data, int datasize, bool preset)
{
   if (datasize <= 4)
      return;
   assert(datasize);
   assert(data);
   void* end = (char*)data + datasize;
   patch_header* ph = (patch_header*)data;
   ph->xmlsize = vt_read_int32LE(ph->xmlsize);

   if (!memcmp(ph->tag, "sub3", 4))
   {
      char* dr = (char*)data + sizeof(patch_header);
      load_xml(dr, ph->xmlsize, preset);
      dr += ph->xmlsize;

      for (int sc = 0; sc < n_scenes; sc++)
      {
         for (int osc = 0; osc < n_oscs; osc++)
         {
            ph->wtsize[sc][osc] = vt_read_int32LE(ph->wtsize[sc][osc]);
            if (ph->wtsize[sc][osc])
            {
               wt_header* wth = (wt_header*)dr;
               if (wth > end)
                  return;

               scene[sc].osc[osc].wt.queue_id = -1;
               scene[sc].osc[osc].wt.queue_filename[0] = 0;
               scene[sc].osc[osc].wt.current_id = -1;

               void* d = (void*)((char*)dr + sizeof(wt_header));

               storage->waveTableDataMutex.lock();
               scene[sc].osc[osc].wt.BuildWT(d, *wth, false);
               if( scene[sc].osc[osc].wavetable_display_name[0] == '\0' )
               {
                  if (scene[sc].osc[osc].wt.flags & wtf_is_sample)
                  {
                     strncpy(scene[sc].osc[osc].wavetable_display_name, "(Patch Sample)", 256);
                  }
                  else
                  {
                     strncpy(scene[sc].osc[osc].wavetable_display_name, "(Patch Wavetable)", 256);
                  }
               }
               storage->waveTableDataMutex.unlock();

               dr += ph->wtsize[sc][osc];
            }
         }
      }
   }
   else
   {
      load_xml(data, datasize, preset);
   }
}

unsigned int SurgePatch::save_patch(void** data)
{
   size_t psize = 0;
   // void **xmldata = new void*();
   void* xmldata = 0;
   patch_header header;

   memcpy(header.tag, "sub3", 4);
   size_t xmlsize = save_xml(&xmldata);
   header.xmlsize = vt_write_int32LE(xmlsize);
   wt_header wth[n_scenes][n_oscs];
   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (int osc = 0; osc < n_oscs; osc++)
      {
         if (uses_wavetabledata(scene[sc].osc[osc].type.val.i))
         {
            memset(wth[sc][osc].tag, 0, 4);
            wth[sc][osc].n_samples = scene[sc].osc[osc].wt.size;
            wth[sc][osc].n_tables = scene[sc].osc[osc].wt.n_tables;
            wth[sc][osc].flags = scene[sc].osc[osc].wt.flags | wtf_int16;
            unsigned int wtsize =
                wth[sc][osc].n_samples * scene[sc].osc[osc].wt.n_tables * sizeof(short) +
                sizeof(wt_header);
            header.wtsize[sc][osc] = vt_write_int32LE(wtsize);
            psize += wtsize;
         }
         else
            header.wtsize[sc][osc] = 0;
      }
   }
   psize += xmlsize + sizeof(patch_header);
   if (patchptr)
      free(patchptr);
   patchptr = malloc(psize);
   char* dw = (char*)patchptr;
   *data = patchptr;
   memcpy(dw, &header, sizeof(patch_header));
   dw += sizeof(patch_header);
   memcpy(dw, xmldata, xmlsize);
   dw += xmlsize;
   free(xmldata);

   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (int osc = 0; osc < n_oscs; osc++)
      {
         if (header.wtsize[sc][osc])
         {
            size_t wtsize = vt_read_int32LE(header.wtsize[sc][osc]);
            int n_tables = wth[sc][osc].n_tables;
            int n_samples = wth[sc][osc].n_samples;

            // do all endian swapping for the wavetables in one place (for ppc)
            wth[sc][osc].n_samples = vt_write_int32LE(wth[sc][osc].n_samples);
            wth[sc][osc].n_tables = vt_write_int16LE(wth[sc][osc].n_tables);
            wth[sc][osc].flags = vt_write_int16LE(wth[sc][osc].flags | wtf_int16);

            memcpy(dw, &wth[sc][osc], sizeof(wt_header));
            short* fp = (short*)(char*)(dw + sizeof(wt_header));

            for (int j = 0; j < n_tables; j++)
            {
               vt_copyblock_W_LE(&fp[j * n_samples],
                                 &scene[sc].osc[osc].wt.TableI16WeakPointers[0][j][FIRoffsetI16],
                                 n_samples);
            }
            dw += wtsize;
         }
      }
   }
   return psize;
}

float convert_v11_reso_to_v12_2P(float reso)
{
   float Qinv =
       (1.0f - 0.99f * limit_range((float)(1.f - (1.f - reso) * (1.f - reso)), 0.0f, 1.0f));
   // return 1.0f - sqrt(1.0f - Qinv / 1.05f);
   return 1.f - sqrt(1.f - ((1.0f - Qinv) / 1.05f));
}

float convert_v11_reso_to_v12_4P(float reso)
{
   return reso * (0.99f / 1.05f);
}

void SurgePatch::load_xml(const void* data, int datasize, bool is_preset)
{
   TiXmlDocument doc;
   int j;
   double d;
   if (datasize)
   {
      assert(datasize < (1 << 22)); // something is weird if the patch is this big
      char* temp = (char*)malloc(datasize + 1);
      memcpy(temp, data, datasize);
      *(temp + datasize) = 0;
      // std::cout << "XML DOC is " << temp << std::endl;
      doc.Parse(temp, nullptr, TIXML_ENCODING_LEGACY);
      free(temp);
   }

   // clear old routings
   for (int sc = 0; sc < n_scenes; sc++)
   {
      scene[sc].modulation_scene.clear();
      scene[sc].modulation_voice.clear();
   }
   modulation_global.clear();

   for (auto& i : fx)
      i.type.val.i = fxt_off;

   TiXmlElement* patch = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("patch"));
   if (!patch)
      return;

   int revision = 0;
   patch->QueryIntAttribute("revision", &revision);
   streamingRevision = revision;
   currentSynthStreamingRevision = ff_revision;

   if( revision > ff_revision )
   {
      std::ostringstream oss;
      oss << "The version of Surge you are running is older than the version with which this patch "
          << "was created. Your version of surge (" << Surge::Build::FullVersionStr << ") has a "
          << "streaming revision of " << ff_revision << ", whereas the patch you are loading was "
          << "created with " << revision << ". Features of the patch will not be available in your "
          << "session. You can always find the latest Surge at https://surge-synthesizer.github.io/";
      Surge::UserInteractions::promptError( oss.str(), "Surge Version is Older than Patch" );
   }
   
   TiXmlElement* meta =  TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("meta"));
   if (meta)
   {
      const char* s;
      if (!is_preset)
      {
         s = meta->Attribute("name");
         if (s)
            name = s;
         s = meta->Attribute("category");
         if (s)
            category = s;
      }

      s = meta->Attribute("comment");
      if (s)
         comment = s;
      s = meta->Attribute("author");
      if (s)
         author = s;
   }

   TiXmlElement* parameters =  TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("parameters"));
   assert(parameters);
   int n = param_ptr.size();

   // delete volume & fx_bypass if it's a preset. Those settings should stick
   if (is_preset)
   {
      TiXmlElement* tp = TINYXML_SAFE_TO_ELEMENT(parameters->FirstChild("volume"));
      if (tp)
         parameters->RemoveChild(tp);
      tp = TINYXML_SAFE_TO_ELEMENT(parameters->FirstChild("fx_bypass"));
      if (tp)
         parameters->RemoveChild(tp);
      tp = TINYXML_SAFE_TO_ELEMENT(parameters->FirstChild("polylimit"));
      if (tp)
         parameters->RemoveChild(tp);
   }

   TiXmlElement* p;
   for (int i = 0; i < n; i++)
   {
      if (!i)
         p = TINYXML_SAFE_TO_ELEMENT(parameters->FirstChild(param_ptr[i]->get_storage_name()));
      else
      {
         if (p)
            p = TINYXML_SAFE_TO_ELEMENT(p->NextSibling(param_ptr[i]->get_storage_name()));
         if (!p)
            p = TINYXML_SAFE_TO_ELEMENT(parameters->FirstChild(param_ptr[i]->get_storage_name()));
      }
      if (p)
      {
         int type;
         if (!(p->QueryIntAttribute("type", &type) == TIXML_SUCCESS))
            type = param_ptr[i]->valtype;

         if (type == (valtypes)vt_float)
         {
            if (p->QueryDoubleAttribute("value", &d) == TIXML_SUCCESS)
               param_ptr[i]->set_storage_value((float)d);
            else
               param_ptr[i]->val.f = param_ptr[i]->val_default.f;
         }
         else
         {
            if (p->QueryIntAttribute("value", &j) == TIXML_SUCCESS)
               param_ptr[i]->set_storage_value(j);
            else
               param_ptr[i]->val.i = param_ptr[i]->val_default.i;
         }

         if ((p->QueryIntAttribute("temposync", &j) == TIXML_SUCCESS) && (j == 1))
            param_ptr[i]->temposync = true;
         else
            param_ptr[i]->temposync = false;

         if ((p->QueryIntAttribute("porta_const_rate", &j) == TIXML_SUCCESS))
         {
            if (j == 1)
               param_ptr[i]->porta_constrate = true;
            else
               param_ptr[i]->porta_constrate = false;
         }
         else
         {
            if (param_ptr[i]->has_portaoptions())
               param_ptr[i]->porta_constrate = false;
         }

         if ((p->QueryIntAttribute("porta_gliss", &j) == TIXML_SUCCESS))
         {
            if (j == 1)
               param_ptr[i]->porta_gliss = true;
            else
               param_ptr[i]->porta_gliss = false;
         }
         else
         {
            if (param_ptr[i]->has_portaoptions())
               param_ptr[i]->porta_gliss = false;
         }

         if ((p->QueryIntAttribute("porta_retrigger", &j) == TIXML_SUCCESS))
         {
            if (j == 1)
               param_ptr[i]->porta_retrigger = true;
            else
               param_ptr[i]->porta_retrigger = false;
         }
         else
         {
            if (param_ptr[i]->has_portaoptions())
               param_ptr[i]->porta_retrigger = false;
         }

         if ((p->QueryIntAttribute("porta_curve", &j) == TIXML_SUCCESS))
         {
            switch (j)
            {
            case porta_log:
            case porta_lin:
            case porta_exp:
               param_ptr[i]->porta_curve = j;
               break;
            }
         }
         else
         {
            if (param_ptr[i]->has_portaoptions())
               param_ptr[i]->porta_curve = porta_lin;
         }


         if ((p->QueryIntAttribute("deform_type", &j) == TIXML_SUCCESS))
            param_ptr[i]->deform_type = j;
         else
         {
            if (param_ptr[i]->has_deformoptions())
               param_ptr[i]->deform_type = type_1;
         }


         if ((p->QueryIntAttribute("deactivated", &j) == TIXML_SUCCESS))
         {
            if(j == 1)
               param_ptr[i]->deactivated = true;
            else
               param_ptr[i]->deactivated = false;
         }
         else
         {
            if( param_ptr[i]->can_deactivate() )
            {
               if ((param_ptr[i]->ctrlgroup == cg_LFO) || // this is the LFO rate special case
                   (param_ptr[i]->ctrlgroup == cg_GLOBAL && param_ptr[i]->ctrltype == ct_freq_hpf)) // this is the global highpass special case 
                  param_ptr[i]->deactivated = false;
               else
                  param_ptr[i]->deactivated = true;
            }
         }

         if ((p->QueryIntAttribute("extend_range", &j) == TIXML_SUCCESS) && (j == 1))
            param_ptr[i]->extend_range = true;
         else
            param_ptr[i]->extend_range = false;

         if ((p->QueryIntAttribute("absolute", &j) == TIXML_SUCCESS) && (j == 1))
            param_ptr[i]->absolute = true;
         else
            param_ptr[i]->absolute = false;

         int sceneId = param_ptr[i]->scene;
         int paramIdInScene = param_ptr[i]->param_id_in_scene;
         TiXmlElement* mr = TINYXML_SAFE_TO_ELEMENT(p->FirstChild("modrouting"));
         while (mr)
         {
            int modsource;
            double depth;
            if ((mr->QueryIntAttribute("source", &modsource) == TIXML_SUCCESS) &&
                (mr->QueryDoubleAttribute("depth", &depth) == TIXML_SUCCESS))
            {
               if (revision < 9)
               {
                  if (modsource > ms_ctrl7)
                     modsource++;
                  // make room for ctrl8 in old patches
               }

               vector<ModulationRouting>* modlist = nullptr;

               if (sceneId != 0)
               {
                  if (isScenelevel((modsources)modsource))
                     modlist = &scene[sceneId - 1].modulation_scene;
                  else
                     modlist = &scene[sceneId - 1].modulation_voice;
               }
               else
               {
                  modlist = &modulation_global;
               }

               ModulationRouting t;
               t.depth = (float)depth;
               t.source_id = modsource;

               if (sceneId != 0)
                  t.destination_id = paramIdInScene;
               else
                  t.destination_id = i;

               modlist->push_back(t);
            }
            mr = TINYXML_SAFE_TO_ELEMENT(mr->NextSibling("modrouting"));
         }
      }
   }

   if (scene[0].pbrange_up.val.i & 0xffffff00) // is outside range, it must have been save
   {
      for (int sc; sc < n_scenes; sc++)
      {
         scene[sc].pbrange_up.val.i = (int)scene[sc].pbrange_up.val.f;
         scene[sc].pbrange_dn.val.i = (int)scene[sc].pbrange_dn.val.f;
      }
   }

   if (revision < 1)
   {
      for (int sc; sc < n_scenes; sc++)
      {
         scene[sc].adsr[0].a_s.val.i = limit_range(scene[sc].adsr[0].a_s.val.i + 1, 0, 2);
         scene[sc].adsr[1].a_s.val.i = limit_range(scene[sc].adsr[1].a_s.val.i + 1, 0, 2);
      }
   }

   if (revision < 2)
   {
      for (int i = 0; i < n_lfos; i++)
      {
         if (scene[0].lfo[i].decay.val.f == scene[0].lfo[i].decay.val_max.f)
            scene[0].lfo[i].sustain.val.f = 1.f;
         else
            scene[0].lfo[i].sustain.val.f = 0.f;
         if (scene[1].lfo[i].decay.val.f == scene[1].lfo[i].decay.val_max.f)
            scene[1].lfo[i].sustain.val.f = 1.f;
         else
            scene[1].lfo[i].sustain.val.f = 0.f;
      }
   }

   if (revision < 3)
   {
      for (auto& sc : scene)
      {
         for (auto& u : sc.filterunit)
         {
            switch (u.type.val.i)
            {
            case fut_lpmoog:
               u.subtype.val.i = 3;
               break;
            case fut_comb:
               u.subtype.val.i = 1;
               break;
            case fut_SNH: // SNH replaced comb_neg in rev 4
               u.type.val.i = fut_comb;
               u.subtype.val.i = 3;
               break;
            }
         }
      }
   }

   if (revision == 3)
   {
      for (auto& sc : scene)
      {
         for (auto& u : sc.filterunit)
         {
            if (u.type.val.i == fut_SNH) // misc replaced comb_neg in rev 4
            {
               u.type.val.i = fut_comb;
               u.subtype.val.i += 2;
            }
         }
      }
   }

   if (revision < 5)
   {
      for (auto& sc : scene)
      {
         if (sc.filterblock_configuration.val.i == fb_stereo)
         {
            sc.pan.val.f = -1.f;
            sc.width.val.f = 1.f;
         }
      }
   }

   if (revision < 6) // adjust resonance of older patches to match new range
   {
      for (auto& sc : scene)
      {
         for (auto& u : sc.filterunit)
         {
            if ((u.type.val.i == fut_lp12) || (u.type.val.i == fut_hp12) ||
                (u.type.val.i == fut_bp12))
            {
               u.resonance.val.f = convert_v11_reso_to_v12_2P(u.resonance.val.f);
            }
            else if ((u.type.val.i == fut_lp24) || (u.type.val.i == fut_hp24))
            {
               u.resonance.val.f = convert_v11_reso_to_v12_4P(u.resonance.val.f);
            }
         }
      }
   }

   if (revision < 8)
   {
      for (auto& sc : scene)
      {
         // set lp/hp filters to subtype 1
         for (auto& u : sc.filterunit)
         {
            if ((u.type.val.i == fut_lp12) || (u.type.val.i == fut_hp12) ||
                (u.type.val.i == fut_bp12) || (u.type.val.i == fut_lp24) ||
                (u.type.val.i == fut_hp24))
            {
               u.subtype.val.i = (revision < 6) ? st_SVF : st_Rough;
            }
            else if (u.type.val.i == fut_br12)
            {
               u.subtype.val.i = 1;
            }
         }

         // convert pan2 to width
         if (sc.filterblock_configuration.val.i == fb_stereo)
         {
            float pan1 = sc.pan.val.f;
            float pan2 = sc.width.val.f;
            sc.pan.val.f = (pan1 + pan2) * 0.5f;
            sc.width.val.f = (pan2 - pan1) * 0.5f;
         }
      }
   }

   if (revision < 9)
   {
   }

   if (revision < 10)
   {
      character.val.i = 0;
   }

   // ensure that filtersubtype is a valid value
   for (auto& sc : scene)
   {
      for (int u = 0; u < n_filterunits_per_scene; u++)
      {
         sc.filterunit[u].subtype.val.i =
             limit_range(sc.filterunit[u].subtype.val.i, 0,
                         max(0, fut_subcount[sc.filterunit[u].type.val.i] - 1));
      }
   }

   /*
   ** extra osc data handling
   */

   // Blank out the display names
   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (int osc = 0; osc < n_oscs; osc++)
      {
         scene[sc].osc[osc].wavetable_display_name[0] = '\0';
      }
   }

   TiXmlElement* eod = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("extraoscdata"));
   if (eod)
   {
      for( auto child = eod->FirstChild(); child; child = child->NextSibling() )
      {
         auto *lkid = TINYXML_SAFE_TO_ELEMENT(child);
         if( lkid &&
             lkid->Attribute( "osc" ) &&
             lkid->Attribute( "scene" ) )
         {
            int sos = std::atoi( lkid->Attribute("osc") );
            int ssc = std::atoi( lkid->Attribute("scene") );

            if( lkid->Attribute( "wavetable_display_name" ) )
            {
               strncpy( scene[ssc].osc[sos].wavetable_display_name, lkid->Attribute( "wavetable_display_name" ), 256 );
            }
         }
      }
      
   }
  

   // reset stepsequences first
   for (auto& stepsequence : stepsequences)
      for (auto& l : stepsequence)
      {
         for (int i = 0; i < n_stepseqsteps; i++)
         {
            l.steps[i] = 0.f;
         }
         l.loop_start = 0;
         l.loop_end = 15;
         l.shuffle = 0.f;
      }
   TiXmlElement* ss = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("stepsequences"));
   if (ss)
      p = TINYXML_SAFE_TO_ELEMENT(ss->FirstChild("sequence"));
   else
      p = nullptr;
   while (p)
   {
      int sc, lfo;
      if ((p->QueryIntAttribute("scene", &sc) == TIXML_SUCCESS) &&
          (p->QueryIntAttribute("i", &lfo) == TIXML_SUCCESS) && within_range(0, sc, 1) &&
          within_range(0, lfo, n_lfos - 1))
      {
         stepSeqFromXmlElement(&(stepsequences[sc][lfo]), p );

      }
      p = TINYXML_SAFE_TO_ELEMENT(p->NextSibling("sequence"));
   }

   // restore msegs
   for (auto& ms : msegs )
      for (auto& l : ms )
      {
         ms->n_activeSegments = 0;
         Surge::MSEG::rebuildCache(ms);
      }

   TiXmlElement* ms = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("msegs"));
   if (ms)
      p = TINYXML_SAFE_TO_ELEMENT(ms->FirstChild("mseg"));
   else
      p = nullptr;
   while (p) {
      int v;
      auto sc = 0;
      if( p->QueryIntAttribute( "scene", &v ) == TIXML_SUCCESS ) sc = v;;
      auto mi = 0;
      if( p->QueryIntAttribute( "i", &v ) == TIXML_SUCCESS ) mi = v;
      auto *ms = &( msegs[sc][mi] );

      msegFromXMLElement(ms, p );
      p = TINYXML_SAFE_TO_ELEMENT( p->NextSibling( "mseg" ) );
   }

   // end restore msegs
   
   for (int i = 0; i < n_customcontrollers; i++)
      scene[0].modsources[ms_ctrl1 + i]->reset();

   TiXmlElement* cc = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("customcontroller"));
   if (cc)
      p = TINYXML_SAFE_TO_ELEMENT(cc->FirstChild("entry"));
   else
      p = nullptr;
   while (p)
   {
      int cont, sc;
      if ((p->QueryIntAttribute("i", &cont) == TIXML_SUCCESS) &&
          within_range(0, cont, n_customcontrollers - 1) &&
          ((p->QueryIntAttribute("scene", &sc) != TIXML_SUCCESS) || (sc == 0)))
      {
         if (p->QueryIntAttribute("bipolar", &j) == TIXML_SUCCESS)
            scene[0].modsources[ms_ctrl1 + cont]->set_bipolar(j);
         if (p->QueryDoubleAttribute("v", &d) == TIXML_SUCCESS)
         {
            ((ControllerModulationSource*)scene[0].modsources[ms_ctrl1 + cont])->set_target(d);
         }

         const char* lbl = p->Attribute("label");
         if (lbl)
         {
            strncpy(CustomControllerLabel[cont], lbl, 16);
            CustomControllerLabel[cont][15] = 0;
         }
      }
      p = TINYXML_SAFE_TO_ELEMENT(p->NextSibling("entry"));
   }

   patchTuning.tuningStoredInPatch = false;
   patchTuning.tuningContents = "";
   patchTuning.mappingContents = "";

   TiXmlElement *pt = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("patchTuning"));
   if( pt )
   {
       const char* td;
       if( pt &&
           (td = pt->Attribute("v") ))
       {
           patchTuning.tuningStoredInPatch = true;
           auto tc = base64_decode(td);
           patchTuning.tuningContents = tc;
       }

       if( pt &&
           (td = pt->Attribute("m") ))
       {
           patchTuning.tuningStoredInPatch = true;
           auto tc = base64_decode(td);
           patchTuning.mappingContents = tc;
       }
   }
   
   dawExtraState.isPopulated = false;
   TiXmlElement *de = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("dawExtraState"));
   if( de )
   {
       int pop;
       if( de->QueryIntAttribute("populated", &pop) == TIXML_SUCCESS )
       {
           dawExtraState.isPopulated = (pop != 0);
       }

       if( dawExtraState.isPopulated )
       {
           int ival;
           TiXmlElement *p;

           // This has a lecacy name since it was from before we moved into the editor object
           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("instanceZoomFactor"));
           if( p &&
               p->QueryIntAttribute("v",&ival) == TIXML_SUCCESS)
               dawExtraState.editor.instanceZoomFactor = ival;

           // This is the non-legacy way to save editor state
           p = TINYXML_SAFE_TO_ELEMENT( de->FirstChild( "editor" ) );
           if( p )
           {
              if( p->QueryIntAttribute( "current_scene", &ival ) == TIXML_SUCCESS )
                 dawExtraState.editor.current_scene = ival;

              if( p->QueryIntAttribute( "current_fx", &ival ) == TIXML_SUCCESS )
                 dawExtraState.editor.current_fx = ival;

              if( p->QueryIntAttribute( "modsource", &ival ) == TIXML_SUCCESS )
                 dawExtraState.editor.modsource = (modsources)ival;

              if( p->QueryIntAttribute( "isMSEGOpen", &ival) == TIXML_SUCCESS )
                 dawExtraState.editor.isMSEGOpen = ival;
              else
                 dawExtraState.editor.isMSEGOpen = false;

              for( int sc=0; sc<n_scenes; sc++ )
              {
                 std::string con = "current_osc_" + std::to_string( sc );
                 if( p->QueryIntAttribute( con.c_str(), &ival ) == TIXML_SUCCESS )
                    dawExtraState.editor.current_osc[sc] = ival;
                 con = "modsource_editor_" + std::to_string( sc );
                 if( p->QueryIntAttribute( con.c_str(), &ival ) == TIXML_SUCCESS )
                    dawExtraState.editor.modsource_editor[sc] = (modsources)ival;
              }

           }


           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("mpeEnabled"));
           if( p &&
               p->QueryIntAttribute("v",&ival) == TIXML_SUCCESS)
               dawExtraState.mpeEnabled = ival;

           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("mpePitchBendRange"));
           if( p &&
               p->QueryIntAttribute("v",&ival) == TIXML_SUCCESS)
               dawExtraState.mpePitchBendRange = ival;


           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("hasTuning"));
           if( p &&
               p->QueryIntAttribute("v",&ival) == TIXML_SUCCESS)
           {
               dawExtraState.hasTuning = (ival != 0);
           }

           const char* td;
           if( dawExtraState.hasTuning )
           {
               p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("tuningContents"));
               if( p &&
                   (td = p->Attribute("v") ))
               {
                   auto tc = base64_decode(td);
                   dawExtraState.tuningContents = tc;
               }
           }

           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("hasMapping"));
           if( p &&
               p->QueryIntAttribute("v",&ival) == TIXML_SUCCESS)
           {
               dawExtraState.hasMapping = (ival != 0);
           }

           if( dawExtraState.hasMapping )
           {
               p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("mappingContents"));
               if( p &&
                   (td = p->Attribute("v") ))
               {
                   auto tc = base64_decode(td);
                   dawExtraState.mappingContents = tc;
               }
           }

           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("midictrl_map"));
           if( p )
           {
              auto c = TINYXML_SAFE_TO_ELEMENT(p->FirstChild( "c" ) );
              while( c )
              {
                 int p, v;
                 if( c->QueryIntAttribute( "p", &p ) == TIXML_SUCCESS &&
                     c->QueryIntAttribute( "v", &v ) == TIXML_SUCCESS )
                 {
                    dawExtraState.midictrl_map[p] = v;
                 }
                 c = TINYXML_SAFE_TO_ELEMENT(c->NextSibling( "c" ) );
              }
           }

           p = TINYXML_SAFE_TO_ELEMENT(de->FirstChild("customcontrol_map"));
           if( p )
           {
              auto c = TINYXML_SAFE_TO_ELEMENT(p->FirstChild( "c" ) );
              while( c )
              {
                 int p, v;
                 if( c->QueryIntAttribute( "p", &p ) == TIXML_SUCCESS &&
                     c->QueryIntAttribute( "v", &v ) == TIXML_SUCCESS )
                    dawExtraState.customcontrol_map[p] = v;
                 c = TINYXML_SAFE_TO_ELEMENT(c->NextSibling( "c" ) );
              }
           }

       }
   }
                                              
   if (!is_preset)
   {
      TiXmlElement* mw = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("modwheel"));
      if (mw)
      {
         for (int sc = 0; sc < n_scenes; sc++)
         {
            char str[32];
            snprintf(str, 32, "s%d", sc);
            if (mw->QueryDoubleAttribute(str, &d) == TIXML_SUCCESS)
            {
               ((ControllerModulationSource*)scene[sc].modsources[ms_modwheel])->set_target(d);
            }
         }
      }
   }
}

struct srge_header
{
   int revision;
};

unsigned int SurgePatch::save_xml(void** data) // allocates mem, must be freed by the callee
{
   char tempstr[64];
   assert(data);
   if (!data)
      return 0;
   int n = param_ptr.size();

   TiXmlDeclaration decl("1.0", "UTF-8", "yes");
   TiXmlDocument doc;
   // doc.SetCondenseWhiteSpace(false);
   TiXmlElement patch("patch");
   patch.SetAttribute("revision", ff_revision);

   TiXmlElement meta("meta");
   meta.SetAttribute("name", this->name);
   meta.SetAttribute("category", this->category);
   meta.SetAttribute("comment", comment);
   meta.SetAttribute("author", author);
   patch.InsertEndChild(meta);

   TiXmlElement parameters("parameters");

   for (int i = 0; i < n; i++)
   {
      TiXmlElement p(param_ptr[i]->get_storage_name());

      int s_id = param_ptr[i]->scene;
      int p_id = param_ptr[i]->param_id_in_scene;

      bool skip = false;

      if (param_ptr[i]->ctrlgroup == cg_FX) // skip empty effects
      {
         int unit = param_ptr[i]->ctrlgroup_entry;
         if (fx[unit].type.val.i == fxt_off)
            skip = true;
      }

      if (!skip)
      {
         if (s_id > 0)
         {
            for (int a = 0; a < 2; a++)
            {
               vector<ModulationRouting>* r = &scene[s_id - 1].modulation_scene;
               if (a)
                  r = &scene[s_id - 1].modulation_voice;
               int n = r->size();
               for (int b = 0; b < n; b++)
               {
                  if (r->at(b).destination_id == p_id)
                  {
                     TiXmlElement mr("modrouting");
                     mr.SetAttribute("source", r->at(b).source_id);
                     mr.SetAttribute("depth", float_to_str(r->at(b).depth, tempstr));
                     p.InsertEndChild(mr);
                  }
               }
            }
         }
         else
         {
            vector<ModulationRouting>* r = &modulation_global;
            int n = r->size();
            for (int b = 0; b < n; b++)
            {
               if (r->at(b).destination_id == i)
               {
                  TiXmlElement mr("modrouting");
                  mr.SetAttribute("source", r->at(b).source_id);
                  mr.SetAttribute("depth", float_to_str(r->at(b).depth, tempstr));
                  p.InsertEndChild(mr);
               }
            }
         }

         if (param_ptr[i]->valtype == (valtypes)vt_float)
         {
            p.SetAttribute("type", vt_float);
            // p.SetAttribute("value",float_to_str(param_ptr[i]->val.f,tempstr));
            p.SetAttribute("value", param_ptr[i]->get_storage_value(tempstr));
         }
         else
         {
            p.SetAttribute("type", vt_int);
            p.SetAttribute("value", param_ptr[i]->get_storage_value(tempstr));
         }

         if (param_ptr[i]->temposync)
            p.SetAttribute("temposync", "1");
         if (param_ptr[i]->extend_range)
            p.SetAttribute("extend_range", "1");
         if (param_ptr[i]->absolute)
            p.SetAttribute("absolute", "1");
         if (param_ptr[i]->can_deactivate())
            p.SetAttribute("deactivated", param_ptr[i]->deactivated ? "1" : "0" );
         if (param_ptr[i]->has_portaoptions())
         {
            p.SetAttribute("porta_const_rate", param_ptr[i]->porta_constrate ? "1" : "0");
            p.SetAttribute("porta_gliss", param_ptr[i]->porta_gliss ? "1" : "0");
            p.SetAttribute("porta_retrigger", param_ptr[i]->porta_retrigger ? "1" : "0");
            p.SetAttribute("porta_curve", param_ptr[i]->porta_curve);
         }
         if (param_ptr[i]->has_deformoptions())
            p.SetAttribute("deform_type", param_ptr[i]->deform_type);

         // param_ptr[i]->val.i;
         parameters.InsertEndChild(p);
      }
   }
   patch.InsertEndChild(parameters);

   TiXmlElement eod( "extraoscdata" );
   for (int sc = 0; sc < n_scenes; ++sc)
   {
      for( int os=0; os<n_oscs; ++os )
      {
         std::string streaming_name = "osc_extra_sc" + std::to_string(sc) + "_osc" + std::to_string(os);
         if( uses_wavetabledata(scene[sc].osc[os].type.val.i) )
         {
            TiXmlElement on(streaming_name.c_str());
            on.SetAttribute("wavetable_display_name", scene[sc].osc[os].wavetable_display_name );
            on.SetAttribute("scene", sc );
            on.SetAttribute("osc", os );
            eod.InsertEndChild(on);
         }
      }
   }
   patch.InsertEndChild(eod);
     
   
   TiXmlElement ss("stepsequences");
   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (int l = 0; l < n_lfos; l++)
      {
         if (scene[sc].lfo[l].shape.val.i == lt_stepseq)
         {
            char txt[256], txt2[256];
            TiXmlElement p("sequence");
            p.SetAttribute("scene", sc);
            p.SetAttribute("i", l);

            stepSeqToXmlElement(&(stepsequences[sc][l]), p, l < n_lfos_voice );

            ss.InsertEndChild(p);
         }
      }
   }
   patch.InsertEndChild(ss);

   TiXmlElement mseg( "msegs" );
   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (int l = 0; l < n_lfos; l++)
      {
         if (scene[sc].lfo[l].shape.val.i == lt_mseg)
         {
            TiXmlElement p("mseg");
            p.SetAttribute("scene", sc);
            p.SetAttribute("i", l);

            auto *ms = &(msegs[sc][l]);
            msegToXMLElement(ms, p );
            mseg.InsertEndChild( p );
         }
      }
   }
   patch.InsertEndChild(mseg);
   
   TiXmlElement cc("customcontroller");
   for (int l = 0; l < n_customcontrollers; l++)
   {
      char txt2[256];
      TiXmlElement p("entry");
      p.SetAttribute("i", l);
      p.SetAttribute("bipolar", scene[0].modsources[ms_ctrl1 + l]->is_bipolar() ? 1 : 0);
      p.SetAttribute("v", float_to_str(((ControllerModulationSource*)scene[0].modsources[ms_ctrl1 + l])->target, txt2));
      p.SetAttribute("label", CustomControllerLabel[l]);

      cc.InsertEndChild(p);
   }
   patch.InsertEndChild(cc);

   {
      char txt[256];
      TiXmlElement mw("modwheel");
      for (int sc = 0; sc < n_scenes; sc++)
      {
         char str[32];
         sprintf(str, "s%d", sc);
         mw.SetAttribute(str, float_to_str(((ControllerModulationSource*)scene[sc].modsources[ms_modwheel])->target, txt));
      }
      patch.InsertEndChild(mw);
   }

   if( patchTuning.tuningStoredInPatch )
   {
       TiXmlElement pt( "patchTuning" );
       pt.SetAttribute("v", base64_encode( (unsigned const char *)patchTuning.tuningContents.c_str(), patchTuning.tuningContents.size() ).c_str() );
       if( patchTuning.mappingContents.size() > 0 )
          pt.SetAttribute("m", base64_encode( (unsigned const char *)patchTuning.mappingContents.c_str(), patchTuning.mappingContents.size() ).c_str() );
       
       patch.InsertEndChild(pt);
   }
   
   TiXmlElement dawExtraXML("dawExtraState");
   dawExtraXML.SetAttribute( "populated", dawExtraState.isPopulated ? 1 : 0 );
   
   if( dawExtraState.isPopulated )
   {
      // This has a lecacy name since it was from before we moved into the editor object
       TiXmlElement izf("instanceZoomFactor");
       izf.SetAttribute( "v", dawExtraState.editor.instanceZoomFactor );
       dawExtraXML.InsertEndChild(izf);

       TiXmlElement eds( "editor" );
       eds.SetAttribute( "current_scene", dawExtraState.editor.current_scene );
       eds.SetAttribute( "current_fx", dawExtraState.editor.current_fx );
       eds.SetAttribute( "modsource", dawExtraState.editor.modsource );
       eds.SetAttribute( "isMSEGOpen", dawExtraState.editor.isMSEGOpen );
       for( int sc=0; sc<n_scenes; sc++ )
       {
          std::string con = "current_osc_" + std::to_string( sc );
          eds.SetAttribute( con.c_str(), dawExtraState.editor.current_osc[sc]);
          con = "modsource_editor_" + std::to_string( sc );
          eds.SetAttribute( con.c_str(), dawExtraState.editor.modsource_editor[sc]);
       }

       dawExtraXML.InsertEndChild(eds);

       TiXmlElement mpe("mpeEnabled");
       mpe.SetAttribute("v", dawExtraState.mpeEnabled ? 1 : 0 );
       dawExtraXML.InsertEndChild(mpe);

       TiXmlElement mppb("mpePitchBendRange");
       mppb.SetAttribute("v", dawExtraState.mpePitchBendRange );
       dawExtraXML.InsertEndChild(mppb);

       TiXmlElement tun("hasTuning");
       tun.SetAttribute("v", dawExtraState.hasTuning ? 1 : 0 );
       dawExtraXML.InsertEndChild(tun);

       /*
       ** we really want a cdata here but TIXML is ambiguous whether
       ** it does the right thing when I read the code, and is kinda crufty
       ** so just protect ourselves with a base 64 encoding.
       */
       TiXmlElement tnc("tuningContents");
       tnc.SetAttribute("v", base64_encode( (unsigned const char *)dawExtraState.tuningContents.c_str(),
                                            dawExtraState.tuningContents.size() ).c_str() );
       dawExtraXML.InsertEndChild(tnc);

       TiXmlElement hmp("hasMapping");
       hmp.SetAttribute("v", dawExtraState.hasMapping ? 1 : 0 );
       dawExtraXML.InsertEndChild(hmp);

       /*
       ** we really want a cdata here but TIXML is ambiguous whether
       ** it does the right thing when I read the code, and is kinda crufty
       ** so just protect ourselves with a base 64 encoding.
       */
       TiXmlElement mpc("mappingContents");
       mpc.SetAttribute("v", base64_encode( (unsigned const char *)dawExtraState.mappingContents.c_str(),
                                            dawExtraState.mappingContents.size() ).c_str() );
       dawExtraXML.InsertEndChild(mpc);

       /*
       ** Add the midi controls
       */
       TiXmlElement mcm("midictrl_map");
       for( auto &p : dawExtraState.midictrl_map )
       {
          TiXmlElement c("c");
          c.SetAttribute( "p", p.first );
          c.SetAttribute( "v", p.second );
          mcm.InsertEndChild(c);
       }
       dawExtraXML.InsertEndChild(mcm);
       
       TiXmlElement ccm("customcontrol_map");
       for( auto &p : dawExtraState.customcontrol_map )
       {
          TiXmlElement c("c");
          c.SetAttribute( "p", p.first );
          c.SetAttribute( "v", p.second );
          ccm.InsertEndChild(c);
       }
       dawExtraXML.InsertEndChild(ccm);
   }
   patch.InsertEndChild(dawExtraXML);
   
   doc.InsertEndChild(decl);
   doc.InsertEndChild(patch);

   std::string s;
   s << doc;

   void* d = malloc(s.size());
   memcpy(d, s.data(), s.size());
   *data = d;
   return s.size();
}

void SurgePatch::msegToXMLElement(MSEGStorage* ms, TiXmlElement& p) const
{
   p.SetAttribute( "activeSegments", ms->n_activeSegments );
   p.SetAttribute( "endpointMode", ms->endpointMode );
   p.SetAttribute( "editMode", ms->editMode );
   p.SetAttribute( "loopMode", ms->loopMode );
   p.SetAttribute( "loopStart", ms->loop_start);
   p.SetAttribute( "loopEnd", ms->loop_end );

   TiXmlElement segs( "segments" );
   for( int s=0; s<ms->n_activeSegments; ++s )
   {
      TiXmlElement seg( "segment" );
      seg.SetDoubleAttribute( "duration", ms->segments[s].duration );
      seg.SetDoubleAttribute( "v0", ms->segments[s].v0 );
      seg.SetDoubleAttribute( "nv1", ms->segments[s].nv1 );
      seg.SetDoubleAttribute( "cpduration", ms->segments[s].cpduration );
      seg.SetDoubleAttribute( "cpv", ms->segments[s].cpv );
      seg.SetAttribute( "type", (int)( ms->segments[s].type ));
      seg.SetAttribute( "useDeform", (int)(ms->segments[s].useDeform));
      seg.SetAttribute( "invertDeform", (int)(ms->segments[s].invertDeform));
      segs.InsertEndChild( seg );
   }
   p.InsertEndChild(segs);
}

void SurgePatch::msegFromXMLElement(MSEGStorage* ms, TiXmlElement* p) const
{
   int v;
   ms->n_activeSegments = 0;
   if( p->QueryIntAttribute( "activeSegments", &v ) == TIXML_SUCCESS ) ms->n_activeSegments = v;
   if( p->QueryIntAttribute("endpointMode", &v) == TIXML_SUCCESS )
      ms->endpointMode = (MSEGStorage::EndpointMode)v;
   else
      ms->endpointMode = MSEGStorage::EndpointMode::FREE;
   if( p->QueryIntAttribute( "editMode", &v ) == TIXML_SUCCESS )
      ms->editMode = (MSEGStorage::EditMode)v;
   else
      ms->editMode = MSEGStorage::ENVELOPE;

   if( p->QueryIntAttribute( "loopMode", &v ) == TIXML_SUCCESS )
      ms->loopMode = (MSEGStorage::LoopMode)v;
   else
      ms->loopMode = MSEGStorage::LoopMode::LOOP;
   if( p->QueryIntAttribute( "loopStart", &v ) == TIXML_SUCCESS )
      ms->loop_start = v;
   else
      ms->loop_start = -1;

   if( p->QueryIntAttribute( "loopEnd", &v ) == TIXML_SUCCESS )
      ms->loop_end = v;
   else
      ms->loop_end = -1;


   auto segs = TINYXML_SAFE_TO_ELEMENT(p->FirstChild( "segments" ));
   if( segs )
   {
      auto seg = TINYXML_SAFE_TO_ELEMENT(segs->FirstChild( "segment" ) );
      int idx = 0;
      while( seg )
      {
         double d;
#define MSGF(x) if( seg->QueryDoubleAttribute( #x, &d ) == TIXML_SUCCESS ) ms->segments[idx].x = d;
         MSGF( duration );
         MSGF( v0 );
         MSGF( cpduration );
         MSGF( cpv );
         MSGF( nv1 );

         int t = 0;
         if( seg->QueryIntAttribute( "type", &v ) == TIXML_SUCCESS ) t = v;
         ms->segments[idx].type = (MSEGStorage::segment::Type)t;

         if( seg->QueryIntAttribute( "useDeform", &v) == TIXML_SUCCESS )
            ms->segments[idx].useDeform = v;
         else
            ms->segments[idx].useDeform = true;

         if( seg->QueryIntAttribute( "invertDeform", &v) == TIXML_SUCCESS )
            ms->segments[idx].invertDeform = v;
         else
            ms->segments[idx].invertDeform = false;

         seg = TINYXML_SAFE_TO_ELEMENT( seg->NextSibling( "segment" ) );

         idx++;
      }
      if( idx != ms->n_activeSegments )
      {
         std::cout << "BAD RESTORE " << _D(idx) << _D(ms->n_activeSegments) << std::endl;
      }
   }
   // Rebuild cache
   Surge::MSEG::rebuildCache(ms);

}

void SurgePatch::stepSeqToXmlElement(StepSequencerStorage* ss, TiXmlElement& p, bool streamMask) const
{
   char txt[256], txt2[256];
   for (int s = 0; s < n_stepseqsteps; s++)
   {
      sprintf(txt, "s%i", s);
      if (ss->steps[s] != 0.f)
         p.SetAttribute(txt, float_to_str(ss->steps[s], txt2));
   }

   p.SetAttribute("loop_start", ss->loop_start);
   p.SetAttribute("loop_end", ss->loop_end);
   p.SetAttribute("shuffle", float_to_str(ss->shuffle, txt2));
   if(streamMask)
   {
      uint64_t ttm = ss->trigmask;

      // collapse in case an old surge loads this
      uint64_t old_ttm = ( ttm & 0xFFFF ) | ( ( ttm >> 16 ) & 0xFFFF ) | ( ( ttm >> 32 ) & 0xFFFF );

      p.SetAttribute("trigmask", old_ttm);

      p.SetAttribute("trigmask_0to15", ttm & 0xFFFF );
      ttm = ttm >> 16;
      p.SetAttribute("trigmask_16to31", ttm & 0xFFFF );
      ttm = ttm >> 16;
      p.SetAttribute("trigmask_32to47", ttm & 0xFFFF );
   }
}

void SurgePatch::stepSeqFromXmlElement(StepSequencerStorage* ss, TiXmlElement* p) const
{
   double d;
   int j;
   if (p->QueryDoubleAttribute("shuffle", &d) == TIXML_SUCCESS)
      ss->shuffle = (float)d;
   if (p->QueryIntAttribute("loop_start", &j) == TIXML_SUCCESS)
      ss->loop_start = j;
   if (p->QueryIntAttribute("loop_end", &j) == TIXML_SUCCESS)
      ss->loop_end = j;
   if (p->QueryIntAttribute("trigmask", &j) == TIXML_SUCCESS)
      ss->trigmask = j;

   if (p->QueryIntAttribute("trigmask_0to15", &j ) == TIXML_SUCCESS )
   {
      ss->trigmask &= 0xFFFFFFFFFFFF0000;
      j &= 0xFFFF;
      ss->trigmask |= j;
   };
   if (p->QueryIntAttribute("trigmask_16to31", &j ) == TIXML_SUCCESS )
   {
      ss->trigmask &= 0xFFFFFFFF0000FFFF;
      j &= 0xFFFF;
      uint64_t jl = (uint64_t)j;
      ss->trigmask |= jl << 16;
   };
   if (p->QueryIntAttribute("trigmask_32to47", &j ) == TIXML_SUCCESS )
   {
      ss->trigmask &= 0xFFFF0000FFFFFFFF;
      j &= 0xFFFF;
      uint64_t jl = (uint64_t)j;
      ss->trigmask |= jl << 32;
   };

   for (int s = 0; s < n_stepseqsteps; s++)
   {
      char txt[256];
      sprintf(txt, "s%i", s);
      if (p->QueryDoubleAttribute(txt, &d) == TIXML_SUCCESS)
         ss->steps[s] = (float)d;
      else
         ss->steps[s] = 0.f;
   }
}