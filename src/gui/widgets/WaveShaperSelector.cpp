//
// Created by Paul Walker on 7/10/21.
//

#include "WaveShaperSelector.h"
#include "RuntimeFont.h"
#include "QuadFilterUnit.h"
#include "DSPUtils.h"
#include <iostream>
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Widgets
{

struct WaveShaperAnalysisWidget : public juce::Component, public juce::Slider::Listener
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
        std::cout << "Make a tryit slider" << std::endl;
        tryitSlider = std::make_unique<juce::Slider>();
        tryitSlider->setSliderStyle(juce::Slider::LinearVertical);
        tryitSlider->setDoubleClickReturnValue(true, 0.f, juce::ModifierKeys::noModifiers);
        tryitSlider->setSliderSnapsToMousePosition(false);
        tryitSlider->setRange(-1.0, 1.0);
        tryitSlider->setValue(0.0);
        tryitSlider->addListener(this);
        addAndMakeVisible(*tryitSlider);
    }
    void resized() override { tryitSlider->setBounds(0, 2, 20, getHeight() - 4); }
    void paint(juce::Graphics &g) override
    {
        if (sliderDrivenCurve.empty())
            recalcFromSlider();

        g.fillAll(juce::Colour(10, 10, 20));
        g.setColour(juce::Colours::lightgrey);
        g.drawRect(getLocalBounds());

        // OK so this path is in x=0,1 y=-1,1
        const auto sideOne = 26.0, sideTwo = 44.0;
        auto xf = juce::AffineTransform()
                      .translated(0, -1.0)
                      .scaled(1, -0.5)
                      .scaled(getWidth(), getHeight())
                      .translated(sideOne, 2)
                      .scaled((getWidth() - sideOne - sideTwo) / getWidth(),
                              (getHeight() - 4.0) / getHeight());

        auto re = juce::Rectangle<float>{0, -1, 1, 2}.transformedBy(xf);

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

            if (c == n_db_levs)
            {
                g.setColour(juce::Colour(10, 10, 20));
                g.fillRect(re);
                g.setColour(juce::Colours::lightgrey);
                g.drawRect(re);
            }

            float lineWidth = 1.25;

            if (c == 0)
                g.setColour(juce::Colours::white);
            else
            {
                g.setColour(
                    juce::Colour(40 + ((c - 1) * 20), 48 + ((c - 1) * 20), 56 + ((c - 1) * 20)));
                lineWidth = 0.65;
            }

            {
                juce::Graphics::ScopedSaveState gs(g);
                g.reduceClipRegion(re.toNearestIntEdges());
                g.strokePath(p, juce::PathStrokeType(lineWidth), xf);
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
                g.setColour(juce::Colours::orange);
                g.drawText(wst_ui_names[wstype], tx, juce::Justification::centredLeft);
            }
        }

        // Finally draw the slider driven path
        if (wstype != wst_none)
        {
            juce::Path p;
            for (int i = 0; i < npts; ++i)
            {
                if (i == 0)
                {
                    p.startNewSubPath(sliderDrivenCurve[i].first, sliderDrivenCurve[i].second);
                }
                else
                {
                    p.lineTo(sliderDrivenCurve[i].first, sliderDrivenCurve[i].second);
                }
            }

            {
                juce::Graphics::ScopedSaveState gs(g);
                g.reduceClipRegion(re.toNearestIntEdges());
                g.setColour(juce::Colours::orange);
                g.strokePath(p, juce::PathStrokeType(1.25), xf);
            }

            auto c = n_db_levs;
            auto xpos = re.getRight() + 3;
            auto ypos = re.getBottom() - 12;
            auto tx = juce::Rectangle<float>{xpos, ypos, 50, 12};
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << sliderDb << " dB";

            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.setColour(juce::Colours::orange);
            g.drawText(oss.str().c_str(), tx, juce::Justification::centredLeft);
        }
    }

    void sliderValueChanged(juce::Slider *slider) override
    {
        recalcFromSlider();
        repaint();
    }

    void recalcFromSlider()
    {
        if (wstype == wst_none)
            return;

        sliderDrivenCurve.clear();
        float dx = 1.f / (npts - 1);

        QuadFilterWaveshaperState wss;
        float R[4];
        initializeWaveshaperRegister(wstype, R);
        for (int i = 0; i < n_waveshaper_registers; ++i)
        {
            wss.R[i] = _mm_set1_ps(R[i]);
        }
        wss.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps()); // better way?

        auto wsop = GetQFPtrWaveshaper(wstype);

        sliderDb = tryitSlider->getValue() * 48;
        auto amp = powf(2.f, sliderDb / 18.f);
        auto d1 = _mm_set1_ps(amp);

        for (int i = 0; i < npts; i++)
        {
            float x = i * dx;
            float inval = std::sin(x * 4.0 * M_PI);
            auto ivs = _mm_set1_ps(inval);
            auto ov1 = ivs;
            if (wsop)
            {
                ov1 = wsop(&wss, ivs, d1);
            }
            float r alignas(16)[8];
            _mm_store_ps(r, ov1);

            sliderDrivenCurve.emplace_back(x, r[0]);
        }
    }

    void setWST(ws_type w)
    {
        wstype = w;
        if (responseCurves[wstype][0].empty())
        {
            // so the strategy is make a 128 point curve with 2 sin oscillations in [0,1].
            // We can use the SSE on drive to populate over two calls
            float dx = 1.f / (npts - 1);

            auto d1 = _mm_loadu_ps(ampLevs.data());
            auto d2 = _mm_loadu_ps(ampLevs.data() + 4);

            QuadFilterWaveshaperState ws1, ws2;
            float R[4];
            initializeWaveshaperRegister(wstype, R);
            for (int i = 0; i < n_waveshaper_registers; ++i)
            {
                ws1.R[i] = _mm_set1_ps(R[i]);
                ws2.R[i] = _mm_set1_ps(R[i]);
            }

            ws1.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps()); // better way?
            ws2.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps()); // better way?

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
                    ov2 = wsop(&ws2, ivs, d2);
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
        recalcFromSlider();
    }
    ws_type wstype{wst_none};
    std::unique_ptr<juce::Slider> tryitSlider;

    static constexpr int n_db_levs = 7, npts = 128;
    static std::array<float, n_db_levs> ampLevs, dbLevs;

    typedef std::vector<std::pair<float, float>> curve_t;
    static std::array<std::array<curve_t, n_db_levs + 1>, n_ws_types> responseCurves;

    curve_t sliderDrivenCurve;
    float sliderDb;
};

std::array<float, WaveShaperAnalysisWidget::n_db_levs> WaveShaperAnalysisWidget::dbLevs{
    -48, -24, -12, 0, 12, 24, 48};
std::array<float, WaveShaperAnalysisWidget::n_db_levs> WaveShaperAnalysisWidget::ampLevs{0, 0, 0, 0,
                                                                                         0, 0, 0};
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
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(8));
    g.drawText(wst_ui_names[iValue], getLocalBounds().reduced(0, 1),
               juce::Justification::centredTop);
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
                  .translated(2, -1.3)
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

        g.strokePath(curvePath, juce::PathStrokeType{iValue == wst_none ? 0.6f : 1.f}, xf);
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
    auto b = getBoundsInParent().translated(-270, 0).withWidth(270).withHeight(155);
    analysisWidget->setBounds(b);
    getParentComponent()->addAndMakeVisible(*analysisWidget);
    repaint();
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