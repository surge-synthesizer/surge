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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MULTISWITCH_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MULTISWITCH_H

#include "SurgeJUCEHelpers.h"
#include "WidgetBaseMixin.h"
#include "AccessibleHelpers.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeImage;
class SurgeStorage;

namespace Surge
{
namespace Widgets
{
/*
 * MultiSwitch (f.k.a CHSwitch2 in VSTGUI land) takes a
 * glyph with rows and columns to allow multiple selection
 */
struct MultiSwitch : public juce::Component,
                     public WidgetBaseMixin<MultiSwitch>,
                     public LongHoldMixin<MultiSwitch>,
                     public Surge::Widgets::HasAccessibleSubComponentForFocus
{
    MultiSwitch();
    ~MultiSwitch();

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    int rows{0}, columns{0}, heightOfOneImage{0}, frameOffset{0};
    int getRows() const { return rows; }
    int getColumns() const { return columns; }
    void setRows(int x) { rows = x; }
    void setColumns(int x) { columns = x; }
    void setHeightOfOneImage(int h) { heightOfOneImage = h; }
    void setFrameOffset(int h) { frameOffset = h; }

    int valueToOff(float v)
    {
        return (int)(frameOffset + ((v * (float)(rows * columns - 1) + 0.5f)));
    }
    int coordinateToSelection(int x, int y) const;
    float coordinateToValue(int x, int y) const;
    juce::Point<float> valueToCoordinate(float v) const;

    float value{0};
    float getValue() const override { return value; }
    int getIntegerValue() const { return (int)(value * (float)(rows * columns - 1) + 0.5f); }
    int getIntegerValueFrom(float v) const { return (int)(v * (float)(rows * columns - 1) + 0.5f); }
    void setValue(float f) override { value = f; }
    void setIntegerValue(int v) { setValue(static_cast<float>(v) / (rows * columns - 1)); }

    bool draggable{false};
    void setDraggable(bool d) { draggable = d; }
    bool mousewheelable{false};
    void setMousewheelable(bool w) { mousewheelable = w; }

    bool middleClickable{false};
    void setMiddleClickable(bool m) { middleClickable = m; }

    void paint(juce::Graphics &g) override;

    bool everDragged{false}, isMouseDown{false};
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void setCursorToArrow();

    void startHover(const juce::Point<float> &) override;
    void endHover() override;

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        // fixme - probably use the location of the current element
        startHover(valueToCoordinate(getValue()));
    }

    void focusLost(juce::Component::FocusChangeType cause) override { endHover(); }

    bool keyPressed(const juce::KeyPress &key) override;
    Surge::GUI::WheelAccumulationHelper wheelHelper;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    bool isHovered{false};
    int hoverSelection{0};
    bool isCurrentlyHovered() override { return isHovered; }

    SurgeImage *switchD{nullptr}, *hoverSwitchD{nullptr}, *hoverOnSwitchD{nullptr};
    void setSwitchDrawable(SurgeImage *d) { switchD = d; }
    void setHoverSwitchDrawable(SurgeImage *d) { hoverSwitchD = d; }
    void setHoverOnSwitchDrawable(SurgeImage *d) { hoverOnSwitchD = d; }

    bool iam{false};
    bool isAlwaysAccessibleMomentary() const { return iam; }
    void setIsAlwaysAccessibleMomentary(bool b) { iam = b; }

    void setupAccessibility();
    std::vector<std::unique_ptr<juce::Component>> selectionComponents;
    juce::Component *getCurrentAccessibleSelectionComponent() override;
    void updateAccessibleStateOnUserValueChange() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    bool isDeactivated{false};
    void setDeactivated(bool b) { isDeactivated = b; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSwitch);
};

struct MultiSwitchSelfDraw : public MultiSwitch
{
    MultiSwitchSelfDraw() : MultiSwitch() {}
    juce::Font font{juce::FontOptions{36}};
    void paint(juce::Graphics &g) override;

    std::vector<std::string> labels;
    void setLabels(const std::vector<std::string> &l)
    {
        labels = l;
        repaint();
    }

    virtual bool isCellOn(int r, int c)
    {
        auto idx = r * columns + c;
        auto solo = (rows * columns == 1);
        auto isOn = idx == getIntegerValue() && !solo;
        return isOn;
    }

    void onSkinChanged() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSwitchSelfDraw);
};

struct ClosedMultiSwitchSelfDraw : public MultiSwitchSelfDraw, GUI::IComponentTagValue::Listener
{
    ClosedMultiSwitchSelfDraw() : MultiSwitchSelfDraw() { addListener(this); }

    using CallbackFn = std::function<void(int)>;
    CallbackFn onUpdate = [](int _) {};
    void setOnUpdate(CallbackFn fn) { onUpdate = std::move(fn); }
    void valueChanged(IComponentTagValue *p) override { onUpdate(getIntegerValue()); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClosedMultiSwitchSelfDraw);
};

struct SelfDrawButton : public MultiSwitchSelfDraw, GUI::IComponentTagValue::Listener
{
    SelfDrawButton(const std::string &lab) : MultiSwitchSelfDraw(), label(lab)
    {
        setLabels({label});
        addListener(this);
        setRows(1);
        setColumns(1);
        setDraggable(false);
        setValue(0);
    }
    std::string label;
    std::function<void()> onClick = []() {};
    void valueChanged(IComponentTagValue *p) override
    {
        onClick();
        setValue(0);
        repaint();
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SelfDrawButton);
};

struct SelfDrawToggleButton : public MultiSwitchSelfDraw, GUI::IComponentTagValue::Listener
{
    SelfDrawToggleButton(const std::string &lab) : MultiSwitchSelfDraw(), label(lab)
    {
        setLabels({label});
        addListener(this);
        setRows(1);
        setColumns(1);
        setDraggable(false);
        setValue(0);
    }
    std::string label;
    std::function<void()> onToggle = []() {};
    bool isToggled{false};
    void valueChanged(IComponentTagValue *p) override
    {
        if (isToggled)
        {
            setValue(0);
            isToggled = false;
        }
        else
        {
            setValue(1);
            isToggled = true;
        }

        onToggle();
        repaint();
    }
    void setToggleState(bool b)
    {
        if (b)
        {
            setValue(1);
            isToggled = true;
        }
        else
        {
            setValue(0);
            isToggled = false;
        }
    }
    bool getToggleState() { return getValue() > 0.5; }

    bool isCellOn(int, int) override { return getToggleState(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SelfDrawToggleButton);
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MULTISWITCH_H
