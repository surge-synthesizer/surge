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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_LFOANDSTEPDISPLAY_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_LFOANDSTEPDISPLAY_H

#include "WidgetBaseMixin.h"
#include "SurgeStorage.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Overlays
{
struct TypeinLambdaEditor;
}
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

    void setZoomFactor(int);
    int zoomFactor{100};
    std::unique_ptr<juce::Image> backingImage;
    bool waveformIsUpdated{false};
    bool forceRepaint{false};
    LFOStorage *lfoStorageFromLastDrawingCall{nullptr};
    pdata paramsFromLastDrawCall[n_scene_params];
    pdata settingsFromLastDrawCall[8];
    int zoomFactorFromLastDrawCall{-1};

    bool paramsHasChanged();

    void repaintIfIdIsInRange(int id)
    {
        auto *firstLfoParam = &lfodata->rate;
        auto *lastLfoParam = &lfodata->release;

        bool lfoInvalid = false;

        while (firstLfoParam <= lastLfoParam && !lfoInvalid)
        {
            if (firstLfoParam->id == id)
            {
                lfoInvalid = true;
            }

            firstLfoParam++;
        }

        if (lfoInvalid)
        {
            repaint();
        }
    }

    void repaintIfAnythingIsTemposynced()
    {
        if (isAnythingTemposynced())
        {
            forceRepaint = true;
            repaint();
        }
    }
    bool isAnythingTemposynced()
    {
        bool drawBeats = false;
        auto *c = &lfodata->rate;
        auto *e = &lfodata->release;

        while (c <= e && !drawBeats)
        {
            if (c->temposync)
            {
                drawBeats = true;
            }

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

    void showStepRMB(const juce::MouseEvent &);
    void showStepRMB(int step);

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

    void showStepTypein(int i);

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

    std::unique_ptr<Surge::Overlays::TypeinLambdaEditor> stepEditor;

    SurgeGUIEditor *guiEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOAndStepDisplay);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_LFOANDSTEPDISPLAY_H
