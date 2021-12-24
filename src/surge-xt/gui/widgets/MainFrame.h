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

#include "SurgeImage.h"
#include "Parameter.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Widgets
{
struct MainFrame : public juce::Component
{
    MainFrame();
    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e) { editor = e; }

    SurgeImage *bg{nullptr};
    void setBackground(SurgeImage *d)
    {
        bg = d;
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        if (bg)
            bg->draw(g, 1.0);
    }

    bool debugFocus{false};
    juce::Rectangle<int> focusRectangle;
    void paintOverChildren(juce::Graphics &g) override
    {
        if (!debugFocus)
            return;
        if (focusRectangle.getWidth() > 0 && focusRectangle.getHeight() > 0)
        {
            g.setColour(juce::Colours::red);
            g.drawRect(focusRectangle.expanded(1), 2);
            g.setColour(juce::Colours::yellow.withAlpha(0.1f));
            g.fillRect(focusRectangle);
        }
    }

    void resized() override
    {
        for (auto &c : cgOverlays)
            if (c)
                c->setBounds(getLocalBounds());
    }

    void clearGroupLayers()
    {
        modGroup.reset(nullptr);
        for (auto &c : cgOverlays)
            c.reset(nullptr);
    }

    juce::Component *getControlGroupLayer(ControlGroup cg);
    juce::Component *getModButtonLayer();
    juce::Component *getSynthControlsLayer();

#if SURGE_JUCE_ACCESSIBLE
    std::unique_ptr<juce::ComponentTraverser> createFocusTraverser() override;
#endif

    void mouseDown(const juce::MouseEvent &event) override;

    struct OverlayComponent : public juce::Component
    {
#if SURGE_JUCE_ACCESSIBLE
        OverlayComponent()
        {
            setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
            setAccessible(true);
        }

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
        {
            return std::make_unique<juce::AccessibilityHandler>(*this,
                                                                juce::AccessibilityRole::group);
        }
        std::unique_ptr<juce::ComponentTraverser> createFocusTraverser() override;
#endif
    };

    void addChildComponentThroughEditor(juce::Component &comp);
    std::array<std::unique_ptr<juce::Component>, endCG> cgOverlays;
    std::unique_ptr<juce::Component> modGroup, synthControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainFrame);
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_MAINFRAME_H
