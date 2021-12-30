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

#ifndef SURGE_XT_EFFECTLABEL_H
#define SURGE_XT_EFFECTLABEL_H

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
