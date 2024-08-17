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

#ifndef SURGE_SRC_COMMON_SKINMODEL_H
#define SURGE_SRC_COMMON_SKINMODEL_H
#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
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
 * And finally, we don't want the core of surge to reference VSTGUI.
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
 * share a skin identity. For instance, all of the oscillator pitch sliders (of which there
 * are 6 as of 1.8.0) are a single point of skin identity. Thus attaching them to skin
 * data models in SurgePatch makes sense.
 *
 * And I took comfort that this data is completely free of any rendering code. Nothing
 * here links or requires vstgui etc... This is pure data about naming (via skin ids
 * and object namespaces) and defaults.
 */

namespace Surge
{
namespace Skin
{

namespace Parameters
{

enum NumberfieldControlModes
{
    NONE = 0,
    POLY_COUNT,
    PB_DEPTH,
    NOTENAME,
    MIDICHANNEL_FROM_127,
    MSEG_SNAP_H,
    MSEG_SNAP_V,
    WTSE_RESOLUTION,
    WTSE_FRAMES
};
}

struct Component
{
    /*
     * We could have done something much clevere with templates and per-class enums
     * but decided not to. Instead here is an enum which is the union of all possible
     * properties that an item can have.
     */
    enum Properties
    {
        X = 1001,
        Y,
        W,
        H,

        BACKGROUND,
        HOVER_IMAGE,
        HOVER_ON_IMAGE,
        IMAGE, // Note for most components "image" binds to "background" but some have it separate

        ROWS,
        COLUMNS,
        FRAMES,
        FRAME_OFFSET,
        DRAGGABLE_SWITCH,
        MOUSEWHEELABLE_SWITCH,
        ACCESSIBLE_AS_MOMENTARY_BUTTON,

        NUMBERFIELD_CONTROLMODE,

        BACKGROUND_COLOR,
        FRAME_COLOR,

        SLIDER_TRAY,
        HANDLE_IMAGE,
        HANDLE_HOVER_IMAGE,
        HANDLE_TEMPOSYNC_IMAGE,
        HANDLE_TEMPOSYNC_HOVER_IMAGE,
        HIDE_SLIDER_LABEL,

        CONTROL_TEXT,
        FONT_SIZE,
        FONT_STYLE,
        FONT_FAMILY,
        TEXT,
        TEXT_ALIGN,
        TEXT_ALL_CAPS,
        TEXT_COLOR,
        TEXT_HOVER_COLOR,
        TEXT_HOFFSET,
        TEXT_VOFFSET,

        GLYPH_PLACEMENT,
        GLYPH_W,
        GLYPH_H,
        GLPYH_ACTIVE,
        GLYPH_IMAGE,
        GLYPH_HOVER_IMAGE
    };

    Component() noexcept;
    explicit Component(const std::string &internalClassname) noexcept;
    ~Component();

    struct Payload
    {
        unsigned int id = -1;
        std::unordered_map<Properties, std::vector<std::string>> propertyNamesMap;
        std::unordered_map<Properties, std::string> propertyDocString;
        std::unordered_set<Properties> hasPropertySet;
        std::string internalClassname;
        std::unordered_set<std::string> aliases;

        // Do groups outside of the property since they have references
        std::shared_ptr<Payload> groupLeader;
        std::vector<std::shared_ptr<Payload>> group; // only correct in the leader
        bool isGrouped;
    };
    std::shared_ptr<Payload> payload;

    /*
     * To allow aliasing and backwards compatibility, a property binds to the skin engine
     * with multiple names for a given class.
     */
    Component &withProperty(Properties p, const std::initializer_list<std::string> &names,
                            const std::string &s = "add doc")
    {
        payload->propertyNamesMap[p] = names;
        payload->propertyDocString[p] = s;
        payload->hasPropertySet.insert(p);
        return *this;
    }

    Component &withAlias(const std::string &s)
    {
        payload->aliases.insert(s);
        return *this;
    }

    bool hasAlias(const std::string &s)
    {
        return payload->aliases.find(s) != payload->aliases.end();
    }

    bool hasProperty(Properties p)
    {
        return payload->hasPropertySet.find(p) != payload->hasPropertySet.end();
    }

