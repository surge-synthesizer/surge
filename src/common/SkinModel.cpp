/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "SkinModel.h"
#include "resource.h"

using Surge::Skin::Connector;
using namespace Surge::Skin::Components;

/*
 * This file, in conjunction with the SVG assets it references by ID, is a complete
 * description of the surge default UI layout. The action callbacks bind to it and
 * the engine to lay it out are the collaboration betweeh this, SkinSupport, and SurgeGUIEditor
 */

namespace Surge
{
namespace Skin
{

namespace Components
{
Component None = Component("NONE").withAlias("None").withAlias("none");

Component MultiSwitch =
    Component("CHSwitch2")
        .withAlias("MultiSwitch")
        .withProperty(Component::ROWS, {"rows"}, {"Number of rows in the switch"})
        .withProperty(Component::COLUMNS, {"cols", "columns"}, {"Number of columns in the switch"})
        .withProperty(
            Component::FRAMES, {"frames"},
            {"Number of frames in the graphical asset for the switch (based on rows and columns)"})
        .withProperty(Component::FRAME_OFFSET, {"frame_offset"},
                      {"Used in case when a graphical asset for the switch has multiple variations "
                       "to reach to, valid values are from 0 to 'frames'"})
        .withProperty(Component::BACKGROUND, {"image", "bg_resource", "bg_id"},
                      {"Base image of the switch"})
        .withProperty(Component::DRAGGABLE_SWITCH, {"draggable"},
                      {"Is the switch draggable as a slider via mouse/touch or not. Valid values: "
                       "true, false"})
        .withProperty(
            Component::MOUSEWHEELABLE_SWITCH, {"mousewheelable"},
            {"Is the switch mousewheelable as a slider via mouse/touch or not. Valid values: "
             "true, false"})
        .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, {"accessible_as_buttons"},
                      {"Is the accessible display buttons (true) or radio buttons (false, def)"})
        .withProperty(Component::HOVER_IMAGE, {"hover_image"},
                      {"Hover image of the switch - required if you "
                       "set the base image and want feedback on mouse hover"})
        .withProperty(Component::HOVER_ON_IMAGE, {"hover_on_image"},
                      {"Hover image of the switch used when mouse hovers over the currently "
                       "selected value on the switch"});

Component Slider =
    Component("CSurgeSlider")
        .withAlias("Slider")
        .withProperty(Component::SLIDER_TRAY, {"slider_tray", "handle_tray"},
                      {"Background image of the slider, contains the slider tray/groove only"})
        .withProperty(Component::HANDLE_IMAGE, {"handle_image"}, {"Slider handle image"})
        .withProperty(
            Component::HANDLE_HOVER_IMAGE, {"handle_hover_image"},
            {"Hovered slider handle image - required if you want feedback on mouse hover"})
        .withProperty(Component::HANDLE_TEMPOSYNC_IMAGE, {"handle_temposync_image"},
                      {"Slider handle image when parameter is tempo-synced"})
        .withProperty(Component::HANDLE_TEMPOSYNC_HOVER_IMAGE, {"handle_temposync_hover_image"},
                      {"Hovered slider handle image when parameter is tempo-synced - required if "
                       "you want feedback on mouse hover"})
        .withProperty(Component::FONT_SIZE, {"font_size"}, {"Font size in points, integer only"})
        .withProperty(Component::FONT_FAMILY, {"font_family"}, {"Font family ttf from skin"})
        .withProperty(Component::FONT_STYLE, {"font_style"},
                      {"Valid values: normal, bold, italic, underline, strikethrough"})
        .withProperty(Component::TEXT_ALIGN, {"text_align"}, {"Valid values: left, center, right"})
        .withProperty(Component::TEXT_HOFFSET, {"text_hoffset"},
                      {"Horizontal offset of parameter name label, integer only"})
        .withProperty(Component::TEXT_VOFFSET, {"text_voffset"},
                      {"Vertical offset of parameter name label, integer only"})
        .withProperty(Component::HIDE_SLIDER_LABEL, {"hide_slider_label"},
                      {"Hides the parameter name label"});

Component Switch =
    Component("CSwitchControl")
        .withAlias("Switch")
        .withProperty(Component::BACKGROUND, {"image", "bg_resource", "bg_id"},
                      {"Base image of the switch"})
        .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, {"accessible_as_buttons"},
                      {"Is the accessible display buttons (true) or radio buttons (false, def)"});

Component FilterSelector =
    Component("FilterSelector")
        .withProperty(Component::BACKGROUND, {"image"},
                      {"Base image of the filter type menu itself; it is offset by the glyph if "
                       "glyph is active"})
        .withProperty(Component::HOVER_IMAGE, {"hover_image"},
                      {"Hover image of the filter type menu - required if you "
                       "set the base image and want feedback on mouse hover"})
        .withProperty(Component::GLYPH_IMAGE, {"glyph_image"},
                      {"Resource name of the glyph (filter type icons) image"})
        .withProperty(
            Component::GLYPH_HOVER_IMAGE, {"glyph_hover_image"},
            {"Resource name of the glyph (filter type icons) image that shows on mouse hover"})
        .withProperty(Component::GLPYH_ACTIVE, {"glyph_active"},
                      {"Show or hide the filter type icons glyph. Valid values: true, false"})
        .withProperty(Component::GLYPH_PLACEMENT, {"glyph_placement"},
                      {"Valid values: above, below, left, right"})
        .withProperty(Component::GLYPH_W, {"glyph_w"}, {"Width of one glyph frame in pixels"})
        .withProperty(Component::GLYPH_H, {"glyph_h"}, {"Height of one glyph frame in pixels"});

Component LFODisplay = Component("CLFOGui").withAlias("LFODisplay");

Component OscMenu =
    Component("COSCMenu")
        .withAlias("OscMenu")
        .withProperty(Component::FONT_SIZE, {"font_size"}, {"Font size in points, integer only"})
        .withProperty(Component::FONT_STYLE, {"font_style"},
                      {"Valid values: normal, bold, italic, underline, strikethrough"})
        .withProperty(Component::TEXT_ALIGN, {"text_align"}, {"Valid values: left, center, right"})
        .withProperty(Component::TEXT_ALL_CAPS, {"text_allcaps"},
                      {"Makes the oscillator menu text all caps. Valid values: true, false"})
        .withProperty(Component::TEXT_HOFFSET, {"text_hoffset"},
                      {"Horizontal offset of oscillator menu text label, integer only"})
        .withProperty(Component::TEXT_VOFFSET, {"text_voffset"},
                      {"Vertical offset of oscillator menu text label, integer only"});

Component FxMenu = Component("CFXMenu").withAlias("FxMenu");

Component NumberField =
    Component("CNumberField")
        .withAlias("NumberField")
        .withProperty(Component::BACKGROUND, {"image", "bg_resource", "bg_id"},
                      {"Base image of the numberfield"})
        .withProperty(Component::NUMBERFIELD_CONTROLMODE, {"numberfield_controlmode"},
                      {"Control mode of the numberfield. Refer to the list of ctrl_mode constants "
                       "in SurgeParamConfig.h"})
        .withProperty(Component::TEXT_COLOR, {"text_color"},
                      {"Valid color formats are #RRGGBB, #RRGGBBAA, and named colors"})
        .withProperty(Component::TEXT_HOVER_COLOR, {"text_color.hover"},
                      {"Valid color formats are #RRGGBB, #RRGGBBAA, and named colors"});

Component VuMeter = Component("CVuMeter").withAlias("VuMeter");

Component Custom = Component("--CUSTOM--");

Component Group = Component("--GROUP--");

/*
 * We could treat label like everything else but we didn't in S1.7/1.8 and Skin V1
 * so we are stuck with this slight special casing
 */
Component Label =
    Component("Internal Label")
        .withProperty(Component::TEXT, {"text"}, {"Arbitrary text to be displayed on the label."})
        .withProperty(Component::CONTROL_TEXT, {"control_text"},
                      {"Text tied to a particular Surge XT parameter. Use skin component connector "
                       "name as a value. Overrules arbitrary text set with 'text' property"})
        .withProperty(Component::TEXT_ALIGN, {"text_align"}, {"Valid values: left, center, right"})
        .withProperty(Component::FONT_SIZE, {"font_size"}, {"Font size in points, integer only"})
        .withProperty(Component::FONT_STYLE, {"font_style"},
                      {"Valid values: normal, bold, italic, underline, strikethrough"})
        .withProperty(Component::TEXT_COLOR, {"color", "text_color"},
                      {"Valid color formats are #RRGGBB, #RRGGBBAA, and named colors"})
        .withProperty(Component::BACKGROUND_COLOR, {"bg_color"},
                      {"Background color of the label. Valid color formats are #RRGGBB, "
                       "#RRGGBBAA, or named colors"})
        .withProperty(Component::FRAME_COLOR, {"frame_color"},
                      {"Color of the label frame/border. Valid color formats are #RRGGBB, "
                       "#RRGGBBAA, and named colors"})
        .withProperty(Component::IMAGE, {"image"},
                      {"Resource name of the image to be displayed by the label. Overrides "
                       "background and frame/border colors"});

// ToDo - obviously expand properties
Component WaveShaperSelector = Component("WaveShaperSelector");

} // namespace Components

