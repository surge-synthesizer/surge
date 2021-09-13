//
// Created by Paul Walker on 7/10/21.
//

#include "WaveShaperSelector.h"
#include "RuntimeFont.h"
#include "QuadFilterUnit.h"
#include "DSPUtils.h"
#include <iostream>
#include "AccessibleHelpers.h"
#include "ModulatableSlider.h"
#include "SurgeGUIUtils.h"

namespace Surge
{
namespace Widgets
{
std::array<std::vector<std::pair<float, float>>, n_ws_types> WaveShaperSelector::wsCurves;

WaveShaperSelector::WaveShaperSelector() {}

WaveShaperSelector::~WaveShaperSelector() {}

void WaveShaperSelector::paint(juce::Graphics &g)
{
    if (wsCurves[iValue].empty())
    {
        /*
         * The waveshapers are re-entrant as long as they have a unique state pointer
         */
        auto drive = _mm_set1_ps(1.f);
        float xs alignas(16)[4], vals alignas(16)[4];

        for (int i = 0; i < 4; ++i)
        {
            xs[i] = 0.f;
            vals[0] = 0.f;
        }

        auto wsop = GetQFPtrWaveshaper(iValue);
        QuadFilterWaveshaperState s;
        float R alignas(16)[4];

        initializeWaveshaperRegister(iValue, R);

        for (int i = 0; i < 4; ++i)
        {
            s.R[i] = _mm_load_ps(R);
        }

        s.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());

        float dx = 0.05;

        // Give a few warmup pixels for the ADAAs
        for (float x = -2 - 3 * dx; x <= 2; x += dx)
        {
            if (wsop)
            {
                vals[0] = x;
                auto in = _mm_load_ps(vals);
                auto r = wsop(&s, in, drive);
                for (int i = 0; i < 4; ++i)
                    s.R[i] = _mm_load_ps(R);
                s.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());

                _mm_store_ps(vals, r);
                if (x >= -2)
                    wsCurves[iValue].emplace_back(x, vals[0]);
            }
            else
            {
                if (x >= -2)
                    wsCurves[iValue].emplace_back(x, x);
            }
        }
    }

    if (bg)
    {
        bg->draw(g, 1.0);

        if (isLabelHovered && bgHover)
        {
            bgHover->draw(g, 1.0);
        }
    }

    if (isLabelHovered)
    {
        g.setColour(skin->getColor(Colors::Waveshaper::TextHover));
    }
    else
    {
        g.setColour(skin->getColor(Colors::Waveshaper::Text));
    }

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(7));
    g.drawText(wst_ui_names[iValue], getLocalBounds().withHeight(labelHeight),
               juce::Justification::centred);

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
                  .translated(2, -1.3)
                  .scaled(0.25, -0.4)
                  .scaled(waveArea.getWidth(), waveArea.getHeight())
                  .translated(waveArea.getX(), waveArea.getY());
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.setColour(skin->getColor(Colors::Waveshaper::Display::Dots));

        int nxd = 7, nyd = 7;

        for (int xd = 1; xd < nxd - 1; ++xd)
        {
            float normx = 4.f * xd / (nxd - 1) - 2;

            for (int yd = 0; yd < nyd; ++yd)
            {
                float normy = 2.f * yd / (nyd - 1) - 1;
                auto p = juce::Point<float>(normx, normy).transformedBy(xf);

                g.fillEllipse(p.x - 0.5, p.y - 0.5, 1, 1);
            }
        }
    }
    {
        juce::Graphics::ScopedSaveState gs(g);

        g.reduceClipRegion(waveArea);
        if (isWaveHovered)
            g.setColour(skin->getColor(Colors::Waveshaper::Display::WaveHover));
        else
            g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave));
        g.strokePath(curvePath, juce::PathStrokeType{iValue == wst_none ? 0.6f : 1.f}, xf);
    }
}

void WaveShaperSelector::resized()
{
    labelArea = getLocalBounds().withHeight(labelHeight);
    waveArea = getLocalBounds().withTrimmedTop(labelHeight).reduced(1);
}

void WaveShaperSelector::setValue(float f)
{
    value = f;
    iValue = Parameter::intUnscaledFromFloat(value, n_ws_types - 1, 0);
    repaint();
}

void WaveShaperSelector::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    lastDragDistance = 0;
    everDragged = false;
    everMoved = false;

    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
    }
    if (labelArea.contains(event.position.toInt()))
    {
        auto m = event.mods.withFlags(juce::ModifierKeys::popupMenuClickModifier);
        notifyControlModifierClicked(m);
    }
}

void WaveShaperSelector::mouseDrag(const juce::MouseEvent &e)
{
    auto d = e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY();
    if (fabs(d - lastDragDistance) > 0)
    {
        if (!everMoved)
        {
            if (!Surge::GUI::showCursor(storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    true);
            }
        }
        everMoved = true;
    }
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
    if (everMoved)
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
            auto p = e.mouseDownPosition;
            p = localPointToGlobal(p);
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
        }
    }
    if (everDragged)
    {
        notifyEndEdit();
    }
    everDragged = false;
    everMoved = false;
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

void WaveShaperSelector::jog(int by)
{
    notifyBeginEdit();
    setValue(nextValueInOrder(value, by));
    notifyValueChanged();
    notifyEndEdit();
    repaint();
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

void WaveShaperSelector::parentHierarchyChanged() {}

void WaveShaperSelector::onSkinChanged()
{
    bg = associatedBitmapStore->getImage(IDB_WAVESHAPER_BG);
    bgHover = associatedBitmapStore->getImageByStringID(
        skin->hoverImageIdForResource(IDB_WAVESHAPER_BG, GUI::Skin::HOVER));
}

#if SURGE_JUCE_ACCESSIBLE

template <> struct DiscreteAHRange<WaveShaperSelector>
{
    static int iMaxV(WaveShaperSelector *t) { return n_ws_types - 1; }
    static int iMinV(WaveShaperSelector *t) { return 0; }
};

template <> struct DiscreteAHStringValue<WaveShaperSelector>
{
    static std::string stringValue(WaveShaperSelector *comp, double ahValue)
    {
        auto r = (int)std::round(ahValue);
        if (r < 0 || r > n_ws_types - 1)
            return "ERROR";
        return wst_names[r];
    }
};

std::unique_ptr<juce::AccessibilityHandler> WaveShaperSelector::createAccessibilityHandler()
{
    return std::make_unique<DiscreteAH<WaveShaperSelector>>(this);
}
#endif

} // namespace Widgets
} // namespace Surge