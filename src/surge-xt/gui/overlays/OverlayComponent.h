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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_OVERLAYCOMPONENT_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_OVERLAYCOMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "UserDefaults.h"
#include <optional>
#include <utility>

class Parameter;
class SurgePatch;

namespace Surge
{
namespace Overlays
{
struct OverlayComponent : juce::Component
{
    OverlayComponent() : juce::Component() {}
    OverlayComponent(const juce::String &s) : juce::Component(s) {}
    juce::Rectangle<int> enclosingParentPosition;
    void setEnclosingParentPosition(const juce::Rectangle<int> &e) { enclosingParentPosition = e; }
    juce::Rectangle<int> getEnclosingParentPosition() { return enclosingParentPosition; }

    std::string enclosingParentTitle{"Overlay"};
    void setEnclosingParentTitle(const std::string &s) { enclosingParentTitle = s; }
    std::string getEnclosingParentTitle() { return enclosingParentTitle; }

    bool hasIndependentClose{true};
    void setHasIndependentClose(bool b) { hasIndependentClose = b; }
    bool getHasIndependentClose() { return hasIndependentClose; }
    virtual bool shouldRepaintOnParamChange(const SurgePatch &, Parameter *p) { return true; }
    virtual void forceDataRefresh() {}

    // For A11Y: should the overlay be granted keyboard focus as soon as it appears.
    virtual bool wantsInitialKeyboardFocus() const { return true; }

    /*
     * This is called when a parent wrapper finally decides to show me, which will
     * be after I am added visibly to a hidden element. Basically use it to grab focus.
     */
    virtual void shownInParent() {}

    typedef std::tuple<bool, Surge::Storage::DefaultKey, Surge::Storage::DefaultKey,
                       Surge::Storage::DefaultKey>
        tearoutTuple_t;
    tearoutTuple_t canTearOut{false, Surge::Storage::nKeys, Surge::Storage::nKeys,
                              Surge::Storage::nKeys};
    void setCanTearOut(tearoutTuple_t t) { canTearOut = t; }
    tearoutTuple_t getCanTearOut() { return canTearOut; }
    virtual void onTearOutChanged(bool isTornOut) {}

    std::pair<bool, Surge::Storage::DefaultKey> canTearOutResize{false, Surge::Storage::nKeys};
    void setCanTearOutResize(std::pair<bool, Surge::Storage::DefaultKey> b)
    {
        canTearOutResize = b;
    }
    std::pair<bool, Surge::Storage::DefaultKey> getCanTearOutResize() { return canTearOutResize; }
    int minw{-1}, minh{-1};
    void setMinimumSize(int w, int h)
    {
        minw = w;
        minh = h;
    }

    std::pair<bool, Surge::Storage::DefaultKey> canMoveAround{false, Surge::Storage::nKeys};
    void setCanMoveAround(std::pair<bool, Surge::Storage::DefaultKey> b) { canMoveAround = b; }
    bool getCanMoveAround() { return canMoveAround.first; }
    juce::Point<int> defaultLocation;
    Surge::Storage::DefaultKey getMoveAroundKey() { return canMoveAround.second; }

    /*
     * Use this as 'true' for 'transient' dialogs like settings which
     * shouldn't persist across editor sessions
     */
    virtual bool getRetainOpenStateOnEditorRecreate() { return true; }

    /*
     * If you want to chicken box the close, return a message from here. The pair
     * is the OK/Cancel title and message, respectively. If you are ok to close,
     * return std::nullopt, which is the default behavior.
     */
    virtual std::optional<std::pair<std::string, std::string>> getPreCloseChickenBoxMessage()
    {
        return std::nullopt;
    }
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_OVERLAYCOMPONENT_H