namespace Global
{
Connector active_scene = Connector("global.active_scene", 7, 12, 40, 42, Components::MultiSwitch)
                             .withHSwitch2Properties(IDB_SCENE_SELECT, 2, 2, 1);
Connector scene_mode = Connector("global.scene_mode", 54, 12, 40, 42, Components::MultiSwitch)
                           .withHSwitch2Properties(IDB_SCENE_MODE, 4, 4, 1);
Connector fx_disable = Connector("global.fx_disable", 0, 0);
Connector fx_bypass = Connector("global.fx_bypass", 607, 12, 135, 27, Components::MultiSwitch)
                          .withHSwitch2Properties(IDB_FX_GLOBAL_BYPASS, 4, 1, 4);
Connector character = Connector("global.character", 607, 42, 135, 12, Components::MultiSwitch)
                          .withHSwitch2Properties(IDB_OSC_CHARACTER, 3, 1, 3);
Connector master_volume = Connector("global.volume", 760, 29);

Connector fx1_return = Connector("global.fx1_return", 763, 141);
Connector fx2_return = Connector("global.fx2_return", 763, 162);
Connector fx3_return = Connector("global.fx3_return", 763, 141);
Connector fx4_return = Connector("global.fx4_return", 763, 162);
} // namespace Global

namespace Scene
{
Connector polylimit =
    Connector("scene.polylimit", 100, 41, 43, 14, Components::NumberField)
        .withBackground(IDB_NUMFIELD_POLY_SPLIT)
        .withProperty(Component::NUMBERFIELD_CONTROLMODE, Surge::Skin::Parameters::POLY_COUNT)
        .withProperty(Component::TEXT_COLOR, "scene.split_poly.text")
        .withProperty(Component::TEXT_HOVER_COLOR, "scene.split_poly.text.hover");
Connector splitpoint =
    Connector("scene.splitpoint", 100, 11, 43, 14, Components::NumberField)
        .withBackground(IDB_NUMFIELD_POLY_SPLIT)
        .withProperty(Component::TEXT_COLOR, "scene.split_poly.text")
        .withProperty(Component::TEXT_HOVER_COLOR, "scene.split_poly.text.hover");
// this doesn't have a cm since it is special

Connector pbrange_dn =
    Connector("scene.pbrange_dn", 159, 110, 30, 13, Components::NumberField)
        .withProperty(Component::NUMBERFIELD_CONTROLMODE, Surge::Skin::Parameters::PB_DEPTH)
        .withBackground(IDB_NUMFIELD_PITCHBEND)
        .withProperty(Component::TEXT_COLOR, "scene.pbrange.text")
        .withProperty(Component::TEXT_HOVER_COLOR, "scene.pbrange.text.hover");
Connector pbrange_up =
    Connector("scene.pbrange_up", 187, 110, 30, 13, Components::NumberField)
        .withProperty(Component::NUMBERFIELD_CONTROLMODE, Surge::Skin::Parameters::PB_DEPTH)
        .withBackground(IDB_NUMFIELD_PITCHBEND)
        .withProperty(Component::TEXT_COLOR, "scene.pbrange.text")
        .withProperty(Component::TEXT_HOVER_COLOR, "scene.pbrange.text.hover");
Connector playmode = Connector("scene.playmode", 239, 87, 50, 47, Components::MultiSwitch)
                         .withHSwitch2Properties(IDB_PLAY_MODE, 6, 6, 1);
Connector drift = Connector("scene.drift", 156, 141).asHorizontal().asWhite();
Connector noise_color = Connector("scene.noise_color", 156, 162).asHorizontal().asWhite();

Connector fmrouting = Connector("scene.fmrouting", 309, 83, 134, 52, Components::MultiSwitch)
                          .withHSwitch2Properties(IDB_OSC_FM_ROUTING, 4, 1, 4);
Connector fmdepth = Connector("scene.fmdepth", 306, 152).asHorizontal().asWhite();

Connector octave = Connector("scene.octave", 202, 194, 96, 18, Components::MultiSwitch)
                       .withHSwitch2Properties(IDB_SCENE_OCTAVE, 7, 1, 7);
Connector pitch = Connector("scene.pitch", 156, 212).asHorizontal();
Connector portatime = Connector("scene.portamento", 156, 234).asHorizontal();

Connector keytrack_root =
    Connector("scene.keytrack_root", 311, 266, 43, 14, Components::NumberField)
        .withBackground(IDB_NUMFIELD_KEYTRACK_ROOT)
        .withProperty(Component::NUMBERFIELD_CONTROLMODE, Surge::Skin::Parameters::NOTENAME)
        .withProperty(Component::TEXT_COLOR, "scene.keytrackroot.text")
        .withProperty(Component::TEXT_HOVER_COLOR, "scene.keytrackroot.text.hover");

Connector scene_output_panel =
    Connector("scene.output.panel", 606, 78, Components::Group).asVertical();
Connector volume =
    Connector("scene.volume", 0, 0).asHorizontal().asWhite().inParent("scene.output.panel");
Connector pan =
    Connector("scene.pan", 0, 21).asHorizontal().asWhite().inParent("scene.output.panel");
Connector width =
    Connector("scene.width", 0, 42).asHorizontal().asWhite().inParent("scene.output.panel");
Connector send_fx_1 = Connector("scene.send_fx_1", 0, 63)
                          .asHorizontal()
                          .asWhite()
                          .inParent("scene.output.panel")
                          .asStackedGroupLeader();
Connector send_fx_2 = Connector("scene.send_fx_2", 0, 84)
                          .asHorizontal()
                          .asWhite()
                          .inParent("scene.output.panel")
                          .asStackedGroupLeader();
Connector send_fx_3 = Connector("scene.send_fx_3", 0, 63)
                          .asHorizontal()
                          .asWhite()
                          .inParent("scene.output.panel")
                          .inStackedGroup(send_fx_1);
Connector send_fx_4 = Connector("scene.send_fx_4", 0, 84)
                          .asHorizontal()
                          .asWhite()
                          .inParent("scene.output.panel")
                          .inStackedGroup(send_fx_2);

Connector vel_sensitivity =
    Connector("scene.velocity_sensitivity", 699, 301).asVertical().asWhite();
Connector gain = Connector("scene.gain", 719, 301).asVertical().asWhite();
} // namespace Scene

