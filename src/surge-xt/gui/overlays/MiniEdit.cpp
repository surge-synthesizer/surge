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

#include "MiniEdit.h"
#include "SkinModel.h"
#include "SurgeImageStore.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"
#include "widgets/SurgeTextButton.h"
#include "AccessibleHelpers.h"

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
    if (!skin || !associatedBitmapStore)
    {
        // This is a software error obvs
        g.fillAll(juce::Colours::red);
        return;
    }

    g.fillAll(skin->getColor(Colors::Overlay::Background));

    auto fullRect = getDisplayRegion();

    auto tbRect = fullRect.withHeight(18);

    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Background));
    g.fillRect(tbRect);
    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Text));
    g.setFont(skin->fontManager->getLatoAtSize(10, juce::Font::bold));
    g.drawText(title, tbRect, juce::Justification::centred);

    auto d = associatedBitmapStore->getImage(IDB_SURGE_ICON);

    if (d)
    {
        d->drawAt(g, fullRect.getX() + 2, fullRect.getY() + 2, 1.0);
    }

    auto bodyRect = fullRect.withTrimmedTop(18);

    g.setColour(skin->getColor(Colors::Dialog::Background));
    g.fillRect(bodyRect);

    g.setColour(skin->getColor(Colors::Dialog::Label::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9));

    auto labelRect = bodyRect.withHeight(20).reduced(6, 0);

    g.drawText(label, labelRect, juce::Justification::centredLeft);

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(fullRect.expanded(1), 2);
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