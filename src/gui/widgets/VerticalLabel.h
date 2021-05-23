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

#ifndef SURGE_XT_VERTICAL_LABEL_H
#define SURGE_XT_VERTICAL_LABEL_H

#include <JuceHeader.h>
#include <string>

namespace Surge
{
namespace Widgets
{
/*
 * This is a pure play juce component which is refered to as such so doesn't
 * need any fo the EFVG and Mixin stuff to interact with the value callback mechanism
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

    juce::Font font;
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
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_VERTICAL_LABEL_H
