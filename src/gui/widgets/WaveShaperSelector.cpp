//
// Created by Paul Walker on 7/10/21.
//

#include "WaveShaperSelector.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
void WaveShaperSelector::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
    g.drawText(wst_ui_names[iValue], getLocalBounds(), juce::Justification::centredTop);
    g.fillRect(0, 12, getWidth(), 1);

    g.setColour(juce::Colours::darkgrey);
    g.fillRect(buttonM);
    g.setColour(juce::Colours::white);
    g.drawText("-", buttonM, juce::Justification::centred);
    g.setColour(juce::Colours::lightgrey);
    g.drawRect(buttonM);

    g.setColour(juce::Colours::darkgrey);
    g.fillRect(buttonP);
    g.setColour(juce::Colours::white);
    g.drawText("+", buttonP, juce::Justification::centred);
    g.setColour(juce::Colours::lightgrey);
    g.drawRect(buttonP);
}

void WaveShaperSelector::resized()
{
    auto b = getLocalBounds().withY(getHeight() - 12).withHeight(12);
    buttonM = b.withTrimmedRight(getWidth() / 2);
    buttonP = b.withTrimmedLeft(getWidth() / 2);
}

void WaveShaperSelector::mouseDown(const juce::MouseEvent &event)
{
    lastDragDistance = 0;
    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
    }

    if (buttonM.toFloat().contains(event.position))
    {
        setValue(nextValueInOrder(value, -1));
        notifyValueChanged();
    }
    if (buttonP.toFloat().contains(event.position))
    {
        setValue(nextValueInOrder(value, +1));
        notifyValueChanged();
    }
}

void WaveShaperSelector::mouseDrag(const juce::MouseEvent &e)
{
    auto d = e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY();
    if (fabs(d - lastDragDistance) > 10)
    {
        int inc = 1;
        if (d - lastDragDistance < 0)
        {
            inc = -1;
        }

        setValue(nextValueInOrder(value, inc));
        notifyValueChanged();
        repaint();
        lastDragDistance = d;
    }
}

void WaveShaperSelector::mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w)
{
    int dir = wheelAccumulationHelper.accumulate(w, false, true);
    if (dir != 0)
    {
        notifyBeginEdit();
        setValue(nextValueInOrder(value, -dir));
        notifyValueChanged();
        notifyEndEdit();
        repaint();
    }
}

float WaveShaperSelector::nextValueInOrder(float v, int inc)
{
    int iv = Parameter::intUnscaledFromFloat(v, n_ws_types - 1, 0);
    if (!intOrdering.empty() && n_ws_types == intOrdering.size())
    {
        int pidx = 0;

        for (int idx = 0; idx < intOrdering.size(); idx++)
        {
            if (intOrdering[idx] == iv)
            {
                pidx = idx;
                break;
            }
        }

        int nidx = pidx + inc;

        if (nidx < 0)
        {
            nidx = intOrdering.size() - 1;
        }
        else if (nidx >= intOrdering.size())
        {
            nidx = 0;
        }

        while (intOrdering[nidx] < 0)
        {
            nidx += inc;

            if (nidx < 0)
            {
                nidx = intOrdering.size() - 1;
            }

            if (nidx >= intOrdering.size())
            {
                nidx = 0;
            }
        }

        iv = intOrdering[nidx];
    }
    else
    {
        iv = iv + inc;

        if (iv < 0)
        {
            iv = n_ws_types - 1;
        }

        if (iv > n_ws_types - 1)
        {
            iv = 0;
        }
    }

    // This is the get_value_f01 code
    float r = Parameter::intScaledToFloat(iv, n_ws_types - 1, 0);

    return r;
}
} // namespace Widgets
} // namespace Surge