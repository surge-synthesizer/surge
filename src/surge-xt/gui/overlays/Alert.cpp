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
    return juce::Rectangle<int>(0, 0, 360, 90).withCentre(getBounds().getCentre());
}

void Alert::paint(juce::Graphics &g)
{
    if (!skin || !associatedBitmapStore)
    {
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

    auto icon = associatedBitmapStore->getImage(IDB_SURGE_ICON);

    if (icon)
    {
        const auto iconSize = 14;
#if MAC
        icon->drawAt(g, fullRect.getRight() - iconSize + 2, fullRect.getY() + 1, 1);
#else
        icon->drawAt(g, fullRect.getX() + 2, fullRect.getY() + 1, 1);
#endif
    }

    auto bodyRect = fullRect.withTrimmedTop(18);

    g.setColour(skin->getColor(Colors::Dialog::Background));
    g.fillRect(bodyRect);
    g.setColour(skin->getColor(Colors::Dialog::Label::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9));
    g.drawFittedText(label, bodyRect.withTrimmedLeft(2).withTrimmedRight(2),
                     juce::Justification::centredTop, 4);

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(fullRect.expanded(1), 2);

    auto buttonRowRect = fullRect.withHeight(17).translated(0, 72);

    okButton->setBounds(buttonRowRect.withWidth(50).translated(80, 0));
    cancelButton->setBounds(buttonRowRect.withWidth(50).translated(230, 0));
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