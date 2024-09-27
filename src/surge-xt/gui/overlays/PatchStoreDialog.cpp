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

#include "PatchStoreDialog.h"
#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "widgets/TypeAheadTextEditor.h"
#include "widgets/SurgeTextButton.h"
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Overlays
{

struct PatchStoreDialogCategoryProvider : public Surge::Widgets::TypeAheadDataProvider
{
    PatchStoreDialogCategoryProvider() {}
    SurgeStorage *storage;

    std::vector<int> data;
    std::vector<int> searchFor(const std::string &s) override
    {
        std::vector<int> res;
        std::map<std::string, int> alreadySeen;

        if (storage)
        {
            int idx = 0;

            for (auto &c : storage->patch_category)
            {
                if (!c.isFactory || (idx < storage->firstThirdPartyCategory &&
                                     c.name.find("Tutorial") == std::string::npos))
                {
                    auto it = std::search(
                        c.name.begin(), c.name.end(), s.begin(), s.end(),
                        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });

                    if (it != c.name.end())
                    {
                        // De-duplicate if factory and user are both there.
                        // Replies on fact that factory comes before user in order.
                        if (alreadySeen.find(c.name) != alreadySeen.end())
                        {
                            auto didx = alreadySeen[c.name];

                            for (auto it = res.begin(); it != res.end();)
                            {
                                if (*it == didx)
                                {
                                    it = res.erase(it);
                                    break;
                                }
                                else
                                {
                                    it++;
                                }
                            }
                        }

                        res.push_back(idx);
                        alreadySeen[c.name] = idx;
                    }
                }
                idx++;
            }
        }

        // Now sort that res
        std::sort(res.begin(), res.end(), [this](const auto &a, const auto &b) {
            const auto pa = storage->patch_category[a];
            const auto pb = storage->patch_category[b];

            if (pa.isFactory == pb.isFactory)
            {
                return pa.name < pb.name;
            }
            else
            {
                // putting b here puts the user patches first
                return pb.isFactory;
            }
        });

        return res;
    }

    juce::Font font{juce::FontOptions{12}};
    juce::Colour hl, hlbg, txt, bg;

    void paintDataItem(int searchIndex, juce::Graphics &g, int width, int height,
                       bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.fillAll(hlbg);
            g.setColour(hl);
        }
        else
        {
            g.fillAll(bg);
            g.setColour(txt);
        }

        g.setFont(font);
        g.drawText(textBoxValueForIndex(searchIndex), 4, 0, width - 4, height,
                   juce::Justification::centredLeft);
    }

    std::string textBoxValueForIndex(int idx) override
    {
        if (storage)
        {
            return storage->patch_category[idx].name;
        }

        return "";
    }
};

