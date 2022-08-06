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

#include "FilterAnalysis.h"
#include "RuntimeFont.h"
#include "SkinColors.h"
#include <fmt/core.h>
#include "sst/filters/FilterPlotter.h"
#include <thread>

namespace Surge
{
namespace Overlays
{
struct FilterAnalysisEvaluator
{
    FilterAnalysis *an;
    FilterAnalysisEvaluator(FilterAnalysis *a) : an(a)
    {
        analysisThread = std::make_unique<std::thread>(callRunThread, this);
    }
    ~FilterAnalysisEvaluator()
    {
        {
            auto lock = std::unique_lock<std::mutex>(dataLock);
            continueWaiting = false;
        }
        cv.notify_one();
        analysisThread->join();
    }

    static void callRunThread(FilterAnalysisEvaluator *that) { that->runThread(); }
    void runThread()
    {
        uint64_t lastIB = 0;
        auto fp = sst::filters::FilterPlotter(15);
        while (continueWaiting)
        {
            if (lastIB == inboundUpdates)
            {
                auto lock = std::unique_lock<std::mutex>(dataLock);
                cv.wait(lock);
            }

            if (lastIB != inboundUpdates)
            {
                int cty, csu;
                float ccu, cre, cgn;
                {
                    auto lock = std::unique_lock<std::mutex>(dataLock);
                    cty = type;
                    csu = subtype;
                    ccu = cutoff;
                    cre = resonance;
                    cgn = gain;
                    lastIB = inboundUpdates;
                }

                auto par = sst::filters::FilterPlotParameters();
                par.inputAmplitude *= cgn;
                auto data = fp.plotFilterMagnitudeResponse(
                    (sst::filters::FilterType)cty, (sst::filters::FilterSubType)csu, ccu, cre, par);

                {
                    auto lock = std::unique_lock<std::mutex>(dataLock);
                    outboundUpdates++;
                    dataCopy = data;

                    juce::MessageManager::getInstance()->callAsync(
                        [safethat = juce::Component::SafePointer(an)] {
                            if (safethat)
                                safethat->repaint();
                        });
                }
            }
        }
    }

    void request(int t, int s, float c, float r, float g)
    {
        {
            auto lock = std::unique_lock<std::mutex>(dataLock);

            type = t;
            subtype = s;
            cutoff = c;
            resonance = r;
            gain = powf(2.f, g / 18.f);
            inboundUpdates++;
        }
        cv.notify_one();
    }

    std::pair<std::vector<float>, std::vector<float>> dataCopy;
    std::atomic<uint64_t> inboundUpdates{1}, outboundUpdates{1};
    int type{0}, subtype{0};
    float cutoff{60}, resonance{0}, gain{1.f};
    std::mutex dataLock;
    std::condition_variable cv;
    std::unique_ptr<std::thread> analysisThread;
    bool hasWork{false}, continueWaiting{true};
};

FilterAnalysis::FilterAnalysis(SurgeGUIEditor *e, SurgeStorage *s) : editor(e), storage(s)
{
    evaluator = std::make_unique<FilterAnalysisEvaluator>(this);
    f1Button = std::make_unique<Surge::Widgets::SelfDrawToggleButton>("Filter 1");
    f1Button->setStorage(storage);
    f1Button->setToggleState(true);
    f1Button->onToggle = [this]() { selectFilter(0); };
    addAndMakeVisible(*f1Button);

    f2Button = std::make_unique<Surge::Widgets::SelfDrawToggleButton>("Filter 2");
    f2Button->setStorage(storage);
    f2Button->setToggleState(true);
    f2Button->onToggle = [this]() { selectFilter(1); };
    addAndMakeVisible(*f2Button);

    selectFilter(0);
    repushData();
}

FilterAnalysis::~FilterAnalysis() = default;

void FilterAnalysis::onSkinChanged()
{
    f1Button->setSkin(skin, associatedBitmapStore);
    f2Button->setSkin(skin, associatedBitmapStore);
}

void FilterAnalysis::paint(juce::Graphics &g)
{
    auto &fs = editor->getPatch().scene[editor->current_scene].filterunit[whichFilter];

    static constexpr auto lowFreq = 10.f;
    static constexpr auto highFreq = 24000.f;
    static constexpr auto dbMin = -42.f;
    static constexpr auto dbMax = 12.f;
    constexpr auto dbRange = dbMax - dbMin;

    auto freqToX = [&](float freq, int width) {
        auto xNorm = std::log(freq / lowFreq) / std::log(highFreq / lowFreq);
        return xNorm * (float)width;
    };

    auto dbToY = [&](float db, int height) { return (float)height * (dbMax - db) / dbRange; };

    char nm[256], snm[256];

    fs.type.get_display(nm);
    fs.subtype.get_display(snm);

    auto label = fmt::format("{}", nm);

    if (sst::filters::fut_subcount[fs.type.val.i] > 0)
    {
        label = fmt::format("{} ({})", nm, snm);
    }

    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    auto lb = getLocalBounds().transformedBy(getTransform().inverted());
    auto dRect = lb.withTrimmedTop(15).reduced(4);
    auto width = dRect.getWidth();
    auto height = dRect.getHeight();
    auto labelHeight = 9;
    auto font = skin->fontManager->getLatoAtSize(7);

    // horizontal and vertical grids
    {
        auto gs = juce::Graphics::ScopedSaveState(g);

        g.addTransform(juce::AffineTransform().translated(dRect.getX(), dRect.getY()));
        g.setFont(font);

        for (float freq : {20.f, 40.f, 60.f, 80.f, 100.f, 200.f, 400.f, 600.f, 800.f, 1000.f,
                           2000.f, 4000.f, 6000.f, 8000.f, 10000.f, 20000.f})
        {
            const auto xPos = freqToX(freq, width);
            juce::Line line{juce::Point{xPos, 0.f}, juce::Point{xPos, (float)height}};

            if (freq == 100.f || freq == 1000.f || freq == 10000.f)
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
            }
            else
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical));
            }

