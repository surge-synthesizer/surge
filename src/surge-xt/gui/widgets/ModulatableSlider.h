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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MODULATABLESLIDER_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MODULATABLESLIDER_H

#include "ParameterInfowindow.h"
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "ModulatableControlInterface.h"
#include "SurgeJUCEHelpers.h"
#include "AccessibleHelpers.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct ModulatableSlider : public juce::Component,
                           public WidgetBaseMixin<ModulatableSlider>,
                           public LongHoldMixin<ModulatableSlider>,
                           public ModulatableControlInterface,
                           public HasExtendedAccessibleGroupName
{
    ModulatableSlider();
    ~ModulatableSlider();

    enum MoveRateState
    {
        kUnInitialized = 0,
        kLegacy,
        kSlow,
        kMedium,
        kExact
    };

    enum TouchscreenMode
    {
        kUnassigned = 0,
        kDisabled,
        kEnabled
    };

    static MoveRateState sliderMoveRateState;
    static TouchscreenMode touchscreenMode;

    ctrltypes parameterType{ct_none};

    /**
     * The slider is 'light' backgrounded (which matters in the classic skin only)
     * @param b
     */
    virtual void setIsLightStyle(bool b) { isLightStyle = b; }
    bool isLightStyle{false};

    bool initiatedChange{false};

    /**
     * Vertical sliders have smaller vertical footprints in the LFO region, as a remnant from
     * the classic skin. Support the option of intent here even though subclasses may do nothing
     * with this
     *
     * @param b
     */
    virtual void setIsMiniVertical(bool b) { isMiniVertical = b; }
    bool isMiniVertical{false};

    Surge::ParamConfig::Orientation orientation;
    void setOrientation(Surge::ParamConfig::Orientation o) { orientation = o; }

    void paint(juce::Graphics &g) override;

    bool isHovered{false};
    float valueOnMouseDown{0.f}, modValueOnMouseDown{0.f}, lastDistance{0.f};
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;
    void startHover(const juce::Point<float> &) override;
    void endHover() override;
    bool isCurrentlyHovered() override { return isHovered; }
    bool keyPressed(const juce::KeyPress &key) override;
    void focusGained(juce::Component::FocusChangeType cause) override
    {
        startHover(getBounds().getBottomLeft().toFloat());
        repaint();
    }
    void focusLost(juce::Component::FocusChangeType cause) override
    {
        endHover();
        repaint();
    }
    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    /*
     * This sets up the legacy mouse -> value scaling
     */
    float legacyMoveRate{1.0};
    virtual void setMoveRate(float f) { legacyMoveRate = f; }

    bool forceModHandle{false};
    void setAlwaysUseModHandle(bool b)
    {
        forceModHandle = b;
        repaint();
    }

    float value{0.f};
    float getValue() const override { return value; }
    void setValue(float f) override
    {
        value = f;
        repaint();
    }

    Surge::GUI::IComponentTagValue *asControlValueInterface() override { return this; }
    juce::Component *asJuceComponent() override { return this; }

    void onSkinChanged() override;

    SurgeImage *pTray, *pHandle, *pHandleHover, *pTempoSyncHandle, *pTempoSyncHoverHandle;

    std::function<std::string()> customToAccessibleString{nullptr};

  private:
    // I know right Here I am using private!

    /*
     * These are the state variables which determine the position of the items in the graphics
     * for a given draw or mouse gesture. I made them members so I don't have to copy their
     * calculation. The method updateLocationState() updates based on current state of the widget
     */
    void updateLocationState();

    int trayw{0}, trayh{0}; // How large is the tray
    float range{0};         // how big in logical pixels is the
    // What are the XY positions for the handle center, the mod handle center, the
    // other end of the bar, and the bar positions in force mode to zero
    float handleCX{0}, handleCY{0};
    float handleMX{0}, handleMY{0};
    float barNMX{0}, barNMY{0};
    float barFM0X{0}, barFM0Y{0}, barFMNX{0}, barFMNY{0};
    float handleX0{0}, handleY0{0};

    // how far logically do we move in tray space for our current state
    int trayTypeX{0}, trayTypeY{0};
    // How far do we move in the image to make the handle image be the mod
    float modHandleX{0};

    juce::Point<float> mouseDownFloatPosition;

    // Should we draw the label, and where?
    bool drawLabel{false};
    juce::Rectangle<int> labelRect;
    // How big is the handle
    juce::Rectangle<int> handleSize;
    // Where do we position the tray relative to local bounds?
    juce::AffineTransform trayPosition;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatableSlider);
};

// A "modulatable" slider, that looks like the above class, but *is not* used for modulating a
// parameter. Basically, use it when you need a slider but it doesn't actually need to be a
// parameter that you modulate. Fakes all the stuff like drawing, double-click-to-reset, hovering,
// etc. that the ModulatableSlider gives you.
struct SelfUpdatingModulatableSlider : public ModulatableSlider,
                                       public juce::Timer,
                                       GUI::IComponentTagValue::Listener
{
    using CallbackFn = std::function<void(float)>;

    SelfUpdatingModulatableSlider() : ModulatableSlider() { addListener(this); }

    CallbackFn onUpdate = [](float _) {};
    void setOnUpdate(CallbackFn fn) { onUpdate = std::move(fn); }

    float defaultValue = 0.f;
    void setDefaultValue(float f)
    {
        defaultValue = f;
        setValue(f);
    }

    void setValue(float f) override
    {
        ModulatableSlider::setValue(f);
        infoWindow.setLabels("", createDisplayString());
    }

    void valueChanged(IComponentTagValue *p) override
    {
        auto v = getValue();
        setQuantitizedDisplayValue(v);
        infoWindow.setLabels("", createDisplayString());
        onUpdate(v);
        repaint();
    }

    int32_t controlModifierClicked(IComponentTagValue *p, const juce::ModifierKeys &mods,
                                   bool isDoubleClickEvent) override
    {
        if (isDoubleClickEvent)
        {
            setValue(defaultValue);
            valueChanged(p);
        }
        return 0;
    }

    void onSkinChanged() override;

    // Pieces that allow for the pop-up hover window to work. If you want the hover information to
    // appear, you must set the root window that it will appear in (generally either the
    // SurgeGUIEditor singleton or whatever Overlay this slider is in).
    //
    // This should only ever be set once! It will throw otherwise!
    juce::Component *rootWindow{nullptr};
    void setRootWindow(juce::Component *root)
    {
        if (rootWindow)
        {
            throw(std::runtime_error("Attempting to re-set the root window."));
        }
        rootWindow = root;
    }

    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;
    void timerCallback() override;
    bool infoWindowInitialized{false};
    bool infoWindowShowing{false};
    ParameterInfowindow infoWindow;

    // Some additional parts to make the informational window a little more nicer.
    std::string unit{""};
    void setUnit(const std::string &u)
    {
        unit = u;
        infoWindow.setLabels("", createDisplayString());
    }
    std::size_t precision{0};
    void setPrecision(const std::size_t p)
    {
        precision = p;
        infoWindow.setLabels("", createDisplayString());
    }
    float lowEnd{0.f};
    float highEnd{1.f};
    void setRange(const float low, const float high)
    {
        lowEnd = low;
        highEnd = high;
        infoWindow.setLabels("", createDisplayString());
    }
    std::string createDisplayString() const;

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SelfUpdatingModulatableSlider);
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MODULATABLESLIDER_H
