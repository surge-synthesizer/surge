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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_KEYBINDINGSOVERLAY_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_KEYBINDINGSOVERLAY_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "OverlayComponent.h"
#include "SkinSupport.h"
#include "widgets/MultiSwitch.h"

class SurgeStorage;
class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
struct KeyBindingsListBoxModel;
struct KeyBindingsOverlay : public OverlayComponent, public Surge::GUI::SkinConsumingComponent
{
    static constexpr int okCancelAreaHeight = 30, margin = 2, btnHeight = 17, btnWidth = 50;

    SurgeStorage *storage{nullptr};
    SurgeGUIEditor *editor{nullptr};
    KeyBindingsOverlay(SurgeStorage *storage, SurgeGUIEditor *editor);
    ~KeyBindingsOverlay();

    void resetAllToDefault();
    void createVKBLayoutMenu();
    void changeVKBLayout(const std::string layout);

    void paint(juce::Graphics &g) override;
    void resized() override;

    void onSkinChanged() override;

    bool isLearning{false};
    int learnAction{0};
    bool keyPressed(const juce::KeyPress &key) override;

    std::unique_ptr<Surge::Widgets::SelfDrawButton> okS, cancelS, resetAll, vkbLayout;
    std::unique_ptr<KeyBindingsListBoxModel> bindingListBoxModel;
    std::unique_ptr<juce::ListBox> bindingList;
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_KEYBINDINGSOVERLAY_H
