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

#include "FormulaModulatorEditor.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"

FormulaModulatorEditor::FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                               FormulaModulatorStorage *fs)
    : editor(ed), formulastorage(fs)
{
    applyButton = std::make_unique<juce::TextButton>("Apply");
    applyButton->setButtonText("Apply");
    applyButton->setEnabled(false);
    applyButton->setBounds(5, 340, 40, 15);
    applyButton->addListener(this);
    addAndMakeVisible(applyButton.get());

    mainDocument = std::make_unique<juce::CodeDocument>();
    mainDocument->insertText(0, fs->formula);
    mainDocument->addListener(this);

    tokenizer = std::make_unique<juce::LuaTokeniser>();

    mainEditor = std::make_unique<juce::CodeEditorComponent>(*mainDocument, tokenizer.get());
    mainEditor->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
    mainEditor->setBounds(5, 25, 730, 310);
    mainEditor->setColour(juce::CodeEditorComponent::backgroundColourId,
                          juce::Colour(253, 246, 227));
    mainEditor->setColour(juce::CodeEditorComponent::lineNumberTextId, juce::Colour(203, 75, 22));
    mainEditor->setColour(juce::CodeEditorComponent::lineNumberBackgroundId,
                          juce::Colour(238, 232, 213));

    addAndMakeVisible(mainEditor.get());

    warningLabel = std::make_unique<juce::Label>("Warning");
    warningLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(14));
    warningLabel->setBounds(5, 5, 730, 20);
    warningLabel->setColour(juce::Label::textColourId, juce::Colour(255, 0, 0));
    warningLabel->setText("WARNING: Dont use this! Super alpha! It will crash and probably won't "
                          "load in the future or work now. And have fun!",
                          juce::NotificationType::dontSendNotification);
    addAndMakeVisible(warningLabel.get());
}
void FormulaModulatorEditor::buttonClicked(juce::Button *button)
{
    if (button == applyButton.get())
    {
        formulastorage->formula = mainDocument->getAllContent().toStdString();
        applyButton->setEnabled(false);
        editor->invalidateFrame();
    }
}

void FormulaModulatorEditor::resized()
{
    auto w = getWidth() - 5;
    auto h = getHeight() - 5; // this is a hack obvs
    int m = 3, m2 = m * 2;
    warningLabel->setBounds(m, m, w - m2, 20);
    mainEditor->setBounds(m, 20 + m2, w - m2, h - 40 - m2 - m2);
    applyButton->setBounds(m, h - 20, 50, 20 - m);
}
void FormulaModulatorEditor::codeDocumentTextInserted(const juce::String &newText, int insertIndex)
{
    applyButton->setEnabled(true);
}
void FormulaModulatorEditor::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    applyButton->setEnabled(true);
}
