// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sst/jucegui/components/Knob.h"
#include "sst/jucegui/components/Label.h"
#include "sst/jucegui/components/ToggleButton.h"

namespace surge::fx
{
struct ParameterCaption : sst::jucegui::components::Label
{
    void paint(juce::Graphics &g) override
    {
        auto font = getFont(Styles::labelfont);
        if (overrideSize > 0)
            font = font.withHeight(overrideSize);
        g.setFont(font);
        g.setColour(getColour(Styles::labelcolor));
        g.drawFittedText(text, getLocalBounds(), justification, 1, 0.85f);
    }
};

struct ParameterKnob : sst::jucegui::components::Knob
{
    using Knob = sst::jucegui::components::Knob;

    struct Binding : sst::jucegui::data::Continuous
    {
        float value{0.f}, defaultValue{0.f}, discreteStep{0.f};
        bool bipolar{false}, blocked{false};
        std::string name, text;
        std::function<void(float, bool)> edit;
        std::function<bool(const std::string &)> editText;
        bool acceptsText{false};

        float getValue() const override { return value; }
        float getDefaultValue() const override { return defaultValue; }
        float getQuantizedStepSize() const override
        {
            return discreteStep > 0.f ? discreteStep : Continuous::getQuantizedStepSize();
        }
        std::string getLabel() const override { return name; }
        std::string getValueAsStringFor(float) const override { return text; }
        bool isBipolar() const override { return bipolar; }
        void setValueFromGUI(const float &v) override { write(v, false); }
        void setValueFromGUIQuantized(const float &v) override { write(v, true); }
        void setValueFromModel(const float &v) override { value = v; }
        bool trySetValueAsString(const std::string &s) override
        {
            return !blocked && acceptsText && editText && editText(s);
        }
        void setValueAsString(const std::string &s) override { trySetValueAsString(s); }

        void write(float v, bool quantized)
        {
            if (!blocked && edit)
                edit(v, quantized);
        }
    } binding;

    ParameterKnob()
    {
        setSource(&binding);
        setDrawLabel(false);
    }
    ~ParameterKnob() override { clearSource(); }

    bool isEditingBlocked() const { return binding.blocked; }
    bool opensChoiceMenu{false};
    bool drawFader{false};
    std::function<void()> onEditValue;

    void setOpensChoiceMenu(bool enabled)
    {
        if (opensChoiceMenu == enabled)
            return;
        opensChoiceMenu = enabled;
        invalidateAccessibilityHandler();
        setMouseCursor(enabled ? juce::MouseCursor::PointingHandCursor
                               : juce::MouseCursor::NormalCursor);
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        if (!opensChoiceMenu)
        {
            struct SliderValue : juce::AccessibilityValueInterface
            {
                explicit SliderValue(ParameterKnob &owner) : knob(owner) {}
                bool isReadOnly() const override { return knob.isEditingBlocked(); }
                double getCurrentValue() const override { return knob.binding.value; }
                juce::String getCurrentValueAsString() const override { return knob.binding.text; }
                AccessibleValueRange getRange() const override
                {
                    const auto step = knob.binding.discreteStep;
                    return {{0., 1.}, step > 0.f ? step : 0.01};
                }
                void setValue(double value) override
                {
                    if (isReadOnly() || !std::isfinite(value))
                        return;
                    knob.onBeginEdit();
                    knob.binding.setValueFromGUI(static_cast<float>(std::clamp(value, 0., 1.)));
                    knob.onEndEdit();
                }
                void setValueAsString(const juce::String &value) override
                {
                    knob.binding.trySetValueAsString(value.toStdString());
                }

                ParameterKnob &knob;
            };

            return std::make_unique<juce::AccessibilityHandler>(
                *this, juce::AccessibilityRole::slider,
                juce::AccessibilityActions().addAction(juce::AccessibilityActionType::showMenu,
                                                       [this] { onPopupMenu({}); }),
                juce::AccessibilityHandler::Interfaces{std::make_unique<SliderValue>(*this)});
        }

        struct MenuAccessibility : juce::AccessibilityHandler
        {
            explicit MenuAccessibility(ParameterKnob &owner)
                : AccessibilityHandler(owner, juce::AccessibilityRole::button,
                                       juce::AccessibilityActions()
                                           .addAction(juce::AccessibilityActionType::press,
                                                      [&owner] { owner.onPopupMenu({}); })
                                           .addAction(juce::AccessibilityActionType::showMenu,
                                                      [&owner] { owner.onPopupMenu({}); })),
                  knob(owner)
            {
            }

            juce::String getTitle() const override
            {
                return juce::String(knob.binding.name) + ": " + knob.binding.text;
            }

            ParameterKnob &knob;
        };
        return std::make_unique<MenuAccessibility>(*this);
    }

