/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "WaveShaperAnalysis.h"
#include "RuntimeFont.h"
#include "SkinColors.h"

namespace Surge
{
namespace Overlays
{
WaveShaperAnalysis::WaveShaperAnalysis(SurgeStorage *s)
{
    tryitSlider = std::make_unique<Surge::Widgets::ModulatableSlider>();
    tryitSlider->setOrientation(Surge::ParamConfig::kVertical);
    tryitSlider->setValue(0.5);
    tryitSlider->setQuantitizedDisplayValue(0.5);
    tryitSlider->setBipolarFn([]() { return true; });
    tryitSlider->setIsLightStyle(true);
    tryitSlider->setStorage(s);
    tryitSlider->addListener(this);
    addAndMakeVisible(*tryitSlider);
}

void WaveShaperAnalysis::onSkinChanged() { tryitSlider->setSkin(skin, associatedBitmapStore); }
void WaveShaperAnalysis::resized() { tryitSlider->setBounds(40 - 11 - 20, 25, 22, 84); }

void WaveShaperAnalysis::paint(juce::Graphics &g)
{
    if (sliderDrivenCurve.empty())
        recalcFromSlider();

    g.fillAll(skin->getColor(Colors::Waveshaper::Analysis::Background));

    // OK so this path is in x=0,1 y=-1,1
    const auto sideOne = 45.f, sideTwo = 2.f, top = 25.f;
    auto xf = juce::AffineTransform()
                  .translated(0, -1.0)
                  .scaled(1, -0.5)
                  .scaled(getWidth(), getHeight())
                  .translated(sideOne, 2 + top)
                  .scaled((getWidth() - sideOne - sideTwo) / getWidth(),
                          (getHeight() - 4.0 - top) / getHeight());

    auto re = juce::Rectangle<float>{0, -1, 1, 2}.transformedBy(xf);

    juce::Path p, pInput;

    for (int i = 0; i < npts; ++i)
    {
        if (i == 0)
        {
            p.startNewSubPath(sliderDrivenCurve[i].first, sliderDrivenCurve[i].second);
            pInput.startNewSubPath(sliderDrivenCurve[i].first,
                                   std::sin(sliderDrivenCurve[i].first * 4.0 * M_PI));
        }
        else
        {
            p.lineTo(sliderDrivenCurve[i].first, sliderDrivenCurve[i].second);
            pInput.lineTo(sliderDrivenCurve[i].first,
                          std::sin(sliderDrivenCurve[i].first * 4.0 * M_PI));
        }
    }

    {
        juce::Graphics::ScopedSaveState gs(g);
        g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave));
        g.drawRect(re);
        g.setColour(skin->getColor(Colors::Waveshaper::Display::Dots));
        for (int yd = -4; yd <= 4; ++yd)
        {
            float yp = yd * 0.2;
            for (float xp = 0.05; xp < 1; xp += 0.05)
            {
                auto cxp = xp, cyp = yp;
                xf.transformPoint(cxp, cyp);
                g.fillEllipse(cxp - 0.5, cyp - 0.5, 1, 1);
            }
        }

        g.drawLine(re.getX(), re.getCentreY(), re.getX() + re.getWidth(), re.getCentreY());
        g.reduceClipRegion(re.toNearestIntEdges());
        g.setColour(skin->getColor(Colors::Waveshaper::Display::WaveHover));
        g.strokePath(pInput, juce::PathStrokeType(1.0), xf);
        g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave));
        if (wstype != wst_none)
            g.strokePath(p, juce::PathStrokeType(1.25), xf);
    }
    g.setColour(skin->getColor(Colors::Waveshaper::Analysis::Border));
    g.drawRect(re);

    {
        float xpos = 2.0;
        auto ypos = tryitSlider->getBounds().getBottom() + 2.f;
        auto tx = juce::Rectangle<float>{xpos, ypos, 40 - 4, 12.f};

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << sliderDb;
        g.setFont(skin->getFont(Fonts::WaveShaperAnalysis::DBAmount));
        g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave));
        g.drawText(oss.str().c_str(), tx, juce::Justification::centred);

        g.setFont(skin->getFont(Fonts::WaveShaperAnalysis::DBLabel));
        tx = tx.translated(0, 12);
        g.drawText("Drive (db)", tx, juce::Justification::centred);
    }

    g.setColour(skin->getColor(Colors::Waveshaper::Analysis::Title));
    g.setFont(skin->getFont(Fonts::WaveShaperAnalysis::Title));
    auto txtr = getLocalBounds().withHeight(top - 2).reduced(1);
    g.drawText(wst_names[wstype], txtr, juce::Justification::centred);
}

void WaveShaperAnalysis::valueChanged(Surge::GUI::IComponentTagValue *p)
{
    recalcFromSlider();
    tryitSlider->setQuantitizedDisplayValue(tryitSlider->getValue());
    repaint();
}

void WaveShaperAnalysis::recalcFromSlider()
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

    sliderDb = tryitSlider->getValue() * 96 - 48;
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

void WaveShaperAnalysis::setWSType(int w)
{
    wstype = w;
    recalcFromSlider();
}

} // namespace Overlays
}; // namespace Surge