PatchStoreDialog::PatchStoreDialog()
{
    auto makeEd = [this](const std::string &n) {
        auto ed = std::make_unique<juce::TextEditor>(n);
        ed->setJustification(juce::Justification::centredLeft);
        ed->setWantsKeyboardFocus(true);
        ed->addListener(this);
        Surge::Widgets::fixupJuceTextEditorAccessibility(*ed);
        ed->setTitle(n);
        addAndMakeVisible(*ed);
        return std::move(ed);
    };

    nameEd = makeEd("patch name");
    nameEd->setSelectAllWhenFocused(true);
    nameEd->setWantsKeyboardFocus(true);
    authorEd = makeEd("patch author");
    authorEd->setSelectAllWhenFocused(true);
    tagEd = makeEd("patch tags");
    tagEd->setVisible(showTagsField);
    licenseEd = makeEd("patch license");
    licenseEd->setSelectAllWhenFocused(true);
    commentEd = makeEd("patch comment");
    commentEd->setMultiLine(true, true);
    commentEd->setReturnKeyStartsNewLine(true);
    commentEd->setJustification(juce::Justification::topLeft);

    categoryProvider = std::make_unique<PatchStoreDialogCategoryProvider>();
    auto ta = std::make_unique<Surge::Widgets::TypeAhead>("patch category", categoryProvider.get());

    ta->setJustification(juce::Justification::centredLeft);
    ta->setSelectAllWhenFocused(true);
    ta->addListener(this);
    ta->setToElementZeroOnReturn = true;

    catEd = std::move(ta);
    addAndMakeVisible(*catEd);

    auto makeL = [this](const std::string &n) {
        auto lb = std::make_unique<juce::Label>(n);
        lb->setText(n, juce::dontSendNotification);
        addAndMakeVisible(*lb);
        return std::move(lb);
    };

    nameEdL = makeL("Name");
    authorEdL = makeL("Author");
    tagEdL = makeL("Tags");
    licenseEdL = makeL("License");
    catEdL = makeL("Category");
    commentEdL = makeL("Comment");

    okButton = std::make_unique<Widgets::SurgeTextButton>("patchOK");
    okButton->setButtonText("OK");
    okButton->setWantsKeyboardFocus(true);
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<Widgets::SurgeTextButton>("patchCancel");
    cancelButton->setButtonText("Cancel");
    cancelButton->setWantsKeyboardFocus(true);
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);

    okOverButton = std::make_unique<Widgets::SurgeTextButton>("factoryOverwrite");
    okOverButton->setButtonText("Factory Overwrite");
    okOverButton->setWantsKeyboardFocus(true);
    okOverButton->addListener(this);
    addAndMakeVisible(*okOverButton);

    auto stbTitle = "Store Tuning in Patch";

    storeTuningLabel = makeL(Surge::GUI::toOSCase(stbTitle));

    storeTuningButton = std::make_unique<juce::ToggleButton>();
    storeTuningButton->setButtonText("");
    storeTuningButton->setTitle(stbTitle);
    storeTuningButton->setDescription(stbTitle);
    addAndMakeVisible(*storeTuningButton);
}

PatchStoreDialog::~PatchStoreDialog() = default;

void PatchStoreDialog::setShowFactoryOverwrite(bool s) { okOverButton->setVisible(s); }

void PatchStoreDialog::setStorage(SurgeStorage *s)
{
    storage = s;
    categoryProvider->storage = s;
}

void PatchStoreDialog::setStoreTuningInPatch(const bool value)
{
    storeTuningButton->setToggleState(value, juce::dontSendNotification);
}

void PatchStoreDialog::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::Dialog::Background));
}

void PatchStoreDialog::setSurgeGUIEditor(SurgeGUIEditor *e)
{
    editor = e;

    if (!editor || !editor->synth->storage.datapathOverriden)
    {
        okOverButton->setVisible(false);
        resized();
    }
}

void PatchStoreDialog::shownInParent()
{
    if (nameEd->isShowing() && isVisible())
    {
        nameEd->grabKeyboardFocus();
    }
}

void PatchStoreDialog::textEditorFocusLost(juce::TextEditor &ed)
{
    if (!editor || !storage)
    {
        return;
    }

    ed.setHighlightedRegion(juce::Range(-1, -1));
}

