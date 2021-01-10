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

#include "SkinModel.h"
#include "resource.h"

using Surge::Skin::Connector;

/*
 * This file, in conjunction with the SVG assets it references by ID, is a complete
 * description of the surge default UI layout. The action callbacks bind to it and
 * the engine to lay it out are the collaboration betweeh this, SkinSupport, and SurgeGUIEditor
 */

namespace Surge
{
   namespace Skin
   {
      namespace Global
      {
         Connector active_scene = Connector("global.active_scene", 7, 12, 40, 42, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_SCENE_SELECT, 2, 2, 1);
         Connector scene_mode = Connector("global.scene_mode", 54, 12, 40, 42, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_SCENE_MODE, 4, 4, 1);
         Connector fx_disable = Connector("global.fx_disable", 0, 0);
         Connector fx_bypass = Connector("global.fx_bypass", 607, 12, 135, 27, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_FX_GLOBAL_BYPASS, 4, 1, 4);
         Connector character = Connector("global.character", 607, 42, 135, 12, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_OSC_CHARACTER, 3, 1, 3);
         Connector master_volume = Connector("global.volume", 756, 29);

         Connector fx1_return = Connector("global.fx1_return", 759, 141);
         Connector fx2_return = Connector("global.fx2_return", 759, 162);
      }

      namespace Scene
      {
         Connector polylimit = Connector("scene.polylimit", 100, 41, 43, 14, Connector::NUMBERFIELD)
                   .withBackground(IDB_NUMFIELD_POLY_SPLIT)
                   .withProperty(Connector::NUMBERFIELD_CONTROLMODE, cm_polyphony)
                   .withProperty(Connector::TEXT_COLOR, "scene.split_poly.text")
                   .withProperty(Connector::TEXT_HOVER_COLOR, "scene.split_poly.text.hover");
         Connector splitpoint = Connector("scene.splitpoint", 100, 11, 43, 14, Connector::NUMBERFIELD)
                   .withBackground(IDB_NUMFIELD_POLY_SPLIT)
                   .withProperty(Connector::TEXT_COLOR, "scene.split_poly.text")
                   .withProperty(Connector::TEXT_HOVER_COLOR, "scene.split_poly.text.hover");
                   // this doesn't have a cm since it is special

         Connector pbrange_dn = Connector("scene.pbrange_dn", 159, 110, 30, 13, Connector::NUMBERFIELD)
                   .withProperty(Connector::NUMBERFIELD_CONTROLMODE, cm_pbdepth)
                   .withBackground(IDB_NUMFIELD_PITCHBEND)
                   .withProperty(Connector::TEXT_COLOR, "scene.pbrange.text")
                   .withProperty(Connector::TEXT_HOVER_COLOR, "scene.pbrange.text.hover");
         Connector pbrange_up = Connector("scene.pbrange_up", 187, 110, 30, 13, Connector::NUMBERFIELD)
                   .withProperty(Connector::NUMBERFIELD_CONTROLMODE, cm_pbdepth)
                   .withBackground(IDB_NUMFIELD_PITCHBEND)
                   .withProperty(Connector::TEXT_COLOR, "scene.pbrange.text")
                   .withProperty(Connector::TEXT_HOVER_COLOR, "scene.pbrange.text.hover");
         Connector playmode = Connector("scene.playmode", 239, 87, 50, 47, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_PLAY_MODE,6,6,1);
         Connector drift = Connector("scene.drift", 156, 141).asHorizontal().asWhite();
         Connector noise_color = Connector("scene.noise_color", 156, 162).asHorizontal().asWhite();

         Connector fmrouting = Connector("scene.fmrouting", 309, 83, 134, 52, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_OSC_FM_ROUTING, 4, 1, 4);
         Connector fmdepth = Connector("scene.fmdepth", 306, 152).asHorizontal().asWhite();

