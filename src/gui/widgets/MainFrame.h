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

#ifndef SURGE_XT_MAINFRAME_H
#define SURGE_XT_MAINFRAME_H

#include <JuceHeader.h>
#include <efvg/escape_from_vstgui.h>

class SurgeGUIEditor;

namespace Surge
{
namespace Widgets
{
struct MainFrame : public juce::Component
{
    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e) { editor = e; }

    juce::Drawable *bg{nullptr};
    void setBackground(juce::Drawable *d)
    {
        bg = d;
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        if (bg)
            bg->draw(g, 1.0);
    }

    void mouseDown(const juce::MouseEvent &event) override;

    /*
     * Kill these before I'm done
     */
    void localToFrame(VSTGUI::CPoint &p) { UNIMPL; };
    void setZoom(float z) { OKUNIMPL; }
    void getPosition(float &x, float &y)
    {
        auto b = getScreenPosition();
        x = b.x;
        y = b.y;
    }
    void getCurrentMouseLocation(VSTGUI::CPoint &w)
    {
        auto pq = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
        auto b = getLocalPoint(nullptr, pq);
        w.x = b.x;
        w.y = b.y;
    }
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_MAINFRAME_H
