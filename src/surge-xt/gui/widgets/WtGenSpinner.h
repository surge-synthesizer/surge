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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_WTGENSPINNER_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_WTGENSPINNER_H

#include "juce_gui_basics/juce_gui_basics.h"

#include <functional>

namespace Surge
{
namespace Widgets
{

struct WtGenSpinner : juce::Component, juce::Timer
{
    static constexpr float radiusScale = 0.15f;
    static constexpr float strokeWidth = 4.0f;
    static constexpr float trackWidth = 1.5f;

    std::function<bool()> isGenerating;           // parent-supplied: still generating?
    juce::Colour arcColour{juce::Colours::white}; // accent colour, set by the parent
    float phase{0.f};                             // 0..1 animation phase

    // Bounding box of the circle, so timerCallback can repaint just this region instead of the
    // whole component.
    juce::Rectangle<int> arcBounds() const
    {
        const float half = arcRadius() + strokeWidth;
        return juce::Rectangle<float>{half * 2.f, half * 2.f}
            .withCentre(getLocalBounds().toFloat().getCentre())
            .getSmallestIntegerContainer();
    }

    float arcRadius() const { return juce::jmin(getWidth(), getHeight()) * radiusScale; }

    WtGenSpinner()
    {
        setInterceptsMouseClicks(false, false);
        setAccessible(false);
    }

    // Show + start animating.
    void kick()
    {
        if (!isVisible())
        {
            setVisible(true);
            toFront(false);
        }
        if (!isTimerRunning())
        {
            startTimerHz(30);
        }
    }

    void timerCallback() override
    {
        if (!isGenerating || !isGenerating())
        {
            setVisible(false);
            stopTimer();
            return;
        }
        phase += 0.01f;
        if (phase > 1.f)
        {
            phase -= 1.f;
        }
        repaint(arcBounds());
    }

    void paint(juce::Graphics &g) override
    {
        const auto centre = getLocalBounds().toFloat().getCentre();
        const float r = arcRadius();

        const float rotation = phase * juce::MathConstants<float>::twoPi * 2.0f;
        const float sweepPhase = phase * juce::MathConstants<float>::twoPi;
        const float sweep =
            juce::jmap(0.5f - 0.5f * std::cos(sweepPhase), juce::MathConstants<float>::pi * 0.25f,
                       juce::MathConstants<float>::pi * 1.5f);

        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, r, r, 0.0f, rotation, rotation + sweep, true);

        g.setColour(arcColour.withAlpha(0.1f));
        g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, trackWidth);

        g.setColour(arcColour);
        g.strokePath(arc, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_WIDGETS_WTGENSPINNER_H
