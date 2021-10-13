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

#include "SurgeJUCELookAndFeel.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"
#include "version.h"

#include <juce_audio_utils/juce_audio_utils.h>

void SurgeJUCELookAndFeel::onSkinChanged()
{
    setColour(topWindowBorderId, skin->getColor(Colors::Dialog::Titlebar::Background));
    setColour(tempoTypeinTextId, juce::Colours::black);
    setColour(tempoTypeinHighlightId, juce::Colours::red);

    setColour(juce::DocumentWindow::backgroundColourId, juce::Colour(48, 48, 48));
    setColour(juce::TextButton::buttonColourId, juce::Colour(32, 32, 32));
    setColour(juce::CaretComponent::caretColourId, skin->getColor(Colors::Dialog::Entry::Caret));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(32, 32, 32));
    setColour(juce::ListBox::backgroundColourId, juce::Colour(32, 32, 32));
    setColour(juce::ListBox::backgroundColourId, juce::Colour(32, 32, 32));
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(212, 212, 212));
    setColour(juce::ScrollBar::trackColourId, juce::Colour(128, 128, 128));
    setColour(juce::Slider::thumbColourId, juce::Colour(212, 212, 212));
    setColour(juce::Slider::trackColourId, juce::Colour(128, 128, 128));
    setColour(juce::Slider::backgroundColourId, juce::Colour((juce::uint8)255, 255, 255, 20.f));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(32, 32, 32));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(48, 48, 48));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(96, 96, 96));

    setColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoBackgroundId,
              skin->getColor(Colors::Dialog::Background));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoLabelId,
              skin->getColor(Colors::Dialog::Label::Text));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBackgroundId,
              skin->getColor(Colors::Dialog::Entry::Background));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBorderId,
              skin->getColor(Colors::Dialog::Entry::Border));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinHighlightId,
              skin->getColor(Colors::Dialog::Entry::Focus));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId,
              skin->getColor(Colors::Dialog::Entry::Text));

    setColour(juce::MidiKeyboardComponent::textLabelColourId,
              skin->getColor(Colors::VirtualKeyboard::Text));
    setColour(juce::MidiKeyboardComponent::shadowColourId,
              skin->getColor(Colors::VirtualKeyboard::Shadow));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::Black));
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::White));
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::Separator));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::MouseOver));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::Pressed));
    setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId,
              skin->getColor(Colors::VirtualKeyboard::OctaveJog::Background));
    setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId,
              skin->getColor(Colors::VirtualKeyboard::OctaveJog::Arrow));
}

void SurgeJUCELookAndFeel::drawLabel(juce::Graphics &graphics, juce::Label &label)
{
    LookAndFeel_V4::drawLabel(graphics, label);
}

int SurgeJUCELookAndFeel::getTabButtonBestWidth(juce::TabBarButton &b, int d)
{
    auto f = skin->getFont(Fonts::Widgets::TabButtonFont);
    auto r = f.getStringWidth(b.getButtonText()) + 20;
    return r;
}

void SurgeJUCELookAndFeel::drawTabButton(juce::TabBarButton &button, juce::Graphics &g,
                                         bool isMouseOver, bool isMouseDown)
{
    auto o = button.getTabbedButtonBar().getOrientation();

    if (o != juce::TabbedButtonBar::TabsAtBottom)
    {
        juce::LookAndFeel_V4::drawTabButton(button, g, isMouseOver, isMouseDown);
        return;
    }

    auto activeArea = button.getActiveArea();
    auto fillColor = skin->getColor(Colors::JuceWidgets::TabbedBar::InactiveTabBackground);
    auto textColor = skin->getColor(Colors::JuceWidgets::TabbedBar::Text);

    if (button.getToggleState())
    {
        fillColor = skin->getColor(Colors::JuceWidgets::TabbedBar::ActiveTabBackground);
    }

    if (isMouseOver || isMouseDown)
    {
        textColor = skin->getColor(Colors::JuceWidgets::TabbedBar::TextHover);
    }

    g.setColour(skin->getColor(Colors::JuceWidgets::TabbedBar::Border));
    g.drawRect(activeArea);

    auto fa = activeArea.withTrimmedLeft(1).withTrimmedRight(1).withTrimmedBottom(1);
    g.setColour(fillColor);
    g.fillRect(fa);

    g.setColour(textColor);
    auto f = skin->getFont(Fonts::Widgets::TabButtonFont);
    g.setFont(f);
    g.drawText(button.getButtonText(), activeArea, juce::Justification::centred);
}

void SurgeJUCELookAndFeel::drawTextEditorOutline(juce::Graphics &g, int width, int height,
                                                 juce::TextEditor &textEditor)
{
    if (dynamic_cast<juce::AlertWindow *>(textEditor.getParentComponent()) == nullptr)
    {
        if (textEditor.isEnabled())
        {
            if (textEditor.hasKeyboardFocus(true) && !textEditor.isReadOnly())
            {
                g.setColour(textEditor.findColour(juce::TextEditor::focusedOutlineColourId));
                // This is the only change from V4; use a 1 rather than 2 here
                g.drawRect(0, 0, width, height, 1);
            }
            else
            {
                g.setColour(textEditor.findColour(juce::TextEditor::outlineColourId));
                g.drawRect(0, 0, width, height);
            }
        }
    }
}

