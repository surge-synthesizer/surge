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

#include "CoveringMessageOverlay.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Overlays
{
void CoveringMessageOverlay::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.7f));
    auto rT = getLocalBounds().withHeight(40).translated(0, 10);
    auto rM = getLocalBounds().withTrimmedTop(75);

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(30));
    g.setColour(juce::Colours::white);
    g.drawText(pt, rT, juce::Justification::centred);

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(12));
    g.setColour(juce::Colours::white);
    g.drawMultiLineText(et, 20, rM.getY(), getWidth() - 40, juce::Justification::centred);
}
} // namespace Overlays
} // namespace Surge