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

#ifndef SURGE_SRC_SURGE_FX_SURGELOOKANDFEEL_H
#define SURGE_SRC_SURGE_FX_SURGELOOKANDFEEL_H
#include "version.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "BinaryData.h"

class SurgeLookAndFeel : public juce::LookAndFeel_V4
{
  private:
    std::unique_ptr<juce::Drawable> surgeLogo;

  public:
    enum SurgeColourIds
    {
        componentBgStart = 0x2700001,
        componentBgEnd,

        orange,
        orangeMedium,
        orangeDark,

        knobEdge,
        knobBg,
        knobHandle,

        knobEdgeDisable,
        knobBgDisable,
        knobHandleDisable,

        paramEnabledBg,
        paramEnabledEdge,
        paramDisabledBg,
        paramDisabledEdge,
        paramDisplay,

        fxButtonFill,
        fxButtonEdge,
        fxButtonHighlighted,
        fxButtonDown,
        fxButonToggled,

        fxButtonTextUnselected,
        fxButtonTextSelected,
    };

    SurgeLookAndFeel()
    {
        juce::Colour surgeGrayBg = juce::Colour(205, 206, 212);
        juce::Colour surgeOrange = juce::Colour(255, 144, 0);
        juce::Colour white = juce::Colours::white;
        juce::Colour black = juce::Colours::black;
        juce::Colour surgeOrangeDark = juce::Colour(101, 50, 3);
        juce::Colour surgeOrangeMedium = juce::Colour(227, 112, 8);
        juce::Colour fxStandaloneGray1 = juce::Colour(32, 32, 32);
        juce::Colour fxStandaloneGray2 = juce::Colour(48, 48, 48);
        juce::Colour fxStandaloneGray3 = juce::Colour(128, 128, 128);
        juce::Colour fxStandaloneGray4 = juce::Colour(212, 212, 212);

        // TODO: Why don't these work?!
        setColour(juce::DocumentWindow::backgroundColourId, fxStandaloneGray2);
        setColour(juce::ComboBox::backgroundColourId, fxStandaloneGray1);
        setColour(juce::TextButton::buttonColourId, fxStandaloneGray1);
        setColour(juce::TextEditor::backgroundColourId, fxStandaloneGray1);
        setColour(juce::ListBox::backgroundColourId, fxStandaloneGray1);
        setColour(juce::ListBox::backgroundColourId, fxStandaloneGray1);
        setColour(juce::ScrollBar::thumbColourId, fxStandaloneGray4);
        setColour(juce::ScrollBar::trackColourId, fxStandaloneGray3);
        setColour(juce::Slider::thumbColourId, fxStandaloneGray4);
        setColour(juce::Slider::trackColourId, fxStandaloneGray3);
        setColour(juce::Slider::backgroundColourId, juce::Colour((juce::uint8)255, 255, 255, 20.f));

        setColour(SurgeColourIds::componentBgStart,
                  surgeGrayBg.interpolatedWith(surgeOrangeMedium, 0.1));
        setColour(SurgeColourIds::componentBgEnd, surgeGrayBg);
        setColour(SurgeColourIds::orange, surgeOrange);
        setColour(SurgeColourIds::orangeDark, surgeOrangeDark);
        setColour(SurgeColourIds::orangeMedium, surgeOrangeMedium);

        setColour(SurgeColourIds::knobHandle, white);
        setColour(SurgeColourIds::knobBg, surgeOrange);
        setColour(SurgeColourIds::knobEdge, black);

        auto disableOpacity = 0.666;
        setColour(SurgeColourIds::knobHandleDisable,
                  black.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::knobBgDisable,
                  surgeOrange.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::knobEdgeDisable, black);

        setColour(SurgeColourIds::fxButtonTextUnselected, white);
        setColour(SurgeColourIds::fxButtonTextSelected, black);

        setColour(SurgeColourIds::paramEnabledBg, black);
        setColour(SurgeColourIds::paramEnabledEdge, surgeOrange);
        setColour(SurgeColourIds::paramDisabledBg,
                  black.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::paramDisabledEdge, juce::Colour(150, 150, 150));
        setColour(SurgeColourIds::paramDisplay, white);

        surgeLogo = juce::Drawable::createFromImageData(BinaryData::SurgeLogoOnlyBlack_svg,
                                                        BinaryData::SurgeLogoOnlyBlack_svgSize);
    }

