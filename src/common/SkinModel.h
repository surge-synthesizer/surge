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
#include <string>
#include <iostream>
#include <unordered_map>
#include <memory>
#include "SurgeParamConfig.h"
#include "SkinColors.h"

/*
 * # SKIN MODEL / SKIN / PARAMETER / SURGEGUIEDITOR COLLABORATION
 *
 * This is the developer documentation on the core architecture of the skinning and
 * UI engine in surge.
 *
 * The Surge UI comprises two things
 * - Items bound to a parameter. Pitch slider, say.
 * - Control items which do something. Oscillator Selector, say
 *
 * Each of these elements has some visual control with a callback
 * handler to do its action; and each is bound to a well named
 * control concept or parameter.
 *
 * Moreover, we want Surge UI to have a reasonable default compiled in,
 * and we want that default to be completely over-rideable in our VSTGUI
 * implementation.
 *
 * And finally, we don't want the core of surge to refernce VSTGUI.
 *
 * So that gets us the following set of collaborations
 *
 * Parameter: A parameter exposed as a DAW parameter with a name, value, type.
 *            This is a logical concept consumed by the DSP algos and so on
 * Connector: A connector is the expression of a default layout and control
 *            type. It has a UID, size, type, and other properties. See
 *            below for why this is not in src/common/gui. Most importantly
 *            the parameter is either declared as a global with a NonParameter
 *            connector type - which means it is an unbound control - or
 *            it is declared as a global and consumed in the constructor of Parameter
 * Skin: In src/common/gui. Skin maps a union of the Connectors and an XML document
 *       to give a runtime representation of the UI. It contains functions like
 *       "give me a renderable component by id". If the skin document is empty
 *       there is no difference content wise between a Surge::UI::Skin::Control
 *        and a Surge::Skin::Connector; but the Connector is just the starting
 *        point for XML parse so skins can override it
 * SurgeGUIEditor: As well as callbacks, SGE is now basically a loop over the skin
 *                 data structure connecting things.
 *
 * SO why is this in src/common and not src/common/gui?
 *
 * Well the reason is that surge lays out its UI by parameter. Basically it's
 * a renderer where parameters attach to UI components. That happens at parameter
 * creation time in SurgePatch (now) and that's a logical place to give something a
 * UI 'name'.
 *
 * This acts as the symbols for the UI names and those symbols carry reasonable
 * defaults for class and position which the skin engine can override.
 *
 * So our two choices were
 * 1. Have a collection of symbols without an UI information here and augment it
 *    separately with positions in src/common/gui or
 * 2. Have the data (but not the renderer) for the layout information in src/common
 *
 * I chose #2 mostly because #1 is especially tedious since a collection of parameters
 * share a skin identity. For instance, all of the oscilator pitch sliders (of which there
 * are 6 as of 1.8.0) are a single point of skin identity. Thus attaching them to skin
 * data models in SurgePatch makes sense.
 *
 * And I took comfort that this data is completely free of any rendering code. Nothing
 * here links or requires vstgui etc... This is pure data about naming (via skin ids
 * and object namespaces) and defaults.
 */

namespace Surge
{
   namespace Skin {

      struct Connector {
         enum Component {
            NONE,
            SLIDER,
            HSWITCH2,
            SWITCH,
            FILTERSELECTOR, // For now make this its own type even though internally it is a menuas
            LFO,
            OSCMENU, // For now these are distinct compoennts. Later they will be menusliders
            FXMENU,
            NUMBERFIELD,
            VU_METER,

            CUSTOM, // A special case which is handled by an if in C++ in SGE for now
            GROUP
         };

         enum Properties {
            BACKGROUND=1001,
            ROWS,
            COLUMNS,
            FRAMES,
            FRAME_OFFSET,
            NUMBERFIELD_CONTROLMODE,
            DRAGGABLE_HSWITCH,
            TEXT_COLOR,
            TEXT_HOVER_COLOR
         };

         /*
          * Some UI elements do not bind to a parameter. These special functions
          * have to identify themselves here so they can be appropriately bound
          * to actions. These are functional connections not widget types.
          */
         enum NonParameterConnection
         {
            PARAMETER_CONNECTED = 0,
            SURGE_MENU,
            OSCILLATOR_DISPLAY,
            OSCILLATOR_SELECT,
            JOG_PATCHCATEGORY,
            JOG_PATCH,
            JOG_FX,

            STORE_PATCH,

            STATUS_MPE,
            STATUS_TUNE,
            STATUS_ZOOM,

            LFO_LABEL,
            FXPRESET_LABEL,

            PATCH_BROWSER,
            FX_SELECTOR,

            MAIN_VU_METER,

            MSEG_EDITOR_WINDOW,
            MSEG_EDITOR_OPEN,

            LFO_MENU,

            N_NONCONNECTED
         };

         Connector() noexcept;
         ~Connector() = default;

         Connector( const std::string &id, float x, float y ) noexcept;
         Connector( const std::string &id, float x, float y, Component c ) noexcept;
         Connector( const std::string &id, float x, float y, float w, float h, Component c ) noexcept;
         Connector( const std::string &id, float x, float y, float w, float h, Component c, NonParameterConnection n ) noexcept;
         Connector( const std::string &id, float x, float y, NonParameterConnection n ) noexcept;

         static Connector connectorByID(const std::string &id);
         static Connector connectorByNonParameterConnection( NonParameterConnection n );
         static std::vector<Connector> connectorsByComponentType( Component c );

