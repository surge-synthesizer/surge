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

#include "Parameter.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "SurgeGUIUtils.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include <fmt/core.h>

namespace Surge
{
namespace Widgets
{

struct GroupTagTraverser : public juce::ComponentTraverser
{
    /*
     * This is a crude implementation which just sorts all components
     */
    juce::Component *on{nullptr};
    GroupTagTraverser(juce::Component *c) : on(c) {}
    juce::Component *getDefaultComponent(juce::Component *parentComponent) override
    {
        return nullptr;
    }
    juce::Component *getNextComponent(juce::Component *current) override { return nullptr; }
    juce::Component *getPreviousComponent(juce::Component *current) override { return nullptr; }
    std::vector<juce::Component *> getAllComponents(juce::Component *parentComponent) override
    {
        std::vector<juce::Component *> res;
        for (auto c : on->getChildren())
            res.push_back(c);
        std::sort(res.begin(), res.end(), [this](auto a, auto b) { return lessThan(a, b); });
        return res;
    }

    bool lessThan(const juce::Component *a, const juce::Component *b)
    {
        int acg = -1, bcg = -1;
        auto ap = a->getProperties().getVarPointer("ControlGroup");
        auto bp = b->getProperties().getVarPointer("ControlGroup");
        if (ap)
            acg = *ap;
        if (bp)
            bcg = *bp;

        if (acg != bcg)
            return acg < bcg;

        auto at = dynamic_cast<const Surge::GUI::IComponentTagValue *>(a);
        auto bt = dynamic_cast<const Surge::GUI::IComponentTagValue *>(b);

        if (at && bt)
            return at->getTag() < bt->getTag();

        if (at && !bt)
            return false;

        if (!at && bt)
            return true;

        auto cd = a->getDescription().compare(b->getDescription());
        if (cd < 0)
            return true;
        if (cd > 0)
            return false;

        // so what the hell else to do?
        return a < b;
    }
};

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
            : button(b), mswitch(s), juce::AccessibilityHandler(
                                         *b, b->role,
                                         juce::AccessibilityActions()
                                             .addAction(juce::AccessibilityActionType::showMenu,
                                                        [this]() { this->showMenu(); })
                                             .addAction(juce::AccessibilityActionType::press,
                                                        [this]() { this->press(); }))
        {
        }
        void press() { button->onPress(mswitch); }
        void showMenu() { button->onMenuKey(mswitch); }

        juce::AccessibleState getCurrentState() const override
        {
            auto state = AccessibilityHandler::getCurrentState();

            if (button->role == juce::AccessibilityRole::radioButton)
            {
                state = state.withCheckable();
                if (button->onGetIsChecked(mswitch))
                    state = state.withChecked();
            }
            return state;
        }

        T *mswitch;
        OverlayAsAccessibleButton<T> *button;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBAH);
    };

    juce::AccessibilityRole role;
    std::function<void(T *)> onPress = [](T *) {};
    std::function<bool(T *)> onMenuKey = [](T *) { return false; };
    std::function<bool(T *)> onReturnKey = [](T *) { return false; };
    std::function<bool(T *)> onGetIsChecked = [](T *) { return false; };

    bool keyPressed(const juce::KeyPress &) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<RBAH>(this, under);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayAsAccessibleButton<T>);
};

template <typename T>
struct OverlayAsAccessibleButtonWithValue : public OverlayAsAccessibleButton<T>
{
    OverlayAsAccessibleButtonWithValue(
        T *s, const std::string &label,
        juce::AccessibilityRole r = juce::AccessibilityRole::radioButton)
        : OverlayAsAccessibleButton<T>(s, label, r)
    {
    }

    struct BValue : public juce::AccessibilityValueInterface
    {
        explicit BValue(OverlayAsAccessibleButtonWithValue<T> *s) : slider(s) {}

        OverlayAsAccessibleButtonWithValue<T> *slider;

