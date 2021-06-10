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

#ifndef SURGE_XT_PATCHSTOREDIALOG_H
#define SURGE_XT_PATCHSTOREDIALOG_H

#include <JuceHeader.h>
#include "SkinSupport.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
struct PatchStoreDialog : public juce::Component,
                          public Surge::GUI::SkinConsumingComponent,
                          public juce::Button::Listener
{
    PatchStoreDialog();
    void paint(juce::Graphics &g) override;
    void resized() override;

    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e) { editor = e; }

    void setName(const std::string &n) { nameEd->setText(n, juce::dontSendNotification); }
    void setAuthor(const std::string &a) { authorEd->setText(a, juce::dontSendNotification); }
    void setCategory(const std::string &c) { catEd->setText(c, juce::dontSendNotification); }
    void setComment(const std::string &c) { commentEd->setText(c, juce::dontSendNotification); }

    void onSkinChanged() override;
    void buttonClicked(juce::Button *button) override;
    std::unique_ptr<juce::TextEditor> nameEd, authorEd, catEd, commentEd;
    std::unique_ptr<juce::Label> nameEdL, authorEdL, catEdL, commentEdL;
    std::unique_ptr<juce::TextButton> okButton, cancelButton;
    std::unique_ptr<juce::ToggleButton> storeTuningButton;
    std::unique_ptr<juce::Label> storeTuningLabel;
};
} // namespace Overlays
} // namespace Surge
#endif // SURGE_XT_PATCHSTOREDIALOG_H