    virtual void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                                  float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                  juce::Slider &slider) override
    {
        auto fill = findColour(SurgeColourIds::knobBg);
        auto edge = findColour(SurgeColourIds::knobEdge);
        auto tick = findColour(SurgeColourIds::knobHandle);

        if (!slider.isEnabled())
        {
            fill = findColour(SurgeColourIds::knobBgDisable);
            edge = findColour(SurgeColourIds::knobEdgeDisable);
            tick = findColour(SurgeColourIds::knobHandleDisable);
        }

        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10);
        g.setColour(fill);
        g.fillEllipse(bounds);
        g.setColour(edge);
        g.drawEllipse(bounds, 1.0);

        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto arcRadius = radius;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto thumbWidth = 5;

        juce::Point<float> thumbPoint(
            bounds.getCentreX() +
                arcRadius * std::cos(toAngle - juce::MathConstants<float>::halfPi),
            bounds.getCentreY() +
                arcRadius * std::sin(toAngle - juce::MathConstants<float>::halfPi));

        g.setColour(tick);
        g.fillEllipse(juce::Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));
        g.setColour(edge);
        g.drawEllipse(juce::Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint), 1.0);
        g.setColour(tick);
        g.fillEllipse(
            juce::Rectangle<float>(thumbWidth, thumbWidth).withCentre(bounds.getCentre()));

        auto l = juce::Line<float>(thumbPoint, bounds.getCentre());
        g.drawLine(l, thumbWidth);
    }

    virtual void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                                      const juce::Colour &backgroundColour,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(2.f, 2.f);

        auto isBlack = [](auto const &col) {
            return !(col.getRed() + col.getGreen() + col.getBlue());
        };

        auto col = button.findColour(SurgeColourIds::fxButtonFill);

        float edgeThickness = 1.5;

        if (isBlack(col))
        {
            col = findColour(SurgeColourIds::orangeDark);
        }

        auto edge = juce::Colours::black;

        if (shouldDrawButtonAsHighlighted)
        {
            edge = juce::Colours::white;
            edgeThickness = 2.f;
        }

        if (shouldDrawButtonAsDown)
        {
            edge = edge.darker(0.4);
        }

        if (button.getToggleState())
        {
            col = col.brighter(0.5);
        }

        g.setColour(col);
        g.fillRoundedRectangle(bounds, 3);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, 3, edgeThickness);
    }

    virtual void drawButtonText(juce::Graphics &g, juce::TextButton &button,
                                bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown) override
    {
        button.setColour(juce::TextButton::ColourIds::textColourOffId,
                         findColour(SurgeColourIds::fxButtonTextUnselected));

        button.setColour(juce::TextButton::ColourIds::textColourOnId,
                         findColour(SurgeColourIds::fxButtonTextSelected));

        LookAndFeel_V4::drawButtonText(g, button, shouldDrawButtonAsHighlighted,
                                       shouldDrawButtonAsDown);
    }

    void drawCornerResizer(juce::Graphics &g, int w, int h, bool, bool) override {}

    void paintComponentBackground(juce::Graphics &g, int w, int h)
    {
        int orangeHeight = 40;

        g.fillAll(findColour(SurgeColourIds::componentBgStart));

        juce::ColourGradient cg(findColour(SurgeColourIds::componentBgStart), 0, 0,
                                findColour(SurgeColourIds::componentBgEnd), 0, h - orangeHeight,
                                false);
        g.setGradientFill(cg);
        g.fillRect(0, 0, w, h - orangeHeight);

        g.setColour(findColour(SurgeColourIds::orange));
        g.fillRect(0, h - orangeHeight, w, orangeHeight);

        juce::Rectangle<float> logoBound{3, h - orangeHeight + 4.f, orangeHeight - 8.f,
                                         orangeHeight - 8.f};
        surgeLogo->drawWithin(g, logoBound,
                              juce::RectanglePlacement::xMid | juce::RectanglePlacement::yMid, 1.0);

        g.setColour(juce::Colours::black);
        g.drawLine(0, h - orangeHeight, w, h - orangeHeight);

        // text
        g.setFont(juce::FontOptions(12));
        g.drawSingleLineText(Surge::Build::FullVersionStr, w - 3, h - 26.f,
                             juce::Justification::right);
        g.drawSingleLineText(Surge::Build::BuildDate, w - 3, h - 6.f, juce::Justification::right);
    }
};

