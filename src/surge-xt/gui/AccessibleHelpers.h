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

#ifndef SURGE_XT_ACCESSIBLEHELPERS_H
#define SURGE_XT_ACCESSIBLEHELPERS_H

#if SURGE_JUCE_ACCESSIBLE
#include "Parameter.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "SurgeGUIUtils.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{

template <typename T> struct DiscreteAHRange
{
    static int iMaxV(T *t) { return t->iMax; }
    static int iMinV(T *t) { return t->iMin; }
};

template <typename T> struct DiscreteAHStringValue
{
    static std::string stringValue(T *comp, double ahValue)
    {
        auto sge = comp->template firstListenerOfType<SurgeGUIEditor>();
        if (sge)
        {
            return sge->getDisplayForTag(comp->getTag());
        }
        return std::to_string(ahValue);
    }
};

template <typename T> struct DiscreteRO
{
    static bool isReadOnly(T *comp) { return false; }
};

template <typename T> struct DiscreteAHGetSetValue
{
    static double getCurrentValue(T *comp)
    {
        auto cv = Parameter::intUnscaledFromFloat(comp->getValue(), DiscreteAHRange<T>::iMaxV(comp),
                                                  DiscreteAHRange<T>::iMinV(comp));
        return cv;
    }

    static void setValue(T *comp, double newValue)
    {
        comp->notifyBeginEdit();
        comp->setValue(Parameter::intScaledToFloat(newValue, DiscreteAHRange<T>::iMaxV(comp),
                                                   DiscreteAHRange<T>::iMinV(comp)));
        comp->notifyValueChanged();
        comp->notifyEndEdit();
        comp->repaint();
    }
};

template <typename T, juce::AccessibilityRole R = juce::AccessibilityRole::slider>
struct DiscreteAH : public juce::AccessibilityHandler
{
    struct DAHValue : public juce::AccessibilityValueInterface
    {
        explicit DAHValue(T *s) : comp(s) {}

        T *comp;

        bool isReadOnly() const override { return DiscreteRO<T>::isReadOnly(comp); }
        double getCurrentValue() const override
        {
            return DiscreteAHGetSetValue<T>::getCurrentValue(comp);
        }
        void setValue(double newValue) override
        {
            DiscreteAHGetSetValue<T>::setValue(comp, newValue);
        }
        juce::String getCurrentValueAsString() const override
        {
            return DiscreteAHStringValue<T>::stringValue(comp, getCurrentValue());
        }
        void setValueAsString(const juce::String &newValue) override
        {
            setValue(newValue.getDoubleValue());
        }
        AccessibleValueRange getRange() const override
        {
            return {
                {(double)DiscreteAHRange<T>::iMinV(comp), (double)DiscreteAHRange<T>::iMaxV(comp)},
                1};
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DAHValue);
    };

    explicit DiscreteAH(T *s)
        : comp(s), juce::AccessibilityHandler(
                       *s, R,
                       juce::AccessibilityActions().addAction(
                           juce::AccessibilityActionType::showMenu, [this]() { this->showMenu(); }),
                       AccessibilityHandler::Interfaces{std::make_unique<DAHValue>(s)})
    {
    }

    void showMenu()
    {
        auto m = juce::ModifierKeys().withFlags(juce::ModifierKeys::rightButtonModifier);
        comp->notifyControlModifierClicked(m);
    }

    T *comp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiscreteAH);
};

template <typename T> struct OverlayAsAccessibleButton : public juce::Component
{
    OverlayAsAccessibleButton(T *s, const std::string &label,
                              juce::AccessibilityRole r = juce::AccessibilityRole::radioButton)
        : under(s), role(r)
    {
        setDescription(label);
        setTitle(label);
        setInterceptsMouseClicks(false, false);
        setAccessible(true);
        setWantsKeyboardFocus(true);
    }

    T *under;

    struct RBAH : public juce::AccessibilityHandler
    {
        explicit RBAH(OverlayAsAccessibleButton<T> *b, T *s)
            : button(b),
              mswitch(s), juce::AccessibilityHandler(*b, b->role,
                                                     juce::AccessibilityActions().addAction(
                                                         juce::AccessibilityActionType::press,
                                                         [this]() { this->press(); }))
        {
        }
        void press() { button->onPress(mswitch); }

        T *mswitch;
        OverlayAsAccessibleButton<T> *button;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBAH);
    };

    juce::AccessibilityRole role;
    std::function<void(T *)> onPress = [](T *) {};

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<RBAH>(this, under);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayAsAccessibleButton<T>);
};

