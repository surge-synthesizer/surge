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
    textEd->setWantsKeyboardFocus(true);
}

void TypeinParamEditor::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::Dialog::Background));
    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(getLocalBounds());

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
    g.setColour(skin->getColor(Colors::Dialog::Label::Text));

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
        g.setColour(skin->getColor(Colors::Dialog::Label::Error));
        g.drawText("Input out of range!", r, juce::Justification::centred);
    }
    else
    {
        g.drawText(primaryVal, r, juce::Justification::centred);
    }

    g.setColour(skin->getColor(Colors::Dialog::Label::Text));

    if (isMod)
    {
        r = r.translated(0, 12);
        g.drawText(secondaryVal, r, juce::Justification::centred);
    }
}

juce::Rectangle<int> TypeinParamEditor::getRequiredSize()
{
    int ht = 50 + (isMod ? 24 : 0);
    auto r = juce::Rectangle<int>(0, 0, 144, ht);
    return r;
}

void TypeinParamEditor::setBoundsToAccompany(const juce::Rectangle<int> &controlRect,
                                             const juce::Rectangle<int> &parentRect)
{
    auto r = getRequiredSize();
    r = r.withX(controlRect.getX()).withY(controlRect.getY() - r.getHeight());

    if (!parentRect.contains(r))
    {
        r = r.withY(controlRect.getBottom());
    }

    setBounds(r);
}

void TypeinParamEditor::resized()
{
    auto ter = juce::Rectangle<int>(0, getHeight() - 24, 144, 22).reduced(4, 2);
    textEd->setBounds(ter);
}

void TypeinParamEditor::visibilityChanged()
{
    if (isVisible())
    {
        textEd->setWantsKeyboardFocus(true);
    }

    if (editor)
    {
        if (isVisible())
        {
            editor->vkbForward++;
        }
        else
        {
            editor->vkbForward--;
        }
    }
}

void TypeinParamEditor::onSkinChanged()
{
    textEd->setColour(juce::TextEditor::backgroundColourId,
                      skin->getColor(Colors::Dialog::Entry::Background));
    textEd->setColour(juce::TextEditor::textColourId, skin->getColor(Colors::Dialog::Entry::Text));
    textEd->setColour(juce::TextEditor::highlightedTextColourId,
                      skin->getColor(Colors::Dialog::Entry::Text));
    textEd->setColour(juce::TextEditor::highlightColourId,
                      skin->getColor(Colors::Dialog::Entry::Focus));
    textEd->setColour(juce::TextEditor::outlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));
    textEd->setColour(juce::TextEditor::focusedOutlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));

    repaint();
}

bool TypeinParamEditor::handleTypein(const std::string &value)
{
    if (!editor)
        return false;

    bool res = false;
    if (typeinMode == Param)
    {
        if (p && ms > 0)
        {
            res = editor->setParameterModulationFromString(p, ms, modScene, modidx, value);
        }
        else
        {
            res = editor->setParameterFromString(p, value);
        }
    }
    else
    {
        res = editor->setControlFromString(ms, value);
    }
    return res;
}

void TypeinParamEditor::textEditorReturnKeyPressed(juce::TextEditor &te)
{
    auto res = handleTypein(te.getText().toStdString());

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