void SurgeJUCELookAndFeel::drawDocumentWindowTitleBar(juce::DocumentWindow &window,
                                                      juce::Graphics &g, int w, int h,
                                                      int titleSpaceX, int titleSpaceY,
                                                      const juce::Image *image, bool iconOnLeft)
{
    g.fillAll(skin->getColor(Colors::Dialog::Titlebar::Background));
    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Text));

    auto wt = window.getName();

    juce::String surgeLabel = "Surge XT";
    auto surgeVersion = Surge::Build::FullVersionStr;
    auto fontSurge = Surge::GUI::getFontManager()->getLatoAtSize(14);
    auto fontVersion = Surge::GUI::getFontManager()->getFiraMonoAtSize(14);

    if (wt != "Surge XT")
    {
        surgeLabel = wt;
        surgeVersion = "Surge XT";
        fontVersion = fontSurge;
    }

    auto sw = fontSurge.getStringWidth(surgeLabel);
    auto vw = fontVersion.getStringWidth(surgeVersion);

    auto ic = associatedBitmapStore->getImage(IDB_SURGE_ICON);

    // Surge icon is 12 x 14 so draw that in the center
    auto titleCenter = w / 2;
    auto textMargin = 5;
    auto titleTextWidth = sw + vw + textMargin;

    if (ic)
    {
        ic->drawAt(g, titleCenter - (titleTextWidth / 2) - 14 - textMargin, h / 2 - 7, 1.0);
    }

    auto boxSurge = juce::Rectangle<int>(titleCenter - (titleTextWidth / 2), 0, sw, h);
    g.setFont(fontSurge);
    g.drawText(surgeLabel, boxSurge, juce::Justification::centredLeft);

    auto boxVersion =
        juce::Rectangle<int>(titleCenter - (titleTextWidth / 2) + sw + textMargin, 0, vw, h);
    g.setFont(fontVersion);
    g.drawText(surgeVersion, boxVersion, juce::Justification::centredLeft);
}

class SurgeJUCELookAndFeel_DocumentWindowButton : public juce::Button
{
  public:
    SurgeJUCELookAndFeel_DocumentWindowButton(const juce::String &name, juce::Colour c,
                                              const juce::Path &normal, const juce::Path &toggled)
        : Button(name), colour(c), normalShape(normal), toggledShape(toggled)
    {
    }

    void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(96, 96, 96));
            g.fillAll();
        }

        g.setColour(colour);
        auto &p = getToggleState() ? toggledShape : normalShape;

        auto reducedRect = juce::Justification(juce::Justification::centred)
                               .appliedToRectangle(juce::Rectangle<int>(getHeight(), getHeight()),
                                                   getLocalBounds())
                               .toFloat()
                               .reduced((float)getHeight() * 0.3f);

        g.fillPath(p, p.getTransformToScaleToFit(reducedRect, true));
    }

  private:
    juce::Colour colour;
    juce::Path normalShape, toggledShape;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeJUCELookAndFeel_DocumentWindowButton)
};

juce::Button *SurgeJUCELookAndFeel::createDocumentWindowButton(int buttonType)
{
    juce::Path shape;
    auto crossThickness = 0.25f;

    if (buttonType == juce::DocumentWindow::closeButton)
    {
        shape.addLineSegment({0.0f, 0.0f, 1.0f, 1.0f}, crossThickness);
        shape.addLineSegment({1.0f, 0.0f, 0.0f, 1.0f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("close", juce::Colour(212, 64, 64),
                                                             shape, shape);
    }

    if (buttonType == juce::DocumentWindow::minimiseButton)
    {
        shape.addLineSegment({0.0f, 0.5f, 1.0f, 0.5f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("minimise", juce::Colour(255, 212, 32),
                                                             shape, shape);
    }

    if (buttonType == juce::DocumentWindow::maximiseButton)
    {
        shape.addLineSegment({0.5f, 0.0f, 0.5f, 1.0f}, crossThickness);
        shape.addLineSegment({0.0f, 0.5f, 1.0f, 0.5f}, crossThickness);

        juce::Path fullscreenShape;
        fullscreenShape.startNewSubPath(45.0f, 100.0f);
        fullscreenShape.lineTo(0.0f, 100.0f);
        fullscreenShape.lineTo(0.0f, 0.0f);
        fullscreenShape.lineTo(100.0f, 0.0f);
        fullscreenShape.lineTo(100.0f, 45.0f);
        fullscreenShape.addRectangle(45.0f, 45.0f, 100.0f, 100.0f);
        juce::PathStrokeType(30.0f).createStrokedPath(fullscreenShape, fullscreenShape);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("maximise", juce::Colour(0xff0A830A),
                                                             shape, fullscreenShape);
    }

    jassertfalse;
    return nullptr;
}
