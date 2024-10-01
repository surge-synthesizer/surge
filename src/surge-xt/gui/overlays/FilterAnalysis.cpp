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

#include "FilterAnalysis.h"
#include "RuntimeFont.h"
#include "SkinColors.h"
#include <fmt/core.h>
#include "sst/filters/FilterPlotter.h"
#include <thread>
#include "Tunings.h"
#include "SurgeGUIEditorTags.h"

static constexpr auto GRAPH_MIN_FREQ = 13.57f;
static constexpr auto GRAPH_MAX_FREQ = 25087.f;

static constexpr auto GRAPH_MIN_DB = -42.f;
static constexpr auto GRAPH_MAX_DB = 18.f;

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

FilterAnalysis::FilterAnalysis(SurgeGUIEditor *e, SurgeStorage *s, SurgeSynthesizer *synth)
    : editor(e), storage(s), synth(synth)
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

    constexpr auto dbRange = GRAPH_MAX_DB - GRAPH_MIN_DB;

    auto freqToX = [&](float freq, int width) {
        auto xNorm = std::log(freq / GRAPH_MIN_FREQ) / std::log(GRAPH_MAX_FREQ / GRAPH_MIN_FREQ);
        return xNorm * (float)width;
    };

    auto dbToY = [&](float db, int height) {
        return (float)height * (GRAPH_MAX_DB - db) / dbRange;
    };

    std::string nm, snm;

    nm = fs.type.get_display();
    snm = fs.subtype.get_display();

    auto label = fmt::format("{}", nm);

    if (sst::filters::fut_subcount[fs.type.val.i] > 0)
    {
        label = fmt::format("{} ({})", nm, snm);
    }

    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    const auto lb = getLocalBounds().transformedBy(getTransform().inverted());
    const auto dRect = lb.withTrimmedTop(18).reduced(4);
    const auto width = dRect.getWidth();
    const auto height = dRect.getHeight();
    const auto labelHeight = 9;
    const auto font = skin->fontManager->getLatoAtSize(7);

    // horizontal and vertical grids
    {
        auto gs = juce::Graphics::ScopedSaveState(g);

        g.addTransform(juce::AffineTransform().translated(dRect.getX(), dRect.getY()));
        g.setFont(font);

        for (float freq : {20.f, 40.f, 60.f, 80.f, 100.f, 200.f, 400.f, 600.f, 800.f, 1000.f,
                           2000.f, 4000.f, 6000.f, 8000.f, 10000.f, 20000.f})
        {
            const auto xPos = freqToX(freq, width);
            const juce::Line line{juce::Point{xPos, 0.f}, juce::Point{xPos, (float)height}};

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
            const auto labelRect =
                juce::Rectangle{SST_STRING_WIDTH_INT(font, freqString), labelHeight}
                    .withBottomY(height - 2)
                    .withRightX((int)xPos);

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));
            g.drawFittedText(freqString, labelRect, juce::Justification::bottom, 1);
        }

        for (float dB : {-36.f, -30.f, -24.f, -18.f, -12.f, -6.f, 0.f, 6.f, 12.f})
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
            const auto labelRect =
                juce::Rectangle{SST_STRING_WIDTH_INT(font, dbString), labelHeight}
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
            const auto yDraw = dbToY(-6, height);

            plotPath.startNewSubPath(0, yDraw);
            plotPath.lineTo(dRect.getX() + width, yDraw);
        }
        else
        {
            for (int i = 0; i < nPoints; ++i)
            {
                if (freqAxis[i] < GRAPH_MIN_FREQ / 2.f || freqAxis[i] > GRAPH_MAX_FREQ * 1.01f)
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

        const auto cg = juce::ColourGradient::vertical(
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

    {
        auto gs = juce::Graphics::ScopedSaveState(g);

        g.addTransform(juce::AffineTransform().translated(dRect.getX(), dRect.getY()));

        // draws the ruler and a point to show the position of cutoff frequency and resonance
        {
            const double freq = std::pow(2, (evaluator->cutoff) / 12) * 440.0;
            const auto xPos = freqToX(std::min(freq, 25000.0), width);
            const int yPos = height - evaluator->resonance * height;
            const float r = width * 0.0175f;

            g.setColour(skin->getColor(Colors::MSEGEditor::Curve).withMultipliedAlpha(0.666));
            g.drawVerticalLine(xPos, 0.f, height);
            g.drawHorizontalLine(yPos, 0.f, width);

            hotzone = juce::Rectangle<float>(xPos - r / 2.f, yPos - r / 2.f, r, r);

            g.setColour(skin->getColor(isPressed ? Colors::MSEGEditor::CurveHighlight
                                                 : Colors::MSEGEditor::Curve));
            g.fillEllipse(hotzone);
        }
    }

    const auto txtr = lb.withHeight(15);

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

void FilterAnalysis::mouseDrag(const juce::MouseEvent &event)
{
    auto dRect = getLocalBounds().transformedBy(getTransform().inverted());

    float rx0 = dRect.getX();
    float rx1 = dRect.getX() + dRect.getWidth() - 1;
    float ry0 = dRect.getY();
    float ry1 = dRect.getY() + dRect.getHeight() - 1;

    juce::Point<float> mousePoint =
        event.getPosition()
            .transformedBy(
                juce::AffineTransform().translated(dRect.getX(), dRect.getY()).inverted())
            .toFloat();

    mousePoint.setX(std::clamp(mousePoint.getX(), rx0, rx1));
    mousePoint.setY(std::clamp(mousePoint.getY(), ry0, ry1));

    if (event.mods.isLeftButtonDown() && dRect.contains(mousePoint.toInt()))
    {
        auto &ss = editor->getPatch().scene[editor->current_scene];
        auto &fs = ss.filterunit[whichFilter];

        auto width = dRect.getWidth();
        auto height = dRect.getHeight();

        auto xNorm = mousePoint.x / (float)width;
        auto freq = std::pow(GRAPH_MAX_FREQ / GRAPH_MIN_FREQ, xNorm) * GRAPH_MIN_FREQ;

        float cutoff =
            limit_range(12.0f * std::log2(freq / 440.0f), fs.cutoff.val_min.f, fs.cutoff.val_max.f);
        float resonance = limit01((height - mousePoint.y) / (float)height);
        float f = fs.cutoff.value_to_normalized(cutoff);

        fs.cutoff.val.f = cutoff;
        fs.resonance.val.f = resonance;

        // assert if filter cutoff or resonance params are not assigned in SurgeGUIEditor.cpp
        jassert(editor->filterCutoffSlider[whichFilter] != nullptr);
        jassert(editor->filterResonanceSlider[whichFilter] != nullptr);

        editor->filterCutoffSlider[whichFilter]->asControlValueInterface()->setValue(f);
        editor->filterCutoffSlider[whichFilter]->setQuantitizedDisplayValue(f);
        editor->filterCutoffSlider[whichFilter]->asJuceComponent()->repaint();

        editor->filterResonanceSlider[whichFilter]->asControlValueInterface()->setValue(resonance);
        editor->filterResonanceSlider[whichFilter]->setQuantitizedDisplayValue(resonance);
        editor->filterResonanceSlider[whichFilter]->asJuceComponent()->repaint();

        repushData();

        long tag = editor->filterCutoffSlider[whichFilter]->asControlValueInterface()->getTag();
        int ptag = tag - start_paramtags;
        SurgeSynthesizer::ID ptagid;
        synth->fromSynthSideId(ptag, ptagid);
        synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));

        tag = editor->filterResonanceSlider[whichFilter]->asControlValueInterface()->getTag();
        ptag = tag - start_paramtags;
        synth->fromSynthSideId(ptag, ptagid);
        synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));
    }
}

