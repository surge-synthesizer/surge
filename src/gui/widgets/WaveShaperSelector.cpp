//
// Created by Paul Walker on 7/10/21.
//

#include "WaveShaperSelector.h"
#include "RuntimeFont.h"
#include "QuadFilterUnit.h"

namespace Surge
{
namespace Widgets
{
std::array<std::vector<std::pair<float, float>>, n_ws_types> WaveShaperSelector::wsCurves;

void WaveShaperSelector::paint(juce::Graphics &g)
{
    if (wsCurves[iValue].empty())
    {
        /*
         * The waveshapers are re-entrant as long as they have a unique state pointer
         */
        auto drive = _mm_set1_ps(1.f);
        float xs alignas(16)[4], vals alignas(16)[4];
        auto wsop = GetQFPtrWaveshaper(iValue);
        QuadFilterWaveshaperState s;
        float dx = 0.05;
        for (float x = -2; x <= 2; x += 4 * dx)
        {
            if (wsop)
            {
                for (int i = 0; i < n_waveshaper_registers; ++i)
                    s.R[i] = _mm_setzero_ps();

                for (int i = 0; i < 4; ++i)
                {
                    vals[i] = x + i * dx;
                }
                auto in = _mm_load_ps(vals);
                auto r = wsop(&s, in, drive);
                _mm_store_ps(vals, r);
                for (int i = 0; i < 4; ++i)
                {
                    wsCurves[iValue].emplace_back(x + i * dx, vals[i]);
                }
            }
            else
            {
                for (int i = 0; i < 4; ++i)
                {
                    wsCurves[iValue].emplace_back(x + i * dx, 0.f);
                }
            }
        }
    }
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

    // So the wave is in -2,2 in x and -1,1 in y
    juce::Path curvePath;
    bool f = true;
    for (const auto &el : wsCurves[iValue])
    {
        if (f)
        {
            curvePath.startNewSubPath(el.first, el.second);
        }
        else
        {
            curvePath.lineTo(el.first, el.second);
        }
        f = false;
    }
    // Now what's the transform? Well
    auto xf = juce::AffineTransform()
                  .translated(2, -1.2)
                  .scaled(0.25, -0.4)
                  .scaled(waveArea.getWidth(), waveArea.getHeight())
                  .translated(waveArea.getX(), waveArea.getY());
    {
        juce::Graphics::ScopedSaveState gs(g);

        g.reduceClipRegion(waveArea);
        g.setColour(juce::Colours::white);
        g.strokePath(curvePath, juce::PathStrokeType{1.5}, xf);
    }
}

void WaveShaperSelector::resized()
{
    auto b = getLocalBounds().withY(getHeight() - 12).withHeight(12);
    buttonM = b.withTrimmedRight(getWidth() / 2);
    buttonP = b.withTrimmedLeft(getWidth() / 2);
    waveArea = getLocalBounds().withTrimmedTop(12).withTrimmedBottom(12);
}

void WaveShaperSelector::mouseDown(const juce::MouseEvent &event)
{
    lastDragDistance = 0;
    everDragged = false;
    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
    }

    if (buttonM.toFloat().contains(event.position))
    {
        notifyBeginEdit();
        setValue(nextValueInOrder(value, -1));
        notifyValueChanged();
        notifyEndEdit();
    }
    if (buttonP.toFloat().contains(event.position))
    {
        notifyBeginEdit();
        setValue(nextValueInOrder(value, +1));
        notifyValueChanged();
        notifyEndEdit();
    }
}

void WaveShaperSelector::mouseDrag(const juce::MouseEvent &e)
{
    auto d = e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY();
    if (fabs(d - lastDragDistance) > 10)
    {
        if (!everDragged)
        {
            notifyBeginEdit();
            everDragged = true;
        }
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

void WaveShaperSelector::mouseUp(const juce::MouseEvent &e)
{
    if (everDragged)
    {
        notifyEndEdit();
    }
    everDragged = false;
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