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

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "SurgeJUCELookAndFeel.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"
#include "version.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include "UserDefaults.h"
#include "sst/plugininfra/misc_platform.h"

// here and only here we using namespace juce so I can copy and override stuff from v4 easily
using namespace juce;

void SurgeJUCELookAndFeel::onSkinChanged()
{
    setColour(topWindowBorderId, skin->getColor(Colors::Dialog::Titlebar::Background));

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

    int menuMode = 0;

    if (hasStorage())
    {
        menuMode = Surge::Storage::getUserDefaultValue(storage(), Surge::Storage::MenuLightness, 2);
    }

    if (menuMode == 1)
    {
        if (sst::plugininfra::misc_platform::isDarkMode())
        {
            lastDark = 1;
            setColour(PopupMenu::backgroundColourId, Colour(48, 48, 48));
            setColour(PopupMenu::highlightedBackgroundColourId, Colour(96, 96, 96));
            setColour(PopupMenu::textColourId, juce::Colours::white);
            setColour(PopupMenu::headerTextColourId, juce::Colours::white);
            setColour(PopupMenu::highlightedTextColourId, juce::Colour(240, 240, 250));
        }
        else
        {
            lastDark = 0;
            setColour(PopupMenu::backgroundColourId, juce::Colours::white);
            setColour(PopupMenu::highlightedBackgroundColourId, Colour(220, 220, 230));
            setColour(PopupMenu::textColourId, juce::Colours::black);
            setColour(PopupMenu::headerTextColourId, juce::Colours::black);
            setColour(PopupMenu::highlightedTextColourId, juce::Colour(0, 0, 40));
        }
    }
    else
    {
        setColour(PopupMenu::backgroundColourId, skin->getColor(Colors::PopupMenu::Background));
        setColour(PopupMenu::highlightedBackgroundColourId,
                  skin->getColor(Colors::PopupMenu::HiglightedBackground));
        setColour(PopupMenu::textColourId, skin->getColor(Colors::PopupMenu::Text));
        setColour(PopupMenu::headerTextColourId, skin->getColor(Colors::PopupMenu::HeaderText));
        setColour(PopupMenu::highlightedTextColourId,
                  skin->getColor(Colors::PopupMenu::HighlightedText));
    }

    setColour(AlertWindow::backgroundColourId, skin->getColor(Colors::Dialog::Background));
    setColour(AlertWindow::outlineColourId, skin->getColor(Colors::Dialog::Border));
    setColour(AlertWindow::textColourId, skin->getColor(Colors::Dialog::Label::Text));

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

    setColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelBgId,
              skin->getColor(Colors::VirtualKeyboard::Wheel::Background));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelBorderId,
              skin->getColor(Colors::VirtualKeyboard::Wheel::Border));
    setColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelValueId,
              skin->getColor(Colors::VirtualKeyboard::Wheel::Value));

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
    setColour(MidiKeyboardComponent::upDownButtonBackgroundColourId,
              skin->getColor(Colors::VirtualKeyboard::OctaveJog::Background));
    setColour(MidiKeyboardComponent::upDownButtonArrowColourId,
              skin->getColor(Colors::VirtualKeyboard::OctaveJog::Arrow));

    setColour(ToggleButton::textColourId, skin->getColor(Colors::Dialog::Label::Text));
    setColour(ToggleButton::tickColourId, skin->getColor(Colors::Dialog::Checkbox::Tick));
    setColour(ToggleButton::tickDisabledColourId, skin->getColor(Colors::Dialog::Checkbox::Border));
}

void SurgeJUCELookAndFeel::drawLabel(Graphics &graphics, Label &label)
{
    LookAndFeel_V4::drawLabel(graphics, label);
}

