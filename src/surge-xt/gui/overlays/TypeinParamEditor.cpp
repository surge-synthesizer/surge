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

#include "TypeinParamEditor.h"
#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Overlays
{
TypeinParamEditor::TypeinParamEditor()
{
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);

    textEd = std::make_unique<juce::TextEditor>("typeinParamEditor");
    textEd->addListener(this);
    textEd->setSelectAllWhenFocused(true);
    textEd->setIndents(4, (textEd->getHeight() - textEd->getTextHeight()) / 2);
    textEd->setJustification(juce::Justification::centred);
    textEd->setTitle("New Value");
    textEd->setDescription("New Value");
    Surge::Widgets::fixupJuceTextEditorAccessibility(*textEd);
    addAndMakeVisible(*textEd);

    textEd->setWantsKeyboardFocus(true);
}

void TypeinParamEditor::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::Dialog::Background));
    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(getLocalBounds());

    g.setFont(skin->fontManager->getLatoAtSize(10));
    g.setColour(skin->getColor(Colors::Dialog::Label::Text));

    auto r =
        getLocalBounds().translated(0, 2).withHeight(14).withTrimmedLeft(2).withTrimmedRight(2);
    g.drawText(mainLabel, r, juce::Justification::centred);

    g.setFont(skin->fontManager->getLatoAtSize(8));

    if (isMod)
    {
        r = r.translated(0, 12);
        g.drawText(modbyLabel, r, juce::Justification::centred);
    }

    r = r.translated(0, 12);

    if (wasInputInvalid)
    {
        g.setColour(skin->getColor(Colors::Dialog::Label::Error));
        g.drawText(errorToDisplay, r, juce::Justification::centred, false);
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
    auto r = juce::Rectangle<int>(0, 0, 180, ht);
    return r;
}

void TypeinParamEditor::setBoundsToAccompany(const juce::Rectangle<int> &controlRect,
                                             const juce::Rectangle<int> &parentRect)
{
    auto r = getRequiredSize();

    auto rw = r.getWidth();
    auto cw = controlRect.getWidth();
    auto dw = (rw - cw) / 2;
    r = r.withX(controlRect.getX() - dw).withY(controlRect.getY() - r.getHeight());

    if (!parentRect.contains(r))
    {
        int margin = 1;

        if (r.getX() < 0)
        {
            r = r.withX(margin);
        }

        if (r.getY() < 0)
        {
            r = r.withY(controlRect.getBottom());
        }

        if (r.getRight() > parentRect.getWidth())
        {
            r = r.withRightX(parentRect.getWidth() - margin);
        }

        if (r.getBottom() > parentRect.getHeight())
        {
            r = r.withBottom(controlRect.getY());
        }
    }

    setBounds(r);
}

void TypeinParamEditor::resized()
{
    auto ter = juce::Rectangle<int>(0, getHeight() - 24, getWidth(), 22).reduced(4, 2);
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

    if (!isVisible())
    {
        doReturnFocus();
    }
}

void TypeinParamEditor::onSkinChanged()
{
    textEd->setFont(skin->fontManager->getLatoAtSize(11));

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

bool TypeinParamEditor::handleTypein(const std::string &value, std::string &errMsg)
{
    if (!editor)
        return false;

    bool res = false;

    if (typeinMode == Param)
    {
        if (p && ms > 0)
        {
            res = editor->setParameterModulationFromString(p, ms, modScene, modidx, value, errMsg);

            bool focusModEditor = Surge::Storage::getUserDefaultValue(
                &(editor->synth->storage), Surge::Storage::FocusModEditorAfterAddModulationFrom,
                false);

            if (activateModsourceAfterEnter > -1 && focusModEditor)
            {
                editor->setModsourceSelected(ms, modidx);
                activateModsourceAfterEnter = -1;
            }
        }
        else
        {
            res = editor->setParameterFromString(p, value, errMsg);
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
    auto res = handleTypein(te.getText().toStdString(), errorToDisplay);

    if (res)
    {
        doReturnFocus();
        setVisible(false);
    }
    else
    {
        wasInputInvalid = true;
        repaint();
        juce::Timer::callAfterDelay(5000, [that = juce::Component::SafePointer(this)]() {
            if (that)
            {
                that->wasInputInvalid = false;
                that->repaint();
            }
        });
    }
}

void TypeinParamEditor::textEditorEscapeKeyPressed(juce::TextEditor &te)
{
    activateModsourceAfterEnter = -1;
    doReturnFocus();
    setVisible(false);
}

void TypeinParamEditor::grabFocus() { textEd->grabKeyboardFocus(); }

void TypeinParamEditor::doReturnFocus()
{
    if (returnFocusComp)
    {
        returnFocusComp->grabKeyboardFocus();
    }
    returnFocusComp = nullptr;
}
} // namespace Overlays
} // namespace Surge