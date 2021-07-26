//
// Created by Paul Walker on 7/10/21.
//

#include "WaveShaperSelector.h"
#include "RuntimeFont.h"
#include "QuadFilterUnit.h"
#include "DSPUtils.h"

namespace Surge
{
namespace Widgets
{

struct WaveShaperAnalysisWidget : public juce::Component
{
    WaveShaperAnalysisWidget()
    {
        if (ampLevs[0] == 0)
        {
            for (int i = 0; i < n_db_levs; ++i)
            {
                ampLevs[i] =
                    powf(2.f, dbLevs[i] / 18.f); // db_to_amp(dbLevs[i]); db_to_amp is limited. Why?
            }
        }
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(10, 10, 20));
        g.setColour(juce::Colours::lightgrey);
        g.drawRect(getLocalBounds());

        // Draw original last
        for (int c = n_db_levs; c >= 0; --c)
        {
            juce::Path p;

            for (int i = 0; i < npts; ++i)
            {
                if (i == 0)
                {
                    p.startNewSubPath(responseCurves[wstype][c][i].first,
                                      responseCurves[wstype][c][i].second);
                }
                else
                {
                    p.lineTo(responseCurves[wstype][c][i].first,
                             responseCurves[wstype][c][i].second);
                }
            }

            // OK so this path is in x=0,1 y=-1,1
            auto xf =
                juce::AffineTransform()
                    .translated(0, -1.0)
                    .scaled(1, -0.5)
                    .scaled(getWidth(), getHeight())
                    .translated(2, 2)
                    .scaled((getWidth() - 48.0) / getWidth(), (getHeight() - 4.0) / getHeight());

            auto re = juce::Rectangle<float>{0, -1, 1, 2}.transformedBy(xf);

            if (c == n_db_levs)
            {
                g.setColour(juce::Colour(10, 10, 20));
                g.fillRect(re);
                g.setColour(juce::Colours::lightgrey);
                g.drawRect(re);
            }

            if (c == 0)
                g.setColour(juce::Colours::white);
            else
            {
                g.setColour(juce::Colour(0xFF * (c + 11) / 20, 0x90 * (c + 11) / 20, 0));
            }
            {
                juce::Graphics::ScopedSaveState gs(g);
                g.reduceClipRegion(re.toNearestIntEdges());
                g.strokePath(p, juce::PathStrokeType(1.0), xf);
            }

            auto xpos = re.getRight() + 3;
            auto ypos = re.getY() + (c + 1) * 12;
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            auto tx = juce::Rectangle<float>{xpos, ypos, 50, 12};
            std::ostringstream oss;
            if (c == 0)
                oss << "Signal";
            else
                oss << dbLevs[c - 1] << "dB";
            g.drawText(oss.str().c_str(), tx, juce::Justification::centredLeft);
            if (c == 0)
            {
                tx = tx.translated(0, -12);
                g.drawText(wst_ui_names[wstype], tx, juce::Justification::centredLeft);
            }
        }
    }
    void setWST(ws_type w)
    {
        wstype = w;
        if (responseCurves[wstype][0].empty())
        {
            // so the stragety is make a 128 point curve with a 2 sin oscillations in [0,1].
            // We can use the SSE on drive to populate over two calls
            float dx = 1.f / (npts - 1);

            auto d1 = _mm_loadu_ps(ampLevs.data());
            auto d2 = _mm_loadu_ps(ampLevs.data() + 4);

            // FIXME - get this when we deal with init
            QuadFilterWaveshaperState ws1, ws2;
            for (int i = 0; i < n_waveshaper_registers; ++i)
            {
                ws1.R[i] = _mm_setzero_ps();
                ws2.R[i] = _mm_setzero_ps();
            }

            auto wsop = GetQFPtrWaveshaper(wstype);

            for (int i = 0; i < npts; i++)
            {
                float x = i * dx;
                float inval = std::sin(x * 4.0 * M_PI);
                auto ivs = _mm_set1_ps(inval);
                auto ov1 = ivs;
                auto ov2 = ivs;
                if (wsop)
                {
                    ov1 = wsop(&ws1, ivs, d1);
                    ov2 = wsop(&ws1, ivs, d2);
                }
                float r alignas(16)[8];
                _mm_store_ps(r, ov1);
                _mm_store_ps(r + 4, ov2);

                responseCurves[wstype][0].emplace_back(x, inval);
                for (int j = 0; j < n_db_levs; ++j)
                {
                    responseCurves[wstype][j + 1].emplace_back(x, r[j]);
                }
            }
        }
    }
    ws_type wstype{wst_none};