         Connector octave = Connector("scene.octave", 202, 194, 96, 18, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_SCENE_OCTAVE, 7, 1, 7);
         Connector pitch = Connector("scene.pitch", 156, 212).asHorizontal();
         Connector portatime = Connector("scene.portamento", 156, 234).asHorizontal();

         Connector keytrack_root = Connector("scene.keytrack_root", 311, 266, 43, 14, Connector::NUMBERFIELD)
                  .withBackground(IDB_NUMFIELD_KEYTRACK_ROOT)
                  .withProperty(Connector::NUMBERFIELD_CONTROLMODE, cm_notename)
                  .withProperty(Connector::TEXT_COLOR, "scene.keytrackroot.text")
                  .withProperty(Connector::TEXT_HOVER_COLOR, "scene.keytrackroot.text.hover");

         Connector scene_output_panel = Connector("scene.output.panel", 606, 78, Connector::GROUP);
            Connector volume = Connector("scene.volume", 0, 0).asHorizontal().asWhite().inParent("scene.output.panel");
            Connector pan = Connector("scene.pan", 0, 21).asHorizontal().asWhite().inParent("scene.output.panel");
            Connector width = Connector("scene.width", 0, 42).asHorizontal().asWhite().inParent("scene.output.panel");
            Connector send_fx_1 = Connector("scene.send_fx_1", 0, 63).asHorizontal().asWhite().inParent("scene.output.panel");
            Connector send_fx_2 = Connector("scene.send_fx_2", 0, 84).asHorizontal().asWhite().inParent("scene.output.panel");

         Connector vel_sensitivity = Connector("scene.velocity_sensitivity", 699, 301).asVertical().asWhite();
         Connector gain = Connector("scene.gain", 719, 301).asVertical().asWhite();
      }

      namespace Osc
      {
         Connector osc_select = Connector("osc.select", 66, 69, 75, 13, Connector::HSWITCH2, Connector::OSCILLATOR_SELECT)
                   .withHSwitch2Properties(IDB_OSC_SELECT, 3, 1, 3);
         Connector osc_display = Connector("osc.display", 4, 81, 141, 99, Connector::CUSTOM, Connector::OSCILLATOR_DISPLAY);

         Connector keytrack = Connector("osc.keytrack", 4, 180, 45, 9, Connector::SWITCH)
                   .withBackground(IDB_OSC_KEYTRACK);
         Connector retrigger = Connector("osc.retrigger", 51, 180, 45, 9, Connector::SWITCH)
                   .withBackground(IDB_OSC_RETRIGGER);
         
         Connector octave = Connector("osc.octave", 0, 194, 96, 18, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_OSC_OCTAVE, 7, 1, 7);
         Connector osc_type = Connector("osc.type", 96, 194, 49, 18, Connector::OSCMENU);

         Connector osc_param_panel = Connector("osc.param.panel", 6, 212, Connector::GROUP);
            Connector pitch   = Connector("osc.pitch", 0, 0).asHorizontal().inParent("osc.param.panel");
            Connector param_1 = Connector("osc.param_1", 0, 22).asHorizontal().inParent("osc.param.panel");
            Connector param_2 = Connector("osc.param_2", 0, 44).asHorizontal().inParent("osc.param.panel");
            Connector param_3 = Connector("osc.param_3", 0, 66).asHorizontal().inParent("osc.param.panel");
            Connector param_4 = Connector("osc.param_4", 0, 88).asHorizontal().inParent("osc.param.panel");
            Connector param_5 = Connector("osc.param_5", 0, 110).asHorizontal().inParent("osc.param.panel");
            Connector param_6 = Connector("osc.param_6", 0, 132).asHorizontal().inParent("osc.param.panel");
            Connector param_7 = Connector("osc.param_7", 0, 154).asHorizontal().inParent("osc.param.panel");
      }

