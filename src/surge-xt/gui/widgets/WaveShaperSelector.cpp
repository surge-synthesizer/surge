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

#include "WaveShaperSelector.h"
#include "RuntimeFont.h"
#include "DSPUtils.h"
#include <iostream>
#include "AccessibleHelpers.h"
#include "ModulatableSlider.h"
#include "SurgeGUIUtils.h"

namespace Surge
{
namespace Widgets
{
using WaveshaperType = sst::waveshapers::WaveshaperType;
std::array<std::vector<std::pair<float, float>>, (int)WaveshaperType::n_ws_types>
    WaveShaperSelector::wsCurves;

WaveShaperSelector::WaveShaperSelector()
    : juce::Component(), WidgetBaseMixin<WaveShaperSelector>(this)
{
}

WaveShaperSelector::~WaveShaperSelector() {}

void WaveShaperSelector::paint(juce::Graphics &g)
{
    float dOpacity = (isDeactivated ? 0.5 : 1.0);
    float dThick = (isDeactivated ? 0.6 : 1.0);
    if (wsCurves[(int)iValue].empty())
    {
        /*
         * The waveshapers are re-entrant as long as they have a unique state pointer
         */
        auto drive = SIMD_MM(set1_ps)(1.f);
        float xs alignas(16)[4], vals alignas(16)[4];

        for (int i = 0; i < 4; ++i)
        {
            xs[i] = 0.f;
            vals[0] = 0.f;
        }

        auto wsop = sst::waveshapers::GetQuadWaveshaper(iValue);
        sst::waveshapers::QuadWaveshaperState s;
        float R alignas(16)[4];

        sst::waveshapers::initializeWaveshaperRegister(iValue, R);

        for (int i = 0; i < 4; ++i)
        {
            s.R[i] = SIMD_MM(load_ps)(R);
        }

        s.init = SIMD_MM(cmpeq_ps)(SIMD_MM(setzero_ps)(), SIMD_MM(setzero_ps)());

        float dx = 0.05;

        // Give a few warmup pixels for the ADAAs
        for (float x = -2 - 3 * dx; x <= 2; x += dx)
        {
            if (wsop)
            {
                vals[0] = x;
                auto in = SIMD_MM(load_ps)(vals);
                auto r = wsop(&s, in, drive);
                for (int i = 0; i < 4; ++i)
                    s.R[i] = SIMD_MM(load_ps)(R);
                s.init = SIMD_MM(cmpeq_ps)(SIMD_MM(setzero_ps)(), SIMD_MM(setzero_ps)());

                SIMD_MM(store_ps)(vals, r);
                if (x >= -2)
                    wsCurves[(int)iValue].emplace_back(x, vals[0]);
            }
            else
            {
                if (x >= -2)
                    wsCurves[(int)iValue].emplace_back(x, x);
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
        g.setColour(skin->getColor(Colors::Waveshaper::TextHover).withAlpha(dOpacity));
    }
    else
    {
        g.setColour(skin->getColor(Colors::Waveshaper::Text).withAlpha(dOpacity));
    }

    g.setFont(skin->fontManager->getLatoAtSize(7));
    g.drawText(wst_ui_names[(int)iValue], getLocalBounds().withHeight(labelHeight),
               juce::Justification::centred);

    // So the wave is in -2,2 in x and -1,1 in y
    juce::Path curvePath;
    bool f = true;

    for (const auto &el : wsCurves[(int)iValue])
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
            g.setColour(skin->getColor(Colors::Waveshaper::Display::WaveHover).withAlpha(dOpacity));
        else
            g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave).withAlpha(dOpacity));
        g.strokePath(curvePath,
                     juce::PathStrokeType{iValue == WaveshaperType::wst_none ? 0.6f : dThick}, xf);
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
    iValue = static_cast<WaveshaperType>(
        Parameter::intUnscaledFromFloat(value, (int)WaveshaperType::n_ws_types - 1, 0));
    repaint();
}

void WaveShaperSelector::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (waveArea.contains(event.position.toInt()))
    {
        notifyControlModifierDoubleClicked(event.mods.allKeyboardModifiers);
    }
}

void WaveShaperSelector::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    mouseDownLongHold(event);

    lastDragDistance = 0;
    everDragged = false;
    everMoved = false;

    if (labelArea.contains(event.position.toInt()) && event.mods.isLeftButtonDown())
    {
        auto m = event.mods.withFlags(juce::ModifierKeys::popupMenuClickModifier);
        notifyControlModifierClicked(m);
    }

    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
    }
}

void WaveShaperSelector::mouseDrag(const juce::MouseEvent &event)
{
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    mouseDragLongHold(event);

    auto d = event.getDistanceFromDragStartX() - event.getDistanceFromDragStartY();

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

void WaveShaperSelector::mouseUp(const juce::MouseEvent &event)
{
    mouseUpLongHold(event);

    if (everMoved)
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);

            auto p = event.mouseDownPosition;
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

void WaveShaperSelector::mouseWheelMove(const juce::MouseEvent &event,
                                        const juce::MouseWheelDetails &w)
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
    int iv = Parameter::intUnscaledFromFloat(v, (int)WaveshaperType::n_ws_types - 1, 0);
    if (!intOrdering.empty() && (int)WaveshaperType::n_ws_types == intOrdering.size())
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
            iv = (int)WaveshaperType::n_ws_types - 1;
        }

        if (iv > (int)WaveshaperType::n_ws_types - 1)
        {
            iv = 0;
        }
    }

    // This is the get_value_f01 code
    float r = Parameter::intScaledToFloat(iv, (int)WaveshaperType::n_ws_types - 1, 0);

    return r;
}

void WaveShaperSelector::parentHierarchyChanged() {}

void WaveShaperSelector::onSkinChanged()
{
    bg = associatedBitmapStore->getImage(IDB_WAVESHAPER_BG);
    bgHover = associatedBitmapStore->getImageByStringID(
        skin->hoverImageIdForResource(IDB_WAVESHAPER_BG, GUI::Skin::HOVER));
}

template <> struct DiscreteAHRange<WaveShaperSelector>
{
    static int iMaxV(WaveShaperSelector *t) { return (int)WaveshaperType::n_ws_types - 1; }
    static int iMinV(WaveShaperSelector *t) { return 0; }
};

template <> struct DiscreteAHStringValue<WaveShaperSelector>
{
    static std::string stringValue(WaveShaperSelector *comp, double ahValue)
    {
        auto r = (int)std::round(ahValue);
        if (r < 0 || r > (int)WaveshaperType::n_ws_types - 1)
            return "ERROR";
        return sst::waveshapers::wst_names[r];
    }
};

std::unique_ptr<juce::AccessibilityHandler> WaveShaperSelector::createAccessibilityHandler()
{
    return std::make_unique<DiscreteAH<WaveShaperSelector>>(this);
}

bool WaveShaperSelector::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu)
    {
        notifyControlModifierClicked(juce::ModifierKeys(), true);
        return true;
    }

    if (action != Increase && action != Decrease)
        return false;

    int dir = (action == Increase ? 1 : -1);
    notifyBeginEdit();
    setValue(nextValueInOrder(value, dir));
    notifyValueChanged();
    notifyEndEdit();
    repaint();

    return true;
}

} // namespace Widgets
} // namespace Surge
