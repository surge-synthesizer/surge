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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_OVERLAYWRAPPER_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_OVERLAYWRAPPER_H

#include "SkinSupport.h"
#include "UserDefaults.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "SurgeGUIUtils.h"

class SurgeGUIEditor;
class SurgeImage;
class SurgeStorage;

namespace Surge
{
namespace Overlays
{

class OverlayComponent;
struct OverlayWrapper : public juce::Component,
                        public Surge::GUI::SkinConsumingComponent,
                        public juce::Button::Listener
{
    OverlayWrapper();                             // The default form
    OverlayWrapper(const juce::Rectangle<int> &); // The modal form

    static constexpr int titlebarSize = 14, margin = 1;

    std::string title;
    void setWindowTitle(const std::string &t)
    {
        title = t;
        setTitle(t);
        setDescription(t);
    }

    void paint(juce::Graphics &g) override;

    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e) { editor = e; }

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    std::unique_ptr<juce::Component> primaryChild;
    void addAndTakeOwnership(std::unique_ptr<juce::Component> c);
    std::unique_ptr<juce::Button> closeButton, tearOutButton;
    void buttonClicked(juce::Button *button) override;

    juce::Point<int> mouseDownWithinTarget;

    bool isDragging{false};
    bool allowDrag{true};
    void mouseDown(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &e) override;

    void visibilityChanged() override;

    void resized() override;

    SurgeImage *icon{nullptr};
    void setIcon(SurgeImage *d) { icon = d; }

    // this is can, size key, standalone pin key, plugin pin key
    typedef std::tuple<bool, Surge::Storage::DefaultKey, Surge::Storage::DefaultKey,
                       Surge::Storage::DefaultKey>
        tearoutTuple_t;
    tearoutTuple_t canTearOutData{false, Surge::Storage::nKeys, Surge::Storage::nKeys,
                                  Surge::Storage::nKeys};
    bool canTearOut, isAlwaysOnTop;
    // this tuple is can: the sie key,
    void setCanTearOut(tearoutTuple_t t)
    {
        canTearOutData = t;
        canTearOut = std::get<0>(t);
        if (Surge::GUI::getIsStandalone())
            isAlwaysOnTop = Surge::Storage::getUserDefaultValue(storage, std::get<2>(t), false);
        else
            isAlwaysOnTop = Surge::Storage::getUserDefaultValue(storage, std::get<3>(t), true);
    }
    tearoutTuple_t getCanTearOut() { return canTearOutData; }

    std::pair<bool, Surge::Storage::DefaultKey> canTearOutResizePair{false, Surge::Storage::nKeys};
    bool canTearOutResize;
    void setCanTearOutResize(std::pair<bool, Surge::Storage::DefaultKey> b)
    {
        canTearOutResizePair = b;
        canTearOutResize = b.first;
    }

    bool resizeRecordsSize{true};
    void doTearOut(const juce::Point<int> &showAt = juce::Point<int>(-1, -1));
    void doTearIn();
    bool isTornOut();
    juce::Point<int> currentTearOutLocation();
    juce::Rectangle<int> locationBeforeTearOut, childLocationBeforeTearOut;
    juce::Component::SafePointer<juce::Component> parentBeforeTearOut{nullptr};

    bool hasInteriorDec{true};
    void supressInteriorDecoration();

    bool showCloseButton{true};
    void setShowCloseButton(bool b) { showCloseButton = b; }

    bool retainOnRecreate{true};
    bool getRetainOpenStateOnEditorRecreate() { return retainOnRecreate; }
    void setRetainOpenStateOnEditorRecreate(bool b) { retainOnRecreate = b; }

    void onClose();

    std::function<void()> closeOverlay = []() {};
    void setCloseOverlay(std::function<void()> f) { closeOverlay = std::move(f); }

    juce::Rectangle<int> componentBounds;
    bool isModal{false};
    bool getIsModal() const { return isModal; }

    std::unique_ptr<juce::DocumentWindow> tearOutParent;
    std::unique_ptr<juce::ComponentBoundsConstrainer> tearOutConstrainer;

    void onSkinChanged() override;

    OverlayComponent *getPrimaryChildAsOverlayComponent();

    /*
     * All overlays should use default focus order not the wonky tag first and
     * then description and so on order for the main frame (which is laying out controls
     * in a differently structured way).
     */
    std::unique_ptr<juce::ComponentTraverser> createKeyboardFocusTraverser() override
    {
        return std::make_unique<juce::KeyboardFocusTraverser>();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayWrapper);
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_OVERLAYWRAPPER_H
