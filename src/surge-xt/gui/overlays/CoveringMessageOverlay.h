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
#include "SurgeGUIEditor.h"

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
    AudioEngineNotRunningOverlay(SurgeGUIEditor *e) : CoveringMessageOverlay(), editor(e)
    {
        setPrimaryTitle("AUDIO ENGINE NOT RUNNING");
        setExplanatoryText(
            R"MSG(This instance of Surge does not have a running audio engine. Either the audio engine
in your DAW is suspended, your instance of Surge is suspended, or in the standalone
application you haven't connected your output to at least one stereo output bus.

You can click to dismiss this message and the Surge UI will continue to operate, but
no sound will be produced under MIDI input unless you activate your audio system.
)MSG");

#if SURGE_JUCE_ACCESSIBLE
        setTitle(pt);
        setDescription(pt);
        setAccessible(true);
#endif
    }

    void mouseUp(const juce::MouseEvent &e) override { editor->clearNoProcessingOverlay(); }
    SurgeGUIEditor *editor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngineNotRunningOverlay);
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_COVERINGMESSAGEOVERLAY_H