namespace Osc
{
Connector osc_select =
    Connector("osc.select", 66, 69, 75, 13, Components::MultiSwitch, Connector::OSCILLATOR_SELECT)
        .withHSwitch2Properties(IDB_OSC_SELECT, 3, 1, 3);
Connector osc_display =
    Connector("osc.display", 4, 81, 141, 99, Components::Custom, Connector::OSCILLATOR_DISPLAY);

Connector keytrack =
    Connector("osc.keytrack", 3, 181, 47, 10, Components::Switch).withBackground(IDB_OSC_KEYTRACK);
Connector retrigger = Connector("osc.retrigger", 51, 181, 47, 10, Components::Switch)
                          .withBackground(IDB_OSC_RETRIGGER);

Connector octave = Connector("osc.octave", 0, 194, 96, 18, Components::MultiSwitch)
                       .withHSwitch2Properties(IDB_OSC_OCTAVE, 7, 1, 7);
Connector osc_type = Connector("osc.type", 96, 194, 49, 18, Components::OscMenu)
                         .withProperty(Component::FONT_SIZE, 8)
                         .withProperty(Component::FONT_STYLE, "bold")
                         .withProperty(Component::TEXT_ALL_CAPS, "true")
                         .withProperty(Component::TEXT_ALIGN, "center")
                         .withProperty(Component::TEXT_HOFFSET, -2)
                         .withProperty(Component::TEXT_VOFFSET, -1);

Connector osc_param_panel = Connector("osc.param.panel", 6, 212, Components::Group);
Connector pitch = Connector("osc.pitch", 0, 0).asHorizontal().inParent("osc.param.panel");
Connector param_1 = Connector("osc.param_1", 0, 22).asHorizontal().inParent("osc.param.panel");
Connector param_2 = Connector("osc.param_2", 0, 44).asHorizontal().inParent("osc.param.panel");
Connector param_3 = Connector("osc.param_3", 0, 66).asHorizontal().inParent("osc.param.panel");
Connector param_4 = Connector("osc.param_4", 0, 88).asHorizontal().inParent("osc.param.panel");
Connector param_5 = Connector("osc.param_5", 0, 110).asHorizontal().inParent("osc.param.panel");
Connector param_6 = Connector("osc.param_6", 0, 132).asHorizontal().inParent("osc.param.panel");
Connector param_7 = Connector("osc.param_7", 0, 154).asHorizontal().inParent("osc.param.panel");
} // namespace Osc