        bool isReadOnly() const override { return true; }
        double getCurrentValue() const override { return slider->onGetValue(slider->under); }
        void setValue(double newValue) override {}
        virtual juce::String getCurrentValueAsString() const override
        {
            return std::to_string(getCurrentValue());
        }
        virtual void setValueAsString(const juce::String &newValue) override {}
        AccessibleValueRange getRange() const override { return {{0, slider->range}, 1}; }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BValue);
    };

    struct RBAHV : public juce::AccessibilityHandler
    {
        explicit RBAHV(OverlayAsAccessibleButtonWithValue<T> *b, T *s)
            : button(b),
              mswitch(s), juce::AccessibilityHandler(
                              *b, b->role,
                              juce::AccessibilityActions()
                                  .addAction(juce::AccessibilityActionType::showMenu,
                                             [this]() { this->showMenu(); })
                                  .addAction(juce::AccessibilityActionType::press,
                                             [this]() { this->press(); }),
                              AccessibilityHandler::Interfaces{std::make_unique<BValue>(b)})
        {
        }
        void press() { button->onPress(mswitch); }
        void showMenu() { button->onMenuKey(mswitch); }

        T *mswitch;
        OverlayAsAccessibleButtonWithValue<T> *button;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBAHV);
    };

    std::function<float(T *)> onGetValue = [](T *) { return -1; };
    double range{1};
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<RBAHV>(this, OverlayAsAccessibleButton<T>::under);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayAsAccessibleButtonWithValue<T>);
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

    bool keyPressed(const juce::KeyPress &key) override;

    struct SValue : public juce::AccessibilityValueInterface
    {
        explicit SValue(OverlayAsAccessibleSlider<T> *s) : slider(s) {}

        OverlayAsAccessibleSlider<T> *slider;

        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return slider->onGetValue(slider->under); }
        void setValue(double newValue) override { slider->onSetValue(slider->under, newValue); }
        virtual juce::String getCurrentValueAsString() const override
        {
            return slider->onValueToString(slider->under, getCurrentValue());
        }
        virtual void setValueAsString(const juce::String &newValue) override
        {
            setValue(newValue.getDoubleValue());
        }
        AccessibleValueRange getRange() const override
        {
            return {{slider->min, slider->max}, slider->step};
        }

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
    std::function<std::string(T *, float f)> onValueToString = [](T *, float f) {
        return fmt::format("{:.3f}", f);
    };
    std::function<void(T *, int, bool, bool)> onJogValue = [](T *, int, bool, bool) {
        jassert(false);
    };
    // called with 1 0 -1 for max default min
    std::function<void(T *, int)> onMinMaxDef = [](T *, int) {};

    double min{-1}, max{1}, step{0.01};

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
    ToMax,
    ToMin,
    ToDefault,
    OpenMenu,
    Return
};

enum AccessibleKeyModifier
{
    NoModifier,
    Fine,
    Quantized
};

inline std::tuple<AccessibleKeyEditAction, AccessibleKeyModifier>
accessibleEditActionInternal(const juce::KeyPress &key)
{
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

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        return {Return, NoModifier};
    }

    if (key.getKeyCode() == juce::KeyPress::homeKey)
    {
        return {ToMax, NoModifier};
    }

    if (key.getKeyCode() == juce::KeyPress::endKey)
    {
        return {ToMin, NoModifier};
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey)
    {
        return {ToDefault, NoModifier};
    }

    return {None, NoModifier};
}

inline std::tuple<AccessibleKeyEditAction, AccessibleKeyModifier>
accessibleEditAction(const juce::KeyPress &key, SurgeStorage *storage)
{
    jassert(storage);
    if (!storage || !Surge::GUI::allowKeyboardEdits(storage))
        return {None, NoModifier};

    if (storage &&
        !Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::DefaultKey::MenuAndEditKeybindingsFollowKeyboardFocus, true))
        return {None, NoModifier};

    return accessibleEditActionInternal(key);
}

inline bool isAccessibleKey(const juce::KeyPress &key)
{
    auto [a, m] = accessibleEditActionInternal(key);
    return a != None;
}

template <typename T> bool OverlayAsAccessibleSlider<T>::keyPressed(const juce::KeyPress &key)
{
    if (!under->storage)
        return false;

    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, under->storage);
    auto ah = getAccessibilityHandler();

    if (action == Increase)
    {
        onJogValue(under, +1, key.getModifiers().isShiftDown(), key.getModifiers().isCtrlDown());
        if (ah)
            ah->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        return true;
    }
    if (action == Decrease)
    {
        onJogValue(under, -1, key.getModifiers().isShiftDown(), key.getModifiers().isCtrlDown());
        if (ah)
            ah->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        return true;
    }

    if (action == ToMax)
    {
        onMinMaxDef(under, 1);
        if (ah)
            ah->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        return true;
    }

    if (action == ToMin)
    {
        onMinMaxDef(under, -1);
        if (ah)
            ah->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        return true;
    }

    if (action == ToDefault)
    {
        onMinMaxDef(under, 0);
        if (ah)
            ah->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        return true;
    }
    return false;
}

template <typename T> bool OverlayAsAccessibleButton<T>::keyPressed(const juce::KeyPress &key)
{
    jassert(under->storage);
    if (!under->storage)
        return false;

    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, under->storage);

    if (action == OpenMenu)
    {
        return onMenuKey(under);
    }

    if (action == Return)
    {
        return onReturnKey(under);
    }
    return false;
}

inline void fixupJuceTextEditorAccessibility(const juce::Component &te)
{
#if MAC
    for (auto c : te.getChildren())
    {
        c->setAccessible(false);
    }
#endif
}

struct HasAccessibleSubComponentForFocus
{
    virtual ~HasAccessibleSubComponentForFocus() = default;
    virtual juce::Component *getCurrentAccessibleSelectionComponent() = 0;
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_ACCESSIBLEHELPERS_H
