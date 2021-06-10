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

#include "TypeinParamEditor.h"
#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Overlays
{
TypeinParamEditor::TypeinParamEditor()
{
    textEd = std::make_unique<juce::TextEditor>("typeinParamEditor");
    textEd->addListener(this);
    textEd->setSelectAllWhenFocused(true);
    textEd->setFont(Surge::GUI::getFontManager()->getLatoAtSize(11));
    textEd->setIndents(4, (textEd->getHeight() - textEd->getTextHeight()) / 2);
    textEd->setJustification(juce::Justification::centred);
    addAndMakeVisible(*textEd);
}

void TypeinParamEditor::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::Dialog::Background));
    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(getLocalBounds());

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
    g.setColour(skin->getColor(Colors::Slider::Label::Dark));

    auto r = getLocalBounds().translated(0, 2).withHeight(14);
    g.drawText(mainLabel, r, juce::Justification::centred);

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(8));

    if (isMod)
    {
        r = r.translated(0, 12);
        g.drawText(modbyLabel, r, juce::Justification::centred);
    }

    r = r.translated(0, 12);

    if (wasInputInvalid)
    {
        g.setColour(juce::Colours::red);
        g.drawText("Input out of range!", r, juce::Justification::centred);
    }
    else
    {
        g.drawText(primaryVal, r, juce::Justification::centred);
    }

    g.setColour(skin->getColor(Colors::Slider::Label::Dark));

    if (isMod)
    {
        r = r.translated(0, 12);
        g.drawText(secondaryVal, r, juce::Justification::centred);
    }
}

void TypeinParamEditor::setBoundsToAccompany(const juce::Rectangle<int> &controlRect,
                                             const juce::Rectangle<int> &parentRect)
{
    int ht = 56 + (isMod ? 22 : 0);
    auto r = juce::Rectangle<int>(0, 0, 144, ht)
                 .withX(controlRect.getX())
                 .withY(controlRect.getY() - ht);

    if (!parentRect.contains(r))
    {
        r = r.withY(controlRect.getBottom());
    }

    auto ter = juce::Rectangle<int>(0, ht - 22, 144, 22).reduced(2);
    textEd->setBounds(ter);

    setBounds(r);
}

void TypeinParamEditor::onSkinChanged()
{
    textEd->setColour(juce::TextEditor::backgroundColourId,
                      skin->getColor(Colors::Dialog::Entry::Background));
    textEd->setColour(juce::TextEditor::textColourId, skin->getColor(Colors::Dialog::Entry::Text));
    textEd->setColour(juce::TextEditor::highlightColourId,
                      skin->getColor(Colors::Dialog::Entry::Focus));
    textEd->setColour(juce::TextEditor::outlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));
    textEd->setColour(juce::TextEditor::focusedOutlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));

    repaint();
}
void TypeinParamEditor::textEditorReturnKeyPressed(juce::TextEditor &te)
{
    if (!editor)
        return;

    bool res = false;
    if (typeinMode == Param)
    {
        if (p && ms > 0)
        {
            res = editor->setParameterModulationFromString(p, ms, te.getText().toStdString());
        }
        else
        {
            res = editor->setParameterFromString(p, te.getText().toStdString());
        }
    }
    else
    {
        res = editor->setControlFromString(ms, te.getText().toStdString());
    }
    if (res)
    {
        setVisible(false);
    }
    else
    {
        wasInputInvalid = true;
        repaint();
        juce::Timer::callAfterDelay(2000, [this]() {
            wasInputInvalid = false;
            repaint();
        });
    }
}

void TypeinParamEditor::textEditorEscapeKeyPressed(juce::TextEditor &te) { setVisible(false); }

void TypeinParamEditor::grabFocus() { textEd->grabKeyboardFocus(); }
} // namespace Overlays
} // namespace Surge