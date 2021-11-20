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

#include "PatchStoreDialog.h"
#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "widgets/TypeAheadTextEditor.h"
#include "widgets/SurgeTextButton.h"

namespace Surge
{
namespace Overlays
{

#define HAS_TAGS_FIELD 0

struct PatchStoreDialogCategoryProvider : public Surge::Widgets::TypeAheadDataProvider
{
    PatchStoreDialogCategoryProvider() {}
    SurgeStorage *storage;

    std::vector<int> data;
    std::vector<int> searchFor(const std::string &s) override
    {
        std::vector<int> res;
        if (storage)
        {
            int idx = 0;
            for (auto &c : storage->patch_category)
            {
                if (!c.isFactory)
                {
                    auto it = std::search(
                        c.name.begin(), c.name.end(), s.begin(), s.end(),
                        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
                    if (it != c.name.end())
                    {
                        res.push_back(idx);
                    }
                }
                idx++;
            }
        }
        return res;
    }

    juce::Font font{12};
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
            return storage->patch_category[idx].name;
        return "";
    }
};

PatchStoreDialog::PatchStoreDialog()
{
    auto makeEd = [this](const std::string &n) {
        auto ed = std::make_unique<juce::TextEditor>(n);
        ed->setJustification(juce::Justification::centredLeft);
        ed->setWantsKeyboardFocus(true);

        addAndMakeVisible(*ed);
        return std::move(ed);
    };

    nameEd = makeEd("patch name");
    nameEd->setSelectAllWhenFocused(true);
    nameEd->setWantsKeyboardFocus(true);
    authorEd = makeEd("patch author");
    authorEd->setSelectAllWhenFocused(true);
    commentEd = makeEd("patch comment");
    commentEd->setMultiLine(true, true);
    commentEd->setReturnKeyStartsNewLine(true);
    commentEd->setJustification(juce::Justification::topLeft);
#if HAS_TAGS_FIELD
    tagEd = makeEd("patch tags");
#endif

    categoryProvider = std::make_unique<PatchStoreDialogCategoryProvider>();
    auto ta = std::make_unique<Surge::Widgets::TypeAhead>("patch category", categoryProvider.get());
    ta->setJustification(juce::Justification::centredLeft);
    ta->setSelectAllWhenFocused(true);
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
#if HAS_TAGS_FIELD
    tagEdL = makeL("Tags");
#endif
    catEdL = makeL("Category");
    commentEdL = makeL("Comment");

    okButton = std::make_unique<Widgets::SurgeTextButton>("patchOK");
    okButton->setButtonText("OK");
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<Widgets::SurgeTextButton>("patchCancel");
    cancelButton->setButtonText("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);

    okOverButton = std::make_unique<Widgets::SurgeTextButton>("factoryOverwrite");
    okOverButton->setButtonText("Factory Overwrite");
    okOverButton->addListener(this);
    addAndMakeVisible(*okOverButton);

    storeTuningButton = std::make_unique<juce::ToggleButton>();
    storeTuningButton->setToggleState(false, juce::dontSendNotification);
    storeTuningButton->setButtonText("");
    addAndMakeVisible(*storeTuningButton);

    storeTuningLabel = makeL(Surge::GUI::toOSCaseForMenu("Store Tuning in Patch"));
}

PatchStoreDialog::~PatchStoreDialog() = default;

void PatchStoreDialog::setStorage(SurgeStorage *s)
{
    storage = s;
    categoryProvider->storage = s;
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

void PatchStoreDialog::parentHierarchyChanged()
{
    if (nameEd->isShowing() && isVisible())
        nameEd->grabKeyboardFocus();
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
    };

    resetColors(nameEd);
    resetColors(authorEd);
    resetColors(catEd);
    catEd->setColour(Surge::Widgets::TypeAhead::ColourIds::emptyBackgroundId,
                     skin->getColor(Colors::Dialog::Entry::Background));
    catEd->setColour(Surge::Widgets::TypeAhead::ColourIds::borderid,
                     skin->getColor(Colors::Dialog::Entry::Border));

    categoryProvider->font = skin->getFont(Fonts::PatchStore::TextEntry);
    categoryProvider->txt = skin->getColor(Colors::Dialog::Entry::Text);
    categoryProvider->bg = skin->getColor(Colors::Dialog::Entry::Background);
    categoryProvider->hl = categoryProvider->txt;
    categoryProvider->hlbg = skin->getColor(Colors::Dialog::Entry::Focus);

#if HAS_TAGS_FIELD
    resetColors(tagEd);
#endif
    resetColors(commentEd);

    auto resetLabel = [this](const auto &label) {
        label->setFont(skin->getFont(Fonts::PatchStore::Label));
        label->setColour(juce::Label::textColourId, skin->getColor(Colors::Dialog::Label::Text));
    };

    resetLabel(nameEdL);
    resetLabel(authorEdL);
#if HAS_TAGS_FIELD
    resetLabel(tagEdL);
#endif
    resetLabel(catEdL);
    resetLabel(commentEdL);
    resetLabel(storeTuningLabel);

    okButton->setSkin(skin, associatedBitmapStore);
    cancelButton->setSkin(skin, associatedBitmapStore);
    okOverButton->setSkin(skin, associatedBitmapStore);
}

void PatchStoreDialog::setIsRename(bool b)
{
    isRename = b;
    okButton->setButtonText(isRename ? "Rename" : "OK");
}

void PatchStoreDialog::resized()
{
    auto h = 25;
    auto commH = getHeight() - 5 * h + 8;
    auto xSplit = 70;
    auto buttonWidth = 50;
    auto margin = 4;
    auto margin2 = 2;
    auto r = getLocalBounds().withHeight(h);
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

    if (isVisible())
    {
        nameEd->grabKeyboardFocus();
    }

#if HAS_TAGS_FIELD
    ce = ce.translated(0, h);
    tagEd->setBounds(ce);
    tagEd->setIndents(4, (tagEd->getHeight() - tagEd->getTextHeight()) / 2);
#endif

    ce = ce.translated(0, h);

    auto q = ce.withHeight(commH - margin);
    commentEd->setBounds(q);
    ce = ce.translated(0, commH);

    auto be = ce.withWidth(buttonWidth).withRightX(ce.getRight()).translated(0, margin2 * 3);
    cancelButton->setBounds(be);
    be = be.translated(-buttonWidth - margin, 0);
    okButton->setBounds(be);

    if (okOverButton->isVisible())
    {
        be = be.translated(-buttonWidth - (margin * 2), 0);
        okOverButton->setBounds(be.withLeft(be.getX() - buttonWidth));
    }

    auto cl = r.withRight(xSplit).reduced(2).translated(0, margin2 * 3);
    nameEdL->setBounds(cl);
    cl = cl.translated(0, h);
    catEdL->setBounds(cl);
    cl = cl.translated(0, h);
    authorEdL->setBounds(cl);

#if HAS_TAGS_FIELD
    cl = cl.translated(0, h);
    tagEdL->setBounds(cl);
#endif

    cl = cl.translated(0, h);
    commentEdL->setBounds(cl);
    cl = cl.translated(0, commH);

    if (!editor || editor->synth->storage.isStandardTuning)
    {
        storeTuningButton->setVisible(false);
        storeTuningLabel->setVisible(false);
    }
    else
    {
        cl = cl.withWidth(getWidth() - 6 * margin - 3 * buttonWidth).translated(0, margin2 * 3);

        auto fb = cl.withWidth(h);
        auto lb = cl.withX(fb.getRight() - margin2);

        storeTuningButton->setBounds(fb);
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
        synth->storage.getPatch().comment = commentEd->getText().toStdString();

#if HAS_TAGS_FIELD
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
            tags.emplace_back(tagString.toStdString());

        synth->storage.getPatch().tags = tags;
#endif

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
        if (button == okOverButton.get())
        {
            synth->savePatch(true);
        }
        else
        {
            synth->savePatch();
        }

        onOK();

        // make this last in case it ends up destroying me
        editor->closeOverlay(SurgeGUIEditor::SAVE_PATCH);
    }
}

void PatchStoreDialog::setTags(const std::vector<SurgePatch::Tag> &itags)
{
#if HAS_TAGS_FIELD

    std::string pfx = "";
    std::ostringstream oss;
    for (const auto &t : itags)
    {
        oss << pfx << t.tag;
        pfx = ", ";
    }

    tagEd->setText(oss.str(), juce::NotificationType::dontSendNotification);
#endif
}

} // namespace Overlays
} // namespace Surge