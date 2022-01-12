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

#ifndef SURGE_XT_MINIEDIT_H
#define SURGE_XT_MINIEDIT_H

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
