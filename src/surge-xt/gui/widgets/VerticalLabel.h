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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_VERTICALLABEL_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_VERTICALLABEL_H

#include "juce_gui_basics/juce_gui_basics.h"

#include <string>

namespace Surge
{
namespace Widgets
{
/*
 * This is a pure play juce component which is referred to as such so doesn't
 * need any for the EFVG and Mixin stuff to interact with the value callback mechanism
 */
struct VerticalLabel : public juce::Component
{
    VerticalLabel();
    ~VerticalLabel();

    void paint(juce::Graphics &g) override;
    std::string text;
    void setText(const std::string &t)
    {
        text = t;
        repaint();
    }

    juce::Font font{juce::FontOptions()};
    void setFont(const juce::Font &f)
    {
        font = f;
        repaint();
    }

    juce::Colour fontColour;
    void setFontColour(const juce::Colour &f)
    {
        fontColour = f;
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VerticalLabel);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_VERTICAL_LABEL_H
