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

#include "VuMeter.h"
#include "SurgeBitmaps.h"
#include "basic_dsp.h"

namespace Surge
{
namespace Widgets
{
VuMeter::VuMeter() = default;
VuMeter::~VuMeter() = default;

void VuMeter::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    g.reduceClipRegion(getLocalBounds());

    juce::Colour bgCol = skin->getColor(Colors::VuMeter::Background);

    g.setColour(bgCol);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);

    g.setColour(skin->getColor(Colors::VuMeter::Border));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3, 1);

    int offi;
    bool stereo = false;
    switch (vu_type)
    {
    case vut_vu:
        offi = 0;
        break;
    case vut_vu_stereo:
        stereo = true;
        offi = 1;
        break;
    case vut_gain_reduction:
        offi = 2;
        break;
    default:
        offi = 0;
        break;
    }
    auto t = juce::AffineTransform().translated(0, -offi * getHeight());

    if (hVuBars)
        hVuBars->draw(g, 1.0, t);

    // And now the calculation
    float w = getWidth();
    /*
     * Where does this constant come from?
     * Well in 'scale' we map x -> x^3 to compress the range
     * cuberoot(2) = 1.26 and change
     * 1 / cuberoot(2) = 0.7937
     * so this scales it so the zerodb point is 1/cuberoot(2) along
     * still not entirely sure why, but there you go
     */
    float zerodb = (0.7937 * w);
    auto scale = [](float x) {
        x = limit_range(0.5f * x, 0.f, 1.f);
        return powf(x, 0.3333333333f);
    };

    if (vu_type == vut_gain_reduction)
    {
        float occludeTo = std::min((scale(vL) * (w - 1)) - zerodb, 0.f); // Strictly negative
        auto dG = getLocalBounds().reduced(1, 1).withTrimmedRight(-occludeTo);
        g.setColour(bgCol);
        g.fillRect(dG);
    }
    else
    {
        auto dL = getLocalBounds().reduced(1, 1);
        auto dR = getLocalBounds().reduced(1, 1);

        auto occludeFromL = scale(vL) * w;
        auto occludeFromR = scale(vR) * w;

        if (stereo)
        {
            dL = dL.withBottom(6);
            dR = dR.withTop(6);
        }

        dL = dL.withTrimmedLeft(occludeFromL);
        dR = dR.withTrimmedLeft(occludeFromR);
        g.setColour(bgCol);
        g.fillRect(dL);
        if (stereo)
            g.fillRect(dR);
    }
}

void VuMeter::onSkinChanged()
{
    hVuBars = associatedBitmapStore->getDrawable(IDB_VUMETER_BARS);
    jassert(hVuBars);
}
} // namespace Widgets
} // namespace Surge