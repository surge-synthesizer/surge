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
WaveShaperAnalysis::WaveShaperAnalysis(SurgeGUIEditor *e, SurgeStorage *s) : editor(e), storage(s)
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

    linkButton = std::make_unique<Surge::Widgets::SelfDrawToggleButton>("Link");
    linkButton->setStorage(storage);
    linkButton->setToggleState(true);
    linkButton->onToggle = [this]() { linkToggled(); };
    addAndMakeVisible(*linkButton);
    linkToggled();
}

void WaveShaperAnalysis::onSkinChanged()
{
    tryitSlider->setSkin(skin, associatedBitmapStore);
    linkButton->setSkin(skin, associatedBitmapStore);
}
void WaveShaperAnalysis::resized()
{
    linkButton->setBounds(2, 2, 22 + 8, 15);
    tryitSlider->setBounds(8, 25, 22, 84);
}

void WaveShaperAnalysis::linkToggled()
{
    linked = linkButton->getToggleState();
    if (linked)
    {
        tryitSlider->setDeactivated(true);
    }
    else
    {
        tryitSlider->setDeactivated(false);
    }
    tryitSlider->repaint();
    recalcFromSlider();
    repaint();
}

void WaveShaperAnalysis::paint(juce::Graphics &g)
{
    if (sliderDrivenCurve.empty() || lastDbValue != getDbValue())
    {
        recalcFromSlider();
    }

    g.fillAll(skin->getColor(Colors::Waveshaper::Preview::Background));

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

    re.expand(2, 2);

    {
        juce::Graphics::ScopedSaveState gs(g);
        g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave));
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
        g.setColour(skin->getColor(Colors::Waveshaper::Display::WaveHover));
        g.strokePath(pInput, juce::PathStrokeType(1.0), xf);

        if (wstype != wst_none)
        {
            g.setColour(skin->getColor(Colors::Waveshaper::Display::Wave));
            g.strokePath(p, juce::PathStrokeType(1.25), xf);
        }
    }

    g.setColour(skin->getColor(Colors::Waveshaper::Preview::Border));
    g.drawRect(re);

    {
        auto ypos = tryitSlider->getBounds().getBottom() + 0.f;
        auto tx = juce::Rectangle<float>{0, ypos, 40 - 4, 12.f};

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << getDbValue();

        g.setFont(skin->getFont(Fonts::Waveshaper::Preview::DriveAmount));
        g.setColour(skin->getColor(Colors::Waveshaper::Preview::Text));
        g.drawText(oss.str(), tx, juce::Justification::centred);

        tx = tx.translated(0, 12);

        g.setFont(skin->getFont(Fonts::Waveshaper::Preview::DriveLabel));
        g.setColour(skin->getColor(Colors::Waveshaper::Preview::Text));
        g.drawText("Drive (dB)", tx, juce::Justification::centred);
    }

    auto txtr = getLocalBounds().withHeight(top - 6);
    std::ostringstream title;
    title << "Current: " << wst_names[wstype];

    g.setColour(skin->getColor(Colors::Waveshaper::Preview::Text));
    g.setFont(skin->getFont(Fonts::Waveshaper::Preview::Title));
    g.drawText(title.str(), txtr, juce::Justification::centred);
}

void WaveShaperAnalysis::valueChanged(Surge::GUI::IComponentTagValue *p)
{
    recalcFromSlider();
    tryitSlider->setQuantitizedDisplayValue(tryitSlider->getValue());

    repaint();
}

int32_t WaveShaperAnalysis::controlModifierClicked(Surge::GUI::IComponentTagValue *p,
                                                   const juce::ModifierKeys &mods,
                                                   bool isDoubleClickEvent)
{
    if (isDoubleClickEvent)
    {
        tryitSlider->setValue(0.5f);
        valueChanged(tryitSlider.get());
    }

    return 0;
}

void WaveShaperAnalysis::recalcFromSlider()
{
    lastDbValue = getDbValue();
    sliderDrivenCurve.clear();

    QuadFilterWaveshaperState wss;
    float dx = 1.f / (npts - 1);
    float R[4];

    initializeWaveshaperRegister(wstype, R);

    for (int i = 0; i < n_waveshaper_registers; ++i)
    {
        wss.R[i] = _mm_set1_ps(R[i]);
    }

    wss.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps()); // better way?

    auto wsop = GetQFPtrWaveshaper(wstype);

    auto sliderDb = getDbValue();

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

float WaveShaperAnalysis::getDbValue()
{
    float sliderDb = 0.f;
    if (!linked)
    {
        sliderDb = tryitSlider->getValue() * 96 - 48;
    }
    else
    {
        auto cs = editor->current_scene;
        auto &p = editor->getPatch().scene[cs].wsunit.drive;
        auto f = p.get_extended(p.val.f);
        sliderDb = f;
    }
    return sliderDb;
}

} // namespace Overlays
}; // namespace Surge
