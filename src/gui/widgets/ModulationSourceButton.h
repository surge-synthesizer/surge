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

#ifndef SURGE_XT_MODULATIONSOURCEBUTTON_H
#define SURGE_XT_MODULATIONSOURCEBUTTON_H

#include <JuceHeader.h>
#include "WidgetBaseMixin.h"
#include "ModulationSource.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct ModulationSourceButton : public juce::Component,
                                public WidgetBaseMixin<ModulationSourceButton>
{
    ModulationSourceButton();

    void paint(juce::Graphics &g) override;

    float value{0};

    void setValue(float v) override
    {
        value = v;
        repaint();
    }
    float getValue() const override { return value; }

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    std::string label;
    void setLabel(const std::string &s) { label = s; }

    modsources modSource;
    void setModSource(modsources ms) { modSource = ms; }
    bool isMeta{false}, isBipolar{false};
    void setIsMeta(bool b) { isMeta = b; }
    void setBipolar(bool b) { isBipolar = b; }

    bool hasAlternate{false}, useAlternate{false};
    modsources alternateSource{ms_original};
    std::string alternateLabel;

    void setAlternate(modsources alt, const std::string &s)
    {
        alternateSource = alt;
        alternateLabel = s;
        hasAlternate = true;
    }

    modsources getAlternate() { return alternateSource; }
    bool getHasAlternate() { return hasAlternate; }
    void setUseAlternate(bool b) { useAlternate = b; }
    bool getUseAlternate() const { return useAlternate; }

    bool isUsed{false};

    void setUsed(bool b) { isUsed = b; }

    int state{0};

    void setState(int s) { state = s; }
    int getState() const { return state; }

    bool secondaryHoverActive{false};

    void setSecondaryHover(bool b)
    {
        bool os = secondaryHoverActive;
        secondaryHoverActive = b;
        if (b != os)
            repaint();
    }

    bool isHovered{false};

    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    void onSkinChanged() override;

    bool isTinted;

    void update_rt_vals(bool t, int, bool)
    {
        if (t != isTinted)
        {
            isTinted = t;
            repaint();
        }
    }

    bool everDragged{false};

    static constexpr int splitHeight = 14;

    juce::Drawable *arrow{nullptr};

    enum MouseState
    {
        NONE,
        CLICK,
        CLICK_ARROW,
        DRAG_VALUE,
        DRAG_COMPONENT_HAPPEN
    } mouseMode{NONE};
    MouseState getMouseMode() const { return mouseMode; }
    juce::ComponentDragger componentDragger;
    juce::Rectangle<int> mouseDownBounds;
    float valAtMouseDown{0};
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MODULATIONSOURCEBUTTON_H