      namespace Mixer
      {
         Connector mixer_panel = Connector("mixer.panel", 154, 263, Connector::GROUP);
            Connector mute_o1 = Connector("mixer.mute_o1", 1, 0).asMixerMute().inParent("mixer.panel");
            Connector mute_o2 = Connector("mixer.mute_o2", 21, 0).asMixerMute().inParent("mixer.panel");
            Connector mute_o3 = Connector("mixer.mute_o3", 41, 0).asMixerMute().inParent("mixer.panel");
            Connector mute_ring12 = Connector("mixer.mute_ring12", 61, 0).asMixerMute().inParent("mixer.panel");
            Connector mute_ring23 = Connector("mixer.mute_ring23", 81, 0).asMixerMute().inParent("mixer.panel");
            Connector mute_noise = Connector("mixer.mute_noise", 101, 0).asMixerMute().inParent("mixer.panel");
   
            Connector solo_o1 = Connector("mixer.solo_o1", 1, 11).asMixerSolo().inParent("mixer.panel");
            Connector solo_o2 = Connector("mixer.solo_o2", 21, 11).asMixerSolo().inParent("mixer.panel");
            Connector solo_o3 = Connector("mixer.solo_o3", 41, 11).asMixerSolo().inParent("mixer.panel");
            Connector solo_ring12 = Connector("mixer.solo_ring12", 61, 11).asMixerSolo().inParent("mixer.panel");
            Connector solo_ring23 = Connector("mixer.solo_ring23", 81, 11).asMixerSolo().inParent("mixer.panel");
            Connector solo_noise = Connector("mixer.solo_noise", 101, 11).asMixerSolo().inParent("mixer.panel");
   
            Connector route_o1 = Connector("mixer.route_o1", 1, 22).asMixerRoute().inParent("mixer.panel");
            Connector route_o2 = Connector("mixer.route_o2", 21, 22).asMixerRoute().inParent("mixer.panel");
            Connector route_o3 = Connector("mixer.route_o3", 41, 22).asMixerRoute().inParent("mixer.panel");
            Connector route_ring12 = Connector("mixer.route_ring12", 61, 22).asMixerRoute().inParent("mixer.panel");
            Connector route_ring23 = Connector("mixer.route_ring23", 81, 22).asMixerRoute().inParent("mixer.panel");
            Connector route_noise = Connector("mixer.route_noise", 101, 22).asMixerRoute().inParent("mixer.panel");
   
            Connector level_o1 = Connector("mixer.level_o1", 0, 38).asVertical().asWhite().inParent("mixer.panel");
            Connector level_o2 = Connector("mixer.level_o2", 20, 38).asVertical().asWhite().inParent("mixer.panel");
            Connector level_o3 = Connector("mixer.level_o3", 40, 38).asVertical().asWhite().inParent("mixer.panel");
            Connector level_ring12 = Connector("mixer.level_ring12", 60, 38).asVertical().asWhite().inParent("mixer.panel");
            Connector level_ring23 = Connector("mixer.level_ring23", 80, 38).asVertical().asWhite().inParent("mixer.panel");
            Connector level_noise = Connector("mixer.level_noise", 100, 38).asVertical().asWhite().inParent("mixer.panel");
   
            Connector level_prefiltergain = Connector("mixer.level_prefiltergain", 120, 37).asVertical().asWhite().inParent("mixer.panel");
      }

      namespace Filter
      {
         Connector config = Connector("filter.config", 455, 83, 134, 52, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_FILTER_CONFIG, 8, 1, 8);
         Connector feedback = Connector("filter.feedback", 457, 152).asHorizontal().asWhite();

         // FIXME - we should really expose the menu slider fully but for now make a composite FILTERSELECTOR
         Connector type_1 = Connector("filter.type_1", 305, 191, 124, 21, Connector::FILTERSELECTOR);
         Connector subtype_1 = Connector("filter.subtype_1", 432, 194, 12, 18, Connector::SWITCH)
                   .withBackground(IDB_FILTER_SUBTYPE);
         Connector cutoff_1 = Connector("filter.cutoff_1", 306, 212).asHorizontal();
         Connector resonance_1 = Connector("filter.resonance_1", 306, 234).asHorizontal();