    void update(float value, const std::string &text)
    {
        const bool titleChanged = opensChoiceMenu && binding.text != text;
        binding.setValueFromModel(value);
        binding.text = text;
        repaint();
        if (titleChanged)
        {
            if (auto *handler = getAccessibilityHandler())
                handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
        else if (!opensChoiceMenu)
            notifyAccessibleChange();
    }

    void paint(juce::Graphics &g) override
    {
        if (opensChoiceMenu)
        {
            // A discrete algorithm is a menu, not a continuous rotary control.
            const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
            const bool highlighted = isMouseOver();
            g.setColour(getColour(Styles::background).brighter(highlighted ? 0.18f : 0.08f));
            g.fillRoundedRectangle(bounds, 3.f);
            g.setColour(getColour(highlighted ? Styles::value_hover : Styles::brightoutline));
            g.drawRoundedRectangle(bounds, 3.f, 1.f);
            const auto x = bounds.getRight() - 8.f;
            const auto y = bounds.getCentreY();
            juce::Path arrow;
            arrow.addTriangle(x - 3.f, y - 1.5f, x + 3.f, y - 1.5f, x, y + 2.f);
            g.setColour(getColour(highlighted ? Styles::value_hover : Styles::labelcolor));
            g.fillPath(arrow);
            g.setColour(getColour(Styles::labelcolor));
            const bool compact = getWidth() <= 100;
            g.setFont(getFont(Styles::labelfont).withHeight(compact ? 11.f : 16.f).boldened());
            g.drawFittedText(binding.text,
                             bounds.withTrimmedLeft(6.f).withTrimmedRight(15.f).toNearestInt(),
                             juce::Justification::centredLeft, compact ? 2 : 1, 0.85f);
            return;
        }

        juce::Graphics::ScopedSaveState state(g);
        if (isEditingBlocked())
            g.beginTransparencyLayer(0.45f);
        if (drawFader)
            paintFader(g);
        else
            Knob::paint(g);
        if (isEditingBlocked())
            g.endTransparencyLayer();
    }

    void paintFader(juce::Graphics &g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto track = bounds.reduced(5.f, 6.f);
        const auto toY = [&](float value) {
            return track.getBottom() - juce::jlimit(0.f, 1.f, value) * track.getHeight();
        };
        const auto y = toY(binding.value);
        const auto zero = toY(binding.defaultValue);
        g.setColour(getColour(Styles::gutter));
        g.fillRoundedRectangle(track, 2.f);
        g.setColour(getColour(Styles::brightoutline));
        g.drawHorizontalLine(juce::roundToInt(zero), bounds.getX(), bounds.getRight());
        const auto accent = getColour(isMouseOver() ? Styles::value_hover : Styles::value);
        g.setColour(accent.withAlpha(0.8f));
        g.fillRect(track.withY(std::min(y, zero)).withHeight(std::abs(y - zero)));
        g.setColour(y < zero || y > zero ? accent : getColour(Styles::handle));
        g.fillRoundedRectangle(bounds.getCentreX() - 5.5f, y - 2.f, 11.f, 4.f, 1.f);
    }

    void focusGained(FocusChangeType cause) override
    {
        Knob::focusGained(cause);
        repaintParent();
    }
    void focusLost(FocusChangeType cause) override
    {
        Knob::focusLost(cause);
        repaintParent();
    }

    void mouseDown(const juce::MouseEvent &e) override
    {
        rubberbandValue.reset();
        if (!e.mods.isPopupMenu())
        {
            if (isEditingBlocked())
                return;
            if (opensChoiceMenu)
            {
                grabKeyboardFocus();
                onPopupMenu(e.mods);
                return;
            }
            if (e.mods.isAltDown())
                rubberbandValue = binding.value;
        }
        Knob::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent &e) override
    {
        if (rubberbandValue && mouseMode == DRAG)
            binding.setValueFromGUI(*rubberbandValue);
        rubberbandValue.reset();
        Knob::mouseUp(e);
    }
    void mouseDoubleClick(const juce::MouseEvent &e) override
    {
        if (e.getNumberOfClicks() == 2 && !opensChoiceMenu && !isEditingBlocked() &&
            !e.mods.isPopupMenu())
            Knob::mouseDoubleClick(e);
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (!isEditingBlocked())
            Knob::mouseDrag(e);
    }
    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) override
    {
        if (isEditingBlocked())
            return;
        if (binding.discreteStep > 0.f)
        {
            const auto delta = wheel.deltaY != 0.f ? wheel.deltaY : wheel.deltaX;
            if (delta == 0.f)
                return;
            const auto step =
                std::copysign(binding.discreteStep, wheel.isReversed ? -delta : delta);
            onBeginEdit();
            binding.setValueFromGUI(std::clamp(binding.value + step, 0.f, 1.f));
            onEndEdit();
            return;
        }
        Knob::mouseWheelMove(e, wheel);
    }

