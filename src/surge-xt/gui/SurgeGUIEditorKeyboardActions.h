/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2022 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#ifndef SURGE_SURGEGUIEDITORKEYBOARDACTIONS_H
#define SURGE_SURGEGUIEDITORKEYBOARDACTIONS_H

#include <string>
#include "SurgeGUIUtils.h"

namespace Surge
{
namespace GUI
{
enum KeyboardActions
{
    UNDO,
    REDO,

    SAVE_PATCH,
    FIND_PATCH,
    FAVORITE_PATCH,

    PREV_PATCH,
    NEXT_PATCH,
    PREV_CATEGORY,
    NEXT_CATEGORY,

    // TODO: UPDATE WHEN ADDING MORE OSCILLATORS
    OSC_1,
    OSC_2,
    OSC_3,

    // TODO: FIX SCENE ASSUMPTION
    TOGGLE_SCENE,
    TOGGLE_MODULATOR_ARM,

#if WINDOWS
    TOGGLE_DEBUG_CONSOLE,
#endif
    SHOW_KEYBINDINGS_EDITOR,
    SHOW_LFO_EDITOR,
    SHOW_MODLIST,
    SHOW_TUNING_EDITOR,
    TOGGLE_OSCILLOSCOPE,
    TOGGLE_VIRTUAL_KEYBOARD,

    VKB_OCTAVE_DOWN,
    VKB_OCTAVE_UP,
    VKB_VELOCITY_DOWN_10PCT,
    VKB_VELOCITY_UP_10PCT,

    ZOOM_TO_DEFAULT,
    ZOOM_PLUS_10,
    ZOOM_PLUS_25,
    ZOOM_MINUS_10,
    ZOOM_MINUS_25,

    FOCUS_NEXT_CONTROL_GROUP,
    FOCUS_PRIOR_CONTROL_GROUP,

    REFRESH_SKIN,
    SKIN_LAYOUT_GRID,

    OPEN_MANUAL,
    TOGGLE_ABOUT,