         Connector balance = Connector("filter.balance", 456, 224).asHorizontal();

         Connector type_2 = Connector("filter.type_2", 605, 191, 124, 21, Connector::FILTERSELECTOR);
         Connector subtype_2 = Connector("filter.subtype_2", 732, 194, 12, 18, Connector::SWITCH)
                   .withBackground(IDB_FILTER_SUBTYPE);
         Connector cutoff_2 = Connector("filter.cutoff_2", 600, 212).asHorizontal();
         Connector resonance_2 = Connector("filter.resonance_2", 600, 234).asHorizontal();

         Connector f2_offset_mode = Connector("filter.f2_offset_mode", 734, 217, 12, 18, Connector::SWITCH)
                   .withBackground(IDB_FILTER2_OFFSET);
         Connector f2_link_resonance = Connector("filter.f2_link_resonance", 734, 239, 12, 18, Connector::SWITCH)
                   .withBackground(IDB_FILTER2_RESONANCE_LINK);

         Connector keytrack_1 = Connector("filter.keytrack_1", 309, 301).asVertical().asWhite();
         Connector keytrack_2 = Connector("filter.keytrack_2", 329, 301).asVertical().asWhite();

         Connector envmod_1 = Connector("filter.envmod_1", 549, 301).asVertical().asWhite();
         Connector envmod_2 = Connector("filter.envmod_2", 569, 301).asVertical().asWhite();

         Connector waveshaper_drive = Connector("filter.waveshaper_drive", 419, 301).asVertical().asWhite();
         Connector waveshaper_type = Connector("filter.waveshaper_type", 388, 311, 28, 51, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_WAVESHAPER_MODE, 6, 6, 1);

         Connector highpass = Connector("filter.highpass", 354, 301).asVertical().asWhite();
      }

      namespace FEG
      {
         // If we never refer to it by type we can just construct it and don't need it in extern list
         Connector feg_panel = Connector("feg.panel", 459, 267, Connector::GROUP);
            Connector mode = Connector("feg.mode", 93, 0, 34, 18, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_MODE, 2, 2, 1).inParent("feg.panel");
   
            Connector attack_shape = Connector("feg.attack_shape", 4, 1, 20, 16, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3).inParent("feg.panel");
            Connector decay_shape = Connector("feg.decay_shape", 24, 1, 20, 16, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3).withProperty(Connector::FRAME_OFFSET, 3).inParent("feg.panel");
            Connector release_shape = Connector("feg.release_shape", 65, 1, 20, 16, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3).withProperty(Connector::FRAME_OFFSET, 6).inParent("feg.panel");
   
            Connector attack  = Connector("feg.attack", 0, 34).asVertical().asWhite().inParent("feg.panel");
            Connector decay   = Connector("feg.decay", 20, 34).asVertical().asWhite().inParent("feg.panel");
            Connector sustain = Connector("feg.sustain", 40, 34).asVertical().asWhite().inParent("feg.panel");
            Connector release = Connector("feg.release", 60, 34).asVertical().asWhite().inParent("feg.panel");
      }

      namespace AEG
      {
         Connector aeg_panel = Connector("aeg.panel", 609, 267, Connector::GROUP);
            Connector mode = Connector("aeg.mode", 93, 0, 34, 18, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_MODE, 2, 2, 1).inParent("aeg.panel");
   
            Connector attack_shape = Connector("aeg.attack_shape", 4, 1, 20, 16, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3).inParent("aeg.panel");;
            Connector decay_shape = Connector("aeg.decay_shape", 24, 1, 20, 16, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3).withProperty(Connector::FRAME_OFFSET, 3).inParent("aeg.panel");;
            Connector release_shape = Connector("aeg.release_shape", 65, 1, 20, 16, Connector::HSWITCH2)
                      .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3).withProperty(Connector::FRAME_OFFSET, 6).inParent("aeg.panel");;
   
