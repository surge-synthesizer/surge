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

#include "OverlayUtils.h"
#include "widgets/SurgeTextButton.h"
#include "SurgeImageStore.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"

namespace Surge
{
namespace Overlays
{

void paintOverlayWindow(juce::Graphics &g, std::shared_ptr<GUI::Skin> skin,
                        std::shared_ptr<SurgeImageStore> associatedBitmapStore,
                        juce::Rectangle<int> displayRegion, std::string title)
{

    if (!skin || !associatedBitmapStore)
    {
        g.fillAll(juce::Colours::red);
        return;
    }

    g.fillAll(skin->getColor(Colors::Overlay::Background));
    auto tbRect = displayRegion.withHeight(18);

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
        icon->drawAt(g, displayRegion.getRight() - iconSize + 2, displayRegion.getY() + 1, 1);
#else
        icon->drawAt(g, displayRegion.getX() + 2, displayRegion.getY() + 1, 1);
#endif
    }

    auto bodyRect = displayRegion.withTrimmedTop(18);

    g.setColour(skin->getColor(Colors::Dialog::Background));
    g.fillRect(bodyRect);

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(displayRegion.expanded(1), 2);
}

} // namespace Overlays
} // namespace Surge