    n_kbdActions
};

inline std::string keyboardActionName(KeyboardActions a)
{
    switch (a)
    {
    case UNDO:
        return "UNDO";
    case REDO:
        return "REDO";

    case SAVE_PATCH:
        return "SAVE_PATCH";
    case FIND_PATCH:
        return "FIND_PATCH";
    case FAVORITE_PATCH:
        return "FAVORITE_PATCH";

    case PREV_PATCH:
        return "PREV_PATCH";
    case NEXT_PATCH:
        return "NEXT_PATCH";
    case PREV_CATEGORY:
        return "PREV_CATEGORY";
    case NEXT_CATEGORY:
        return "NEXT_CATEGORY";

    // TODO: UPDATE WHEN ADDING MORE OSCILLATORS
    case OSC_1:
        return "OSC_1";
    case OSC_2:
        return "OSC_2";
    case OSC_3:
        return "OSC_3";

    // TODO: FIX SCENE ASSUMPTION
    case TOGGLE_SCENE:
        return "TOGGLE_SCENE";
    case TOGGLE_MODULATOR_ARM:
        return "TOGGLE_MODULATOR_ARM";

#if WINDOWS
    case TOGGLE_DEBUG_CONSOLE:
        return "TOGGLE_DEBUG_CONSOLE";
#endif
    case SHOW_KEYBINDINGS_EDITOR:
        return "SHOW_KEYBINDINGS_EDITOR";
    case SHOW_LFO_EDITOR:
        return "SHOW_LFO_EDITOR";
    case SHOW_MODLIST:
        return "SHOW_MODLIST";
    case SHOW_TUNING_EDITOR:
        return "SHOW_TUNING_EDITOR";
    case TOGGLE_VIRTUAL_KEYBOARD:
        return "TOGGLE_VIRTUAL_KEYBOARD";
    case TOGGLE_OSCILLOSCOPE:
        return "TOGGLE_OSCILLOSCOPE";

    case VKB_OCTAVE_DOWN:
        return "VKB_OCTAVE_DOWN";
    case VKB_OCTAVE_UP:
        return "VKB_OCTAVE_UP";
    case VKB_VELOCITY_DOWN_10PCT:
        return "VKB_VELOCITY_DOWN_10%";
    case VKB_VELOCITY_UP_10PCT:
        return "VKB_VELOCITY_UP_10%";

    case ZOOM_TO_DEFAULT:
        return "ZOOM_TO_DEFAULT";
    case ZOOM_PLUS_10:
        return "ZOOM_PLUS_10";
    case ZOOM_PLUS_25:
        return "ZOOM_PLUS_25";
    case ZOOM_MINUS_10:
        return "ZOOM_MINUS_10";
    case ZOOM_MINUS_25:
        return "ZOOM_MINUS_25";

    case REFRESH_SKIN:
        return "REFRESH_SKIN";
    case SKIN_LAYOUT_GRID:
        return "SKIN_LAYOUT_GRID";

    case FOCUS_NEXT_CONTROL_GROUP:
        return "FOCUS_NEXT_CONTROL_GROUP";
    case FOCUS_PRIOR_CONTROL_GROUP:
        return "FOCUS_PRIOR_CONTROL_GROUP";

    case OPEN_MANUAL:
        return "OPEN_MANUAL";
    case TOGGLE_ABOUT:
        return "TOGGLE_ABOUT";

    case n_kbdActions:
        jassert(false);
        return "ERROR_NKBD";
    }

    jassert(false);
    return "ERROR_OVER";
}

// This is the user-facing stringification of an action.
inline std::string keyboardActionDescription(KeyboardActions a)
{
    std::string desc;
    bool skipOSCase = false;

    switch (a)
    {
    case UNDO:
        desc = "Undo";
        break;
    case REDO:
        desc = "Redo";
        break;

    case SAVE_PATCH:
        desc = "Save Patch";
        break;
    case FIND_PATCH:
        desc = "Find Patch";
        break;
    case FAVORITE_PATCH:
        desc = "Mark Patch as Favorite";
        break;

    case PREV_PATCH:
        desc = "Previous Patch";
        break;
    case NEXT_PATCH:
        desc = "Next Patch";
        break;
    case PREV_CATEGORY:
        desc = "Previous Category";
        break;
    case NEXT_CATEGORY:
        desc = "Next Category";
        break;

    // TODO: UPDATE WHEN ADDING MORE OSCILLATORS
    case OSC_1:
        desc = "Select Oscillator 1";
        break;
    case OSC_2:
        desc = "Select Oscillator 2";
        break;
    case OSC_3:
        desc = "Select Oscillator 3";
        break;

    // TODO: FIX SCENE ASSUMPTION
    case TOGGLE_SCENE:
        desc = "Toggle Scene A/B";
        break;

    case TOGGLE_MODULATOR_ARM:
        desc = "Toggle Modulator Armed State";
        break;

#if WINDOWS
    case TOGGLE_DEBUG_CONSOLE:
        desc = "Debug Console";
        break;
#endif
    case SHOW_KEYBINDINGS_EDITOR:
        desc = "Keyboard Shortcut Editor";
        break;
    case SHOW_LFO_EDITOR:
        desc = "LFO Editor (MSEG or Formula)";
        break;
    case SHOW_MODLIST:
        desc = "Modulation List";
        break;
    case SHOW_TUNING_EDITOR:
        desc = "Tuning Editor";
        break;
    case TOGGLE_VIRTUAL_KEYBOARD:
        desc = "Virtual Keyboard";
        break;
    case TOGGLE_OSCILLOSCOPE:
        desc = "Oscilloscope";
        break;

    case VKB_OCTAVE_DOWN:
        desc = Surge::GUI::toOSCase("Virtual Keyboard: ") + Surge::GUI::toOSCase("Octave Down");
        skipOSCase = true;
        break;
    case VKB_OCTAVE_UP:
        desc = Surge::GUI::toOSCase("Virtual Keyboard: ") + Surge::GUI::toOSCase("Octave Up");
        skipOSCase = true;
        break;
    case VKB_VELOCITY_DOWN_10PCT:
        desc =
            Surge::GUI::toOSCase("Virtual Keyboard: ") + Surge::GUI::toOSCase("Velocity Down 10%");
        skipOSCase = true;
        break;
    case VKB_VELOCITY_UP_10PCT:
        desc = Surge::GUI::toOSCase("Virtual Keyboard: ") + Surge::GUI::toOSCase("Velocity Up 10%");
        skipOSCase = true;
        break;

    case ZOOM_TO_DEFAULT:
        desc = "Zoom to Default";
        break;
    case ZOOM_PLUS_10:
        desc = "Zoom +10%";
        break;
    case ZOOM_PLUS_25:
        desc = "Zoom +25%";
        break;
    case ZOOM_MINUS_10:
        desc = "Zoom -10%";
        break;
    case ZOOM_MINUS_25:
        desc = "Zoom -25%";
        break;

    case REFRESH_SKIN:
        desc = "Refresh Skin";
        break;
    case SKIN_LAYOUT_GRID:
        desc = "Toggle Layout Grid";
        break;

    case FOCUS_NEXT_CONTROL_GROUP:
        desc = "Move Focus to Next Control Group";
        break;
    case FOCUS_PRIOR_CONTROL_GROUP:
        desc = "Move Focus to Previous Control Group";
        break;

    case OPEN_MANUAL:
        desc = "Open Manual";
        break;
    case TOGGLE_ABOUT:
        desc = "About Surge XT";
        skipOSCase = true;
        break;

    default:
        jassert(false);
        desc = "<Unknown Action>";
        break;
    };

    return skipOSCase ? desc : Surge::GUI::toOSCase(desc);
}

} // namespace GUI
} // namespace Surge

#endif // SURGE_SURGEGUIEDITORKEYBOARDACTIONS_H
