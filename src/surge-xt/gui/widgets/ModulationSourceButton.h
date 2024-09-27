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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MODULATIONSOURCEBUTTON_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MODULATIONSOURCEBUTTON_H

#include "WidgetBaseMixin.h"
#include "ModulationSource.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <string>
#include "SurgeJUCEHelpers.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct ModulationSourceButton : public juce::Component,
                                public WidgetBaseMixin<ModulationSourceButton>,
                                public LongHoldMixin<ModulationSourceButton>
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

    void setAccessibleLabel(const std::string &s)
    {
#if MAC
        setDescription(std::string("Modulator ") + s);
        setTitle(s);
#else
        setDescription("Modulator");
        setTitle(s);
#endif
    }

    modsources getCurrentModSource() const { return std::get<0>(modlist[modlistIndex]); }
    int getCurrentModIndex() const { return std::get<1>(modlist[modlistIndex]); }
    std::string getCurrentModLabel() const { return std::get<2>(modlist[modlistIndex]); }

    void setCurrentModLabel(const std::string &s)
    {
        jassert(modlist.size() == 1 && modlistIndex == 0);
        std::get<2>(modlist[modlistIndex]) = s;
        repaint();
    }

    typedef std::vector<std::tuple<modsources, int, std::string, std::string>> modlist_t;
    modlist_t modlist;
    int modlistIndex{0};
    void setModList(const modlist_t &m)
    {
        modlistIndex = limit_range(modlistIndex, 0, (int)(m.size() - 1));
        modlist = m;
        setAccessibleLabel(getCurrentModLabel());
        selectAccButton->setVisible(true);
        if (isLFO())
        {
            targetAccButton->setVisible(true);
        }
    }
    bool isMeta{false}, isBipolar{false};
    void setIsMeta(bool b) { isMeta = b; }
    void setIsBipolar(bool b);

    bool containsModSource(modsources ms)
    {
        for (auto m : modlist)
            if (std::get<0>(m) == ms)
                return true;
        return false;
    }

    juce::Rectangle<int> hamburgerHome;
    bool needsHamburger() const { return modlist.size() > 1; }

    bool isLFO() const
    {
        for (auto m : modlist)
        {
            if (std::get<0>(m) >= ms_lfo1 && std::get<0>(m) <= ms_slfo6)
            {
                return true;
            }
        }
        return false;
    }
    bool isUsed{false};
    void setUsed(bool b) { isUsed = b; }

    int state{0};

    void setState(int s)
    {
        state = s;
        if ((state & 3) == 2)
        {
            toggleArmAccButton->setTitle("Disarm");
            toggleArmAccButton->setDescription("Disarm");
        }
        else
        {
            toggleArmAccButton->setTitle("Arm");
            toggleArmAccButton->setDescription("Arm");
        }
        if (auto *h = toggleArmAccButton->getAccessibilityHandler())
        {
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
    }
    int getState() const { return state; }
    bool transientArmed{false}; // armed in drop state
    int newlySelected{0};

    bool secondaryHoverActive{false};

    void setSecondaryHover(bool b)
    {
        bool os = secondaryHoverActive;
        secondaryHoverActive = b;
        if (b != os)
            repaint();
    }

    juce::Font font{juce::FontOptions()};

    void setFont(const juce::Font &f)
    {
        font = f;
        repaint();
    }

    bool isHovered{false};

    bool isCurrentlyHovered() override { return isHovered; }

    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;
    void startHover(const juce::Point<float> &f) override;
    void endHover() override;
    bool keyPressed(const juce::KeyPress &key) override;

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        startHover(getBounds().getBottomLeft().toFloat());
    }

    void focusLost(juce::Component::FocusChangeType cause) override { endHover(); }

    void onSkinChanged() override;

    bool isTinted{false};

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

    SurgeImage *arrow{nullptr};

    enum MouseState
    {
        NONE,
        CLICK,
        CLICK_TOGGLE_ARM,
        CLICK_SELECT_ONLY,
        CLICK_ARROW,
        CTRL_CLICK,
        PREDRAG_VALUE,
        DRAG_VALUE,
        DRAG_COMPONENT_HAPPEN,
        HAMBURGER
    } mouseMode{NONE};
    juce::Point<float> mouseDownLocation;

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

    void buildHamburgerMenu(juce::PopupMenu &menu, const bool addedToModbuttonContextMenu);

    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;

    void resized() override;

    std::unique_ptr<juce::Component> targetAccButton, selectAccButton, toggleArmAccButton,
        macroSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationSourceButton);
};

struct ModulationOverviewLaunchButton : public juce::Button,
                                        juce::Button::Listener,
                                        Surge::GUI::SkinConsumingComponent,
                                        Surge::GUI::Hoverable

{
    ModulationOverviewLaunchButton(SurgeGUIEditor *ed, SurgeStorage *s)
        : juce::Button("Open Modulation Overview"), editor(ed), storage(s)
    {
        addListener(this);
    }

    void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

    void buttonClicked(Button *button) override;

    void mouseDown(const juce::MouseEvent &event) override;
    bool keyPressed(const juce::KeyPress &key) override;

    bool isH{false};

    void mouseEnter(const juce::MouseEvent &event) override
    {
        isH = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent &event) override
    {
        isH = false;
        repaint();
    }

    bool isCurrentlyHovered() override { return isH; }

    SurgeGUIEditor *editor{nullptr};
    SurgeStorage *storage{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationOverviewLaunchButton);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MODULATIONSOURCEBUTTON_H
