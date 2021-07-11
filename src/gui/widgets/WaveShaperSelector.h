//
// Created by Paul Walker on 7/10/21.
//

#ifndef SURGE_XT_WAVESHAPERSELECTOR_H
#define SURGE_XT_WAVESHAPERSELECTOR_H

#include <JuceHeader.h>
#include "WidgetBaseMixin.h"
#include "Parameter.h"
#include "SurgeStorage.h"
#include "SurgeJUCEHelpers.h"

namespace Surge
{
namespace Widgets
{
struct WaveShaperSelector : public juce::Component, public WidgetBaseMixin<WaveShaperSelector>
{
    void paint(juce::Graphics &g) override;

    float value;
    int iValue;
    float getValue() const override { return value; }
    void setValue(float f) override
    {
        value = f;
        iValue = Parameter::intUnscaledFromFloat(value, n_ws_types - 1, 0);
        repaint();
    }

    void resized() override;

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;

    float lastDragDistance{0};
    bool everDragged{false};

    std::vector<int> intOrdering;
    void setIntOrdering(const std::vector<int> &io) { intOrdering = io; }

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w) override;
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;
    float nextValueInOrder(float v, int inc);

    juce::Rectangle<int> buttonM, buttonP;
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_WAVESHAPERSELECTOR_H
