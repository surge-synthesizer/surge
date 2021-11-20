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

#ifndef SURGE_SURGETEXTBUTTON_H
#define SURGE_SURGETEXTBUTTON_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "SkinSupport.h"

namespace Surge
{
namespace Widgets
{
struct SurgeTextButton : public juce::TextButton, public Surge::GUI::SkinConsumingComponent
{
    SurgeTextButton(const juce::String &s) : juce::TextButton(s) {}

    void paint(juce::Graphics &g) override;
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_SURGETEXTBUTTON_H