    bool operator==(const Component &that) const { return payload->id == that.payload->id; }
    bool operator!=(const Component &that) const { return payload->id != that.payload->id; }

    static std::vector<int> allComponentIds();
    static Component componentById(int);
    static std::string propertyEnumToString(Properties p);
};

namespace Components
{
extern Component None, Slider, MultiSwitch, Switch, FilterSelector, LFODisplay, OscMenu, FxMenu,
    NumberField, VuMeter, Custom, Group, Label, WaveShaperSelector;
}

struct Connector
{
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
        JOG_WAVESHAPE,
        ANALYZE_WAVESHAPE,
        ANALYZE_FILTERS,

        SAVE_PATCH,
        SAVE_PATCH_DIALOG,

        STATUS_MPE,
        STATUS_TUNE,
        STATUS_ZOOM,

        ACTION_UNDO,
        ACTION_REDO,

        LFO_LABEL,
        FXPRESET_LABEL,

        PATCH_BROWSER,
        FX_SELECTOR,

        MAIN_VU_METER,

        MSEG_EDITOR_WINDOW,
        MSEG_EDITOR_OPEN,

        LFO_MENU,

        FILTER_ANALYSIS_WINDOW,
        OSCILLOSCOPE_WINDOW,
        WAVESHAPER_ANALYSIS_WINDOW,
        FORMULA_EDITOR_WINDOW,
        WTSCRIPT_EDITOR_WINDOW,
        TUNING_EDITOR_WINDOW,
        MOD_LIST_WINDOW,

        N_NONCONNECTED
    };

    Connector() noexcept;
    ~Connector() = default;

    Connector(const std::string &id, float x, float y) noexcept;
    Connector(const std::string &id, float x, float y, const Component &c) noexcept;
    Connector(const std::string &id, float x, float y, float w, float h,
              const Component &c) noexcept;
    Connector(const std::string &id, float x, float y, float w, float h, const Component &c,
              NonParameterConnection n) noexcept;
    Connector(const std::string &id, float x, float y, NonParameterConnection n) noexcept;

    static Connector connectorByID(const std::string &id);
    static Connector connectorByNonParameterConnection(NonParameterConnection n);
    static std::vector<Connector> connectorsByComponentType(const Component &c);

    static std::vector<std::string> allConnectorIDs();

    Connector &withControlStyle(unsigned int flags) noexcept
    {
        payload->controlStyleFlags |= flags;
        return *this;
    }

    Connector &asVertical() noexcept { return withControlStyle(Surge::ParamConfig::kVertical); }
    Connector &asHorizontal() noexcept { return withControlStyle(Surge::ParamConfig::kHorizontal); }
    Connector &asWhite() noexcept { return withControlStyle(Surge::ParamConfig::kWhite); }

    Connector &asMixerSolo() noexcept;
    Connector &asMixerMute() noexcept;
    Connector &asMixerRoute() noexcept;

    Connector &asJogPlusMinus() noexcept;

    Connector &withProperty(Component::Properties p, const std::string &v)
    {
        payload->properties[p] = v;
        return *this;
    }

    Connector &withProperty(Component::Properties p, int v)
    {
        return withProperty(p, std::to_string(v));
    }

    Connector &withHSwitch2Properties(int bgid, int frames, int rows, int cols)
    {
        return withProperty(Component::BACKGROUND, bgid)
            .withProperty(Component::FRAMES, frames)
            .withProperty(Component::ROWS, rows)
            .withProperty(Component::COLUMNS, cols);
    }

    Connector &withBackground(int v) { return withProperty(Component::BACKGROUND, v); }

    Connector &inParent(std::string g)
    {
        payload->parentId = g;
        return *this;
    }

    Connector &asStackedGroupLeader();
    Connector &inStackedGroup(const Connector &leader);

    std::string getProperty(Component::Properties p)
    {
        if (payload->properties.find(p) != payload->properties.end())
            return payload->properties[p];
        return "";
    }