    bool keyPressed(const juce::KeyPress &key) override
    {
        using Keys = juce::KeyPress;
        const auto code = key.getKeyCode();
        if (code == Keys::F10Key && key.getModifiers().isShiftDown())
        {
            onPopupMenu(key.getModifiers());
            return true;
        }
        if (isEditingBlocked())
            return false;
        if (code == Keys::returnKey)
        {
            if (!opensChoiceMenu && binding.acceptsText && onEditValue)
                onEditValue();
            else
                onPopupMenu(key.getModifiers());
            return true;
        }
        if (binding.discreteStep > 0.f && (code == Keys::upKey || code == Keys::downKey))
        {
            const auto step = code == Keys::upKey ? binding.discreteStep : -binding.discreteStep;
            onBeginEdit();
            binding.setValueFromGUI(std::clamp(binding.value + step, 0.f, 1.f));
            onEndEdit();
            return true;
        }
        return Knob::keyPressed(key);
    }

  private:
    void repaintParent()
    {
        if (auto *parent = getParentComponent())
            parent->repaint();
    }
    std::optional<float> rubberbandValue;
};

struct ParameterOption : sst::jucegui::components::ToggleButton
{
    struct Binding : sst::jucegui::data::BinaryDiscrete
    {
        bool value{false}, invert{false};
        std::function<void()> edit;
        std::string getLabel() const override { return {}; }
        int getValue() const override { return value != invert; }
        void setValueFromModel(const int &v) override { value = (v != 0) != invert; }
        void setValueFromGUI(const int &v) override
        {
            setValueFromModel(v);
            if (edit)
                edit();
        }
    } binding;

    ParameterOption()
    {
        setSource(&binding);
        usesPopupMenu = true;
        binding.edit = [this] {
            if (onClick)
                onClick();
        };
    }
    ~ParameterOption() override { setSource(nullptr); }

    bool getToggleState() const { return binding.value; }
    void setToggleState(bool state, juce::NotificationType)
    {
        binding.value = state;
        repaint();
    }
    std::function<void()> onClick;
};
} // namespace surge::fx
