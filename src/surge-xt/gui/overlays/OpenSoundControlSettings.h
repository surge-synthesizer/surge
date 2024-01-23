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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_OpenSoundControlSettings_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_OpenSoundControlSettings_H

#include "SkinSupport.h"
#include "SurgeStorage.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "OverlayComponent.h"

class SurgeGUIEditor;
class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct SurgeTextButton;
}
namespace Overlays
{
struct OpenSoundControlSettings : public OverlayComponent,
                                  public Surge::GUI::SkinConsumingComponent,
                                  public juce::Button::Listener,
                                  public juce::TextEditor::Listener
{
    OpenSoundControlSettings();
    ~OpenSoundControlSettings();
    void paint(juce::Graphics &g) override;
    void resized() override;
    void shownInParent() override;

    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e);

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s);

    void onSkinChanged() override;
    void buttonClicked(juce::Button *button) override;
    void textEditorTextChanged(juce::TextEditor &) override;

    std::unique_ptr<juce::TextEditor> inPort, outPort, outIP;
    std::unique_ptr<juce::Label> inPortL, outPortL, outIPL;
    std::unique_ptr<Widgets::SurgeTextButton> inPortReset, outPortReset, outIPReset;

    std::unique_ptr<Widgets::SurgeTextButton> showSpec;
    std::unique_ptr<juce::Label> OSCHeader;

    std::unique_ptr<juce::ToggleButton> enableOut, enableIn;

    std::unique_ptr<juce::Label> headerLabel;

    void setValuesFromEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSoundControlSettings);
};
} // namespace Overlays
} // namespace Surge
#endif // SURGE_XT_OpenSoundControlSettings_H