    struct Payload
    {
        std::string id{"unknown"};
        float posx = -1, posy = -1;
        float w = -1, h = -1;
        int controlStyleFlags = 0;
        Component defaultComponent = Surge::Skin::Components::None;
        NonParameterConnection nonParamConnection = PARAMETER_CONNECTED;
        std::string parentId = "";
        std::unordered_map<Component::Properties, std::string>
            properties; // since we are base for XML where it's all strings
    };
    std::shared_ptr<Payload> payload;
    Connector(std::shared_ptr<Payload> p) : payload(p) {}
};

namespace AEG
{
extern Surge::Skin::Connector attack, attack_shape, decay, decay_shape, mode, release,
    release_shape, sustain;
}
namespace FEG
{
extern Surge::Skin::Connector attack, attack_shape, decay, decay_shape, mode, release,
    release_shape, sustain;
}
namespace FX
{
extern Surge::Skin::Connector fx_jog, fxPresetLabel, fx_selector, fx_param_panel;
extern Surge::Skin::Connector fx_type, param_1, param_10, param_11, param_12, param_2, param_3,
    param_4, param_5, param_6, param_7, param_8, param_9;
} // namespace FX
namespace Filter
{
extern Surge::Skin::Connector balance, config, cutoff_1, cutoff_2, envmod_1, envmod_2,
    f2_link_resonance, f2_offset_mode, feedback, highpass, keytrack_1, keytrack_2, resonance_1,
    resonance_2, subtype_1, subtype_2, type_1, type_2, waveshaper_drive, waveshaper_type,
    waveshaper_jog, waveshaper_analyze, filter_analyze;
}
namespace Global
{
extern Surge::Skin::Connector active_scene, character, fx1_return, fx2_return, fx3_return,
    fx4_return, fx_bypass, fx_disable, master_volume, scene_mode;
}
namespace LFO
{
extern Surge::Skin::Connector amplitude, attack, decay, deform, delay, hold, phase, rate, release,
    shape, sustain, trigger_mode, unipolar;
extern Surge::Skin::Connector lfo_presets, mseg_editor, lfo_title_label;
} // namespace LFO
namespace Mixer
{
extern Surge::Skin::Connector level_noise, level_o1, level_o2, level_o3, level_prefiltergain,
    level_ring12, level_ring23, mute_noise, mute_o1, mute_o2, mute_o3, mute_ring12, mute_ring23,
    route_noise, route_o1, route_o2, route_o3, route_ring12, route_ring23, solo_noise, solo_o1,
    solo_o2, solo_o3, solo_ring12, solo_ring23;
}
namespace Osc
{
extern Surge::Skin::Connector osc_display;
extern Surge::Skin::Connector osc_select;

extern Surge::Skin::Connector keytrack, octave, osc_type, param_1, param_2, param_3, param_4,
    param_5, param_6, param_7, pitch, retrigger;
} // namespace Osc
namespace Scene
{
extern Surge::Skin::Connector polylimit, splitpoint, drift, fmdepth, fmrouting, gain, keytrack_root,
    noise_color, octave, pan, pbrange_dn, pbrange_up, pitch, playmode, portatime, send_fx_1,
    send_fx_2, send_fx_3, send_fx_4, vel_sensitivity, volume, width;
}

namespace ModSources
{
// Still have design work to do here
}

namespace OtherControls
{
extern Surge::Skin::Connector surge_menu;
// extern Surge::Skin::Connector fxSelect;
extern Surge::Skin::Connector patch_category_jog, patch_jog, patch_save;

extern Surge::Skin::Connector status_mpe, status_zoom, status_tune;
extern Surge::Skin::Connector action_undo, action_redo;

extern Surge::Skin::Connector vu_meter;

// These active labels are actually controls

extern Surge::Skin::Connector patch_browser;

extern Surge::Skin::Connector mseg_editor, formula_editor, wtscript_editor, tuning_editor;

extern Surge::Skin::Connector mod_list;

extern Surge::Skin::Connector filter_analysis, oscilloscope, ws_analysis;

extern Surge::Skin::Connector save_patch_dialog;

// In Surge 1.8, the modulation panel is moveable en-masse but individual modulators
// are not relocatable. This item gives you the location of the modulators
extern Surge::Skin::Connector modulation_panel;
}; // namespace OtherControls

} // namespace Skin

} // namespace Surge

#endif // SURGE_SRC_COMMON_SKINMODEL_H
