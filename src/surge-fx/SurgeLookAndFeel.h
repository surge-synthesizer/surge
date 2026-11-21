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

    void drawPopupMenuSectionHeader(juce::Graphics &g, const juce::Rectangle<int> &area,
                                    const juce::String &sectionName) override
    {
        if (sectionName == "#GHOST#")
            return; // Draw nothing, leaving an empty gap

        g.setFont(getPopupMenuFont().boldened());
        g.setColour(findColour(juce::PopupMenu::headerTextColourId));

        g.drawFittedText(sectionName, area.getX() + 12, area.getY(), area.getWidth() - 16,
                         (int)((float)area.getHeight() * 0.8f), juce::Justification::bottomLeft, 1);
    }
};
