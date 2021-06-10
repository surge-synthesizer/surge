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
#include "SurgeBitmaps.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Overlays
{
MiniEdit::MiniEdit()
{
    typein = std::make_unique<juce::TextEditor>("minieditTypein");
    typein->setJustification(juce::Justification::centred);
    typein->setFont(Surge::GUI::getFontManager()->getLatoAtSize(11));
    typein->setSelectAllWhenFocused(true);
    typein->addListener(this);
    addAndMakeVisible(*typein);

    okButton = std::make_unique<juce::TextButton>("minieditOK");
    okButton->setButtonText("OK");
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<juce::TextButton>("minieditOK");
    cancelButton->setButtonText("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);
}
MiniEdit::~MiniEdit() {}
juce::Rectangle<int> MiniEdit::getDisplayRegion()
{
    auto fullRect = juce::Rectangle<int>(0, 0, 160, 80).withCentre(getBounds().getCentre());
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
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(11));
    g.drawText(title, tbRect, juce::Justification::centred);

    auto d = associatedBitmapStore->getDrawable(IDB_SURGE_ICON);
    if (d)
    {
        d->drawAt(g, fullRect.getX(), fullRect.getY(), 1.0);
    }

    auto bodyRect = fullRect.withTrimmedTop(18);
    g.setColour(skin->getColor(Colors::Dialog::Background));
    g.fillRect(bodyRect);

    g.setColour(skin->getColor(Colors::Dialog::Label::Text));
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
    auto labelRect = bodyRect.withHeight(22).withTrimmedLeft(3).withTrimmedRight(3);
    g.drawText(label, labelRect, juce::Justification::centredLeft);

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(fullRect);
}

void MiniEdit::onSkinChanged()
{
    typein->setColour(juce::TextEditor::backgroundColourId,
                      skin->getColor(Colors::Dialog::Entry::Background));
    typein->setColour(juce::TextEditor::textColourId, skin->getColor(Colors::Dialog::Entry::Text));
    typein->setColour(juce::TextEditor::highlightColourId,
                      skin->getColor(Colors::Dialog::Entry::Focus));
    typein->setColour(juce::TextEditor::outlineColourId,
                      skin->getColor(Colors::Dialog::Entry::Border));
    repaint();
}
void MiniEdit::resized()
{
    auto fullRect = getDisplayRegion();
    auto eh = 18, mg = 2, bw = 40;

    auto typeinBox = fullRect.translated(0, 2 * eh + mg * 2)
                         .withHeight(eh)
                         .withTrimmedLeft(2 * mg)
                         .withTrimmedRight(2 * mg);
    typein->setBounds(typeinBox);
    typein->setIndents(4, (typein->getHeight() - typein->getTextHeight()) / 2);

    auto buttonRow = fullRect.translated(0, 3 * eh + mg * 3)
                         .withHeight(eh)
                         .withTrimmedLeft(mg)
                         .withTrimmedRight(mg);
    auto okRect = buttonRow.withTrimmedLeft(40).withWidth(38);
    auto canRect = buttonRow.withTrimmedLeft(80).withWidth(38);

    okButton->setBounds(okRect);
    cancelButton->setBounds(canRect);

    if (isVisible())
        grabFocus();
}
void MiniEdit::buttonClicked(juce::Button *button)
{
    if (button == okButton.get())
    {
        callback(typein->getText().toStdString());
    }
    setVisible(false);
}
void MiniEdit::visibilityChanged()
{
    if (isVisible())
        grabFocus();
}
void MiniEdit::textEditorReturnKeyPressed(juce::TextEditor &editor)
{
    callback(typein->getText().toStdString());
    setVisible(false);
}
void MiniEdit::textEditorEscapeKeyPressed(juce::TextEditor &editor) { setVisible(false); }
} // namespace Overlays
} // namespace Surge