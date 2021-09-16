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
