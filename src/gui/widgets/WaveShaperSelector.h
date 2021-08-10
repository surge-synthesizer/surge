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

    void toggleAnalysis();
    void closeAnalysis();
    void openAnalysis();
    std::unique_ptr<WaveShaperAnalysisWidget> analysisWidget;

    void resized() override;

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;

    void parentHierarchyChanged() override;

    float lastDragDistance{0};
    bool everDragged{false};

    std::vector<int> intOrdering;
    void setIntOrdering(const std::vector<int> &io) { intOrdering = io; }

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w) override;
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;
    float nextValueInOrder(float v, int inc);

    juce::Rectangle<int> buttonM, buttonP, buttonA, waveArea;

    static std::array<std::vector<std::pair<float, float>>, n_ws_types> wsCurves;

#if SURGE_JUCE_ACCESSIBLE
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveShaperSelector);
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_WAVESHAPERSELECTOR_H
