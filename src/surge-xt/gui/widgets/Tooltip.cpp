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

#include "Tooltip.h"

#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Widgets
{

void Tooltip::paint(juce::Graphics &g)
{
    namespace clr = Colors::PatchBrowser::CommentTooltip;
    g.fillAll(skin->getColor(clr::Border));
    g.setColour(skin->getColor(clr::Background));
    g.fillRect(getLocalBounds().reduced(1));
    g.setColour(skin->getColor(clr::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9));
    g.drawMultiLineText(text, 4, g.getCurrentFont().getHeight() + 2, getWidth(), alignment);
}

void Tooltip::positionForText(const juce::Point<int> &centerPoint, const std::string &s,
                              const int maxTooltipWidth, const juce::Justification customAlignment)
{
    text = s;
    alignment = customAlignment;

    std::stringstream ss(text);
    std::string to;

    int numLines = 0;

    auto ft = skin->fontManager->getLatoAtSize(9);
    auto width = 0.f;

    while (std::getline(ss, to, '\n'))
    {
        auto w = SST_STRING_WIDTH_FLOAT(ft, to);

        // in case of an empty line, we still need to count it as an extra row
        // so bump it up a bit so that the rows calculation ceils to 1
        if (w == 0.f)
        {
            w = 1.f;
        }

        if (maxTooltipWidth > 0)
        {
            auto rows = std::ceil(w / (float)maxTooltipWidth);
            numLines += (int)rows;
        }
        else
        {
            numLines++;
        }

        width = std::max(w, width);
    }

    const auto effectiveWidth =
        maxTooltipWidth > 0 ? std::min(width, (float)maxTooltipWidth) : width;
    const auto height = std::max((numLines * (ft.getHeight() + 2)) + 2, 16.f);
    const auto margin = 10;

    auto r = juce::Rectangle<int>()
                 .withCentre(juce::Point(centerPoint.x, centerPoint.y))
                 .withSizeKeepingCentre(effectiveWidth + margin, height)
                 .translated(0, height / 2);

    setBounds(r);
    repaint();
}

} // namespace Widgets
} // namespace Surge