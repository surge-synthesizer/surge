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

#include "MiniEdit.h"
#include "SkinModel.h"
#include "SurgeImageStore.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"
#include "widgets/SurgeTextButton.h"
#include "AccessibleHelpers.h"
#include "OverlayUtils.h"

namespace Surge
{
namespace Overlays
{
MiniEdit::MiniEdit()
{
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setAccessible(true);
    typein = std::make_unique<juce::TextEditor>("minieditTypein");
    typein->setJustification(juce::Justification::centred);
    typein->setSelectAllWhenFocused(true);
    typein->setWantsKeyboardFocus(true);
    typein->addListener(this);
    typein->setTitle("Value");
    Surge::Widgets::fixupJuceTextEditorAccessibility(*typein);

    addAndMakeVisible(*typein);

    okButton = std::make_unique<Surge::Widgets::SurgeTextButton>("minieditOK");
    okButton->setButtonText("OK");
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<Surge::Widgets::SurgeTextButton>("minieditOK");
    cancelButton->setButtonText("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);
}

MiniEdit::~MiniEdit() {}

juce::Rectangle<int> MiniEdit::getDisplayRegion()
{
    auto fullRect = juce::Rectangle<int>(0, 0, 180, 90).withCentre(getBounds().getCentre());
    return fullRect;
}

void MiniEdit::paint(juce::Graphics &g)
{
    auto fullRect = getDisplayRegion();
    auto labelRect = fullRect.withTrimmedTop(18).withHeight(20).reduced(6, 0);

    paintOverlayWindow(g, skin, associatedBitmapStore, fullRect, title);

    g.setColour(skin->getColor(Colors::Dialog::Label::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9));
    g.drawText(label, labelRect, juce::Justification::centredLeft);
}

void MiniEdit::onSkinChanged()
{
    typein->setFont(skin->fontManager->getLatoAtSize(11));

    typein->setColour(juce::TextEditor::backgroundColourId,
                      skin->getColor(Colors::Dialog::Entry::Background));
    typein->setColour(juce::TextEditor::textColourId, skin->getColor(Colors::Dialog::Entry::Text));
    typein->setColour(juce::TextEditor::highlightedTextColourId,
                      skin->getColor(Colors::Dialog::Entry::Text));
    typein->setColour(juce::TextEditor::highlightColourId,
                      skin->getColor(Colors::Dialog::Entry::Focus));
    typein->setColour(juce::TextEditor::outlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));
    typein->setColour(juce::TextEditor::focusedOutlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));

    okButton->setSkin(skin, associatedBitmapStore);
    cancelButton->setSkin(skin, associatedBitmapStore);

    repaint();
}

void MiniEdit::resized()
{
    auto typeinHeight = 18, margin = 2, btnHeight = 17, btnWidth = 50;

    auto fullRect = getDisplayRegion();
    auto dialogCenter = fullRect.getWidth() / 2;
    auto typeinBox = fullRect.translated(0, 2 * typeinHeight + margin * 2)
                         .withHeight(typeinHeight)
                         .withTrimmedLeft(3 * margin)
                         .withTrimmedRight(3 * margin);

    typein->setBounds(typeinBox);
    typein->setIndents(4, (typein->getHeight() - typein->getTextHeight()) / 2);

    auto buttonRow =
        fullRect.translated(0, fullRect.getBottom() - typeinBox.getBottom() + typeinHeight + 15)
            .withHeight(btnHeight)
            .withTrimmedLeft(margin)
            .withTrimmedRight(margin);
    auto okRect = buttonRow.withTrimmedLeft(dialogCenter - btnWidth - margin).withWidth(btnWidth);
    auto canRect = buttonRow.withTrimmedLeft(dialogCenter + margin).withWidth(btnWidth);

    okButton->setBounds(okRect);
    cancelButton->setBounds(canRect);

    if (isVisible())
    {
        grabFocus();
    }
}

void MiniEdit::buttonClicked(juce::Button *button)
{
    if (button == okButton.get())
    {
        callback(typein->getText().toStdString());
    }

    doReturnFocus();
    setVisible(false);
}

void MiniEdit::visibilityChanged()
{
    if (isVisible())
    {
        grabFocus();
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

void MiniEdit::textEditorReturnKeyPressed(juce::TextEditor &editor)
{
    callback(typein->getText().toStdString());
    doReturnFocus();
    setVisible(false);
}

void MiniEdit::textEditorEscapeKeyPressed(juce::TextEditor &editor)
{
    doReturnFocus();
    setVisible(false);
}

void MiniEdit::doReturnFocus()
{
    if (returnFocusComp)
    {
        returnFocusComp->grabKeyboardFocus();
    }
    returnFocusComp = nullptr;
}
} // namespace Overlays
} // namespace Surge