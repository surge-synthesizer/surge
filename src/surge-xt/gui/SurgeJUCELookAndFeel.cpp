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

// here and only here we using namespace juce so I can copy and override stuff from v4 easily
using namespace juce;

void SurgeJUCELookAndFeel::onSkinChanged()
{
    setColour(topWindowBorderId, skin->getColor(Colors::Dialog::Titlebar::Background));
    setColour(tempoTypeinTextId, Colours::black);
    setColour(tempoTypeinHighlightId, Colours::red);

    setColour(DocumentWindow::backgroundColourId, Colour(48, 48, 48));
    setColour(TextButton::buttonColourId, Colour(32, 32, 32));
    setColour(CaretComponent::caretColourId, skin->getColor(Colors::Dialog::Entry::Caret));
    setColour(TextEditor::backgroundColourId, Colour(32, 32, 32));
    setColour(ListBox::backgroundColourId, Colour(32, 32, 32));
    setColour(ListBox::backgroundColourId, Colour(32, 32, 32));
    setColour(ScrollBar::thumbColourId, Colour(212, 212, 212));
    setColour(ScrollBar::trackColourId, Colour(128, 128, 128));
    setColour(Slider::thumbColourId, Colour(212, 212, 212));
    setColour(Slider::trackColourId, Colour(128, 128, 128));
    setColour(Slider::backgroundColourId, Colour((uint8)255, 255, 255, 20.f));
    setColour(ComboBox::backgroundColourId, Colour(32, 32, 32));
    setColour(PopupMenu::backgroundColourId, Colour(48, 48, 48));
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(96, 96, 96));

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

    setColour(MidiKeyboardComponent::textLabelColourId,
              skin->getColor(Colors::VirtualKeyboard::Text));
    setColour(MidiKeyboardComponent::shadowColourId,
              skin->getColor(Colors::VirtualKeyboard::Shadow));
    setColour(MidiKeyboardComponent::blackNoteColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::Black));
    setColour(MidiKeyboardComponent::whiteNoteColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::White));
    setColour(MidiKeyboardComponent::keySeparatorLineColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::Separator));
    setColour(MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::MouseOver));
    setColour(MidiKeyboardComponent::keyDownOverlayColourId,
              skin->getColor(Colors::VirtualKeyboard::Key::Pressed));
    setColour(MidiKeyboardComponent::upDownButtonArrowColourId,
              skin->getColor(Colors::VirtualKeyboard::OctaveJog::Background));
    setColour(MidiKeyboardComponent::upDownButtonArrowColourId,
              skin->getColor(Colors::VirtualKeyboard::OctaveJog::Arrow));
}

void SurgeJUCELookAndFeel::drawLabel(Graphics &graphics, Label &label)
{
    LookAndFeel_V4::drawLabel(graphics, label);
}

int SurgeJUCELookAndFeel::getTabButtonBestWidth(TabBarButton &b, int d)
{
    auto f = skin->getFont(Fonts::Widgets::TabButtonFont);
    auto r = f.getStringWidth(b.getButtonText()) + 20;
    return r;
}