void PatchStoreDialog::onSkinChanged()
{
    auto resetColors = [this](const auto &typein) {
        typein->setFont(skin->getFont(Fonts::PatchStore::TextEntry));
        typein->setColour(juce::TextEditor::backgroundColourId,
                          skin->getColor(Colors::Dialog::Entry::Background));
        typein->setColour(juce::TextEditor::textColourId,
                          skin->getColor(Colors::Dialog::Entry::Text));
        typein->setColour(juce::TextEditor::highlightedTextColourId,
                          skin->getColor(Colors::Dialog::Entry::Text));
        typein->setColour(juce::TextEditor::highlightColourId,
                          skin->getColor(Colors::Dialog::Entry::Focus));
        typein->setColour(juce::TextEditor::outlineColourId,
                          skin->getColor(Colors::Dialog::Entry::Border));
        typein->setColour(juce::TextEditor::focusedOutlineColourId,
                          skin->getColor(Colors::Dialog::Entry::Border));

        typein->applyColourToAllText(skin->getColor(Colors::Dialog::Entry::Text));
    };

    auto resetLabel = [this](const auto &label) {
        label->setFont(skin->getFont(Fonts::PatchStore::Label));
        label->setColour(juce::Label::textColourId, skin->getColor(Colors::Dialog::Label::Text));
    };

    resetColors(nameEd);
    resetColors(authorEd);
    resetColors(catEd);
    resetColors(tagEd);
    resetColors(licenseEd);
    resetColors(commentEd);

    resetLabel(nameEdL);
    resetLabel(authorEdL);
    resetLabel(catEdL);
    resetLabel(tagEdL);
    resetLabel(licenseEdL);
    resetLabel(commentEdL);
    resetLabel(storeTuningLabel);

    storeTuningButton->setColour(juce::ToggleButton::tickDisabledColourId,
                                 skin->getColor(Colors::Dialog::Checkbox::Border));
    storeTuningButton->setColour(juce::ToggleButton::tickColourId,
                                 skin->getColor(Colors::Dialog::Checkbox::Tick));

    catEd->setColour(Surge::Widgets::TypeAhead::ColourIds::emptyBackgroundId,
                     skin->getColor(Colors::Dialog::Entry::Background));
    catEd->setColour(Surge::Widgets::TypeAhead::ColourIds::borderid,
                     skin->getColor(Colors::Dialog::Entry::Border));

    categoryProvider->font = skin->getFont(Fonts::PatchStore::TextEntry);
    categoryProvider->txt = skin->getColor(Colors::Dialog::Entry::Text);
    categoryProvider->bg = skin->getColor(Colors::Dialog::Entry::Background);
    categoryProvider->hl = categoryProvider->txt;
    categoryProvider->hlbg = skin->getColor(Colors::Dialog::Entry::Focus);

    okButton->setSkin(skin, associatedBitmapStore);
    cancelButton->setSkin(skin, associatedBitmapStore);
    okOverButton->setSkin(skin, associatedBitmapStore);
}

void PatchStoreDialog::setIsRename(bool b) { isRename = b; }