int SurgeJUCELookAndFeel::getTabButtonBestWidth(TabBarButton &b, int d)
{
    auto f = skin->getFont(Fonts::Widgets::TabButtonFont);
    auto r = SST_STRING_WIDTH_INT(f, b.getButtonText()) + 20;
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
    String surgeVersion = Surge::Build::FullVersionStr;
    auto fontSurge = skin->fontManager->getLatoAtSize(14, juce::Font::bold);
    auto fontVersion = skin->fontManager->getFiraMonoAtSize(14, juce::Font::bold);

#if BUILD_IS_DEBUG
    surgeVersion += " DEBUG";
#endif

    if (Surge::Build::IsRelease)
        surgeVersion = "";

    if (wt != "Surge XT")
    {
        surgeLabel = "Surge XT -";
        surgeVersion = wt;
        fontVersion = fontSurge;
    }

    auto sw = SST_STRING_WIDTH_INT(fontSurge, surgeLabel);
    auto vw = SST_STRING_WIDTH_INT(fontVersion, surgeVersion);

    auto icon = associatedBitmapStore->getImage(IDB_SURGE_ICON);

    // Surge icon is 12 x 14 so draw that in the center
    auto titleCenter = w / 2;
    auto textMargin = Surge::Build::IsRelease ? 0 : 5;
    auto titleTextWidth = sw + vw + textMargin;

    if (icon)
    {
        icon->drawAt(g, titleCenter - (titleTextWidth / 2) - 14 - textMargin, h / 2 - 7, 1.0);
    }

    auto boxSurge = juce::Rectangle<int>(titleCenter - (titleTextWidth / 2), 0, sw, h);
    g.setFont(fontSurge);
    g.drawText(surgeLabel, boxSurge, Justification::centredLeft);

    auto boxVersion =
        juce::Rectangle<int>(titleCenter - (titleTextWidth / 2) + sw + textMargin, 0, vw, h);
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

        auto reducedRect = Justification(Justification::centred)
                               .appliedToRectangle(juce::Rectangle<int>(getHeight(), getHeight()),
                                                   getLocalBounds())
                               .toFloat()
                               .reduced((float)getHeight() * 0.25f);

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
        // HACK: fake a top line so that we get minimize aligned to the bottom
        shape.addLineSegment({0.0f, 0.0f, 1.0f, 0.0f}, 0.0f);
        shape.addLineSegment({0.0f, 0.9f, 1.0f, 0.9f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("minimize", Colour(255, 212, 32),
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

        return new SurgeJUCELookAndFeel_DocumentWindowButton("maximize", Colour(0xff0A830A), shape,
                                                             fullscreenShape);
    }

    jassertfalse;
    return nullptr;
}

juce::Font SurgeJUCELookAndFeel::getPopupMenuFont()
{
    return skin->fontManager->getLatoAtSize(15);
    // return juce::LookAndFeel_V4::getPopupMenuFont();
}

juce::Font SurgeJUCELookAndFeel::getPopupMenuBoldFont()
{
    // return juce::Font("Comic Sans MS", 15, juce::Font::plain);
    return skin->fontManager->getLatoAtSize(15, juce::Font::bold);
}

// overridden here just to make the shortcut text same size as normal menu entry text
void SurgeJUCELookAndFeel::drawPopupMenuItem(Graphics &g, const juce::Rectangle<int> &area,
                                             const bool isSeparator, const bool isActive,
                                             const bool isHighlighted, const bool isTicked,
                                             const bool hasSubMenu, const String &text,
                                             const String &shortcutKeyText, const Drawable *icon,
                                             const Colour *const textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        r.removeFromTop(roundToInt(((float)r.getHeight() * 0.5f) - 0.5f));

        g.setColour(findColour(PopupMenu::textColourId).withAlpha(0.3f));
        g.fillRect(r.removeFromTop(1));
    }
    else
    {
        auto textColour =
            (textColourToUse == nullptr ? findColour(PopupMenu::textColourId) : *textColourToUse);

        auto r = area.reduced(1);

        if (isHighlighted && isActive)
        {
            g.setColour(findColour(PopupMenu::highlightedBackgroundColourId));
            g.fillRect(r);

            g.setColour(findColour(PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour(textColour.withMultipliedAlpha(isActive ? 1.0f : 0.5f));
        }

        r.reduce(jmin(5, area.getWidth() / 20), 0);

        auto font = getPopupMenuFont();

        auto maxFontHeight = (float)r.getHeight() / 1.3f;

        if (font.getHeight() > maxFontHeight)
            font.setHeight(maxFontHeight);

        g.setFont(font);

        auto iconArea = r.removeFromRight(roundToInt(maxFontHeight)).toFloat();
        auto tickArea = r.removeFromLeft(roundToInt(maxFontHeight)).toFloat();

        if (icon != nullptr)
        {
            icon->drawWithin(g, iconArea.translated(-2, 0),
                             RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize,
                             1.0f);
        }

        if (isTicked)
        {
            auto tick = getTickShape(1.0f);
            g.fillPath(tick, tick.getTransformToScaleToFit(
                                 tickArea.reduced(tickArea.getWidth() / 5, 0).toFloat(), true));
        }

        if (hasSubMenu)
        {
            auto arrowH = 0.6f * getPopupMenuFont().getAscent();

            auto x = static_cast<float>(r.removeFromRight((int)arrowH).getX());
            auto halfH = static_cast<float>(r.getCentreY());

            Path path;
            path.startNewSubPath(x, halfH - arrowH * 0.5f);
            path.lineTo(x + arrowH * 0.6f, halfH);
            path.lineTo(x, halfH + arrowH * 0.5f);

            g.strokePath(path, PathStrokeType(2.0f));
        }

        r.removeFromRight(3);
        g.drawFittedText(text, r, Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setFont(juce::LookAndFeel_V4::getPopupMenuFont().withHeight(13));
            g.drawText(shortcutKeyText, r, Justification::centredRight, true);
        }
    }
}

void SurgeJUCELookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics &g, int width,
                                                              int height,
                                                              const juce::PopupMenu::Options &o)
{
    auto background = findColour(PopupMenu::backgroundColourId);

    g.fillAll(background);

    g.setColour(findColour(PopupMenu::textColourId).withAlpha(0.6f));
    g.drawRect(0, 0, width, height);
}

void SurgeJUCELookAndFeel::updateDarkIfNeeded()
{
    int menuMode = 0;

    if (hasStorage())
    {
        menuMode = Surge::Storage::getUserDefaultValue(storage(), Surge::Storage::MenuLightness, 2);
    }

    if (menuMode == 1)
    {
        auto dm = sst::plugininfra::misc_platform::isDarkMode();
        if ((dm && lastDark == 0) || (!dm && lastDark == 1) || (lastDark < 0))
        {
            onSkinChanged();
        }
    }
}
void SurgeJUCELookAndFeel::drawPopupMenuSectionHeaderWithOptions(Graphics &graphics,
                                                                 const juce::Rectangle<int> &area,
                                                                 const String &sectionName,
                                                                 const PopupMenu::Options &options)
{
    LookAndFeel_V2::drawPopupMenuSectionHeaderWithOptions(graphics, area, sectionName, options);
}

void SurgeJUCELookAndFeel::drawToggleButton(Graphics &g, ToggleButton &button,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    auto tickWidth = jmin(15.0f, (float)button.getHeight() * 0.75f) * 1.2f;

    drawTickBox(g, button, 2.0f, ((float)button.getHeight() - tickWidth) * 0.5f, tickWidth,
                tickWidth, button.getToggleState(), button.isEnabled(),
                shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    g.setColour(button.findColour(ToggleButton::textColourId));
    g.setFont(skin->fontManager->getLatoAtSize(9));

    if (!button.isEnabled())
        g.setOpacity(0.5f);

    g.drawFittedText(
        button.getButtonText(),
        button.getLocalBounds().withTrimmedLeft(roundToInt(tickWidth) + 10).withTrimmedRight(2),
        Justification::centredLeft, 10);
}
