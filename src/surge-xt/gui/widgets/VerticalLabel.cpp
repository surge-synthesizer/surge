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