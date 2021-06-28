/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_LFOANDSTEPDISPLAY_H
#define SURGE_XT_LFOANDSTEPDISPLAY_H

#include <JuceHeader.h>
#include "WidgetBaseMixin.h"
#include "SurgeStorage.h"

namespace Surge
{
namespace Widgets
{
struct LFOAndStepDisplay : public juce::Component, public WidgetBaseMixin<LFOAndStepDisplay>
{
    LFOAndStepDisplay();
    void paint(juce::Graphics &g) override;
    void paintWaveform(juce::Graphics &g, const juce::Rectangle<int> &within);
    void paintStepSeq(juce::Graphics &g, const juce::Rectangle<int> &within);
    void paintTypeSelector(juce::Graphics &g, const juce::Rectangle<int> &within);

    float value;
    float getValue() const override { return value; }
    void setValue(float v) override { value = v; }

    bool isStepSequencer() { return lfodata->shape.val.i == lt_stepseq; }
    bool isMSEG() { return lfodata->shape.val.i == lt_mseg; }
    bool isFormula() { return lfodata->shape.val.i == lt_formula; }
    bool isUnipolar() { return lfodata->unipolar.val.b; }
    void invalidateIfIdIsInRange(int j) {}
    void invalidateIfAnythingIsTemposynced()
    {
        if (isAnythingTemposynced())
            repaint();
    }
    bool isAnythingTemposynced()
    {
        bool drawBeats = false;
        auto *c = &lfodata->rate;
        auto *e = &lfodata->release;
        while (c <= e && !drawBeats)
        {
            if (c->temposync)
                drawBeats = true;
            ++c;
        }
        return drawBeats;
    }

    int tsNum{4}, tsDen{4};
    void setTimeSignature(int num, int den)
    {
        tsNum = num;
        tsDen = den;
        repaint();
    }

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }
    LFOStorage *lfodata{nullptr};
    void setLFOStorage(LFOStorage *s) { lfodata = s; }

    StepSequencerStorage *ss{nullptr};
    void setStepSequencerStorage(StepSequencerStorage *s) { ss = s; }

    MSEGStorage *ms{nullptr};
    void setMSEGStorage(MSEGStorage *s) { ms = s; }

    FormulaModulatorStorage *fs{nullptr};
    void setFormulaStorage(FormulaModulatorStorage *s) { fs = s; }

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    enum DragMode
    {
        NONE,
        ARROW,
        LOOP_START,
        LOOP_END,
        TRIGGERS,
        VALUES
    } dragMode{NONE};
    int draggedStep{-1};
    int keyModMult;
    uint64_t dragTrigger0;
    juce::Point<float> arrowStart, arrowEnd;

    bool edit_trigmask{false};
    void setCanEditEnvelopes(bool b) { edit_trigmask = b; }

    void onSkinChanged() override;

    juce::Drawable *typeImg{nullptr}, *typeImgHover{nullptr}, *typeImgHoverOn{nullptr};

    const static int margin = 2;
    const static int margin2 = 7;
    const static int lpsize = 50;
    const static int scale = 18;
    const static int splitpoint = lpsize + 20;

    std::array<juce::Rectangle<int>, n_lfo_types> shaperect;
    std::array<juce::Rectangle<float>, n_stepseqsteps> steprect, gaterect;
    juce::Rectangle<float> loopStartRect, loopEndRect, rect_steps, rect_steps_retrig;
    juce::Rectangle<float> ss_shift_left, ss_shift_right;
    int lfoTypeHover{-1};
    int ss_shift_hover{-1};
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_LFOANDSTEPDISPLAY_H