namespace Mixer
{
Connector mixer_panel = Connector("mixer.panel", 154, 263, Components::Group);
Connector mute_o1 = Connector("mixer.mute_o1", 1, 0).asMixerMute().inParent("mixer.panel");
Connector mute_o2 = Connector("mixer.mute_o2", 21, 0).asMixerMute().inParent("mixer.panel");
Connector mute_o3 = Connector("mixer.mute_o3", 41, 0).asMixerMute().inParent("mixer.panel");
Connector mute_ring12 = Connector("mixer.mute_ring12", 61, 0).asMixerMute().inParent("mixer.panel");
Connector mute_ring23 = Connector("mixer.mute_ring23", 81, 0).asMixerMute().inParent("mixer.panel");
Connector mute_noise = Connector("mixer.mute_noise", 101, 0).asMixerMute().inParent("mixer.panel");

Connector solo_o1 = Connector("mixer.solo_o1", 1, 11).asMixerSolo().inParent("mixer.panel");
Connector solo_o2 = Connector("mixer.solo_o2", 21, 11).asMixerSolo().inParent("mixer.panel");
Connector solo_o3 = Connector("mixer.solo_o3", 41, 11).asMixerSolo().inParent("mixer.panel");
Connector solo_ring12 =
    Connector("mixer.solo_ring12", 61, 11).asMixerSolo().inParent("mixer.panel");
Connector solo_ring23 =
    Connector("mixer.solo_ring23", 81, 11).asMixerSolo().inParent("mixer.panel");
Connector solo_noise = Connector("mixer.solo_noise", 101, 11).asMixerSolo().inParent("mixer.panel");

Connector route_o1 = Connector("mixer.route_o1", 1, 22).asMixerRoute().inParent("mixer.panel");
Connector route_o2 = Connector("mixer.route_o2", 21, 22).asMixerRoute().inParent("mixer.panel");
Connector route_o3 = Connector("mixer.route_o3", 41, 22).asMixerRoute().inParent("mixer.panel");
Connector route_ring12 =
    Connector("mixer.route_ring12", 61, 22).asMixerRoute().inParent("mixer.panel");
Connector route_ring23 =
    Connector("mixer.route_ring23", 81, 22).asMixerRoute().inParent("mixer.panel");
Connector route_noise =
    Connector("mixer.route_noise", 101, 22).asMixerRoute().inParent("mixer.panel");

Connector level_o1 =
    Connector("mixer.level_o1", 0, 38).asVertical().asWhite().inParent("mixer.panel");
Connector level_o2 =
    Connector("mixer.level_o2", 20, 38).asVertical().asWhite().inParent("mixer.panel");
Connector level_o3 =
    Connector("mixer.level_o3", 40, 38).asVertical().asWhite().inParent("mixer.panel");
Connector level_ring12 =
    Connector("mixer.level_ring12", 60, 38).asVertical().asWhite().inParent("mixer.panel");
Connector level_ring23 =
    Connector("mixer.level_ring23", 80, 38).asVertical().asWhite().inParent("mixer.panel");
Connector level_noise =
    Connector("mixer.level_noise", 100, 38).asVertical().asWhite().inParent("mixer.panel");

Connector level_prefiltergain =
    Connector("mixer.level_prefiltergain", 120, 38).asVertical().asWhite().inParent("mixer.panel");
} // namespace Mixer

