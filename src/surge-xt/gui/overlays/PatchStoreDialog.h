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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_PATCHSTOREDIALOG_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_PATCHSTOREDIALOG_H

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

    void setShowFactoryOverwrite(bool s);

    void setName(const std::string &n) { nameEd->setText(n, juce::dontSendNotification); }
    void setAuthor(const std::string &a) { authorEd->setText(a, juce::dontSendNotification); }
    void setLicense(const std::string &a) { licenseEd->setText(a, juce::dontSendNotification); }
    void setCategory(const std::string &c) { catEd->setText(c, juce::dontSendNotification); }
    void setComment(const std::string &c) { commentEd->setText(c, juce::dontSendNotification); }
    void setTags(const std::vector<SurgePatch::Tag> &t);
    void setStoreTuningInPatch(const bool value);

    void onSkinChanged() override;
    void buttonClicked(juce::Button *button) override;
    std::unique_ptr<juce::TextEditor> nameEd, authorEd, catEd, licenseEd, tagEd, commentEd;
    std::unique_ptr<juce::Label> nameEdL, authorEdL, catEdL, licenseEdL, tagEdL, commentEdL;
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
