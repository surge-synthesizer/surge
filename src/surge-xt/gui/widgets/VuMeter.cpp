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

#include "VuMeter.h"
#include "SurgeImageStore.h"
#include "basic_dsp.h"
#include "SurgeImage.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
VuMeter::VuMeter() : juce::Component(), WidgetBaseMixin<VuMeter>(this)
{
    setAccessible(false);
    setWantsKeyboardFocus(false);
}

VuMeter::~VuMeter() = default;

void VuMeter::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    mouseDownLongHold(event);
}

void VuMeter::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    g.reduceClipRegion(getLocalBounds());

    juce::Colour bgCol = skin->getColor(Colors::VuMeter::Background);

    g.setColour(bgCol);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);

    g.setColour(skin->getColor(Colors::VuMeter::Border));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3, 1);

    if (!isAudioActive)
    {
        g.setColour(skin->getColor(Colors::VuMeter::UnavailableText));
        g.setFont(skin->fontManager->getLatoAtSize(8));
        g.drawText("Audio Output Unavailable", getLocalBounds().withTrimmedBottom(1),
                   juce::Justification::centred);
        return;
    }

    int offi;
    bool stereo = false;

    switch (vu_type)
    {
    case ParamConfig::vut_vu:
        offi = 0;
        break;
    case ParamConfig::vut_vu_stereo:
        stereo = true;
        offi = 1;
        break;
    case ParamConfig::vut_gain_reduction:
        offi = 2;
        break;
    default:
        offi = 0;
        break;
    }

    auto t = juce::AffineTransform().translated(0, -offi * getHeight());

    if (hVuBars)
    {
        hVuBars->draw(g, 1.0, t);
    }

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

    if (vu_type == ParamConfig::vut_gain_reduction)
    {
        float occludeTo = std::min((scale(vL) * (w - 1)) - zerodb, 0.f); // Strictly negative
        auto dG = getLocalBounds().reduced(1, 1).withTrimmedRight(-occludeTo);

        g.setColour(bgCol);
        g.fillRect(dG);
    }
    else
    {
        auto dL = getLocalBounds().reduced(2, 2);
        auto dR = getLocalBounds().reduced(2, 2);

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
        {
            g.fillRect(dR);
        }
    }

    if (storage)
    {
        bool showCPU =
            Surge::Storage::getUserDefaultValue(storage, Surge::Storage::ShowCPUUsage, false);

        if (showCPU)
        {
            if (cpuLevel < 0.33)
            {
                g.setColour(juce::Colour(juce::Colours::white));
            }
            else if (cpuLevel < 0.66)
            {
                g.setColour(juce::Colour(juce::Colours::yellow));
            }
            else if (cpuLevel < 0.95)
            {
                g.setColour(juce::Colour(juce::Colours::orange));
            }
            else
            {
                g.setColour(juce::Colour(juce::Colours::red));
            }

            std::string text = std::to_string((int)(std::min(cpuLevel, 1.f) * 100.f));
            auto bounds = getLocalBounds().withTrimmedRight(3);

            g.setFont(skin->fontManager->getLatoAtSize(9));
            g.drawText(text, bounds, juce::Justification::right);
        }
    }
}

void VuMeter::onSkinChanged()
{
    hVuBars = associatedBitmapStore->getImage(IDB_VUMETER_BARS);
    jassert(hVuBars);
}
} // namespace Widgets
} // namespace Surge