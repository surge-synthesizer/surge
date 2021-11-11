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

#ifndef SURGE_XT_COVERINGMESSAGEOVERLAY_H
#define SURGE_XT_COVERINGMESSAGEOVERLAY_H

#include "juce_gui_basics/juce_gui_basics.h"
namespace Surge
{
namespace Overlays
{
struct CoveringMessageOverlay : public juce::Component
{
    CoveringMessageOverlay() : juce::Component("CoveringMessage") {}
    std::string pt, et;
    void setPrimaryTitle(const std::string &msg)
    {
        pt = msg;
        repaint();
    }
    void setExplanatoryText(const std::string &txt)
    {
        et = txt;
        repaint();
    }

    void paint(juce::Graphics &g) override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoveringMessageOverlay);
};

struct AudioEngineNotRunningOverlay : public CoveringMessageOverlay
{
    AudioEngineNotRunningOverlay() : CoveringMessageOverlay()
    {
        setPrimaryTitle("AUDIO ENGINE NOT RUNNING");
        setExplanatoryText(
            R"MSG(Surge XT requires the audio engine to be running in order for the GUI to
interact with the synth. A running audio engine is required to provide several
core features, including selecting oscillator or FX types and loading patches.

This instance of Surge does not have a running audio engine. Either the audio engine
in your DAW is suspended, your instance of Surge is suspended, or in the standalone
application you haven't connected your output to at least one stereo output bus.

To dismiss this message, start your audio engine, unsuspend this instance of Surge,
or set up output routing in standalone application appropriately.
)MSG");

#if SURGE_JUCE_ACCESSIBLE
        setTitle(pt);
        setDescription(pt);
        setAccessible(true);
#endif
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngineNotRunningOverlay);
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_COVERINGMESSAGEOVERLAY_H
