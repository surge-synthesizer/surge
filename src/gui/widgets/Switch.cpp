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

#include "Switch.h"

namespace Surge
{
namespace Widgets
{

Switch::Switch() { setRepaintsOnMouseActivity(true); }
Switch::~Switch() = default;

void Switch::valueChanged() { jassert(false); }

void Switch::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    auto y = value > 0.5 ? -getLocalBounds().getHeight() : 0;
    if (isMultiIntegerValued())
        y = -getIntegerValue() * getLocalBounds().getHeight();
    auto t = juce::AffineTransform().translated(0, y);
    g.reduceClipRegion(getLocalBounds());
    if (isHovered && hoverSwitchD)
    {
        hoverSwitchD->draw(g, 1.0, t);
    }
    else
    {
        switchD->draw(g, 1.0, t);
    }
}

void Switch::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    if (isMultiIntegerValued())
    {
        if (event.mods.isShiftDown())
        {
            setValueDirection(-1);
        }
        else
        {
            setValueDirection(1);
        }
        notifyValueChanged();
    }
    else
    {
        if (!getUnValueClickable())
        {
            if (value > 0.5)
                value = 0;
            else
                value = 1;
            notifyValueChanged();
        }
    }
}

void Switch::mouseEnter(const juce::MouseEvent &event) { isHovered = true; }
void Switch::mouseExit(const juce::MouseEvent &event) { isHovered = false; }
void Switch::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    auto d = wheel.deltaY;
    accumWheel += d;
    // This is callibrated to be reasonable on a mac but I'm still not sure of the units
    //  On my MBP I get deltas which are 0.0019 all the time.
    float accumLimit = 0.08; // anticipate we will split this by OS

    if (accumWheel > accumLimit || accumWheel < -accumLimit)
    {
        int mul = 1;
        if (accumWheel > 0)
        {
            mul = -1;
        }

        if (wheel.isReversed)
        {
            mul = mul * -1;
        }
        if (isMultiIntegerValued())
        {
            setValueDirection(mul);
            notifyValueChanged();
        }
        else
        {
            auto ov = value;
            value = mul > 0 ? 1 : 0;
            if (ov != value)
                notifyValueChanged();
        }

        accumWheel = 0;
    }
}

} // namespace Widgets
} // namespace Surge