void SurgeJUCELookAndFeel::drawTabButton(TabBarButton &button, Graphics &g, bool isMouseOver,
                                         bool isMouseDown)
{
    auto o = button.getTabbedButtonBar().getOrientation();

    if (o != TabbedButtonBar::TabsAtBottom)
    {
        LookAndFeel_V4::drawTabButton(button, g, isMouseOver, isMouseDown);
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
    g.drawText(button.getButtonText(), activeArea, Justification::centred);
}

void SurgeJUCELookAndFeel::drawTextEditorOutline(Graphics &g, int width, int height,
                                                 TextEditor &textEditor)
{
    if (dynamic_cast<AlertWindow *>(textEditor.getParentComponent()) == nullptr)
    {
        if (textEditor.isEnabled())
        {
            if (textEditor.hasKeyboardFocus(true) && !textEditor.isReadOnly())
            {
                g.setColour(textEditor.findColour(TextEditor::focusedOutlineColourId));
                // This is the only change from V4; use a 1 rather than 2 here
                g.drawRect(0, 0, width, height, 1);
            }
            else
            {
                g.setColour(textEditor.findColour(TextEditor::outlineColourId));
                g.drawRect(0, 0, width, height);
            }
        }
    }
}

void SurgeJUCELookAndFeel::drawDocumentWindowTitleBar(DocumentWindow &window, Graphics &g, int w,
                                                      int h, int titleSpaceX, int titleSpaceY,
                                                      const Image *image, bool iconOnLeft)
{
    g.fillAll(skin->getColor(Colors::Dialog::Titlebar::Background));
    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Text));

    auto wt = window.getName();

    String surgeLabel = "Surge XT";
    std::string surgeVersion = Surge::Build::FullVersionStr;
    auto fontSurge = Surge::GUI::getFontManager()->getLatoAtSize(14);
    auto fontVersion = Surge::GUI::getFontManager()->getFiraMonoAtSize(14);

#if BUILD_IS_DEBUG
    surgeVersion += " DEBUG";
#endif

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

    auto boxSurge = Rectangle<int>(titleCenter - (titleTextWidth / 2), 0, sw, h);
    g.setFont(fontSurge);
    g.drawText(surgeLabel, boxSurge, Justification::centredLeft);

    auto boxVersion =
        Rectangle<int>(titleCenter - (titleTextWidth / 2) + sw + textMargin, 0, vw, h);
    g.setFont(fontVersion);
    g.drawText(surgeVersion, boxVersion, Justification::centredLeft);
}

class SurgeJUCELookAndFeel_DocumentWindowButton : public Button
{
  public:
    SurgeJUCELookAndFeel_DocumentWindowButton(const String &name, Colour c, const Path &normal,
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
                .appliedToRectangle(Rectangle<int>(getHeight(), getHeight()), getLocalBounds())
                .toFloat()
                .reduced((float)getHeight() * 0.3f);

        g.fillPath(p, p.getTransformToScaleToFit(reducedRect, true));
    }

  private:
    Colour colour;
    Path normalShape, toggledShape;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeJUCELookAndFeel_DocumentWindowButton)
};

Button *SurgeJUCELookAndFeel::createDocumentWindowButton(int buttonType)
{
    Path shape;
    auto crossThickness = 0.25f;

    if (buttonType == DocumentWindow::closeButton)
    {
        shape.addLineSegment({0.0f, 0.0f, 1.0f, 1.0f}, crossThickness);
        shape.addLineSegment({1.0f, 0.0f, 0.0f, 1.0f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("close", Colour(212, 64, 64), shape,
                                                             shape);
    }

    if (buttonType == DocumentWindow::minimiseButton)
    {
        shape.addLineSegment({0.0f, 0.5f, 1.0f, 0.5f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("minimise", Colour(255, 212, 32),
                                                             shape, shape);
    }

    if (buttonType == DocumentWindow::maximiseButton)
    {
        shape.addLineSegment({0.5f, 0.0f, 0.5f, 1.0f}, crossThickness);
        shape.addLineSegment({0.0f, 0.5f, 1.0f, 0.5f}, crossThickness);

        Path fullscreenShape;
        fullscreenShape.startNewSubPath(45.0f, 100.0f);
        fullscreenShape.lineTo(0.0f, 100.0f);
        fullscreenShape.lineTo(0.0f, 0.0f);
        fullscreenShape.lineTo(100.0f, 0.0f);
        fullscreenShape.lineTo(100.0f, 45.0f);
        fullscreenShape.addRectangle(45.0f, 45.0f, 100.0f, 100.0f);
        PathStrokeType(30.0f).createStrokedPath(fullscreenShape, fullscreenShape);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("maximise", Colour(0xff0A830A), shape,
                                                             fullscreenShape);
    }

    jassertfalse;
    return nullptr;
}