namespace Filter
{
Connector config = Connector("filter.config", 455, 83, 134, 52, Components::MultiSwitch)
                       .withHSwitch2Properties(IDB_FILTER_CONFIG, 8, 1, 8);
Connector feedback = Connector("filter.feedback", 457, 152).asHorizontal().asWhite();

// FIXME - we should really expose the menu slider fully but for now make a composite FILTERSELECTOR
Connector type_1 = Connector("filter.type_1", 305, 191, 124, 21, Components::FilterSelector);
Connector subtype_1 = Connector("filter.subtype_1", 432, 194, 12, 18, Components::Switch)
                          .withBackground(IDB_FILTER_SUBTYPE);
Connector cutoff_1 = Connector("filter.cutoff_1", 306, 212).asHorizontal();
Connector resonance_1 = Connector("filter.resonance_1", 306, 234).asHorizontal();

Connector balance = Connector("filter.balance", 456, 224).asHorizontal();

Connector type_2 = Connector("filter.type_2", 605, 191, 124, 21, Components::FilterSelector);
Connector subtype_2 = Connector("filter.subtype_2", 732, 194, 12, 18, Components::Switch)
                          .withBackground(IDB_FILTER_SUBTYPE);
Connector cutoff_2 = Connector("filter.cutoff_2", 600, 212).asHorizontal();
Connector resonance_2 = Connector("filter.resonance_2", 600, 234).asHorizontal();

Connector f2_offset_mode = Connector("filter.f2_offset_mode", 734, 217, 12, 18, Components::Switch)
                               .withBackground(IDB_FILTER2_OFFSET);
Connector f2_link_resonance =
    Connector("filter.f2_link_resonance", 734, 239, 12, 18, Components::Switch)
        .withBackground(IDB_FILTER2_RESONANCE_LINK);

Connector keytrack_1 = Connector("filter.keytrack_1", 309, 301).asVertical().asWhite();
Connector keytrack_2 = Connector("filter.keytrack_2", 329, 301).asVertical().asWhite();

Connector envmod_1 = Connector("filter.envmod_1", 549, 301).asVertical().asWhite();
Connector envmod_2 = Connector("filter.envmod_2", 569, 301).asVertical().asWhite();

Connector waveshaper_drive = Connector("filter.waveshaper_drive", 421, 301).asVertical().asWhite();
Connector waveshaper_type =
    Connector("filter.waveshaper_type", 386, 310, 34, 47, Components::WaveShaperSelector)
        .withHSwitch2Properties(IDB_WAVESHAPER_MODE, 6, 6, 1);
Connector waveshaper_jog =
    Connector("filter.waveshaper_prevnext", 387, 359, Connector::JOG_WAVESHAPE).asJogPlusMinus();
Connector waveshaper_analyze = Connector("filter.waveshaper_preview", 408, 375, 11, 11,
                                         Components::Switch, Connector::ANALYZE_WAVESHAPE)
                                   .withBackground(IDB_WAVESHAPER_ANALYSIS);
Connector filter_analyze = Connector("filter.filter_preview", 519, 197, 11, 11, Components::Switch,
                                     Connector::ANALYZE_FILTERS)
                               .withBackground(IDB_FILTER_ANALYSIS);

Connector highpass = Connector("filter.highpass", 353, 301).asVertical().asWhite();
} // namespace Filter

