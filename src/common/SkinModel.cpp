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
      namespace FEG { // DONE
         // If we never refer to it by type we can just construct it and don't need it in extern list
         Connector fegPanel = Connector( "FEG.panel", 459, 269, Connector::GROUP );

         Connector mode = Connector( "FEG.mode", 93, 0, 34, 15, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_ENVMODE, 2, 2, 1 )
             .withProperty( Connector::COLUMNS, 1 )
             .inParent("FEG.panel");

         Connector attack_shape = Connector( "FEG.attack_shape", 0, 1, 20, 14, Connector::HSWITCH2 )
                                     .withHSwitch2Properties(IDB_ENVSHAPE, 3, 1, 3 ).inParent("FEG.panel");
         Connector decay_shape = Connector( "FEG.decay_shape", 20, 1, 20, 14, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_ENVSHAPE, 3, 1, 3 )
             .withProperty( Connector::IMGOFFSET, 3 ).inParent("FEG.panel");
         Connector release_shape = Connector( "FEG.release_shape", 60, 1, 20, 14, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_ENVSHAPE, 3, 1, 3 )
             .withProperty( Connector::IMGOFFSET, 6 ).inParent("FEG.panel");

         Connector attack = Connector( "FEG.attack", 0, 31 ).asVertical().asWhite().inParent("FEG.panel");
         Connector decay = Connector( "FEG.decay", 20, 31 ).asVertical().asWhite().inParent("FEG.panel");
         Connector sustain = Connector( "FEG.sustain", 40, 31 ).asVertical().asWhite().inParent("FEG.panel");
         Connector release = Connector( "FEG.release", 60, 31 ).asVertical().asWhite().inParent("FEG.panel");

      }

      namespace AEG { // DONE
         Connector aegPanel = Connector( "AEG.panel", 609, 269, Connector::GROUP );

         Connector mode = Connector( "AEG.mode", 93, 0, 34, 15, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_ENVMODE, 2, 2, 1 ).inParent( "AEG.panel" );

         Connector attack_shape = Connector( "AEG.attack_shape", 0, 1, 20, 14, Connector::HSWITCH2 )
                                      .withHSwitch2Properties( IDB_ENVSHAPE, 3, 1, 3 )
                                      .withProperty(Connector::COLUMNS, 3 ).inParent( "AEG.panel" );;
         Connector decay_shape = Connector( "AEG.decay_shape", 20, 1, 20, 14, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_ENVSHAPE, 3, 1, 3 )
             .withProperty( Connector::IMGOFFSET, 3 ).inParent( "AEG.panel" );;
         Connector release_shape = Connector( "AEG.release_shape", 60, 1, 20, 14, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_ENVSHAPE, 3, 1, 3 )
             .withProperty( Connector::IMGOFFSET, 6 ).inParent( "AEG.panel" );;

         Connector attack = Connector( "AEG.attack", 0, 31 ).asVertical().asWhite().inParent( "AEG.panel" );;
         Connector decay = Connector( "AEG.decay", 20, 31 ).asVertical().asWhite().inParent( "AEG.panel" );;
         Connector sustain = Connector( "AEG.sustain", 40, 31 ).asVertical().asWhite().inParent( "AEG.panel" );;
         Connector release = Connector( "AEG.release", 60, 31 ).asVertical().asWhite().inParent( "AEG.panel" );;
      }

      namespace FX { // DONE except VU but still with the terrible API
         Connector fxJog = Connector( "FX.jog", 759+131-39, 182+20, Connector::JOG_FX ).asJogPlusMinus();
         Connector fxPreset = Connector( "FX.presetlabel", 759, 203, 91, 12, Connector::CUSTOM, Connector::FXPRESET_LABEL );
         Connector fxSelector = Connector( "FX.selector", 767, 71, 123, 51, Connector::CUSTOM, Connector::FX_SELECTOR );
         Connector fxtype = Connector( "FX.fxtype", 765, 182, 131, 15, Connector::FXMENU );

         Connector fxParamPanel = Connector( "FX.param.panel", 759, 214, Connector::GROUP );
         Connector param_1 = Connector( "FX.param_1", 0, 0 ).inParent( "FX.param.panel" );
         Connector param_2 = Connector( "FX.param_2", 0, 20 ).inParent( "FX.param.panel" );
         Connector param_3 = Connector( "FX.param_3", 0, 40 ).inParent( "FX.param.panel" );
         Connector param_4 = Connector( "FX.param_4", 0, 60 ).inParent( "FX.param.panel" );
         Connector param_5 = Connector( "FX.param_5", 0, 80 ).inParent( "FX.param.panel" );
         Connector param_6 = Connector( "FX.param_6", 0, 100 ).inParent( "FX.param.panel" );
         Connector param_7 = Connector( "FX.param_7", 0, 120 ).inParent( "FX.param.panel" );
         Connector param_8 = Connector( "FX.param_8", 0, 140 ).inParent( "FX.param.panel" );
         Connector param_9 = Connector( "FX.param_9", 0, 160 ).inParent( "FX.param.panel" );
         Connector param_10 = Connector( "FX.param_10", 0, 180 ).inParent( "FX.param.panel" );
         Connector param_11 = Connector( "FX.param_11", 0, 200 ).inParent( "FX.param.panel" );
         Connector param_12 = Connector( "FX.param_12", 0, 220 ).inParent( "FX.param.panel" );
      }

      namespace Filter {
         Connector balance = Connector("filter.balance", 456, 224).asHorizontal();
         Connector config = Connector( "filter.config", 455, 89, 134, 52, Connector::HSWITCH2 )
               .withHSwitch2Properties(IDB_FBCONFIG, 8, 1, 8 );
         Connector cutoff_1 = Connector( "filter.cutoff_1", 306, 213 ).asHorizontal();
         Connector cutoff_2 = Connector( "filter.cutoff_2", 600, 213 ).asHorizontal();
         Connector envmod_1 = Connector( "filter.envmod_1", 549, 300 ).asVertical().asWhite();
         Connector envmod_2 = Connector( "filter.envmod_2", 569, 300 ).asVertical().asWhite();
         Connector f2_link_resonance = Connector( "filter.f2_link_resonance", 735, 239, 12, 18, Connector::SWITCH )
               .withBackground(IDB_SWITCH_LINK);
         Connector f2_offset_mode = Connector( "filter.f2_offset_mode", 735, 218, 12, 18, Connector::SWITCH )
               .withBackground(IDB_RELATIVE_TOGGLE);
         Connector feedback = Connector("filter.feedback", 457, 162)
                                  .asHorizontal()
                                  .asWhite();
         Connector highpass = Connector( "filter.highpass", 354, 300 ).asVertical().asWhite();
         Connector keytrack_1 = Connector( "filter.keytrack_1", 309, 300 ).asVertical().asWhite();
         Connector keytrack_2 = Connector( "filter.keytrack_2", 329, 300 ).asVertical().asWhite();
         Connector resonance_1 = Connector( "filter.resonance_1", 306, 234 ).asHorizontal();
         Connector resonance_2 = Connector( "filter.resonance_2", 600, 234 ).asHorizontal();
         Connector subtype_1 = Connector( "filter.subtype_1", 432, 194, 12, 18, Connector::SWITCH )
               .withBackground(IDB_FILTERSUBTYPE);
         Connector subtype_2 = Connector( "filter.subtype_2", 732, 194, 12, 18, Connector::SWITCH )
               .withBackground(IDB_FILTERSUBTYPE);
         // FIXME - we should really expose the menu slider fully but for now make a composite FILTERSELECTOR
         Connector type_1 = Connector( "filter.type_1", 305, 190, 124, 21, Connector::FILTERSELECTOR );
         Connector type_2 = Connector( "filter.type_2", 605, 190, 124, 21, Connector::FILTERSELECTOR );
         Connector waveshaper_drive = Connector( "filter.waveshaper_drive", 419, 300 ).asVertical().asWhite();
         Connector waveshaper_type = Connector( "filter.waveshaper_type", 388, 313, 28, 47, Connector::HSWITCH2 )
               .withHSwitch2Properties(IDB_WAVESHAPER, 6, 6, 1 );
      }

      namespace Global {
         Connector active_scene = Connector( "global.active_scene", 7, 15, 51, 27, Connector::HSWITCH2 )
                  .withHSwitch2Properties(IDB_SCENESWITCH, 2, 1, 2 );
         Connector character = Connector( "global.character", 607, 41, 135, 12, Connector::HSWITCH2 )
             .withHSwitch2Properties(IDB_CHARACTER, 3, 1, 3 );
         Connector fx1_return = Connector( "global.fx1_return", 759, 124 ); // check
         Connector fx2_return = Connector( "global.fx2_return", 759, 145 ); // check
         Connector fx_bypass = Connector( "global.fx_bypass", 607, 12, 135, 27, Connector::HSWITCH2 )
             .withHSwitch2Properties( IDB_FXBYPASS, 4, 1, 4 );
         Connector fx_disable = Connector( "global.fx_disable", 0, 0 );
         Connector master_volume = Connector( "global.master_volume", 756, 29 ); //check
         Connector polylimit = Connector( "global.polylimit", 99, 32, 42, 14, Connector::NUMBERFIELD )
             .withProperty( Connector::NUMBERFIELD_CONTROLMODE, cm_polyphony );
         Connector scene_mode = Connector( "global.scene_mode", 62, 12, 36, 33, Connector::HSWITCH2 )
             .withHSwitch2Properties(IDB_SCENEMODE, 4, 4, 1 );
         Connector splitkey = Connector( "global.splitkey", 99, 10, 43, 14, Connector::NUMBERFIELD );
            // this doesn't have a cm since it is special
      }

      namespace LFO { // DONE
         Connector lfoRatePanel = Connector( "lfo.rate.panel", 23, 478, Connector::GROUP );
         Connector rate = Connector( "lfo.rate", 0, 0 ).asHorizontal().inParent("lfo.rate.panel");
         Connector phase = Connector( "lfo.phase", 0, 21 ).asHorizontal().inParent("lfo.rate.panel");
         Connector deform = Connector( "lfo.deform", 0, 42 ).asHorizontal().inParent("lfo.rate.panel");
         Connector amplitude = Connector( "lfo.amplitude", 0, 63 ).asHorizontal().inParent("lfo.rate.panel");

         Connector lfoEnvelopePanel = Connector( "lfo.envelope.panel", 613, 494, Connector::GROUP );
         Connector delay = Connector( "lfo.delay", 0, 0 ).inParent( "lfo.envelope.panel" );
         Connector attack = Connector( "lfo.attack", 20, 0 ).inParent( "lfo.envelope.panel" );
         Connector hold = Connector( "lfo.hold", 40, 0 ).inParent( "lfo.envelope.panel" );
         Connector decay = Connector( "lfo.decay", 60, 0 ).inParent( "lfo.envelope.panel" );
         Connector sustain = Connector( "lfo.sustain", 80, 0 ).inParent( "lfo.envelope.panel" );
         Connector release = Connector( "lfo.release", 100, 0 ).inParent( "lfo.envelope.panel" );

         Connector shape = Connector( "lfo.shape", 235, 482, 359, 85, Connector::LFO );
         Connector triggermode = Connector( "lfo.triggermode", 170, 484, 51, 39, Connector::HSWITCH2 )
             .withHSwitch2Properties(IDB_LFOTRIGGER, 3, 3, 1);
         Connector unipolar = Connector( "lfo.unipolar", 170, 545, 51, 15, Connector::SWITCH )
               .withBackground( IDB_UNIPOLAR );
      }

      namespace Mix { // Done
         Connector mixerPanel = Connector( "mix.panel", 154, 264, Connector::GROUP );

         Connector mute_o1 = Connector( "mix.mute_o1", 0, 0 ).asMixerMute().inParent( "mix.panel" );
         Connector mute_o2 = Connector( "mix.mute_o2", 20, 0 ).asMixerMute().inParent( "mix.panel" );
         Connector mute_o3 = Connector( "mix.mute_o3", 40, 0 ).asMixerMute().inParent( "mix.panel" );
         Connector mute_ring12 = Connector( "mix.mute_ring12", 60, 0 ).asMixerMute().inParent( "mix.panel" );
         Connector mute_ring23 = Connector( "mix.mute_ring23", 80, 0 ).asMixerMute().inParent( "mix.panel" );
         Connector mute_noise = Connector( "mix.mute_noise", 100, 0 ).asMixerMute().inParent( "mix.panel" );

         Connector solo_o1 = Connector( "mix.solo_o1", 0, 10 ).asMixerSolo().inParent( "mix.panel" );
         Connector solo_o2 = Connector( "mix.solo_o2", 20, 10 ).asMixerSolo().inParent( "mix.panel" );
         Connector solo_o3 = Connector( "mix.solo_o3", 40, 10 ).asMixerSolo().inParent( "mix.panel" );
         Connector solo_ring12 = Connector( "mix.solo_ring12", 60, 10 ).asMixerSolo().inParent( "mix.panel" );
         Connector solo_ring23 = Connector( "mix.solo_ring23", 80, 10 ).asMixerSolo().inParent( "mix.panel" );
         Connector solo_noise = Connector( "mix.solo_noise", 100, 10 ).asMixerSolo().inParent( "mix.panel" );

         Connector route_o1 = Connector( "mix.route_o1", 0, 20 ).asMixerRoute().inParent( "mix.panel" );
         Connector route_o2 = Connector( "mix.route_o2", 20, 20 ).asMixerRoute().inParent( "mix.panel" );
         Connector route_o3 = Connector( "mix.route_o3", 40, 20 ).asMixerRoute().inParent( "mix.panel" );
         Connector route_ring12 = Connector( "mix.route_ring12", 60, 20 ).asMixerRoute().inParent( "mix.panel" );
         Connector route_ring23 = Connector( "mix.route_ring23", 80, 20 ).asMixerRoute().inParent( "mix.panel" );
         Connector route_noise = Connector( "mix.route_noise", 100, 20 ).asMixerRoute().inParent( "mix.panel" );

         Connector level_o1 = Connector( "mix.level_o1", 0, 36 ).asVertical().asWhite().inParent( "mix.panel" );
         Connector level_o2 = Connector( "mix.level_o2", 20, 36 ).asVertical().asWhite().inParent( "mix.panel" );
         Connector level_o3 = Connector( "mix.level_o3", 40, 36 ).asVertical().asWhite().inParent( "mix.panel" );
         Connector level_ring12 = Connector( "mix.level_ring12", 60, 36 ).asVertical().asWhite().inParent( "mix.panel" );
         Connector level_ring23 = Connector( "mix.level_ring23", 80, 36 ).asVertical().asWhite().inParent( "mix.panel" );
         Connector level_noise = Connector( "mix.level_noise", 100, 36 ).asVertical().asWhite().inParent( "mix.panel" );

         Connector level_prefiltergain = Connector( "mix.level_prefiltergain", 120, 36 ).asVertical().asWhite().inParent( "mix.panel" );
      }

      namespace Osc { // All Done
         Connector oscillatorDisplay = Connector( "controls.oscillatordisplay", 6, 81, 136, 99, Connector::CUSTOM, Connector::OSCILLATOR_DISPLAY);
         Connector oscillatorSelect = Connector( "controls.oscillatorselect", 104-36, 69, 75, 13, Connector::HSWITCH2, Connector::OSCILLATOR_SELECT )
             .withHSwitch2Properties( IDB_OSCSELECT, 3, 1, 3 );

         Connector keytrack = Connector( "osc.keytrack", 8, 185, 43, 7, Connector::SWITCH )
               .withBackground( IDB_SWITCH_KTRK );
         Connector octave = Connector( "osc.octave", 3, 193, 96, 18, Connector::HSWITCH2 )
               .withHSwitch2Properties(IDB_OCTAVES_OSC, 7, 1, 7 );
         Connector retrigger = Connector( "osc.retrigger", 56, 185, 43, 7, Connector::SWITCH )
               .withBackground( IDB_SWITCH_RETRIGGER);

         Connector osctype = Connector( "osc.osctype", 105, 194, 41, 18, Connector::OSCMENU );

         Connector oscParamPanel = Connector( "osc.param.panel", 6, 213, Connector::GROUP );
         Connector pitch = Connector( "osc.pitch", 0, 0 ).asHorizontal().inParent("osc.param.panel");
         Connector param_1 = Connector( "osc.param_1", 0, 20 ).asHorizontal().inParent("osc.param.panel");
         Connector param_2 = Connector( "osc.param_2", 0, 40 ).asHorizontal().inParent("osc.param.panel");
         Connector param_3 = Connector( "osc.param_3", 0, 60 ).asHorizontal().inParent("osc.param.panel");
         Connector param_4 = Connector( "osc.param_4", 0, 80 ).asHorizontal().inParent("osc.param.panel");
         Connector param_5 = Connector( "osc.param_5", 0, 100 ).asHorizontal().inParent("osc.param.panel");
         Connector param_6 = Connector( "osc.param_6", 0, 120 ).asHorizontal().inParent("osc.param.panel");
         Connector param_7 = Connector( "osc.param_7", 0, 140 ).asHorizontal().inParent("osc.param.panel");
      }

      namespace Scene {
         Connector drift = Connector( "scene.drift", 156, 141 ).asHorizontal().asWhite();
         Connector fmdepth = Connector( "scene.fmdepth", 306, 162 ).asHorizontal().asWhite();
         Connector fmrouting = Connector( "scene.fmrouting", 309, 89, 134, 52, Connector::HSWITCH2 )
             .withHSwitch2Properties(IDB_FMCONFIG, 4, 1, 4 );
         Connector gain = Connector( "scene.gain", 699, 300 ).asVertical().asWhite();
         Connector keytrack_root = Connector( "scene.keytrack_root", 307, 272, 43, 14, Connector::NUMBERFIELD )
             .withProperty(Connector::NUMBERFIELD_CONTROLMODE, cm_notename );
         Connector noise_color = Connector( "scene.noise_color", 156, 162 ).asHorizontal().asWhite();
         Connector octave = Connector( "scene.octave", 202, 193, 96, 18, Connector::HSWITCH2 )
               .withHSwitch2Properties(IDB_OCTAVES, 7, 1, 7 );
         Connector pbrange_dn = Connector( "scene.pbrange_dn", 164, 112, 24, 10, Connector::NUMBERFIELD )
             .withProperty( Connector::NUMBERFIELD_CONTROLMODE, cm_pbdepth ).asWhite();
         Connector pbrange_up = Connector( "scene.pbrange_up", 189, 112, 24, 10, Connector::NUMBERFIELD )
             .withProperty( Connector::NUMBERFIELD_CONTROLMODE, cm_pbdepth ).asWhite();
         Connector pitch = Connector( "scene.pitch", 156, 213 ).asHorizontal();
         Connector playmode = Connector("scene.playmode", 239, 87, 50, 47, Connector::HSWITCH2)
                                  .withHSwitch2Properties(IDB_POLYMODE,6,6,1);
         Connector portatime = Connector( "scene.portatime", 156, 234 ).asHorizontal();
         Connector velocity_sensitivity = Connector( "scene.velocity_sensitivity", 719, 300 ).asVertical().asWhite();

         Connector sceneOutputPanel = Connector( "scene.output.panel", 606, 78, Connector::GROUP );
         Connector volume = Connector( "scene.volume", 0, 0 ).asHorizontal().asWhite().inParent( "scene.output.panel" );
         Connector pan = Connector( "scene.pan", 0, 21 ).asHorizontal().asWhite().inParent( "scene.output.panel" );
         Connector width = Connector( "scene.width", 0, 42 ).asHorizontal().asWhite().inParent( "scene.output.panel");
         Connector send_fx_1 = Connector( "scene.send_fx_1", 0, 63 ).asHorizontal().asWhite().inParent( "scene.output.panel" );
         Connector send_fx_2 = Connector( "scene.send_fx_2", 0, 84 ).asHorizontal().asWhite().inParent( "scene.output.panel" );
      }

      /*
       * We have a collection of controls whic hdon't bind to parameters but which instead
       * have a nonparameter type. These attach to SurgeGUIEditor by virtue of having the
       * nonparameter type indicating the action, and the variables are not referenced
       * except here at construction time and optionally via their names in the skin engine
       */
      namespace OtherControls {
         Connector surgeMenu = Connector( "controls.surgemenu", 844, 550, 50, 15, Connector::HSWITCH2, Connector::SURGE_MENU )
                                 .withProperty(Connector::BACKGROUND, IDB_BUTTON_MENU )
                                 .withProperty( Connector::DRAGABLE_HSWITCH, false );

         Connector patchCategoryJog = Connector( "controls.patch.categoryjog", 157, 41, Connector::JOG_PATCHCATEGORY).asJogPlusMinus();
         Connector patchJog = Connector( "controls.patch.jog", 242, 41, Connector::JOG_PATCH ).asJogPlusMinus();
         Connector patchStore = Connector( "controls.patch.store", 510, 41, 37, 12, Connector::HSWITCH2, Connector::STORE_PATCH)
                        .withHSwitch2Properties(IDB_BUTTON_STORE, 1, 1, 1 )
                        .withProperty( Connector::DRAGABLE_HSWITCH, false );

         Connector mainVUMeter = Connector( "controls.vumeter", 763, 14, 123, 13, Connector::VU_METER, Connector::MAIN_VU_METER);

         Connector statusPanel = Connector( "controls.status.panel", 562, 13, Connector::GROUP );
         Connector statusMPE = Connector( "controls.status.mpe", 0, 0, 31, 12, Connector::SWITCH, Connector::STATUS_MPE )
               .withBackground( IDB_MPE_BUTTON ).inParent( "controls.status.panel" );
         Connector statusTune = Connector( "controls.status.tune", 0, 14, 31, 12, Connector::SWITCH, Connector::STATUS_TUNE )
             .withBackground( IDB_TUNE_BUTTON ).inParent( "controls.status.panel");
         Connector statusZoom = Connector( "controls.status.zoom", 0, 28, 31, 12, Connector::SWITCH, Connector::STATUS_ZOOM )
             .withBackground( IDB_ZOOM_BUTTON ).inParent( "controls.status.panel");

         // For now these two have component 'CUSTOM' and we hadn pick a component in the code
         Connector lfoLabel = Connector( "controls.lfo.label", 6, 485, 11, 83, Connector::CUSTOM, Connector::LFO_LABEL );

         // modulationPanel is special so shows up as 'CUSTOM' with no connector and is special cased in SGE
         Connector modulationPanel = Connector( "controls.modulationpanel", 2, 402, Connector::CUSTOM );

         Connector patchBrowser = Connector( "controls.patchbrowser", 156, 11, 547-156, 28, Connector::CUSTOM, Connector::PATCH_BROWSER );
      }
   }
}
