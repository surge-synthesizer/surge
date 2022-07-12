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

#include "WidgetBaseMixin.h"
#include "SurgeStorage.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{
struct LFOAndStepDisplay : public juce::Component,
                           public WidgetBaseMixin<LFOAndStepDisplay>,
                           public LongHoldMixin<LFOAndStepDisplay>
{
    LFOAndStepDisplay(SurgeGUIEditor *e);
    void paint(juce::Graphics &g) override;
    void paintWaveform(juce::Graphics &g);
    void paintStepSeq(juce::Graphics &g);
    void paintTypeSelector(juce::Graphics &g);
    void resized() override;

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

    int lfoid{-1};
    void setLFOID(int l) // from 0
    {
        lfoid = l;
    }
    int scene{-1};
    void setScene(int l) // from 0
    {
        scene = l;
    }

    void populateLFOMS(LFOModulationSource *s);

    void setStepToDefault(const juce::MouseEvent &event);
    void setStepValue(const juce::MouseEvent &event);

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
    modsources modsource;
    void setModSource(modsources m) { modsource = m; }
    int modIndex{0};
    void setModIndex(int m) { modIndex = m; }

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
    void mouseExit(const juce::MouseEvent &event) override;

    void endHover() override
    {
        if (stuckHover)
            return;

        lfoTypeHover = -1;
        stepSeqShiftHover = -1;
        overWaveform = false;
        repaint();
    }

    bool isCurrentlyHovered() override
    {
        return (lfoTypeHover >= 0) || (stepSeqShiftHover >= 0) || overWaveform;
    }

    void focusLost(juce::Component::FocusChangeType cause) override { endHover(); }

    struct BeginStepGuard
    {
        LFOAndStepDisplay *that{nullptr};
        BeginStepGuard(LFOAndStepDisplay *t) : that(t) { that->prepareForEdit(); }
        ~BeginStepGuard() { that->postDirtyEdit(); }
    };
    void prepareForEdit();
    void postDirtyEdit()
    {
        jassert(stepDirtyCount == 1);
        stepDirtyCount--;
    }
    void stepSeqDirty();

    int stepDirtyCount{0};
    StepSequencerStorage undoStorageCopy;

    enum DragMode
    {
        NONE,
        ARROW,
        LOOP_START,
        LOOP_END,
        RESET_VALUE,
        TRIGGERS,
        VALUES,
    } dragMode{NONE};

    int draggedStep{-1};
    int keyModMult;
    uint64_t dragTrigger0;
    juce::Point<float> arrowStart, arrowEnd;

    bool edit_trigmask{false};
    void setCanEditEnvelopes(bool b) { edit_trigmask = b; }

    void showLFODisplayPopupMenu(SurgeGUIEditor::OverlayTags tag);

    void onSkinChanged() override;

    bool keyPressed(const juce::KeyPress &key) override;

    SurgeImage *typeImg{nullptr}, *typeImgHover{nullptr}, *typeImgHoverOn{nullptr};

    const static int margin = 2;
    const static int margin2 = 7;
    const static int lpsize = 50;
    const static int scale = 18;
    const static int splitpoint = lpsize + 20;

    std::array<juce::Rectangle<int>, n_lfo_types> shaperect;
    std::array<juce::Rectangle<float>, n_stepseqsteps> steprect, gaterect;

    juce::Rectangle<int> outer, left_panel, main_panel, waveform_display;
    juce::Rectangle<float> loopStartRect, loopEndRect, rect_steps, rect_steps_retrig;
    juce::Rectangle<float> ss_shift_bg, ss_shift_left, ss_shift_right;

    int lfoTypeHover{-1};
    int stepSeqShiftHover{-1};
    bool overWaveform{false};
    void enterExitWaveform(bool isInWF);
    void updateShapeTo(int i);
    void setupAccessibility();

    void shiftLeft(), shiftRight();

    std::unique_ptr<juce::Component> typeLayer, stepLayer;
    std::array<std::unique_ptr<juce::Component>, n_lfo_types> typeAccOverlays;
    std::array<std::unique_ptr<juce::Component>, 2> stepJogOverlays;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::array<std::unique_ptr<juce::Component>, n_stepseqsteps> stepSliderOverlays;
    std::array<std::unique_ptr<juce::Component>, n_stepseqsteps> stepTriggerOverlays;
    std::array<std::unique_ptr<juce::Component>, 2> loopEndOverlays;

    SurgeGUIEditor *guiEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOAndStepDisplay);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_LFOANDSTEPDISPLAY_H
