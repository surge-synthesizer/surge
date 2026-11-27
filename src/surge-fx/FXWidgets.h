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
#ifndef SURGE_SRC_SURGE_FX_FXWIDGETS_H
#define SURGE_SRC_SURGE_FX_FXWIDGETS_H

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
        float quantizeValue(float v) override
        {
            if (discreteStep <= 0.f)
                return Continuous::quantizeValue(v);

            // Use a valid grid point as the origin: Surge integer parameters
            // occupy [0.005, 0.995], not [0, 1].
            return std::clamp(defaultValue +
                                  std::round((v - defaultValue) / discreteStep) * discreteStep,
                              0.f, 1.f);
        }
        std::string getLabel() const override { return name; }
        std::string getValueAsStringFor(float) const override { return text; }
        bool isBipolar() const override { return bipolar; }
        void setValueFromGUI(const float &v) override { write(v, false); }
        void setValueFromGUIQuantized(const float &v) override
        {
            write(discreteStep > 0.f ? quantizeValue(v) : v, true);
        }
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
    enum class Presentation
    {
        rotary,
        fader,
        choice
    };

    bool isChoice() const { return presentation == Presentation::choice; }
    bool isFader() const { return presentation == Presentation::fader; }

    void setPresentation(Presentation next)
    {
        const bool wasChoice = isChoice();
        presentation = next;
        if (isChoice() != wasChoice)
        {
            invalidateAccessibilityHandler();
            setMouseCursor(isChoice() ? juce::MouseCursor::PointingHandCursor
                                      : juce::MouseCursor::NormalCursor);
        }
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        // The upstream slider handler always reports writable and its Show Menu action is a no-op.
        if (!isChoice())
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
        const bool titleChanged = isChoice() && binding.text != text;
        binding.setValueFromModel(value);
        binding.text = text;
        repaint();
        if (titleChanged)
        {
            if (auto *handler = getAccessibilityHandler())
                handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
        else if (!isChoice())
            notifyAccessibleChange();
    }

    void paint(juce::Graphics &g) override
    {
        if (isChoice())
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
        if (isFader())
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
            if (isChoice())
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
        if (e.getNumberOfClicks() != 2 || isChoice() || isEditingBlocked() || e.mods.isPopupMenu())
            return;
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
#if JUCE_MAC
        // sst-jucegui's macOS wheel path scales by delta magnitude even when
        // alwaysQuantize is on, which can leave coarse discrete controls stuck.
        if (binding.discreteStep > 0.f)
        {
            const auto delta = wheel.deltaY != 0.f ? wheel.deltaY : wheel.deltaX;
            if (delta == 0.f)
                return;
            const auto step =
                std::copysign(binding.discreteStep, wheel.isReversed ? -delta : delta);
            onBeginEdit();
            binding.setValueFromGUIQuantized(binding.value + step);
            onEndEdit();
            notifyAccessibleChange();
            return;
        }
#endif
        Knob::mouseWheelMove(e, wheel);
    }

    bool keyPressed(const juce::KeyPress &key) override
    {
        using Action = decltype(accessibleEdit(key));
        const auto action = accessibleEdit(key).action;
        if (action == Action::OpenMenu || action == Action::OpenEditor || action == Action::Trigger)
        {
            onPopupMenu(key.getModifiers());
            return true;
        }
        if (isEditingBlocked())
            return false;
        const bool adjusting = action == Action::Increase || action == Action::Decrease;
        const auto handled = Knob::keyPressed(key);
        // The upstream editor adjusts on arrow keys but does not report them as consumed.
        return handled || adjusting;
    }

  private:
    void repaintParent()
    {
        if (auto *parent = getParentComponent())
            parent->repaint();
    }
    Presentation presentation{Presentation::rotary};
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

#endif // SURGE_SRC_SURGE_FX_FXWIDGETS_H