            Connector attack  = Connector("aeg.attack", 0, 34).asVertical().asWhite().inParent("aeg.panel");;
            Connector decay   = Connector("aeg.decay", 20, 34).asVertical().asWhite().inParent("aeg.panel");;
            Connector sustain = Connector("aeg.sustain", 40, 34).asVertical().asWhite().inParent("aeg.panel");;
            Connector release = Connector("aeg.release", 60, 34).asVertical().asWhite().inParent("aeg.panel");;
      }

      namespace FX
      {
         Connector fx_selector = Connector("fx.selector", 767, 79, 123, 51, Connector::CUSTOM, Connector::FX_SELECTOR);
         Connector fx_type = Connector("fx.type", 761, 194, 133, 18, Connector::FXMENU);
         
         Connector fx_preset = Connector("fx.preset.name", 762, 212, 95, 12, Connector::CUSTOM, Connector::FXPRESET_LABEL);
         Connector fx_jog = Connector("fx.preset.prevnext", 860, 212, Connector::JOG_FX).asJogPlusMinus();

         Connector fx_param_panel = Connector("fx.param.panel", 759, 224, Connector::GROUP);
            Connector param_1  = Connector("fx.param_1", 0, 0).inParent("fx.param.panel");
            Connector param_2  = Connector("fx.param_2", 0, 20).inParent("fx.param.panel");
            Connector param_3  = Connector("fx.param_3", 0, 40).inParent("fx.param.panel");
            Connector param_4  = Connector("fx.param_4", 0, 60).inParent("fx.param.panel");
            Connector param_5  = Connector("fx.param_5", 0, 80).inParent("fx.param.panel");
            Connector param_6  = Connector("fx.param_6", 0, 100).inParent("fx.param.panel");
            Connector param_7  = Connector("fx.param_7", 0, 120).inParent("fx.param.panel");
            Connector param_8  = Connector("fx.param_8", 0, 140).inParent("fx.param.panel");
            Connector param_9  = Connector("fx.param_9", 0, 160).inParent("fx.param.panel");
            Connector param_10 = Connector("fx.param_10", 0, 180).inParent("fx.param.panel");
            Connector param_11 = Connector("fx.param_11", 0, 200).inParent("fx.param.panel");
            Connector param_12 = Connector("fx.param_12", 0, 220).inParent("fx.param.panel");
      }

      namespace LFO
      {
         // For now these two have component 'CUSTOM' and we hadn't pick a component in the code
         Connector lfo_title_label = Connector("lfo.title", 6, 489, 11, 83, Connector::CUSTOM, Connector::LFO_LABEL);
         Connector lfo_presets = Connector("lfo.presets", 6, 484, 13, 11, Connector::SWITCH, Connector::LFO_MENU)
                   .withBackground(IDB_LFO_PRESET_MENU);

         Connector lfo_main_panel = Connector("lfo.main.panel", 28, 478, Connector::GROUP);
            Connector rate = Connector("lfo.rate", 0, 0).asHorizontal().inParent("lfo.main.panel");
            Connector phase = Connector("lfo.phase", 0, 21).asHorizontal().inParent("lfo.main.panel");
            Connector deform = Connector("lfo.deform", 0, 42).asHorizontal().inParent("lfo.main.panel");
            Connector amplitude = Connector("lfo.amplitude", 0, 63).asHorizontal().inParent("lfo.main.panel");

         Connector trigger_mode = Connector("lfo.trigger_mode", 172, 484, 51, 39, Connector::HSWITCH2)
                   .withHSwitch2Properties(IDB_LFO_TRIGGER_MODE, 3, 3, 1);
         Connector unipolar = Connector("lfo.unipolar", 172, 546, 51, 14, Connector::SWITCH)
                   .withBackground(IDB_LFO_UNIPOLAR);