namespace FEG
{
// If we never refer to it by type we can just construct it and don't need it in extern list
Connector feg_panel = Connector("feg.panel", 459, 267, Components::Group);
Connector mode = Connector("feg.mode", 93, 0, 34, 18, Components::MultiSwitch)
                     .withHSwitch2Properties(IDB_ENV_MODE, 2, 2, 1)
                     .inParent("feg.panel");

Connector attack_shape = Connector("feg.attack_shape", 4, 0, 20, 18, Components::MultiSwitch)
                             .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3)
                             .inParent("feg.panel");
Connector decay_shape = Connector("feg.decay_shape", 24, 0, 20, 18, Components::MultiSwitch)
                            .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3)
                            .withProperty(Component::FRAME_OFFSET, 3)
                            .inParent("feg.panel");
Connector release_shape = Connector("feg.release_shape", 65, 0, 20, 18, Components::MultiSwitch)
                              .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3)
                              .withProperty(Component::FRAME_OFFSET, 6)
                              .inParent("feg.panel");

Connector attack = Connector("feg.attack", 0, 34).asVertical().asWhite().inParent("feg.panel");
Connector decay = Connector("feg.decay", 20, 34).asVertical().asWhite().inParent("feg.panel");
Connector sustain = Connector("feg.sustain", 40, 34).asVertical().asWhite().inParent("feg.panel");
Connector release = Connector("feg.release", 60, 34).asVertical().asWhite().inParent("feg.panel");
} // namespace FEG

namespace AEG
{
Connector aeg_panel = Connector("aeg.panel", 609, 267, Components::Group);
Connector mode = Connector("aeg.mode", 93, 0, 34, 18, Components::MultiSwitch)
                     .withHSwitch2Properties(IDB_ENV_MODE, 2, 2, 1)
                     .inParent("aeg.panel");

Connector attack_shape = Connector("aeg.attack_shape", 4, 0, 20, 18, Components::MultiSwitch)
                             .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3)
                             .inParent("aeg.panel");
;
Connector decay_shape = Connector("aeg.decay_shape", 24, 0, 20, 18, Components::MultiSwitch)
                            .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3)
                            .withProperty(Component::FRAME_OFFSET, 3)
                            .inParent("aeg.panel");
;
Connector release_shape = Connector("aeg.release_shape", 65, 0, 20, 18, Components::MultiSwitch)
                              .withHSwitch2Properties(IDB_ENV_SHAPE, 3, 1, 3)
                              .withProperty(Component::FRAME_OFFSET, 6)
                              .inParent("aeg.panel");
;

Connector attack = Connector("aeg.attack", 0, 34).asVertical().asWhite().inParent("aeg.panel");
;
Connector decay = Connector("aeg.decay", 20, 34).asVertical().asWhite().inParent("aeg.panel");
;
Connector sustain = Connector("aeg.sustain", 40, 34).asVertical().asWhite().inParent("aeg.panel");
;
Connector release = Connector("aeg.release", 60, 34).asVertical().asWhite().inParent("aeg.panel");
;
} // namespace AEG

