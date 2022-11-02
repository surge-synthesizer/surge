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
struct PatchStoreDialogCategoryProvider;

struct PatchStoreDialog : public OverlayComponent,
                          public Surge::GUI::SkinConsumingComponent,
                          public juce::Button::Listener
{
    PatchStoreDialog();
    ~PatchStoreDialog();
    void paint(juce::Graphics &g) override;
    void resized() override;
    void shownInParent() override;

    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e);

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s);

    bool showTagsField{false};
    void setShowTagsField(bool s)
    {
        if (s && !showTagsField)
        {
            auto pos = getEnclosingParentPosition();
            pos = pos.withHeight(pos.getHeight() + 25);
            setEnclosingParentPosition(pos);
        }
        if (!s && showTagsField)
        {
            auto pos = getEnclosingParentPosition();
            pos = pos.withHeight(pos.getHeight() - 25);
            setEnclosingParentPosition(pos);
        }
        showTagsField = s;
        tagEd->setVisible(s);
    };

    void setName(const std::string &n) { nameEd->setText(n, juce::dontSendNotification); }
    void setAuthor(const std::string &a) { authorEd->setText(a, juce::dontSendNotification); }
    void setCategory(const std::string &c) { catEd->setText(c, juce::dontSendNotification); }
    void setComment(const std::string &c) { commentEd->setText(c, juce::dontSendNotification); }
    void setTags(const std::vector<SurgePatch::Tag> &t);
    void setStoreTuningInPatch(const bool value);

    void onSkinChanged() override;
    void buttonClicked(juce::Button *button) override;
    std::unique_ptr<juce::TextEditor> nameEd, authorEd, catEd, tagEd, commentEd;
    std::unique_ptr<juce::Label> nameEdL, authorEdL, catEdL, tagEdL, commentEdL;
    std::unique_ptr<Widgets::SurgeTextButton> okButton, okOverButton, cancelButton;
    std::unique_ptr<juce::Label> storeTuningLabel;
    std::unique_ptr<juce::ToggleButton> storeTuningButton;

    bool isRename{false};
    void setIsRename(bool b);
    std::function<void()> onOK = []() {};

    std::unique_ptr<PatchStoreDialogCategoryProvider> categoryProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchStoreDialog);
};
} // namespace Overlays
} // namespace Surge
#endif // SURGE_XT_PATCHSTOREDIALOG_H