class SurgeFXParamDisplay : public juce::Component
{
  public:
    SurgeFXParamDisplay() : juce::Component()
    {
        setAccessible(true);
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        setWantsKeyboardFocus(true);
        overlayEditor = std::make_unique<juce::TextEditor>("edit value");
        overlayEditor->onEscapeKey = [this]() { dismissOverlay(); };
        overlayEditor->onFocusLost = [this]() { dismissOverlay(); };
        overlayEditor->onReturnKey = [this]() { processOverlay(); };
        addChildComponent(*overlayEditor);
    }
    virtual void setGroup(std::string grp)
    {
        group = grp;
        setTitle(name + " " + group);
        setDescription(name + " " + group);
        if (auto *handler = getAccessibilityHandler())
        {
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
        repaint();
    };
    virtual void setName(std::string nm)
    {
        name = nm;
        setTitle(name + " " + group);
        setDescription(name + " " + group);
        if (auto *handler = getAccessibilityHandler())
        {
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
        repaint();
    }
    virtual void setDisplay(std::string dis)
    {
        display = dis;
        repaint();

        if (auto *handler = getAccessibilityHandler())
        {
            if (handler->getValueInterface())
            {
                handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
            }
        }
    };

    virtual void setAppearsDeactivated(bool b)
    {
        appearsDeactivated = b;
        repaint();
    }

    virtual void paint(juce::Graphics &g) override
    {
        auto hScale = getHeight() / 55.0;
        auto bounds = getLocalBounds().toFloat().reduced(2.f, 2.f);
        auto edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledEdge);

        if (isEnabled() && !appearsDeactivated)
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledBg));
        else
        {
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisabledBg));
            edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramDisabledEdge);
        }

        g.fillRoundedRectangle(bounds, 5);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, 5, 1);

        if (isEnabled())
        {
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisplay));
            g.setFont(juce::FontOptions(10 * hScale));
            g.drawSingleLineText(group, bounds.getX() + 5, bounds.getY() + 2 + 10 * hScale);
            g.setFont(juce::FontOptions(12 * hScale));
            g.drawSingleLineText(name, bounds.getX() + 5,
                                 bounds.getY() + 2 + (10 + 3 + 11) * hScale);

            if (!overlayEditor->isVisible())
            {
                g.setFont(juce::FontOptions(20 * hScale));
                g.drawSingleLineText(display, bounds.getX() + 5,
                                     bounds.getY() + bounds.getHeight() - 5);
            }
        }
    }

    void resized() override
    {
        overlayEditor->setBounds(getLocalBounds().reduced(2, 3).withTrimmedTop(getHeight() - 28));
    }

    void mouseDoubleClick(const juce::MouseEvent &e) override
    {
        if (!overlayEditor->isVisible())
            initializeOverlay();
    }

    bool keyPressed(const juce::KeyPress &key) override
    {
        if (key.getKeyCode() == juce::KeyPress::returnKey && !overlayEditor->isVisible())
        {
            initializeOverlay();
            return true;
        }
        return false;
    }

    void initializeOverlay()
    {
        if (!allowsTypein)
            return;

        overlayEditor->setColour(juce::TextEditor::textColourId,
                                 findColour(SurgeLookAndFeel::SurgeColourIds::paramDisplay));
        overlayEditor->setColour(juce::TextEditor::outlineColourId,
                                 juce::Colours::transparentBlack);
        overlayEditor->setColour(juce::TextEditor::focusedOutlineColourId,
                                 juce::Colours::transparentBlack);
        overlayEditor->setColour(juce::TextEditor::ColourIds::highlightColourId,
                                 juce::Colour(0xFF775522));
        overlayEditor->setJustification(juce::Justification::bottomLeft);
        overlayEditor->setFont(juce::FontOptions(20));
        overlayEditor->setText(display, juce::dontSendNotification);
        overlayEditor->setVisible(true);
        overlayEditor->grabKeyboardFocus();
        overlayEditor->selectAll();
    }

    void dismissOverlay() { overlayEditor->setVisible(false); }

    void processOverlay()
    {
        onOverlayEntered(overlayEditor->getText().toStdString());
        dismissOverlay();
    }

    std::function<void(const std::string &)> onOverlayEntered = [](const std::string &) {};
    bool allowsTypein{true};

    struct AH : public juce::AccessibilityHandler
    {
        struct AHV : public juce::AccessibilityValueInterface
        {
            explicit AHV(SurgeFXParamDisplay *s) : comp(s) {}

            SurgeFXParamDisplay *comp;

            bool isReadOnly() const override { return true; }
            double getCurrentValue() const override { return 0.; }

            void setValue(double) override {}
            void setValueAsString(const juce::String &newValue) override {}
            AccessibleValueRange getRange() const override { return {{0, 1}, 1}; }
            juce::String getCurrentValueAsString() const override { return comp->display; }

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AHV);
        };

        explicit AH(SurgeFXParamDisplay *s)
            : comp(s),
              juce::AccessibilityHandler(*s, juce::AccessibilityRole::button,
                                         juce::AccessibilityActions().addAction(
                                             juce::AccessibilityActionType::press,
                                             [this]() { this->comp->initializeOverlay(); }),
                                         AccessibilityHandler::Interfaces{std::make_unique<AHV>(s)})
        {
        }

        SurgeFXParamDisplay *comp;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AH);
    };

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<AH>(this);
    }

  private:
    std::string group = "Uninit";
    std::string name = "Uninit";
    std::string display = "SoftwareError";
    bool appearsDeactivated = false;
    std::unique_ptr<juce::TextEditor> overlayEditor;
};