namespace FX
{
Connector fx_selector =
    Connector("fx.selector", 760, 74, 139, 57, Components::Custom, Connector::FX_SELECTOR);
Connector fx_type = Connector("fx.type", 765, 194, 133, 18, Components::FxMenu);

Connector fx_preset =
    Connector("fx.preset.name", 766, 212, 95, 12, Components::Custom, Connector::FXPRESET_LABEL);
Connector fx_jog = Connector("fx.preset.prevnext", 864, 212, Connector::JOG_FX).asJogPlusMinus();

Connector fx_param_panel = Connector("fx.param.panel", 763, 224, Components::Group);
Connector param_1 = Connector("fx.param_1", 0, 0).inParent("fx.param.panel");
Connector param_2 = Connector("fx.param_2", 0, 20).inParent("fx.param.panel");
Connector param_3 = Connector("fx.param_3", 0, 40).inParent("fx.param.panel");
Connector param_4 = Connector("fx.param_4", 0, 60).inParent("fx.param.panel");
Connector param_5 = Connector("fx.param_5", 0, 80).inParent("fx.param.panel");
Connector param_6 = Connector("fx.param_6", 0, 100).inParent("fx.param.panel");
Connector param_7 = Connector("fx.param_7", 0, 120).inParent("fx.param.panel");
Connector param_8 = Connector("fx.param_8", 0, 140).inParent("fx.param.panel");
Connector param_9 = Connector("fx.param_9", 0, 160).inParent("fx.param.panel");
Connector param_10 = Connector("fx.param_10", 0, 180).inParent("fx.param.panel");
Connector param_11 = Connector("fx.param_11", 0, 200).inParent("fx.param.panel");
Connector param_12 = Connector("fx.param_12", 0, 220).inParent("fx.param.panel");
} // namespace FX

namespace LFO
{
// For now these two have component 'CUSTOM' and we hadn't pick a component in the code
Connector lfo_title_label =
    Connector("lfo.title", 6, 498, 11, 71, Components::Custom, Connector::LFO_LABEL);
Connector lfo_presets =
    Connector("lfo.presets", 6, 484, 13, 11, Components::Switch, Connector::LFO_MENU)
        .withBackground(IDB_LFO_PRESET_MENU)
        .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, true);

Connector lfo_main_panel = Connector("lfo.main.panel", 28, 478, Components::Group);
Connector rate = Connector("lfo.rate", 0, 0).asHorizontal().inParent("lfo.main.panel");
Connector phase = Connector("lfo.phase", 0, 21).asHorizontal().inParent("lfo.main.panel");
Connector deform = Connector("lfo.deform", 0, 42).asHorizontal().inParent("lfo.main.panel");
Connector amplitude = Connector("lfo.amplitude", 0, 63).asHorizontal().inParent("lfo.main.panel");

Connector trigger_mode = Connector("lfo.trigger_mode", 172, 484, 51, 39, Components::MultiSwitch)
                             .withHSwitch2Properties(IDB_LFO_TRIGGER_MODE, 3, 3, 1);
Connector unipolar = Connector("lfo.unipolar", 172, 546, 51, 14, Components::Switch)
                         .withBackground(IDB_LFO_UNIPOLAR);

// combined LFO shape AND LFO display
// TODO: split them to individual skin connectors at some point!
Connector shape = Connector("lfo.shape", 235, 480, 359, 84, Components::LFODisplay);

Connector mseg_editor =
    Connector("lfo.mseg_editor", 597, 484, 11, 11, Components::Switch, Connector::MSEG_EDITOR_OPEN)
        .withBackground(IDB_LFO_MSEG_EDIT)
        .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, true);

Connector lfo_eg_panel = Connector("lfo.envelope.panel", 616, 493, Components::Group);
Connector delay = Connector("lfo.delay", 0, 0).inParent("lfo.envelope.panel");
Connector attack = Connector("lfo.attack", 20, 0).inParent("lfo.envelope.panel");
Connector hold = Connector("lfo.hold", 40, 0).inParent("lfo.envelope.panel");
Connector decay = Connector("lfo.decay", 60, 0).inParent("lfo.envelope.panel");
Connector sustain = Connector("lfo.sustain", 80, 0).inParent("lfo.envelope.panel");
Connector release = Connector("lfo.release", 100, 0).inParent("lfo.envelope.panel");
} // namespace LFO

/*
 * We have a collection of controls which don't bind to parameters but which instead
 * have a non-parameter type. These attach to SurgeGUIEditor by virtue of having the
 * non-parameter type indicating the action, and the variables are not referenced
 * except here at construction time and optionally via their names in the skin engine
 */
