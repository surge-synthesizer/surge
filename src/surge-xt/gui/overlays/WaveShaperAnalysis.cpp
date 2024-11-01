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

#include "WaveShaperAnalysis.h"
#include "RuntimeFont.h"
#include "SkinColors.h"

namespace Surge
{
namespace Overlays
{
WaveShaperAnalysis::WaveShaperAnalysis(SurgeGUIEditor *e, SurgeStorage *s) : editor(e), storage(s)
{
}

void WaveShaperAnalysis::onSkinChanged() {}
void WaveShaperAnalysis::resized() {}

void WaveShaperAnalysis::paint(juce::Graphics &g)
{
    auto lb = getLocalBounds().transformedBy(getTransform().inverted());
    auto width = lb.getWidth();
    auto height = lb.getHeight();

    if (sliderDrivenCurve.empty() || lastDbValue != getDbValue() || lastPFG != getPFG())
    {
        recalcFromSlider();
    }

    g.fillAll(skin->getColor(Colors::Waveshaper::Preview::Background));

    // OK so this path is in x = 0, 1; y = -1, 1
    const auto sideOne = 4.f, sideTwo = 4.f, top = 25.f;
    auto xf = juce::AffineTransform()
                  .translated(0, -1.0)
                  .scaled(1, -0.5)
                  .scaled(width, height)
                  .translated(sideOne, 2 + top)
                  .scaled((width - sideOne - sideTwo) / width, (height - 4.0 - top) / height);

    auto re = juce::Rectangle<float>{0, -1, 1, 2}.transformedBy(xf);

    juce::Path p, pInput;

    for (int i = 0; i < npts; ++i)
    {
        if (i == 0)
        {
            p.startNewSubPath(std::get<0>(sliderDrivenCurve[i]), std::get<2>(sliderDrivenCurve[i]));
            pInput.startNewSubPath(std::get<0>(sliderDrivenCurve[i]),
                                   std::get<1>(sliderDrivenCurve[i]));
        }
        else
        {
            p.lineTo(std::get<0>(sliderDrivenCurve[i]), std::get<2>(sliderDrivenCurve[i]));
            pInput.lineTo(std::get<0>(sliderDrivenCurve[i]), std::get<1>(sliderDrivenCurve[i]));
        }
    }

    re.expand(2, 2);

    {
        auto gs = juce::Graphics::ScopedSaveState(g);

        auto font = skin->fontManager->getLatoAtSize(7);
        g.setFont(font);

        for (float dphase : {0.25f, 0.5f, 0.75f})
        {
            auto x0 = dphase, y0 = -1.f, x1 = dphase, y1 = 1.f;
            xf.transformPoint(x0, y0);
            xf.transformPoint(x1, y1);
            juce::Line line{juce::Point{x0, y0}, juce::Point{x1, y1}};

            g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical));
            g.drawLine(line);
        }

        for (float amp : {-0.5f, 0.f, 0.5f})
        {
            auto x0 = 0.f, y0 = amp, x1 = 1.f, y1 = amp;
            xf.transformPoint(x0, y0);
            xf.transformPoint(x1, y1);
            juce::Line line{juce::Point{x0, y0}, juce::Point{x1, y1}};

            g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryHorizontal));
            g.drawLine(line);
        }
    }

    {
        juce::Graphics::ScopedSaveState gs(g);

        g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
        g.drawLine(re.getX(), re.getCentreY(), re.getX() + re.getWidth(), re.getCentreY());

        {
            auto gs2 = juce::Graphics::ScopedSaveState(g);

            g.reduceClipRegion(re.toNearestIntEdges());
            g.strokePath(pInput, juce::PathStrokeType(0.75), xf);

            if (wstype != sst::waveshapers::WaveshaperType::wst_none)
            {
                {
                    auto gs = juce::Graphics::ScopedSaveState(g);
                    auto fp = p;

                    fp.lineTo(std::get<0>(sliderDrivenCurve.back()), 0);
                    fp.lineTo(0, 0);

                    auto cg = juce::ColourGradient::vertical(
                        skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
                        skin->getColor(Colors::MSEGEditor::GradientFill::EndColor), re);

                    g.setGradientFill(cg);
                    g.fillPath(fp, xf);
                }

                {
                    auto gs = juce::Graphics::ScopedSaveState(g);

                    g.setColour(skin->getColor(Colors::MSEGEditor::Curve));
                    g.strokePath(
                        p, juce::PathStrokeType(1.f, juce::PathStrokeType::JointStyle::curved), xf);
                }
            }
        }
    }

    g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
    g.drawRect(re);

    auto txtr = lb.withHeight(top - 6);
    std::ostringstream title;
    title << sst::waveshapers::wst_names[(int)wstype];

    g.setColour(skin->getColor(Colors::Waveshaper::Preview::Text));
    g.setFont(skin->getFont(Fonts::Waveshaper::Preview::Title));
    g.drawText(title.str(), txtr, juce::Justification::centred);
}

void WaveShaperAnalysis::recalcFromSlider()
{
    lastDbValue = getDbValue();
    lastPFG = getPFG();
    sliderDrivenCurve.clear();

    sst::waveshapers::QuadWaveshaperState wss;
    float dx = 1.f / (npts - 1);
    float R[4];

    initializeWaveshaperRegister(wstype, R);

    for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
    {
        wss.R[i] = SIMD_MM(set1_ps)(R[i]);
    }

    wss.init = SIMD_MM(cmpeq_ps)(SIMD_MM(setzero_ps)(), SIMD_MM(setzero_ps)()); // better way?

    auto wsop = sst::waveshapers::GetQuadWaveshaper(wstype);

    auto sliderDb = getDbValue();
    auto amp = powf(2.f, sliderDb / 18.f);

    auto pfg = powf(2.f, getPFG() / 18.f);
    auto d1 = SIMD_MM(set1_ps)(amp);

    for (int i = 0; i < npts; i++)
    {
        float x = i * dx;
        float inval = pfg * std::sin(x * 4.0 * M_PI);
        auto ivs = SIMD_MM(set1_ps)(inval);
        auto ov1 = ivs;

        if (wsop)
        {
            ov1 = wsop(&wss, ivs, d1);
        }

        float r alignas(16)[8];
        SIMD_MM(store_ps)(r, ov1);

        sliderDrivenCurve.emplace_back(x, inval, r[0]);
    }
}

void WaveShaperAnalysis::setWSType(int w)
{
    wstype = static_cast<sst::waveshapers::WaveshaperType>(w);
    recalcFromSlider();
}

float WaveShaperAnalysis::getDbValue()
{
    float sliderDb = 0.f;
    auto cs = editor->current_scene;
    auto &p = editor->getPatch().scene[cs].wsunit.drive;
    auto f = p.get_extended(p.val.f);
    sliderDb = f;
    return sliderDb;
}

float WaveShaperAnalysis::getPFG()
{
    float sliderDb = 0.f;
    auto cs = editor->current_scene;
    auto &p = editor->getPatch().scene[cs].level_pfg;
    auto f = p.get_extended(p.val.f);
    sliderDb = f;
    return sliderDb;
}
bool WaveShaperAnalysis::shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p)
{
    if (p->ctrlgroup == cg_GLOBAL || p->ctrlgroup == cg_MIX)
    {
        for (int i = 0; i < n_scenes; ++i)
        {
            auto &ws = patch.scene[i].wsunit;
            if (p->id == ws.type.id || p->id == ws.drive.id)
                return true;
            if (p->id == patch.scene[i].level_pfg.id)
                return true;
        }
    }
    return false;
}

} // namespace Overlays
}; // namespace Surge
