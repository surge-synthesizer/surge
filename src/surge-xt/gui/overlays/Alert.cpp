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
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<Surge::Widgets::SurgeTextButton>("alertCancel");
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);
}

Alert::~Alert() {}

void Alert::addToggleButtonAndSetText(const std::string &t)
{
    toggleButton = std::make_unique<juce::ToggleButton>(t);
    addAndMakeVisible(*toggleButton);
}

juce::Rectangle<int> Alert::getDisplayRegion()
{
    if (toggleButton)
    {
        return juce::Rectangle<int>(0, 0, 360, 111).withCentre(getBounds().getCentre());
    }
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
    auto margin = 2, btnHeight = 17, btnWidth = 50, buttonVertTranslate = toggleButton ? 86 : 70;

    auto fullRect = getDisplayRegion();
    auto dialogCenter = fullRect.getWidth() / 2;
    auto buttonRow = fullRect.withHeight(btnHeight).translated(0, buttonVertTranslate);

    if (singleButton)
    {
        auto okRect = buttonRow.withTrimmedLeft(dialogCenter - btnWidth / 2).withWidth(btnWidth);
        okButton->setBounds(okRect);
    }
    else
    {
        auto okRect =
            buttonRow.withTrimmedLeft(dialogCenter - btnWidth - margin).withWidth(btnWidth);
        auto canRect = buttonRow.withTrimmedLeft(dialogCenter + margin).withWidth(btnWidth);

        okButton->setBounds(okRect);
        cancelButton->setBounds(canRect);
    }

    if (toggleButton)
    {
        toggleButton->setBounds(fullRect.withHeight(16).translated(10, 70));
    }
}

void Alert::onSkinChanged()
{
    okButton->setSkin(skin, associatedBitmapStore);
    cancelButton->setSkin(skin, associatedBitmapStore);

    repaint();
}

void Alert::buttonClicked(juce::Button *button)
{
    if (!toggleButton)
    {
        if (button == okButton.get() && onOk)
        {
            onOk();
        }
        else if (button == cancelButton.get() && onCancel)
        {
            onCancel();
        }
    }
    else
    {
        if (button == okButton.get() && onOkForToggleState)
        {
            onOkForToggleState(toggleButton->getToggleState());
        }
        else if (button == cancelButton.get() && onCancelForToggleState)
        {
            onCancelForToggleState(toggleButton->getToggleState());
        }
    }

    setVisible(false);
}
} // namespace Overlays
} // namespace Surge