namespace OtherControls
{
Connector surge_menu = Connector("controls.surge_menu", 848, 550, 50, 15, Components::MultiSwitch,
                                 Connector::SURGE_MENU)
                           .withProperty(Component::BACKGROUND, IDB_MAIN_MENU)
                           .withProperty(Component::MOUSEWHEELABLE_SWITCH, false)
                           .withProperty(Component::DRAGGABLE_SWITCH, false);

Connector patch_browser = Connector("controls.patch_browser", 157, 12, 390, 28, Components::Custom,
                                    Connector::PATCH_BROWSER);
Connector patch_category_jog =
    Connector("controls.category.prevnext", 157, 42, Connector::JOG_PATCHCATEGORY).asJogPlusMinus();
Connector patch_jog =
    Connector("controls.patch.prevnext", 246, 42, Connector::JOG_PATCH).asJogPlusMinus();

Connector action_undo =
    Connector("controls.action.undo", 433, 42, 16, 12, Components::Switch, Connector::ACTION_UNDO)
        .withBackground(IDB_UNDO_BUTTON)
        .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, true);
Connector action_redo =
    Connector("controls.action.redo", 448, 42, 16, 12, Components::Switch, Connector::ACTION_REDO)
        .withBackground(IDB_REDO_BUTTON)
        .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, true);

Connector patch_save = Connector("controls.patch.save", 510, 42, 37, 12, Components::MultiSwitch,
                                 Connector::SAVE_PATCH)
                           .withHSwitch2Properties(IDB_SAVE_PATCH, 1, 1, 1)
                           .withProperty(Component::MOUSEWHEELABLE_SWITCH, false)
                           .withProperty(Component::DRAGGABLE_SWITCH, false)
                           .withProperty(Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, true);

Connector status_panel = Connector("controls.status.panel", 562, 12, Components::Group);
Connector status_mpe =
    Connector("controls.status.mpe", 0, 0, 31, 12, Components::Switch, Connector::STATUS_MPE)
        .withBackground(IDB_MPE_BUTTON)
        .inParent("controls.status.panel");
Connector status_tune =
    Connector("controls.status.tune", 0, 15, 31, 12, Components::Switch, Connector::STATUS_TUNE)
        .withBackground(IDB_TUNE_BUTTON)
        .inParent("controls.status.panel");
Connector status_zoom =
    Connector("controls.status.zoom", 0, 30, 31, 12, Components::Switch, Connector::STATUS_ZOOM)
        .withBackground(IDB_ZOOM_BUTTON)
        .inParent("controls.status.panel");

Connector vu_meter =
    Connector("controls.vu_meter", 767, 15, 123, 13, Components::VuMeter, Connector::MAIN_VU_METER);

Connector mseg_editor = Connector("msegeditor.window", 0, 58, 750, 365, Components::Custom,
                                  Connector::MSEG_EDITOR_WINDOW);

Connector formula_editor = Connector("formulaeditor.window", 0, 58, 750, 365, Components::Custom,
                                     Connector::FORMULA_EDITOR_WINDOW);

Connector wtscript_editor = Connector("wtscripteditor.window", 0, 58, 750, 511, Components::Custom,
                                      Connector::WTSCRIPT_EDITOR_WINDOW);

Connector tuning_editor = Connector("tuningeditor.window", 0, 58, 750, 511, Components::Custom,
                                    Connector::TUNING_EDITOR_WINDOW);

Connector mod_list =
    Connector("modlist.window", 148, 58, 602, 511, Components::Custom, Connector::MOD_LIST_WINDOW);

Connector filter_analysis = Connector("filter.filter_analysis.window", 300, 263, 450, 212,
                                      Components::Custom, Connector::FILTER_ANALYSIS_WINDOW);

Connector oscilloscope = Connector("oscilloscope.window", 0, 58, 750, 365, Components::Custom,
                                   Connector::OSCILLOSCOPE_WINDOW);

Connector ws_analysis = Connector("filter.waveshaper_analysis.window", 450, 237, 300, 160,
                                  Components::Custom, Connector::WAVESHAPER_ANALYSIS_WINDOW);

Connector save_patch_dialog = Connector("controls.patch.save.window", 157, 57, 390, 300,
                                        Components::Custom, Connector::SAVE_PATCH_DIALOG);

// modulation panel is special, so it shows up as 'CUSTOM' with no connector and is special-cased in
// SurgeGUIEditor
Connector modulation_panel = Connector("controls.modulation.panel", 3, 402, Components::Custom);
} // namespace OtherControls
} // namespace Skin
} // namespace Surge
