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