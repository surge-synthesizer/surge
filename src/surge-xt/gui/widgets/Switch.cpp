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
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Widgets
{

Switch::Switch() : juce::Component(), WidgetBaseMixin<Switch>(this)
{
    setRepaintsOnMouseActivity(true);
    setDescription("Switch");
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

    float activationOpacity = isDeactivated ? 0.5 : 1.0;
    auto t = juce::AffineTransform().translated(0, y);

    g.reduceClipRegion(getLocalBounds());

    switchD->draw(g, activationOpacity, t);

    if (!isDeactivated && isHovered && hoverSwitchD)
    {
        hoverSwitchD->draw(g, activationOpacity, t);
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

    mouseDownLongHold(event);

    if (isMultiIntegerValued())
    {
        storage->getPatch().isDirty = true;

        if (event.mods.isShiftDown())
        {
            setValueDirection(-1);
        }
        else
        {
            setValueDirection(1);
        }

        notifyValueChangedWithBeginEnd();
    }
    else
    {
        if (!getUnValueClickable())
        {
            value = (value > 0.5) ? 0 : 1;

            notifyValueChangedWithBeginEnd();
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
            storage->getPatch().isDirty = true;
            setValueDirection(mul);
            notifyValueChangedWithBeginEnd();
        }
        else
        {
            auto ov = value;
            value = mul > 0 ? 1 : 0;

            if (ov != value)
            {
                notifyValueChangedWithBeginEnd();
            }
        }
    }
}

bool Switch::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu)
    {
        notifyControlModifierClicked(juce::ModifierKeys(), true);
        return true;
    }

    if (action == Return && !isMultiIntegerValued())
    {
        auto nv = 1.f;
        if (value > 0.5)
            nv = 0;
        value = nv;
        notifyBeginEdit();
        notifyValueChanged();
        notifyEndEdit();
        repaint();
        return true;
    }

    if (action != Increase && action != Decrease)
        return false;

    int mul = 1;
    if (action == Decrease)
    {
        mul = -1;
    }

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
    return true;
}

struct SwitchAH : public juce::AccessibilityHandler
{
    explicit SwitchAH(Switch *s)
        : mswitch(s),
          juce::AccessibilityHandler(
              *s,
              (s->isAlwaysAccessibleMomentary()) ? juce::AccessibilityRole::button
                                                 : juce::AccessibilityRole::toggleButton,
              juce::AccessibilityActions()
                  .addAction(juce::AccessibilityActionType::showMenu,
                             [this]() { this->showMenu(); })
                  .addAction(juce::AccessibilityActionType::toggle, [this]() { this->press(); })
                  .addAction(juce::AccessibilityActionType::press, [this]() { this->press(); }))
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
            mswitch->notifyValueChangedWithBeginEnd();
        }
        else
        {
            if (!mswitch->getUnValueClickable())
            {
                auto value = (mswitch->getValue() > 0.5) ? 0 : 1;
                mswitch->setValue(value);
                mswitch->notifyValueChangedWithBeginEnd();
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

struct SwitchMultiValAH : public juce::AccessibilityHandler
{
    struct MSValue : public juce::AccessibilityValueInterface
    {
        explicit MSValue(Switch *s) : sw(s) {}

        Switch *sw;

        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return sw->getIntegerValue(); }
        void setValue(double newValue) override
        {
            sw->setIntegerValue((int)newValue);
            sw->repaint();
            sw->notifyValueChanged();
        }
        virtual juce::String getCurrentValueAsString() const override
        {
            auto sge = sw->firstListenerOfType<SurgeGUIEditor>();
            if (sge)
            {
                return sge->getDisplayForTag(sw->getTag());
            }
            return std::to_string(sw->getIntegerValue());
        }
        virtual void setValueAsString(const juce::String &newValue) override
        {
            setValue(newValue.getDoubleValue());
        }
        AccessibleValueRange getRange() const override
        {
            return {{0, (double)sw->getIntegerMaxValue()}, 1};
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSValue);
    };

    explicit SwitchMultiValAH(Switch *s)
        : mswitch(s), juce::AccessibilityHandler(
                          *s, juce::AccessibilityRole::slider,

                          juce::AccessibilityActions()
                              .addAction(juce::AccessibilityActionType::showMenu,
                                         [this]() { this->showMenu(); })
                              .addAction(juce::AccessibilityActionType::press,
                                         [this]() { this->press(); }),
                          juce::AccessibilityHandler::Interfaces{std::make_unique<MSValue>(s)})
    {
    }

    void press()
    {
        mswitch->setValueDirection(1);
        mswitch->notifyValueChangedWithBeginEnd();
    }
    void showMenu()
    {
        auto m = juce::ModifierKeys().withFlags(juce::ModifierKeys::rightButtonModifier);
        mswitch->notifyControlModifierClicked(m);
    }

    Switch *mswitch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchMultiValAH);
};

std::unique_ptr<juce::AccessibilityHandler> Switch::createAccessibilityHandler()
{
    if (isMultiIntegerValued())
    {
        return std::make_unique<SwitchMultiValAH>(this);
    }
    else
    {
        return std::make_unique<SwitchAH>(this);
    }
}

} // namespace Widgets
} // namespace Surge