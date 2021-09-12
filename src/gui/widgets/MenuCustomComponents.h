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

#ifndef SURGE_XT_MODMENUCUSTOMCOMPONENT_H
#define SURGE_XT_MODMENUCUSTOMCOMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "SkinSupport.h"

namespace Surge
{
namespace Widgets
{

struct MenuTitleHelpComponent : juce::PopupMenu::CustomComponent, Surge::GUI::SkinConsumingComponent
{
    MenuTitleHelpComponent(const std::string &l, const std::string &u)
        : label(l), url(u), juce::PopupMenu::CustomComponent(false)
    {
    }

    void getIdealSize(int &idealWidth, int &idealHeight) override;

    void mouseUp(const juce::MouseEvent &e) override;
    void paint(juce::Graphics &g) override;
    void onSkinChanged() override;

    std::string label, url;
    SurgeImage *icons{nullptr};
    bool centerBold{true};
    void setCenterBold(bool b) { centerBold = b; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuTitleHelpComponent);
};

struct MenuCenteredBoldLabel : juce::PopupMenu::CustomComponent
{
    MenuCenteredBoldLabel(const std::string &s) : label(s), juce::PopupMenu::CustomComponent(true)
    {
    }
    void getIdealSize(int &idealWidth, int &idealHeight) override;
    void paint(juce::Graphics &g) override;

    std::string label;
    static void addToMenu(juce::PopupMenu &m, const std::string label);
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
    ModMenuCustomComponent(const std::string &source, const std::string &amount,
                           std::function<void(OpType)> callback);
    ~ModMenuCustomComponent() noexcept;
    void getIdealSize(int &idealWidth, int &idealHeight) override;
    void paint(juce::Graphics &g) override;
    void resized() override;

    void setIsMuted(bool b);

    void onSkinChanged() override;
    SurgeImage *icons{nullptr};

    void mouseUp(const juce::MouseEvent &e) override;

    std::unique_ptr<TinyLittleIconButton> clear, mute, edit;
    std::string source, amount;
    std::function<void(OpType)> callback;

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
    ModMenuForAllComponent(std::function<void(AllAction)> callback);
    std::function<void(AllAction)> allCB;
    void mouseUp(const juce::MouseEvent &e) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMenuForAllComponent);
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_MODMENUCUSTOMCOMPONENT_H
