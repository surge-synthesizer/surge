/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#include "Alert.h"
#include "widgets/SurgeTextButton.h"
#include "SurgeImageStore.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "OverlayUtils.h"

namespace Surge
{
namespace Overlays
{
Alert::Alert()
{

    okButton = std::make_unique<Surge::Widgets::SurgeTextButton>("alertOk");
    okButton->setButtonText("OK");
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<Surge::Widgets::SurgeTextButton>("alertCancel");
    cancelButton->setButtonText("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);
}

Alert::~Alert() {}

juce::Rectangle<int> Alert::getDisplayRegion()
{
    return juce::Rectangle<int>(0, 0, 360, 95).withCentre(getBounds().getCentre());
}

void Alert::paint(juce::Graphics &g)
{
    auto fullRect = getDisplayRegion();
    paintOverlayWindow(g, skin, associatedBitmapStore, fullRect, title);
    g.setColour(skin->getColor(Colors::Dialog::Label::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9));
    g.drawFittedText(label, fullRect.withTrimmedTop(18).reduced(6), juce::Justification::centredTop,
                     4);
}

void Alert::resized()
{
    auto margin = 2, btnHeight = 17, btnWidth = 50;

    auto fullRect = getDisplayRegion();
    auto dialogCenter = fullRect.getWidth() / 2;

    auto buttonRow = fullRect.withHeight(btnHeight).translated(0, 70);
    auto okRect = buttonRow.withTrimmedLeft(dialogCenter - btnWidth - margin).withWidth(btnWidth);
    auto canRect = buttonRow.withTrimmedLeft(dialogCenter + margin).withWidth(btnWidth);

    okButton->setBounds(okRect);
    cancelButton->setBounds(canRect);
}

void Alert::onSkinChanged()
{
    okButton->setSkin(skin, associatedBitmapStore);
    cancelButton->setSkin(skin, associatedBitmapStore);

    repaint();
}

void Alert::buttonClicked(juce::Button *button)
{
    if (button == okButton.get())
    {
        onOk();
    }
    setVisible(false);
}
} // namespace Overlays
} // namespace Surge