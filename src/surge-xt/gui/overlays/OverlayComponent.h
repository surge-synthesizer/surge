/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_OVERLAYCOMPONENT_H
#define SURGE_XT_OVERLAYCOMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Overlays
{
struct OverlayComponent : juce::Component
{
    OverlayComponent() : juce::Component() {}
    OverlayComponent(const juce::String &s) : juce::Component(s) {}
    juce::Rectangle<int> enclosingParentPosition;
    void setEnclosingParentPosition(const juce::Rectangle<int> &e) { enclosingParentPosition = e; }
    juce::Rectangle<int> getEnclosingParentPosition() { return enclosingParentPosition; }

    std::string enclosingParentTitle{"Overlay"};
    void setEnclosingParentTitle(const std::string &s) { enclosingParentTitle = s; }
    std::string getEnclosingParentTitle() { return enclosingParentTitle; }

    bool hasIndependentClose{true};
    void setHasIndependentClose(bool b) { hasIndependentClose = b; }
    bool getHasIndependentClose() { return hasIndependentClose; }
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_OVERLAYCOMPONENT_H