         // combined LFO shape AND LFO display - TODO: split them to individual connectors for 1.9!
         Connector shape = Connector("lfo.shape", 235, 480, 359, 84, Connector::LFO);

         Connector mseg_editor = Connector("lfo.mseg_editor", 597, 484, 11, 11, Connector::SWITCH, Connector::MSEG_EDITOR_OPEN)
                   .withBackground(IDB_LFO_MSEG_EDIT);

         Connector lfo_eg_panel = Connector("lfo.envelope.panel", 616, 493, Connector::GROUP);
            Connector delay = Connector("lfo.delay", 0, 0).inParent("lfo.envelope.panel");
            Connector attack = Connector("lfo.attack", 20, 0).inParent("lfo.envelope.panel");
            Connector hold = Connector("lfo.hold", 40, 0).inParent("lfo.envelope.panel");
            Connector decay = Connector("lfo.decay", 60, 0).inParent("lfo.envelope.panel");
            Connector sustain = Connector("lfo.sustain", 80, 0).inParent("lfo.envelope.panel");
            Connector release = Connector("lfo.release", 100, 0).inParent("lfo.envelope.panel");
      }

      /*
       * We have a collection of controls which don't bind to parameters but which instead
       * have a non-parameter type. These attach to SurgeGUIEditor by virtue of having the
       * non-parameter type indicating the action, and the variables are not referenced
       * except here at construction time and optionally via their names in the skin engine
       */
      namespace OtherControls
      {
         Connector surge_menu = Connector("controls.surge_menu", 844, 550, 50, 15, Connector::HSWITCH2, Connector::SURGE_MENU)
                   .withProperty(Connector::BACKGROUND, IDB_MAIN_MENU)
                   .withProperty(Connector::DRAGGABLE_HSWITCH, false);

         Connector patch_browser = Connector("controls.patch_browser", 157, 12, 390, 28, Connector::CUSTOM, Connector::PATCH_BROWSER);
         Connector patch_category_jog = Connector("controls.category.prevnext", 157, 42, Connector::JOG_PATCHCATEGORY).asJogPlusMinus();
         Connector patch_jog = Connector("controls.patch.prevnext", 246, 42, Connector::JOG_PATCH).asJogPlusMinus();
         Connector patch_store = Connector("controls.patch.store", 510, 42, 37, 12, Connector::HSWITCH2, Connector::STORE_PATCH)
                   .withHSwitch2Properties(IDB_STORE_PATCH, 1, 1, 1)
                   .withProperty(Connector::DRAGGABLE_HSWITCH, false);

         Connector status_panel = Connector("controls.status.panel", 562, 12, Connector::GROUP);
         Connector status_mpe = Connector("controls.status.mpe", 0, 0, 31, 12, Connector::SWITCH, Connector::STATUS_MPE)
                   .withBackground(IDB_MPE_BUTTON).inParent("controls.status.panel");
         Connector status_tune = Connector("controls.status.tune", 0, 15, 31, 12, Connector::SWITCH, Connector::STATUS_TUNE)
                   .withBackground(IDB_TUNE_BUTTON).inParent("controls.status.panel");
         Connector status_zoom = Connector("controls.status.zoom", 0, 30, 31, 12, Connector::SWITCH, Connector::STATUS_ZOOM)
                   .withBackground(IDB_ZOOM_BUTTON).inParent("controls.status.panel");

         Connector vu_meter = Connector("controls.vu_meter", 763, 15, 123, 13, Connector::VU_METER, Connector::MAIN_VU_METER);

         Connector mseg_editor = Connector("msegeditor.window", 0, 57, 750, 365, Connector::CUSTOM, Connector::MSEG_EDITOR_WINDOW);

         // modulation panel is special, so it shows up as 'CUSTOM' with no connector and is special-cased in SurgeGUIEditor
         Connector modulation_panel = Connector("controls.modulation.panel", 2, 402, Connector::CUSTOM);
      }
   }
}