void PatchStoreDialog::resized()
{
    auto h = 25;
    auto commH = getHeight() - (6 + showTagsField) * h + 8;
    auto xSplit = 70;
    auto buttonHeight = 17;
    auto buttonWidth = 50;
    auto margin = 4;
    auto margin2 = 2;
    auto r = getLocalBounds().withHeight(h);
    auto dialogCenter = getLocalBounds().getWidth() / 2;
    auto ce = r.withTrimmedLeft(xSplit)
                  .withTrimmedRight(margin2 * 3)
                  .reduced(margin)
                  .translated(0, margin2 * 3);

    nameEd->setBounds(ce);
    nameEd->setIndents(4, (nameEd->getHeight() - nameEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);
    catEd->setBounds(ce);
    catEd->setIndents(4, (catEd->getHeight() - catEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);
    authorEd->setBounds(ce);
    authorEd->setIndents(4, (authorEd->getHeight() - authorEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);
    licenseEd->setBounds(ce);
    licenseEd->setIndents(4, (licenseEd->getHeight() - licenseEd->getTextHeight()) / 2);

    if (isVisible())
    {
        nameEd->grabKeyboardFocus();
    }

    if (showTagsField)
    {
        ce = ce.translated(0, h);
        tagEd->setBounds(ce);
        tagEd->setIndents(4, (tagEd->getHeight() - tagEd->getTextHeight()) / 2);
    }

    ce = ce.translated(0, h);

    auto q = ce.withHeight(commH - margin);
    commentEd->setBounds(q);
    ce = ce.translated(0, commH);

    auto buttonRow = getLocalBounds().withHeight(buttonHeight).withY(ce.getY() + (margin2 * 3));

    auto be =
        buttonRow.withTrimmedLeft(dialogCenter - buttonWidth - margin2).withWidth(buttonWidth);
    okButton->setBounds(be);
    be = buttonRow.withTrimmedLeft(dialogCenter + margin2).withWidth(buttonWidth);
    cancelButton->setBounds(be);

    if (okOverButton->isVisible())
    {
        be = ce.withWidth(buttonWidth * 2).withRightX(ce.getRight()).translated(0, margin2 * 3);
        okOverButton->setBounds(be);
    }

    auto cl = r.withRight(xSplit).reduced(2).translated(0, margin2 * 3);
    nameEdL->setBounds(cl);
    cl = cl.translated(0, h);
    catEdL->setBounds(cl);
    cl = cl.translated(0, h);
    authorEdL->setBounds(cl);
    cl = cl.translated(0, h);
    licenseEdL->setBounds(cl);

    if (showTagsField)
    {
        cl = cl.translated(0, h);
        tagEdL->setBounds(cl);
    }

    cl = cl.translated(0, h);
    commentEdL->setBounds(cl);
    cl = cl.translated(0, commH)
             .withY(okButton->getY() - 1)
             .withWidth(h + buttonWidth * 2.5)
             .withHeight(okButton->getHeight() + 2);

    if (!editor || editor->synth->storage.isStandardTuning)
    {
        storeTuningButton->setVisible(false);
        storeTuningLabel->setVisible(false);
    }
    else
    {
        auto lb = cl.withX(cl.withWidth(h).getRight() - margin).withWidth(buttonWidth * 2.5);

        storeTuningButton->setBounds(cl);
        storeTuningLabel->setBounds(lb);
    }
}

void PatchStoreDialog::buttonClicked(juce::Button *button)
{
    if (button == cancelButton.get())
    {
        editor->closeOverlay(SurgeGUIEditor::SAVE_PATCH);
    }

    if (button == okButton.get() || button == okOverButton.get())
    {
        auto synth = editor->synth;

        synth->storage.getPatch().name = nameEd->getText().toStdString();
        synth->storage.getPatch().author = authorEd->getText().toStdString();
        synth->storage.getPatch().category = catEd->getText().toStdString();
        synth->storage.getPatch().license = licenseEd->getText().toStdString();
        synth->storage.getPatch().comment = commentEd->getText().toStdString();

        auto tagString = tagEd->getText();
        std::vector<SurgePatch::Tag> tags;

        while (tagString.contains(","))
        {
            auto ltag = tagString.upToFirstOccurrenceOf(",", false, true);

            tagString = tagString.fromFirstOccurrenceOf(",", false, true);
            tags.emplace_back(ltag.trim().toStdString());
        }

        tagString = tagString.trim();

        if (tagString.length())
        {
            tags.emplace_back(tagString.toStdString());
        }

        synth->storage.getPatch().tags = tags;
        synth->storage.getPatch().patchTuning.tuningStoredInPatch =
            storeTuningButton->isVisible() && storeTuningButton->getToggleState();

        if (synth->storage.getPatch().patchTuning.tuningStoredInPatch)
        {
            if (synth->storage.isStandardScale)
            {
                synth->storage.getPatch().patchTuning.scaleContents = "";
            }
            else
            {
                synth->storage.getPatch().patchTuning.scaleContents =
                    synth->storage.currentScale.rawText;
            }
            if (synth->storage.isStandardMapping)
            {
                synth->storage.getPatch().patchTuning.mappingContents = "";
            }
            else
            {
                synth->storage.getPatch().patchTuning.mappingContents =
                    synth->storage.currentMapping.rawText;
                synth->storage.getPatch().patchTuning.mappingName =
                    synth->storage.currentMapping.name;
            }
        }

        // Ignore whatever comes from the DAW
        synth->storage.getPatch().dawExtraState.isPopulated = false;

        auto m = juce::ModifierKeys::getCurrentModifiers();

        synth->savePatch(button == okOverButton.get(), m.isShiftDown());

        onOK();

        // make this last in case it ends up destroying me
        editor->closeOverlay(SurgeGUIEditor::SAVE_PATCH);
    }
}

void PatchStoreDialog::setTags(const std::vector<SurgePatch::Tag> &itags)
{
    std::string pfx = "";
    std::ostringstream oss;

    for (const auto &t : itags)
    {
        oss << pfx << t.tag;
        pfx = ", ";
    }

    tagEd->setText(oss.str(), juce::NotificationType::dontSendNotification);
}

} // namespace Overlays
} // namespace Surge