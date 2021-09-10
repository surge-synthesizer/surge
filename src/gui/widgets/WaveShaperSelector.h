//
// Created by Paul Walker on 7/10/21.
//

#ifndef SURGE_XT_WAVESHAPERSELECTOR_H
#define SURGE_XT_WAVESHAPERSELECTOR_H

#include "WidgetBaseMixin.h"
#include "Parameter.h"
#include "SurgeStorage.h"
#include "SurgeJUCEHelpers.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct WaveShaperAnalysisWidget;
struct WaveShaperSelector : public juce::Component, public WidgetBaseMixin<WaveShaperSelector>
{
    WaveShaperSelector();
    ~WaveShaperSelector();
    void paint(juce::Graphics &g) override;

    float value;
    int iValue;
    float getValue() const override { return value; }
    void setValue(float f) override;

    bool isAnalysisOpen();
    void toggleAnalysis();
    void closeAnalysis();
    void openAnalysis();
    std::unique_ptr<WaveShaperAnalysisWidget> analysisWidget;

    void resized() override;

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override
    {
        bool shouldH = false;
        if (labelArea.contains(event.position.toInt()))
            shouldH = true;
        if (shouldH != isLabelHovered)
        {
            isLabelHovered = shouldH;
            repaint();
        }
        bool shouldWH = false;
        if (waveArea.contains(event.position.toInt()))
            shouldWH = true;
        if (shouldWH != isWaveHovered)
        {
            isWaveHovered = shouldWH;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent &event) override { endHover(); }

    void endHover() override
    {
        isWaveHovered = false;
        isLabelHovered = false;
        repaint();
    }

    void jog(int by);

    void parentHierarchyChanged() override;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    float lastDragDistance{0};
    bool everDragged{false};

    std::vector<int> intOrdering;
    void setIntOrdering(const std::vector<int> &io) { intOrdering = io; }

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w) override;
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;
    float nextValueInOrder(float v, int inc);

    juce::Rectangle<int> waveArea, labelArea;
    static constexpr int labelHeight = 13;

    static std::array<std::vector<std::pair<float, float>>, n_ws_types> wsCurves;

    SurgeImage *bg{nullptr}, *bgHover{nullptr};
    void onSkinChanged() override;
    bool isLabelHovered{false}, isWaveHovered{false};

#if SURGE_JUCE_ACCESSIBLE
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveShaperSelector);
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_WAVESHAPERSELECTOR_H
