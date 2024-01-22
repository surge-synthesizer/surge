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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUCUSTOMCOMPONENTS_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUCUSTOMCOMPONENTS_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "SkinSupport.h"

namespace Surge
{
namespace Widgets
{

struct TinyLittleIconButton : public juce::Component
{
    TinyLittleIconButton(int off, std::function<void()> cb) : offset(off), callback(std::move(cb))
    {
    }

    void setIcon(SurgeImage *img)
    {
        icons = img;
        repaint();
    }

    void paint(juce::Graphics &g) override;
    void mouseUp(const juce::MouseEvent &e) override { callback(); }
    void mouseEnter(const juce::MouseEvent &e) override
    {
        isHovered = true;
        repaint();
    }
    void mouseExit(const juce::MouseEvent &e) override
    {
        isHovered = false;
        repaint();
    }

    bool isHovered{false};
    std::function<void()> callback;
    std::string accLabel;
    SurgeImage *icons{nullptr};
    int offset{0};

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TinyLittleIconButton);
};

struct MenuTitleHelpComponent : juce::PopupMenu::CustomComponent, Surge::GUI::SkinConsumingComponent
{
    std::string manualAccTag = " (open manual)";
    MenuTitleHelpComponent(const std::string &l, const std::string &u)
        : label(l), url(u), juce::PopupMenu::CustomComponent(false)
    {
        setTitle(l + manualAccTag);
        setDescription(l + manualAccTag);
        setAccessible(true);
    }

    MenuTitleHelpComponent(const std::string &l, const std::string &accL, const std::string &u)
        : label(l), url(u), juce::PopupMenu::CustomComponent(false)
    {
        setTitle(accL + manualAccTag);
        setDescription(accL + manualAccTag);
        setAccessible(true);
    }

    void getIdealSize(int &idealWidth, int &idealHeight) override;

    void mouseUp(const juce::MouseEvent &e) override;
    bool keyPressed(const juce::KeyPress &k) override;
    void paint(juce::Graphics &g) override;
    void onSkinChanged() override;

    std::string label, url;
    SurgeImage *icons{nullptr};
    bool isCentered{true};
    bool isBoldened{true};
    void setCentered(bool b) { isCentered = b; }
    void setBoldened(bool b) { isBoldened = b; }
    void launchHelp();

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuTitleHelpComponent);
};

struct MenuCenteredBoldLabel : juce::PopupMenu::CustomComponent
{
    MenuCenteredBoldLabel(const std::string &s) : label(s), juce::PopupMenu::CustomComponent(false)
    {
        setAccessible(true);
        setTitle(label);
    }

    void getIdealSize(int &idealWidth, int &idealHeight) override;
    void paint(juce::Graphics &g) override;

    std::string label;
    bool isSectionHeader{false};
    static void addToMenu(juce::PopupMenu &m, const std::string label);
    static void addToMenuAsSectionHeader(juce::PopupMenu &m, const std::string label);

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
};

struct TinyLittleIconButton;

struct ModMenuCustomComponent : juce::PopupMenu::CustomComponent, Surge::GUI::SkinConsumingComponent
{
    enum OpType
    {
        EDIT,
        CLEAR,
        MUTE
    };

    ModMenuCustomComponent(SurgeStorage *storage, const std::string &source,
                           const std::string &amount, std::function<void(OpType)> callback,
                           bool isTarget = false, bool isApplyToAll = false);
    ~ModMenuCustomComponent() noexcept;

    void getIdealSize(int &idealWidth, int &idealHeight) override;
    void paint(juce::Graphics &g) override;
    void resized() override;

    void setIsMuted(bool b);

    void onSkinChanged() override;
    SurgeImage *icons{nullptr};

    void mouseUp(const juce::MouseEvent &e) override;
    bool keyPressed(const juce::KeyPress &k) override;

    std::unique_ptr<juce::PopupMenu> createAccessibleSubMenu();

    std::unique_ptr<TinyLittleIconButton> clear, mute, edit;
    std::string source, amount;
    std::function<void(OpType)> callback;
    bool isTarget{false};
    bool isMenuExpanded{false};

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMenuCustomComponent);
};

struct ModMenuForAllComponent : ModMenuCustomComponent
{
    enum AllAction
    {
        CLEAR,
        MUTE,
        UNMUTE
    };

    ModMenuForAllComponent(SurgeStorage *storage, std::function<void(AllAction)> callback,
                           bool isTarget = false);

    std::function<void(AllAction)> allCB;
    void mouseUp(const juce::MouseEvent &e) override {}

    std::unique_ptr<juce::PopupMenu> createAccessibleSubMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMenuForAllComponent);
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_MODMENUCUSTOMCOMPONENT_H
