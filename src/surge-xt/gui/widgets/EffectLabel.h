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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_EFFECTLABEL_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_EFFECTLABEL_H

#include "SkinSupport.h"
#include "RuntimeFont.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <string>

namespace Surge
{
namespace Widgets
{
struct EffectLabel : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    EffectLabel()
    {
        setAccessible(false); // the param full name contains what we need
        setInterceptsMouseClicks(false, false);
    }

    ~EffectLabel() = default;

    std::string label;
    void setLabel(const std::string &l)
    {
        label = l;
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        auto lb = getLocalBounds().withTrimmedTop(getHeight() - 2);
        g.setColour(skin->getColor(Colors::Effect::Label::Separator));
        g.fillRect(lb);

        g.setColour(skin->getColor(Colors::Effect::Label::Text));
        g.setFont(skin->getFont(Fonts::Widgets::EffectLabel));
        g.drawText(label, getLocalBounds(), juce::Justification::centredLeft);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectLabel);
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_EFFECTLABEL_H