// mouseDrag doesn't catch cases when we just click with the left mouse button,
// mouseDrag only catches movements holding the button. So let's catch it here.
// this is basically to be able to instantly change filter cutoff and resonance
// just by clicking anywhere in the graph, no mouse movement required
void FilterAnalysis::mouseDown(const juce::MouseEvent &event)
{
    auto dRect = getLocalBounds().transformedBy(getTransform().inverted());
    auto where = event.position;
    const bool withinHotzone = hotzone.contains(where.translated(-dRect.getX(), -dRect.getY()));

    if (dRect.contains(where.toInt()))
    {

        isPressed = true;
        hideCursor(where.toInt());
        editor->filterCutoffSlider[whichFilter]->asControlValueInterface();

        long cutoffTag =
            editor->filterCutoffSlider[whichFilter]->asControlValueInterface()->getTag();
        int cutoffPTag = cutoffTag - start_paramtags;

        long resTag =
            editor->filterResonanceSlider[whichFilter]->asControlValueInterface()->getTag();
        int resPTag = resTag - start_paramtags;

        auto &ss = editor->getPatch().scene[editor->current_scene];
        auto &fs = ss.filterunit[whichFilter];
        auto &pfg = ss.level_pfg;

        editor->undoManager()->pushFilterAnalysisMovement(cutoffPTag, &fs.cutoff, resPTag,
                                                          &fs.resonance);

        mouseDrag(event);
    }
}

void FilterAnalysis::mouseUp(const juce::MouseEvent &event)
{
    auto dRect = getLocalBounds().transformedBy(getTransform().inverted());

    setMouseCursor(juce::MouseCursor::NormalCursor);

    if (dRect.contains(event.position.toInt()))
    {
        isPressed = false;
    }

    if (cursorHidden)
    {
        showCursorAt(hotzone.getCentre().translated(dRect.getX(), dRect.getY()).toInt());
    }

    guaranteeCursorShown();
}

void FilterAnalysis::mouseMove(const juce::MouseEvent &event)
{
    auto dRect = getLocalBounds().transformedBy(getTransform().inverted());
    bool reset = false;
    const bool withinHotzone =
        hotzone.contains(event.position.translated(-dRect.getX(), -dRect.getY()));

    if (withinHotzone != isHovered)
    {
        isHovered = withinHotzone;
        repaint();
    }

    if (isHovered)
    {
        setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
        reset = true;
    }

    if (!reset)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

} // namespace Overlays
}; // namespace Surge
