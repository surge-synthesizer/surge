/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2022 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#ifndef SURGE_KEYBINDINGSOVERLAY_H
#define SURGE_KEYBINDINGSOVERLAY_H

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

    void paint(juce::Graphics &g) override;
    void resized() override;

    void onSkinChanged() override;

    bool isLearning{false};
    int learnAction{0};
    bool keyPressed(const juce::KeyPress &key) override;

    std::unique_ptr<Surge::Widgets::SelfDrawButton> okS, cancelS, resetAll;
    std::unique_ptr<KeyBindingsListBoxModel> bindingListBoxModel;
    std::unique_ptr<juce::ListBox> bindingList;
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_KEYBINDINGSOVERLAY_H
