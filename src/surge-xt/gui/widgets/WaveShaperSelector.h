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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_WAVESHAPERSELECTOR_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_WAVESHAPERSELECTOR_H

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
struct WaveShaperSelector : public juce::Component,
                            public WidgetBaseMixin<WaveShaperSelector>,
                            public LongHoldMixin<WaveShaperSelector>
{
    WaveShaperSelector();
    ~WaveShaperSelector();
    void paint(juce::Graphics &g) override;

    float value;
    sst::waveshapers::WaveshaperType iValue;
    float getValue() const override { return value; }
    void setValue(float f) override;

    void resized() override;

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override
    {
        bool shouldH = false;

        if (labelArea.contains(event.position.toInt()))
        {
            shouldH = true;
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        if (shouldH != isLabelHovered)
        {
            isLabelHovered = shouldH;
            repaint();
        }

        bool shouldWH = false;

        if (waveArea.contains(event.position.toInt()))
        {
            shouldWH = true;
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        }

        if (shouldWH != isWaveHovered)
        {
            isWaveHovered = shouldWH;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent &event) override { endHover(); }

    void endHover() override
    {
        if (stuckHover)
            return;

        isWaveHovered = false;
        isLabelHovered = false;

        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void startHover(const juce::Point<float> &f) override
    {
        isWaveHovered = true;
        repaint();
    }

    bool isCurrentlyHovered() override { return isLabelHovered || isWaveHovered; }

    bool keyPressed(const juce::KeyPress &key) override;

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        startHover(getBounds().getBottomLeft().toFloat());
    }

    void focusLost(juce::Component::FocusChangeType cause) override { endHover(); }

    void jog(int by);

    void parentHierarchyChanged() override;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    float lastDragDistance{0};
    bool everDragged{false},
        everMoved{false}; // the first is if we change the second if we move at all

    std::vector<int> intOrdering;
    void setIntOrdering(const std::vector<int> &io) { intOrdering = io; }

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w) override;
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;
    float nextValueInOrder(float v, int inc);

    juce::Rectangle<int> waveArea, labelArea;
    static constexpr int labelHeight = 13;

    static std::array<std::vector<std::pair<float, float>>,
                      (int)sst::waveshapers::WaveshaperType::n_ws_types>
        wsCurves;

    SurgeImage *bg{nullptr}, *bgHover{nullptr};
    void onSkinChanged() override;
    bool isLabelHovered{false}, isWaveHovered{false}, isDeactivated{false};
    void setDeactivated(bool b) { isDeactivated = b; }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveShaperSelector);
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_WAVESHAPERSELECTOR_H
