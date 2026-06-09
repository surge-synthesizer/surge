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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_TOOLTIP_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_TOOLTIP_H

#include "SkinSupport.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{
struct Tooltip : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    Tooltip() {}
    void paint(juce::Graphics &g) override;

    void positionForText(const juce::Point<int> &centerPoint, const std::string &s,
                         const int maxTooltipWidth,
                         const juce::Justification customAlignment = juce::Justification::left);

    std::string text;
    juce::Justification alignment{juce::Justification::left};

  protected:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Tooltip);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_TOOLTIP_H
