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

// here and only here we using namespace juce so I can copy and override stuff from v4 easily
using namespace juce;

void SurgeJUCELookAndFeel::onSkinChanged()
{
    setColour(tempoTypeinTextId, juce::Colours::black);
    setColour(tempoTypeinHighlightId, juce::Colours::red);
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
    auto fillColor = skin->getColor(Colors::JuceWidgets::TabbedBar::InactiveButtonBackground);
    auto textColor = skin->getColor(Colors::JuceWidgets::TabbedBar::Text);
    if (button.getToggleState())
    {
        fillColor = skin->getColor(Colors::JuceWidgets::TabbedBar::ActiveButtonBackground);
    }
    if (isMouseOver || isMouseDown)
    {
        textColor = skin->getColor(Colors::JuceWidgets::TabbedBar::HoverText);
    }

    g.setColour(skin->getColor(Colors::JuceWidgets::TabbedBar::TabOutline));
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
    // You will have a reference to a skin here so can do skin->getColour and skin->getFont with no
    // problem. This color is a dumb one to pick of course but shows that it works.
    g.fillAll(skin->getColor(Colors::MSEGEditor::GradientFill::StartColor));

    g.setColour(juce::Colours::white);

    auto surgeLabel = "Surge XT";
    auto surgeVersion = Surge::Build::FullVersionStr;
    auto fontSurge = Surge::GUI::getFontManager()->getLatoAtSize(13);
    auto fontVersion = Surge::GUI::getFontManager()->getFiraMonoAtSize(13);

    // we will probably need these in final even though we don't use them here
    auto sw = fontSurge.getStringWidth("Surge XT");
    auto vw = fontVersion.getStringWidth(surgeVersion);

    auto ic = associatedBitmapStore->getImage(IDB_SURGE_ICON);

    // Surge Icon is 12 x 14 so draw that in the center
    auto ix = w / 2 - 6;
    auto iy = h / 2 - 7;

    if (ic)
    {
        ic->drawAt(g, ix, iy, 1.0);
    }

    auto boxSurge = juce::Rectangle<int>(0, 0, ix - 4, h);
    g.setFont(fontSurge);
    g.drawText(surgeLabel, boxSurge, juce::Justification::centredRight);
    auto boxVersion = juce::Rectangle<int>(ix + 12 + 4, 0, ix - 12 - 4, h);
    g.setFont(fontVersion);
    g.drawText(surgeVersion, boxVersion, juce::Justification::centredLeft);
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
            g.setColour(juce::Colours::yellow);
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
juce::Button *SurgeJUCELookAndFeel::createDocumentWindowButton(int buttonType)
{
    Path shape;
    auto crossThickness = 0.15f;

    if (buttonType == DocumentWindow::closeButton)
    {
        shape.addLineSegment({0.0f, 0.0f, 1.0f, 1.0f}, crossThickness);
        shape.addLineSegment({1.0f, 0.0f, 0.0f, 1.0f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("close", Colour(0xff9A131D), shape,
                                                             shape);
    }

    if (buttonType == DocumentWindow::minimiseButton)
    {
        shape.addLineSegment({0.0f, 0.5f, 1.0f, 0.5f}, crossThickness);

        return new SurgeJUCELookAndFeel_DocumentWindowButton("minimise", Colour(0xffaa8811), shape,
                                                             shape);
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
