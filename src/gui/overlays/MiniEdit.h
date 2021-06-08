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

#include <JuceHeader.h>
#include "SkinSupport.h"

namespace Surge
{
namespace Overlays
{
struct MiniEdit : public juce::Component,
                  public Surge::GUI::SkinConsumingComponent,
                  public juce::Button::Listener
{
    MiniEdit();
    ~MiniEdit();
    void paint(juce::Graphics &g) override;
    void onSkinChanged() override;
    void mouseDown(const juce::MouseEvent &e) override { setVisible(false); }

    std::string title, label;
    void setTitle(const std::string t) { title = t; }
    void setLabel(const std::string t) { label = t; }
    void setValue(const std::string t) { typein->setText(t, juce::dontSendNotification); }
    std::function<void(const std::string &s)> callback;

    void resized() override;
    void buttonClicked(juce::Button *button) override;
    juce::Rectangle<int> getDisplayRegion();
    std::unique_ptr<juce::TextEditor> typein;
    std::unique_ptr<juce::TextButton> okButton;
    std::unique_ptr<juce::TextButton> cancelButton;
};
} // namespace Overlays
} // namespace Surge
#endif // SURGE_XT_MINIEDIT_H
