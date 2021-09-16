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
            auto cv = Parameter::intUnscaledFromFloat(
                comp->getValue(), DiscreteAHRange<T>::iMaxV(comp), DiscreteAHRange<T>::iMinV(comp));
            return cv;
        }
        void setValue(double newValue) override
        {
            comp->notifyBeginEdit();
            comp->setValue(Parameter::intScaledToFloat(newValue, DiscreteAHRange<T>::iMaxV(comp),
                                                       DiscreteAHRange<T>::iMinV(comp)));
            comp->notifyValueChanged();
            comp->notifyEndEdit();
            comp->repaint();
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

} // namespace Widgets
} // namespace Surge

#endif

#endif // SURGE_XT_ACCESSIBLEHELPERS_H
