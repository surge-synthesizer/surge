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

#include "version.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "BinaryData.h"
#include "juce/JuceAPICope.h"

using namespace juce;

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
        Colour surgeGrayBg = Colour(205, 206, 212);
        Colour surgeOrange = Colour(255, 144, 0);
        Colour fxStandaloneGray1 = Colour(32, 32, 32);
        Colour fxStandaloneGray2 = Colour(48, 48, 48);
        Colour fxStandaloneGray3 = Colour(128, 128, 128);
        Colour fxStandaloneGray4 = Colour(212, 212, 212);

        setColour(DocumentWindow::backgroundColourId, Colour(48, 48, 48));
        setColour(TextButton::buttonColourId, Colour(32, 32, 32));
        setColour(CaretComponent::caretColourId, Colours::white);
        setColour(TextEditor::backgroundColourId, Colour(32, 32, 32));
        setColour(TextEditor::highlightColourId, Colour(96, 96, 96));
        setColour(ListBox::backgroundColourId, Colour(32, 32, 32));
        setColour(ScrollBar::thumbColourId, Colour(212, 212, 212));
        setColour(ScrollBar::trackColourId, Colour(128, 128, 128));
        setColour(Slider::thumbColourId, Colour(212, 212, 212));
        setColour(Slider::trackColourId, Colour(128, 128, 128));
        setColour(Slider::backgroundColourId, Colour((uint8)255, 255, 255, 20.f));
        setColour(ComboBox::backgroundColourId, Colour(32, 32, 32));

        setColour(PopupMenu::backgroundColourId, Colour(48, 48, 48));
        setColour(PopupMenu::highlightedBackgroundColourId, Colour(96, 96, 96));
        setColour(PopupMenu::textColourId, Colours::white);
        setColour(PopupMenu::headerTextColourId, Colours::white);
        setColour(PopupMenu::highlightedTextColourId, Colour(240, 240, 250));

        setColour(AlertWindow::backgroundColourId, Colour(36, 36, 36));
        setColour(AlertWindow::outlineColourId, Colour(16, 16, 16));
        setColour(AlertWindow::textColourId, Colours::white);

        setColour(SurgeColourIds::componentBgStart, surgeGrayBg);
        setColour(SurgeColourIds::componentBgEnd, surgeGrayBg);
        setColour(SurgeColourIds::orange, surgeOrange);

        setColour(SurgeColourIds::knobHandle, Colours::white);
        setColour(SurgeColourIds::knobBg, surgeOrange);
        setColour(SurgeColourIds::knobEdge, Colours::black);

        auto disableOpacity = 0.5;
        setColour(SurgeColourIds::knobHandleDisable,
                  Colours::black.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::knobBgDisable,
                  surgeOrange.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::knobEdgeDisable, Colours::black);

        setColour(SurgeColourIds::fxButtonTextUnselected, Colours::white);
        setColour(SurgeColourIds::fxButtonTextSelected, Colours::black);

        setColour(SurgeColourIds::paramEnabledBg, Colours::black);
        setColour(SurgeColourIds::paramEnabledEdge, surgeOrange);
        setColour(SurgeColourIds::paramDisabledBg,
                  Colours::black.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::paramDisabledEdge, Colour(150, 150, 150));
        setColour(SurgeColourIds::paramDisplay, Colours::white);

        surgeLogo = Drawable::createFromImageData(BinaryData::SurgeLogoOnlyBlack_svg,
                                                  BinaryData::SurgeLogoOnlyBlack_svgSize);
    }

    void drawDocumentWindowTitleBar(DocumentWindow &window, Graphics &g, int w, int h,
                                    int titleSpaceX, int titleSpaceY, const Image *image,
                                    bool iconOnLeft) override
    {
        g.fillAll(Colour(16, 16, 16));
        g.setColour(Colours::white);

        auto wt = window.getName();

        String surgeLabel = "Surge XT Effects";

        auto titleCenter = w / 2;
        auto titleTextWidth = SST_STRING_WIDTH_INT(g.getCurrentFont(), surgeLabel);

        const int logoSize = h - 12;
        const int logoSpacing = 8;
        int logoX = titleCenter - (titleTextWidth / 2) - logoSize - logoSpacing;

        if (surgeLogo)
        {
            auto logoBounds =
                juce::Rectangle<float>(logoX, (h - logoSize) / 2.f, logoSize, logoSize);
            surgeLogo->drawWithin(g, logoBounds, juce::RectanglePlacement::centred, 1.0f);
        }

        auto textBounds =
            juce::Rectangle<int>(titleCenter - (titleTextWidth / 2), 0, titleTextWidth, h);
        g.drawText(surgeLabel, textBounds, Justification::centredLeft);
    }

    class SurgeLookAndFeel_DocumentWindowButton : public Button
    {
      public:
        SurgeLookAndFeel_DocumentWindowButton(const String &name, Colour c, const Path &normal,
                                              const Path &toggled)
            : Button(name), colour(c), normalShape(normal), toggledShape(toggled)
        {
        }

        void paintButton(Graphics &g, bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
        {
            if (shouldDrawButtonAsHighlighted)
            {
                g.setColour(Colour(96, 96, 96));
                g.fillAll();
            }

            g.setColour(colour);
            auto &p = getToggleState() ? toggledShape : normalShape;

            auto reducedRect =
                Justification(Justification::centred)
                    .appliedToRectangle(juce::Rectangle<int>(getHeight(), getHeight()),
                                        getLocalBounds())
                    .toFloat()
                    .reduced((float)getHeight() * 0.25f);

            g.fillPath(p, p.getTransformToScaleToFit(reducedRect, true));
        }

      private:
        Colour colour;
        Path normalShape, toggledShape;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeLookAndFeel_DocumentWindowButton)
    };

    Button *createDocumentWindowButton(int buttonType) override
    {
        Path shape;
        auto crossThickness = 0.25f;

        if (buttonType == DocumentWindow::closeButton)
        {
            shape.addLineSegment({0.0f, 0.0f, 1.0f, 1.0f}, crossThickness);
            shape.addLineSegment({1.0f, 0.0f, 0.0f, 1.0f}, crossThickness);

            return new SurgeLookAndFeel_DocumentWindowButton("close", Colour(212, 64, 64), shape,
                                                             shape);
        }

        if (buttonType == DocumentWindow::minimiseButton)
        {
            // HACK: fake a top line so that we get minimize aligned to the bottom
            shape.addLineSegment({0.0f, 0.0f, 1.0f, 0.0f}, 0.0f);
            shape.addLineSegment({0.0f, 0.9f, 1.0f, 0.9f}, crossThickness);

            return new SurgeLookAndFeel_DocumentWindowButton("minimize", Colour(255, 212, 32),
                                                             shape, shape);
        }

        if (buttonType == DocumentWindow::maximiseButton) // HACK
        {
            shape.addLineSegment({0.0f, 0.0f, 1.0f, 0.0f}, crossThickness); // top
            shape.addRectangle(0.0f, 0.0f, 1.0f, 1.0f);
            PathStrokeType(crossThickness).createStrokedPath(shape, shape);

            Path fullscreenShape;
            fullscreenShape.startNewSubPath(45.0f, 100.0f);
            fullscreenShape.lineTo(0.0f, 100.0f);
            fullscreenShape.lineTo(0.0f, 0.0f);
            fullscreenShape.lineTo(100.0f, 0.0f);
            fullscreenShape.lineTo(100.0f, 45.0f);
            fullscreenShape.addRectangle(45.0f, 45.0f, 100.0f, 100.0f);
            PathStrokeType(30.0f).createStrokedPath(fullscreenShape, fullscreenShape);

            return new SurgeLookAndFeel_DocumentWindowButton("maximize", Colour(0xff0A830A), shape,
                                                             fullscreenShape);
        }

        jassertfalse;
        return nullptr;
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

    void drawCornerResizer(juce::Graphics &g, int w, int h, bool, bool) override {};

    void paintComponentBackground(juce::Graphics &g, const int w, const int h, const float z)
    {
        const float footer = 40.f * z;
        const float fW = static_cast<float>(w);
        const float fH = static_cast<float>(h);

        g.fillAll(findColour(SurgeColourIds::componentBgStart));

        juce::ColourGradient cg(findColour(SurgeColourIds::componentBgStart), 0.f, 0.f,
                                findColour(SurgeColourIds::componentBgEnd), 0.f, fH - footer,
                                false);
        g.setGradientFill(cg);
        g.fillRect(juce::Rectangle<float>(0.f, 0.f, fW, fH - footer));

        g.setColour(findColour(SurgeColourIds::orange));
        g.fillRect(juce::Rectangle<float>(0.f, fH - footer, fW, footer));

        const float padding = 4.f * z;
        const float logoSide = footer - (padding * 2.f);

        juce::Rectangle<float> logoBound{3.f * z, fH - footer + padding, logoSide, logoSide};

        if (surgeLogo != nullptr)
        {
            surgeLogo->drawWithin(
                g, logoBound, juce::RectanglePlacement::xMid | juce::RectanglePlacement::yMid, 1.f);
        }

        g.setColour(juce::Colours::black);
        g.drawLine(0.f, fH - footer, fW, fH - footer, z);

        g.setFont(SST_JUCE_FONT_OPTIONS(12.f * z));

        const float rightMargin = fW - (8.f * z);
        const float versionY = fH - (23.f * z);
        const float dateY = fH - (9.f * z);

        g.drawSingleLineText(Surge::Build::FullVersionStr, rightMargin, versionY,
                             juce::Justification::right);
        g.drawSingleLineText(Surge::Build::BuildDate, rightMargin, dateY,
                             juce::Justification::right);
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
            g.setFont(SST_JUCE_FONT_OPTIONS(10 * hScale));
            g.drawSingleLineText(group, bounds.getX() + 5, bounds.getY() + 2 + 10 * hScale);
            g.setFont(SST_JUCE_FONT_OPTIONS(12 * hScale));
            g.drawSingleLineText(name, bounds.getX() + 5,
                                 bounds.getY() + 2 + (10 + 3 + 11) * hScale);

            if (!overlayEditor->isVisible())
            {
                g.setFont(SST_JUCE_FONT_OPTIONS(20 * hScale));
                g.drawSingleLineText(display, bounds.getX() + 5,
                                     bounds.getY() + bounds.getHeight() - 5);
            }
        }
    }

    void resized() override
    {
        auto hScale = getHeight() / 55.0;

        overlayEditor->setBounds(
            getLocalBounds().reduced(2, 3).withTrimmedTop(getHeight() - 28 * hScale));
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

        auto hScale = getHeight() / 55.0;

        overlayEditor->setColour(juce::TextEditor::textColourId,
                                 findColour(SurgeLookAndFeel::SurgeColourIds::paramDisplay));
        overlayEditor->setColour(juce::TextEditor::outlineColourId,
                                 juce::Colours::transparentBlack);
        overlayEditor->setColour(juce::TextEditor::focusedOutlineColourId,
                                 juce::Colours::transparentBlack);
        overlayEditor->setColour(juce::TextEditor::ColourIds::highlightColourId,
                                 juce::Colour(0xFF775522));
        overlayEditor->setJustification(juce::Justification::bottomLeft);
        overlayEditor->setFont(SST_JUCE_FONT_OPTIONS(20 * hScale));
        overlayEditor->applyFontToAllText(SST_JUCE_FONT_OPTIONS(20 * hScale), true);
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

class SurgeParamOptionSwitch : public juce::ToggleButton
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
            const auto b = getLocalBounds().toFloat();

            if (getToggleState())
            {
                onImg->drawWithin(g, b, juce::RectanglePlacement::centred, 1.f);
            }
            else
            {
                offImg->drawWithin(g, b, juce::RectanglePlacement::centred, 1.f);
            }

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
        {
            g.fillRoundedRectangle(bounds, radius);
        }

        g.setColour(edge);
        g.drawRoundedRectangle(bounds, radius, 1);

        if (!isEnabled())
        {
            return;
        }

        float controlRadius = bounds.getWidth() - 4;
        float xPos = bounds.getX() + bounds.getWidth() / 2.0 - controlRadius / 2.0;
        float yPos = bounds.getY() + bounds.getHeight() - controlRadius - 2;

        auto kbounds = juce::Rectangle<float>(xPos, yPos, controlRadius, controlRadius);

        g.setColour(handle);
        g.drawRoundedRectangle(kbounds, controlRadius, 1);

        if (getToggleState())
        {
            g.fillRoundedRectangle(kbounds, controlRadius);
        }
    }
};
