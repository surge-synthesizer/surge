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

#include "VerticalLabel.h"

namespace Surge
{
namespace Widgets
{

VerticalLabel::VerticalLabel()
{
    setRepaintsOnMouseActivity(true);
    setAccessible(false); // it's just derocative
    setInterceptsMouseClicks(false, false);
}
VerticalLabel::~VerticalLabel() = default;

void VerticalLabel::paint(juce::Graphics &g)
{
    // g.fillAll(juce::Colours::orangered);
    auto charHeight = font.getHeight() * 0.95;
    float totalHeight = 0;

    for (auto &c : text)
    {
        if (c == '-')
        {
        }
        else if (c == ' ')
        {
            totalHeight += charHeight * 0.5;
        }
        else
        {
            totalHeight += charHeight;
        }
    }

    auto mx = getWidth() / 2.0;
    auto ty = (getHeight() - totalHeight) / 2.0;
    auto currY = ty;

    g.setFont(font);
    g.setColour(fontColour);

    for (auto &c : text)
    {
        if (c == '-')
        {
            continue;
        }

        auto h = charHeight;

        if (c == ' ')
        {
            h = charHeight * 0.5;
        }

        char str[2];
        str[0] = c;
        str[1] = 0;

        g.drawText(str, mx - 10, currY, 20, charHeight * 0.5, juce::Justification::centred);

        currY += h;
    }
}

} // namespace Widgets
} // namespace Surge