            g.drawLine(line);

            const auto over1000 = freq >= 1000.f;
            const auto freqString =
                juce::String(over1000 ? freq / 1000.f : freq) + (over1000 ? "k" : "");
            const auto labelRect = juce::Rectangle{font.getStringWidth(freqString), labelHeight}
                                       .withBottomY(height - 2)
                                       .withRightX((int)xPos);

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));
            g.drawFittedText(freqString, labelRect, juce::Justification::bottom, 1);
        }

        for (float dB : {-36.f, -30.f, -24.f, -18.f, -12.f, -6.f, 0.f, 6.f})
        {
            const auto yPos = dbToY(dB, height);

            juce::Line line{juce::Point{0.f, yPos}, juce::Point{(float)width, yPos}};

            if (dB == 0.f)
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
            }
            else
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryHorizontal));
            }

            g.drawLine(line);

            const auto dbString = juce::String(dB) + " dB";
            const auto labelRect = juce::Rectangle{font.getStringWidth(dbString), labelHeight}
                                       .withBottomY((int)yPos)
                                       .withRightX(width - 2);

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawFittedText(dbString, labelRect, juce::Justification::right, 1);
        }
    }

    // construct filter response curve
    if (catchUpStore != evaluator->outboundUpdates)
    {
        std::pair<std::vector<float>, std::vector<float>> data;
        {
            auto lock = std::unique_lock(evaluator->dataLock);
            data = evaluator->dataCopy;
            catchUpStore = evaluator->outboundUpdates;
        }

        plotPath = juce::Path();

        auto [freqAxis, magResponseDBSmoothed] = data;
        bool started = false;
        const auto nPoints = freqAxis.size();

        if (nPoints == 0)
        {
            auto yDraw = dbToY(-6, height);
            plotPath.startNewSubPath(0, yDraw);
            plotPath.lineTo(dRect.getX() + width, yDraw);
        }
        else
        {
            for (int i = 0; i < nPoints; ++i)
            {
                if (freqAxis[i] < lowFreq / 2.f || freqAxis[i] > highFreq * 1.01f)
                {
                    continue;
                }

                auto xDraw = freqToX(freqAxis[i], width);
                auto yDraw = dbToY(magResponseDBSmoothed[i], height);

                xDraw += dRect.getX();
                yDraw += dRect.getY();

                if (!started)
                {
                    plotPath.startNewSubPath(xDraw, yDraw);
                    started = true;
                }
                else
                {
                    plotPath.lineTo(xDraw, yDraw);
                }
            }
        }
    }

    // plot border
    g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
    g.drawRect(dRect);

    // fill below curve
    {
        auto gs = juce::Graphics::ScopedSaveState(g);
        auto fp = plotPath;

        fp.lineTo(dRect.getX() + width, dRect.getY() + height);
        fp.lineTo(0, dRect.getY() + height);
        fp.closeSubPath();

        g.reduceClipRegion(dRect);

        auto cg = juce::ColourGradient::vertical(
            skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
            skin->getColor(Colors::MSEGEditor::GradientFill::EndColor), dRect);

        g.setGradientFill(cg);
        g.fillPath(fp);
    }

    // plot curve
    {
        auto gs = juce::Graphics::ScopedSaveState(g);

        g.reduceClipRegion(dRect);
        g.setColour(skin->getColor(Colors::MSEGEditor::Curve));
        g.strokePath(plotPath, juce::PathStrokeType(1.f, juce::PathStrokeType::JointStyle::curved));
    }

    auto txtr = lb.withHeight(15);

    g.setColour(skin->getColor(Colors::Waveshaper::Preview::Text));
    g.setFont(skin->getFont(Fonts::Waveshaper::Preview::Title));
    g.drawText(label, txtr, juce::Justification::centred);
}

bool FilterAnalysis::shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p)
{
    if (p->ctrlgroup == cg_FILTER)
    {
        repushData();
        return true;
    }
    if (editor && p->id == patch.scene[editor->current_scene].level_pfg.id)
    {
        repushData();
        return true;
    }
    return false;
}

void FilterAnalysis::repushData()
{
    auto &ss = editor->getPatch().scene[editor->current_scene];
    auto &fs = ss.filterunit[whichFilter];
    auto &pfg = ss.level_pfg;

    auto t = fs.type.val.i;
    auto s = fs.subtype.val.i;
    auto c = fs.cutoff.val.f;
    auto r = fs.resonance.val.f;

    if (whichFilter == 1)
    {
        if (ss.f2_cutoff_is_offset.val.b)
        {
            c += ss.filterunit[0].cutoff.val.f;
        }
        if (ss.f2_link_resonance.val.b)
        {
            r = ss.filterunit[0].resonance.val.f;
        }
    }
    auto g = pfg.val.f;

    evaluator->request(t, s, c, r, g);
}

void FilterAnalysis::selectFilter(int which)
{
    whichFilter = which;
    if (which == 0)
    {
        f1Button->setValue(1);
        f2Button->setValue(0);
    }
    else
    {
        f1Button->setValue(0);
        f2Button->setValue(1);
    }
    repushData();
    repaint();
}

void FilterAnalysis::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    t.transformPoint(w, h);

    f1Button->setBounds(2, 2, 40, 15);
    f2Button->setBounds(w - 42, 2, 40, 15);

    catchUpStore = evaluator->outboundUpdates - 1; // because we need to rebuild the path
}

} // namespace Overlays
}; // namespace Surge