class SurgeTempoSyncSwitch : public juce::ToggleButton
{
  public:
    void setOnOffImage(const char *onimgData, size_t onimgSize, const char *offimgData,
                       size_t offimgSize)
    {
        onImg = juce::Drawable::createFromImageData(onimgData, onimgSize);
        offImg = juce::Drawable::createFromImageData(offimgData, offimgSize);
    }
    std::unique_ptr<juce::Drawable> onImg, offImg;

  protected:
    virtual void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override
    {
        if (isEnabled() && onImg && offImg)
        {
            if (getToggleState())
                onImg->drawAt(g, 0, 0, 1);
            else
                offImg->drawAt(g, 0, 0, 1);
            return;
        }
        auto bounds = getLocalBounds().toFloat().reduced(1.f, 1.f);
        auto edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledEdge);
        auto handle = findColour(SurgeLookAndFeel::SurgeColourIds::orange);
        float radius = 5;

        if (isEnabled())
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledBg));
        else
        {
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisabledBg));
            edge = findColour(SurgeLookAndFeel::SurgeLookAndFeel::paramDisabledEdge);
        }

        if (isEnabled())
            g.fillRoundedRectangle(bounds, radius);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, radius, 1);

        if (!isEnabled())
            return;

        float controlRadius = bounds.getWidth() - 4;
        float xPos = bounds.getX() + bounds.getWidth() / 2.0 - controlRadius / 2.0;
        float yPos = bounds.getY() + bounds.getHeight() - controlRadius - 2;

        auto kbounds = juce::Rectangle<float>(xPos, yPos, controlRadius, controlRadius);
        g.setColour(handle);

        g.drawRoundedRectangle(kbounds, controlRadius, 1);
        if (getToggleState())
            g.fillRoundedRectangle(kbounds, controlRadius);
    }
};
#endif // SURGE_SRC_SURGE_FX_SURGELOOKANDFEEL_H
