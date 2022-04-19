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

#include "SurgeTextButton.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
void SurgeTextButton::paint(juce::Graphics &g)
{
    if (!skin)
    {
        g.fillAll(juce::Colours::orchid);
        return;
    }
    namespace clr = Colors::Dialog::Button;

    // these are the classic skin colors just for now
    auto b = getLocalBounds().toFloat().reduced(0.5, 0.5);
    auto corner = 2.f, cornerIn = 1.5f;
    g.setColour(skin->getColor(clr::Background));
    g.fillRoundedRectangle(b.toFloat(), corner);
    g.setColour(skin->getColor(clr::Border));
    g.drawRoundedRectangle(b.toFloat(), corner, 1);

    bool solo = true;

    auto fc = getLocalBounds().toFloat().reduced(1.5, 1.5);

    auto isHo = isOver() || hasKeyboardFocus(false);
    auto isEn = isEnabled();
    auto isPr = isDown();
    auto isFo = hasKeyboardFocus(false);

    auto fg = skin->getColor(clr::Text);

    if (!isEn)
    {
        // fg = juce::Colour(skin->getColor(clr::DeactivatedText));
    }
    else if (isPr)
    {
        g.setColour(skin->getColor(clr::BackgroundPressed));
        g.fillRoundedRectangle(fc.toFloat(), cornerIn);

        g.setColour(skin->getColor(clr::BorderPressed));
        g.drawRoundedRectangle(fc.toFloat().reduced(0.5), cornerIn, 1);

        fg = skin->getColor(clr::TextPressed);
    }
    else if (isHo)
    {
        g.setColour(skin->getColor(clr::BackgroundHover));
        g.fillRoundedRectangle(fc.toFloat(), cornerIn);

        g.setColour(skin->getColor(clr::BorderHover));
        g.drawRoundedRectangle(fc.toFloat().reduced(0.5), cornerIn, 1);

        fg = skin->getColor(clr::TextHover);
    }

    g.setColour(fg);
    g.setFont(skin->fontManager->getLatoAtSize(8, juce::Font::bold));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
}

} // namespace Widgets
} // namespace Surge