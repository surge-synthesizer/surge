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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_MINIEDIT_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_MINIEDIT_H

#include "SkinSupport.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Widgets
{
struct SurgeTextButton;
}

namespace Overlays
{
struct MiniEdit : public juce::Component,
                  public Surge::GUI::SkinConsumingComponent,
                  public juce::Button::Listener,
                  public juce::TextEditor::Listener
{
    MiniEdit();
    ~MiniEdit();
    void setEditor(SurgeGUIEditor *ed) { editor = ed; }
    void paint(juce::Graphics &g) override;
    void onSkinChanged() override;
    void visibilityChanged() override;
    std::string title, label;
    void setWindowTitle(const std::string &t)
    {
        title = t;
        setTitle(title);
    }
    void setLabel(const std::string &t) { label = t; }
    void setValue(const std::string &t) { typein->setText(t, juce::dontSendNotification); }
    std::function<void(const std::string &s)> callback;

    void resized() override;
    void buttonClicked(juce::Button *button) override;
    juce::Rectangle<int> getDisplayRegion();
    std::unique_ptr<juce::TextEditor> typein;
    std::unique_ptr<Widgets::SurgeTextButton> okButton;
    std::unique_ptr<Widgets::SurgeTextButton> cancelButton;

    void textEditorEscapeKeyPressed(juce::TextEditor &editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;
    void grabFocus() { typein->grabKeyboardFocus(); }

    juce::Component *returnFocusComp{nullptr};
    void setFocusReturnTarget(juce::Component *c) { returnFocusComp = c; }
    void doReturnFocus();
    SurgeGUIEditor *editor{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniEdit);
};
} // namespace Overlays
} // namespace Surge
#endif // SURGE_XT_MINIEDIT_H
