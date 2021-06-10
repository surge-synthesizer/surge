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

namespace Surge
{
namespace Overlays
{

PatchStoreDialog::PatchStoreDialog()
{
    auto makeEd = [this](const std::string &n) {
        auto ed = std::make_unique<juce::TextEditor>(n);
        ed->setJustification(juce::Justification::centredLeft);
        // ed->setSelectAllWhenFocused(true);

        addAndMakeVisible(*ed);
        return std::move(ed);
    };
    nameEd = makeEd("patch name");
    authorEd = makeEd("patch author");
    commentEd = makeEd("patch comment");
    catEd = makeEd("patch category");

    auto makeL = [this](const std::string &n) {
        auto lb = std::make_unique<juce::Label>(n);
        lb->setText(n, juce::dontSendNotification);
        addAndMakeVisible(*lb);
        return std::move(lb);
    };
    nameEdL = makeL("Name");
    authorEdL = makeL("Author");
    catEdL = makeL("Category");
    commentEdL = makeL("Comment");

    okButton = std::make_unique<juce::TextButton>("patchOK");
    okButton->setButtonText("OK");
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<juce::TextButton>("patchCancel");
    cancelButton->setButtonText("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);

    storeTuningButton = std::make_unique<juce::ToggleButton>();
    storeTuningButton->setToggleState(false, juce::dontSendNotification);
    storeTuningButton->setButtonText("");
    addAndMakeVisible(*storeTuningButton);

    storeTuningLabel = makeL("Store tuning in patch");
}

void PatchStoreDialog::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::Dialog::Background));
}

void PatchStoreDialog::onSkinChanged()
{
    auto resetColors = [this](const auto &typein) {
        typein->setFont(Surge::GUI::getFontManager()->getLatoAtSize(11));
        typein->setColour(juce::TextEditor::backgroundColourId,
                          skin->getColor(Colors::Dialog::Entry::Background));
        typein->setColour(juce::TextEditor::textColourId,
                          skin->getColor(Colors::Dialog::Entry::Text));
        typein->setColour(juce::TextEditor::highlightColourId,
                          skin->getColor(Colors::Dialog::Entry::Focus));
        typein->setColour(juce::TextEditor::outlineColourId,
                          skin->getColor(Colors::Dialog::Entry::Border));
    };
    resetColors(nameEd);
    resetColors(authorEd);
    resetColors(catEd);
    resetColors(commentEd);

    auto resetLabel = [this](const auto &label) {
        label->setFont(Surge::GUI::getFontManager()->getLatoAtSize(11));
        label->setColour(juce::Label::textColourId, skin->getColor(Colors::Dialog::Label::Text));
    };
    resetLabel(nameEdL);
    resetLabel(authorEdL);
    resetLabel(catEdL);
    resetLabel(commentEdL);
    resetLabel(storeTuningLabel);

    auto resetButton = [this](const auto &button) {
        // button->setFont(Surge::GUI::getFontManager()->getLatoAtSize(11));
        button->setColour(juce::TextButton::buttonColourId,
                          skin->getColor(Colors::Dialog::Button::Background));
        button->setColour(juce::TextButton::buttonOnColourId,
                          skin->getColor(Colors::Dialog::Button::BackgroundPressed));
        button->setColour(juce::TextButton::textColourOffId,
                          skin->getColor(Colors::Dialog::Button::Text));
        button->setColour(juce::TextButton::textColourOnId,
                          skin->getColor(Colors::Dialog::Button::TextPressed));
    };
    resetButton(okButton);
    resetButton(cancelButton);
}

void PatchStoreDialog::resized()
{
    auto h = getHeight() / 5;
    auto xSplit = 70;
    auto buttonWidth = 60;
    auto margin = 2;
    auto r = getLocalBounds().withHeight(h);
    auto ce = r.withTrimmedLeft(xSplit).reduced(margin);

    nameEd->setBounds(ce);
    nameEd->setIndents(4, (nameEd->getHeight() - nameEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);
    authorEd->setBounds(ce);
    authorEd->setIndents(4, (authorEd->getHeight() - authorEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);
    catEd->setBounds(ce);
    catEd->setIndents(4, (catEd->getHeight() - catEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);
    commentEd->setBounds(ce);
    commentEd->setIndents(4, (commentEd->getHeight() - commentEd->getTextHeight()) / 2);
    ce = ce.translated(0, h);

    auto be = ce.withWidth(buttonWidth).withRightX(ce.getRight());
    cancelButton->setBounds(be);
    be = be.translated(-buttonWidth - margin, 0);
    okButton->setBounds(be);

    auto cl = r.withRight(xSplit).reduced(2);
    nameEdL->setBounds(cl);
    cl = cl.translated(0, h);
    authorEdL->setBounds(cl);
    cl = cl.translated(0, h);
    catEdL->setBounds(cl);
    cl = cl.translated(0, h);
    commentEdL->setBounds(cl);
    cl = cl.translated(0, h);

    if (!editor || editor->synth->storage.isStandardTuning)
    {
        storeTuningButton->setVisible(false);
        storeTuningLabel->setVisible(false);
    }
    else
    {
        cl = cl.withWidth(getWidth() - 4 * margin - 2 * buttonWidth);
        auto fb = cl.withWidth(h);
        storeTuningButton->setBounds(fb);
        auto lb = cl.withX(fb.getRight() + margin);
        storeTuningLabel->setBounds(lb);
    }
}

void PatchStoreDialog::buttonClicked(juce::Button *button)
{
    if (button == cancelButton.get())
    {
        editor->closeStorePatchDialog();
    }

    if (button == okButton.get())
    {
        auto synth = editor->synth;

        synth->storage.getPatch().name = nameEd->getText().toStdString();
        synth->storage.getPatch().author = authorEd->getText().toStdString();
        synth->storage.getPatch().category = catEd->getText().toStdString();
        synth->storage.getPatch().comment = commentEd->getText().toStdString();

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
        synth->savePatch();

        editor->closeStorePatchDialog();
    }
}

} // namespace Overlays
} // namespace Surge