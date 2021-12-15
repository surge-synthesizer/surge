/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "SurgeGUIEditor.h"
#include "Switch.h"
#include "SurgeImage.h"
#include "SurgeGUIUtils.h"

namespace Surge
{
namespace Widgets
{

Switch::Switch()
{
    setRepaintsOnMouseActivity(true);
#if SURGE_JUCE_ACCESSIBLE
    setDescription("Switch");
#endif
}
Switch::~Switch() = default;

void Switch::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    auto y = value > 0.5 ? -getLocalBounds().getHeight() : 0;

    if (isMultiIntegerValued())
    {
        y = -getIntegerValue() * getLocalBounds().getHeight();
    }

    auto t = juce::AffineTransform().translated(0, y);
    g.reduceClipRegion(getLocalBounds());

    switchD->draw(g, 1.0, t);

    if (isHovered && hoverSwitchD)
    {
        hoverSwitchD->draw(g, 1.0, t);
    }
}

void Switch::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    if (event.mods.isPopupMenu() || event.mods.isCommandDown())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    if (isMultiIntegerValued())
    {
        if (event.mods.isShiftDown())
        {
            setValueDirection(-1);
        }
        else
        {
            setValueDirection(1);
        }

        notifyValueChanged();
    }
    else
    {
        if (!getUnValueClickable())
        {
            value = (value > 0.5) ? 0 : 1;

            notifyValueChanged();
        }
    }
}

void Switch::mouseEnter(const juce::MouseEvent &event) { isHovered = true; }

void Switch::mouseExit(const juce::MouseEvent &event) { endHover(); }

void Switch::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    int mul = wheelHelper.accumulate(wheel);

    if (mul != 0)
    {
        if (isMultiIntegerValued())
        {
            setValueDirection(mul);
            notifyValueChanged();
        }
        else
        {
            auto ov = value;
            value = mul > 0 ? 1 : 0;

            if (ov != value)
            {
                notifyValueChanged();
            }
        }
    }
}

bool Switch::keyPressed(const juce::KeyPress &key)
{
    if (!Surge::GUI::allowKeyboardEdits(storage))
        return false;

    bool got{false};
    int mul = 1;
    if (key.getKeyCode() == juce::KeyPress::leftKey)
    {
        got = true;
        mul = -1;
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey)
    {
        got = true;
    }

    if (got)
    {

        if (isMultiIntegerValued())
        {
            setValueDirection(mul);
            notifyBeginEdit();
            notifyValueChanged();
            notifyEndEdit();
        }
        else
        {
            auto ov = value;
            value = mul > 0 ? 1 : 0;

            if (ov != value)
            {
                notifyBeginEdit();
                notifyValueChanged();
                notifyEndEdit();
            }
        }
        repaint();
    }
    return got;
}

#if SURGE_JUCE_ACCESSIBLE
struct SwitchAH : public juce::AccessibilityHandler
{
    explicit SwitchAH(Switch *s)
        : mswitch(s), juce::AccessibilityHandler(
                          *s,
                          s->isMultiIntegerValued() ? juce::AccessibilityRole::button
                                                    : juce::AccessibilityRole::toggleButton,
                          juce::AccessibilityActions()
                              .addAction(juce::AccessibilityActionType::showMenu,
                                         [this]() { this->showMenu(); })
                              .addAction(juce::AccessibilityActionType::press,
                                         [this]() { this->press(); }))
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = AccessibilityHandler::getCurrentState();
        state = state.withCheckable();
        if (mswitch->getValue() > 0.5)
            state = state.withChecked();

        return state;
    }

    void press()
    {
        if (mswitch->isMultiIntegerValued())
        {
            mswitch->setValueDirection(1);
            mswitch->notifyValueChanged();
        }
        else
        {
            if (!mswitch->getUnValueClickable())
            {
                auto value = (mswitch->getValue() > 0.5) ? 0 : 1;
                mswitch->setValue(value);
                mswitch->notifyValueChanged();
            }
        }
    }
    void showMenu()
    {
        auto m = juce::ModifierKeys().withFlags(juce::ModifierKeys::rightButtonModifier);
        mswitch->notifyControlModifierClicked(m);
    }

    Switch *mswitch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchAH);
};

std::unique_ptr<juce::AccessibilityHandler> Switch::createAccessibilityHandler()
{
    return std::make_unique<SwitchAH>(this);
}
#endif

} // namespace Widgets
} // namespace Surge