         static std::vector<std::string> allConnectorIDs();

         Connector & withControlStyle(unsigned int flags ) noexcept
         {
            payload->controlStyleFlags |= flags;
            return *this;
         }

         Connector & asVertical() noexcept {
            return withControlStyle(Surge::ParamConfig::kVertical);
         }
         Connector & asHorizontal() noexcept {
            return withControlStyle(Surge::ParamConfig::kHorizontal);
         }
         Connector & asWhite() noexcept {
            return withControlStyle(kWhite);
         }

         Connector & asMixerSolo() noexcept;
         Connector & asMixerMute() noexcept;
         Connector & asMixerRoute() noexcept;

         Connector & asJogPlusMinus() noexcept;

         Connector & withProperty( Properties p, std::string v )
         {
            payload->properties[p] = v;
            return *this;
         }

         Connector & withProperty( Properties p, int v )
         {
            return withProperty(p, std::to_string(v));
         }

         Connector & withHSwitch2Properties( int bgid, int frames, int rows, int cols )
         {
            return withProperty( Connector::BACKGROUND, bgid )
                     .withProperty( Connector::FRAMES, frames )
                     .withProperty( Connector::ROWS, rows )
                     .withProperty( Connector::COLUMNS, cols );
         }

         Connector & withBackground( int v )
         {
            return withProperty( Connector::BACKGROUND, v );
         }

         Connector & inParent( std::string g ) {
            payload->parentId = g;
            return *this;
         }

         std::string getProperty( Properties p )
         {
            if( payload->properties.find(p) != payload->properties.end() )
               return payload->properties[p];
            return "";
         }

         struct Payload
         {
            std::string id{"unknown"};
            float posx = -1, posy = -1;
            float w = -1, h = -1;
            int controlStyleFlags = 0;
            Component defaultComponent = NONE;
            NonParameterConnection nonParamConnection = PARAMETER_CONNECTED;
            std::string parentId = "";
            std::unordered_map<Properties,std::string> properties; // since we are base for XML where it's all strings
         };
         std::shared_ptr<Payload> payload;
         Connector(std::shared_ptr<Payload> p) : payload(p) {}

      };

      namespace AEG {
         extern Surge::Skin::Connector attack, attack_shape, decay, decay_shape, mode, release, release_shape, sustain;
      }
      namespace FEG {
         extern Surge::Skin::Connector attack, attack_shape, decay, decay_shape, mode, release, release_shape, sustain;
      }
      namespace FX {
         extern Surge::Skin::Connector fx_jog, fxPresetLabel, fx_selector, fx_param_panel;
         extern Surge::Skin::Connector fx_type, param_1, param_10, param_11, param_12, param_2, param_3, param_4, param_5, param_6, param_7, param_8, param_9;
      }
      namespace Filter {
         extern Surge::Skin::Connector balance, config, cutoff_1, cutoff_2, envmod_1, envmod_2, f2_link_resonance, f2_offset_mode, feedback, highpass,
          keytrack_1, keytrack_2, resonance_1, resonance_2, subtype_1, subtype_2, type_1, type_2, waveshaper_drive, waveshaper_type;
      }
      namespace Global {
         extern Surge::Skin::Connector active_scene, character, fx1_return, fx2_return, fx_bypass, fx_disable, master_volume, scene_mode;
      }
      namespace LFO {
         extern Surge::Skin::Connector amplitude, attack, decay, deform, delay, hold, phase, rate, release, shape, sustain, trigger_mode, unipolar;
         extern Surge::Skin::Connector lfo_presets, mseg_editor, lfo_title_label;
      }
      namespace Mixer {
         extern Surge::Skin::Connector level_noise, level_o1, level_o2, level_o3, level_prefiltergain, level_ring12, level_ring23,
          mute_noise, mute_o1, mute_o2, mute_o3, mute_ring12, mute_ring23, route_noise, route_o1, route_o2, route_o3, route_ring12,
          route_ring23, solo_noise, solo_o1, solo_o2, solo_o3, solo_ring12, solo_ring23;
      }
      namespace Osc {
         extern Surge::Skin::Connector osc_display;
         extern Surge::Skin::Connector osc_select;

         extern Surge::Skin::Connector keytrack, octave, osc_type, param_1, param_2, param_3, param_4, param_5, param_6, param_7, pitch, retrigger;
      }
      namespace Scene {
         extern Surge::Skin::Connector polylimit, splitpoint, drift, fmdepth, fmrouting, gain, keytrack_root, noise_color, octave, pan, pbrange_dn, pbrange_up,
          pitch, playmode, portatime, send_fx_1, send_fx_2, vel_sensitivity, volume, width;
      }

      namespace ModSources {
         // Still have design work to do here
      }

      namespace OtherControls {
         extern Surge::Skin::Connector surge_menu;
         // extern Surge::Skin::Connector fxSelect;
         extern Surge::Skin::Connector patch_category_jog, patch_jog, storePatch;

         extern Surge::Skin::Connector status_mpe, status_zoom, status_tune;

         extern Surge::Skin::Connector vu_meter;

         // These active labels are actually controls

         extern Surge::Skin::Connector patch_browser;

         extern Surge::Skin::Connector mseg_editor;

         // In surge 1.8, the modulation panel is moveable en-masse but individual modulators
         // are not relocatable. This item gives you the location of the modulators
         extern Surge::Skin::Connector modulation_panel;
      };

   }


}