template <typename T> struct OverlayAsAccessibleSlider : public juce::Component
{
    OverlayAsAccessibleSlider(T *s, const std::string &label,
                              juce::AccessibilityRole r = juce::AccessibilityRole::slider)
        : under(s), role(r)
    {
        setDescription(label);
        setTitle(label);
        setInterceptsMouseClicks(false, false);
        setAccessible(true);
        setWantsKeyboardFocus(true);
    }

    T *under;

    float getValue() { return 0; }
    void setValue(float f) {}

    struct SValue : public juce::AccessibilityValueInterface
    {
        explicit SValue(OverlayAsAccessibleSlider<T> *s) : slider(s) {}

        OverlayAsAccessibleSlider<T> *slider;

        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return slider->onGetValue(slider->under); }
        void setValue(double newValue) override { slider->onSetValue(slider->under, newValue); }
        virtual juce::String getCurrentValueAsString() const override
        {
            return std::to_string(getCurrentValue());
        }
        virtual void setValueAsString(const juce::String &newValue) override
        {
            setValue(newValue.getDoubleValue());
        }
        AccessibleValueRange getRange() const override { return {{-1, 1}, 0.01}; }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SValue);
    };

    struct RBAH : public juce::AccessibilityHandler
    {
        explicit RBAH(OverlayAsAccessibleSlider<T> *s, T *u)
            : slider(s),
              under(u), juce::AccessibilityHandler(
                            *s, s->role, juce::AccessibilityActions(),
                            AccessibilityHandler::Interfaces{std::make_unique<SValue>(s)})
        {
        }

        T *under;
        OverlayAsAccessibleSlider<T> *slider;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBAH);
    };

    juce::AccessibilityRole role;
    std::function<float(T *)> onGetValue = [](T *) { return 0.f; };
    std::function<void(T *, float f)> onSetValue = [](T *, float f) { return; };

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<RBAH>(this, under);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayAsAccessibleSlider<T>);
};

struct OverlayAsAccessibleContainer : public juce::Component
{
    OverlayAsAccessibleContainer(const std::string &desc) : juce::Component()
    {
        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
        setAccessible(true);
        setDescription(desc);
        setTitle(desc);
        setInterceptsMouseClicks(false, false);
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayAsAccessibleContainer);
};

enum AccessibleKeyEditAction
{
    None,
    Increase,
    Decrease,
    OpenMenu
};

enum AccessibleKeyModifier
{
    NoModifier,
    Fine,
    Quantized
};

inline std::tuple<AccessibleKeyEditAction, AccessibleKeyModifier>
accessibleEditAction(const juce::KeyPress &key, SurgeStorage *storage)
{
    if (!Surge::GUI::allowKeyboardEdits(storage))
        return {None, NoModifier};

    if (key.getKeyCode() == juce ::KeyPress::downKey)
    {
        if (key.getModifiers().isShiftDown())
            return {Decrease, Fine};
        if (key.getModifiers().isCtrlDown())
            return {Decrease, Quantized};
        return {Decrease, NoModifier};
    }

    if (key.getKeyCode() == juce ::KeyPress::upKey)
    {
        if (key.getModifiers().isShiftDown())
            return {Increase, Fine};
        if (key.getModifiers().isCtrlDown())
            return {Increase, Quantized};
        return {Increase, NoModifier};
    }

    if (key.getKeyCode() == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())
    {
        return {OpenMenu, NoModifier};
    }

    if (key.getKeyCode() == 93)
    {
        return {OpenMenu, NoModifier};
    }

    return {None, NoModifier};
}

} // namespace Widgets
} // namespace Surge

#endif

#endif // SURGE_XT_ACCESSIBLEHELPERS_H