    static constexpr int n_db_levs = 8, npts = 128;
    static std::array<float, n_db_levs> ampLevs, dbLevs;

    typedef std::vector<std::pair<float, float>> curve_t;
    static std::array<std::array<curve_t, n_db_levs + 1>, n_ws_types> responseCurves;
};

std::array<float, WaveShaperAnalysisWidget::n_db_levs> WaveShaperAnalysisWidget::dbLevs{
    -24, -12, 0, 6, 8, 12, 18, 24};
std::array<float, WaveShaperAnalysisWidget::n_db_levs> WaveShaperAnalysisWidget::ampLevs{
    0, 0, 0, 0, 0, 0, 0, 0};
std::array<std::vector<std::pair<float, float>>, n_ws_types> WaveShaperSelector::wsCurves;
std::array<std::array<WaveShaperAnalysisWidget::curve_t, WaveShaperAnalysisWidget::n_db_levs + 1>,
           n_ws_types>
    WaveShaperAnalysisWidget::responseCurves;

WaveShaperSelector::WaveShaperSelector() {}

WaveShaperSelector::~WaveShaperSelector() { closeAnalysis(); }

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
                    wsCurves[iValue].emplace_back(x + i * dx, x + i * dx);
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

    g.setColour(juce::Colours::darkgrey);
    g.fillRect(buttonA);
    g.setColour(juce::Colours::white);
    g.drawText(analysisWidget ? "x" : "?", buttonA, juce::Justification::centred);
    g.setColour(juce::Colours::lightgrey);
    g.drawRect(buttonA);

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
        if (iValue == wst_none)
            g.setColour(juce::Colours::lightgrey);
        else
            g.setColour(juce::Colours::white);
        g.strokePath(curvePath, juce::PathStrokeType{iValue == wst_none ? 0.5f : 1.5f}, xf);
    }
}

void WaveShaperSelector::resized()
{
    auto b = getLocalBounds().withY(getHeight() - 12).withHeight(12);
    buttonM = b.withTrimmedRight(2.f * getWidth() / 3);
    buttonP = b.withTrimmedLeft(2.f * getWidth() / 3);
    buttonA = b.withTrimmedRight(getWidth() / 3.f).withTrimmedLeft(getWidth() / 3.f);
    waveArea = getLocalBounds().withTrimmedTop(12).withTrimmedBottom(12);
}

void WaveShaperSelector::setValue(float f)
{
    value = f;
    iValue = Parameter::intUnscaledFromFloat(value, n_ws_types - 1, 0);
    repaint();

    if (analysisWidget)
    {
        analysisWidget->setWST((ws_type)iValue);
        analysisWidget->repaint();
    }
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
    if (buttonA.toFloat().contains(event.position))
    {
        toggleAnalysis();
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
void WaveShaperSelector::parentHierarchyChanged() { closeAnalysis(); }

void WaveShaperSelector::toggleAnalysis()
{
    if (analysisWidget)
        closeAnalysis();
    else
        openAnalysis();
}

void WaveShaperSelector::closeAnalysis()
{
    if (!analysisWidget)
        return;
    if (getParentComponent() &&
        getParentComponent()->getIndexOfChildComponent(analysisWidget.get()) >= 0)
        getParentComponent()->removeChildComponent(analysisWidget.get());
    analysisWidget.reset(nullptr);
    repaint();
}

void WaveShaperSelector::openAnalysis()
{
    if (analysisWidget)
        return;
    analysisWidget = std::make_unique<WaveShaperAnalysisWidget>();
    analysisWidget->setWST((ws_type)iValue);
    auto b = getBoundsInParent().translated(-250, 0).withWidth(250).withHeight(125);
    analysisWidget->setBounds(b);
    getParentComponent()->addAndMakeVisible(*analysisWidget);
    repaint();
}

} // namespace Widgets
} // namespace Surge