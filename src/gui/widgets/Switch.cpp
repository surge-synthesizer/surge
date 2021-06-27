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

void Switch::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    auto y = value > 0.5 ? -getLocalBounds().getHeight() : 0;

    if (isMultiIntegerValued())
    {
        y = -getIntegerValue() * getLocalBounds().getHeight();
    }

    auto t = juce::AffineTransform().translated(0, y);
    g.reduceClipRegion(getLocalBounds());

    switchD->draw(g, 1.0, t);

    if (isHovered && hoverSwitchD)
    {
        hoverSwitchD->draw(g, 1.0, t);
    }
}

void Switch::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isPopupMenu())
    {
        isHovered = false;
        repaint();
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
            value = (value > 0.5) ? 0 : 1;

            notifyValueChanged();
        }
    }
}

void Switch::mouseEnter(const juce::MouseEvent &event) { isHovered = true; }

void Switch::mouseExit(const juce::MouseEvent &event) { isHovered = false; }

void Switch::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    int mul = wheelHelper.accumulate(wheel);

    if (mul != 0)
    {
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
            {
                notifyValueChanged();
            }
        }
    }
}

} // namespace Widgets
} // namespace Surge