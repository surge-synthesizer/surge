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

#include "LuaEditors.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SkinColors.h"

struct EditorColors
{
    static void setColorsFromSkin(juce::CodeEditorComponent *comp,
                                  const Surge::GUI::Skin::ptr_t &skin)
    {
        auto cs = comp->getColourScheme();

        cs.set("Bracket", skin->getColor(Colors::FormulaEditor::Lua::Bracket).asJuceColour());
        cs.set("Comment", skin->getColor(Colors::FormulaEditor::Lua::Comment).asJuceColour());
        cs.set("Error", skin->getColor(Colors::FormulaEditor::Lua::Error).asJuceColour());
        cs.set("Float", skin->getColor(Colors::FormulaEditor::Lua::Number).asJuceColour());
        cs.set("Integer", skin->getColor(Colors::FormulaEditor::Lua::Number).asJuceColour());
        cs.set("Identifier", skin->getColor(Colors::FormulaEditor::Lua::Identifier).asJuceColour());
        cs.set("Keyword", skin->getColor(Colors::FormulaEditor::Lua::Keyword).asJuceColour());
        cs.set("Operator",
               skin->getColor(Colors::FormulaEditor::Lua::Interpunction).asJuceColour());
        cs.set("Punctuation",
               skin->getColor(Colors::FormulaEditor::Lua::Interpunction).asJuceColour());
        cs.set("String", skin->getColor(Colors::FormulaEditor::Lua::String).asJuceColour());

        comp->setColourScheme(cs);

        comp->setColour(juce::CodeEditorComponent::backgroundColourId,
                        skin->getColor(Colors::FormulaEditor::Background).asJuceColour());
        comp->setColour(juce::CodeEditorComponent::highlightColourId,
                        skin->getColor(Colors::FormulaEditor::Highlight).asJuceColour());
        comp->setColour(juce::CodeEditorComponent::defaultTextColourId,
                        skin->getColor(Colors::FormulaEditor::Text).asJuceColour());
        comp->setColour(juce::CodeEditorComponent::lineNumberBackgroundId,
                        skin->getColor(Colors::FormulaEditor::LineNumBackground).asJuceColour());
        comp->setColour(juce::CodeEditorComponent::lineNumberTextId,
                        skin->getColor(Colors::FormulaEditor::LineNumText).asJuceColour());

        comp->retokenise(0, -1);
    }
};

CodeEditorContainerWithApply::CodeEditorContainerWithApply(SurgeGUIEditor *ed, SurgeStorage *s,
                                                           Surge::GUI::Skin::ptr_t skin)
    : editor(ed), storage(s)
{
    applyButton = std::make_unique<juce::TextButton>("Apply");
    applyButton->setButtonText("Apply");
    applyButton->setEnabled(false);
    applyButton->addListener(this);
    addAndMakeVisible(applyButton.get());

    mainDocument = std::make_unique<juce::CodeDocument>();
    mainDocument->addListener(this);
    tokenizer = std::make_unique<juce::LuaTokeniser>();

    mainEditor = std::make_unique<juce::CodeEditorComponent>(*mainDocument, tokenizer.get());
    mainEditor->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
    mainEditor->setTabSize(4, true);
    mainEditor->addKeyListener(this);
    EditorColors::setColorsFromSkin(mainEditor.get(), skin);
    addAndMakeVisible(mainEditor.get());
}

void CodeEditorContainerWithApply::buttonClicked(juce::Button *button)
{
    if (button == applyButton.get())
    {
        applyCode();
    }
}

void CodeEditorContainerWithApply::codeDocumentTextInserted(const juce::String &newText,
                                                            int insertIndex)
{
    applyButton->setEnabled(true);
}
void CodeEditorContainerWithApply::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    applyButton->setEnabled(true);
}
bool CodeEditorContainerWithApply::keyPressed(const juce::KeyPress &key, juce::Component *o)
{
    if (key.getKeyCode() == juce::KeyPress::returnKey &&
        (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
    {
        applyCode();
        return true;
    }
    else
    {
        return Component::keyPressed(key);
    }
}

FormulaModulatorEditor::FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                               FormulaModulatorStorage *fs,
                                               Surge::GUI::Skin::ptr_t skin)
    : CodeEditorContainerWithApply(ed, s, skin), formulastorage(fs)
{
    mainDocument->insertText(0, fs->formulaString);

    warningLabel = std::make_unique<juce::Label>("Warning");
    warningLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(14));
    warningLabel->setBounds(5, 5, 730, 20);
    warningLabel->setColour(juce::Label::textColourId, juce::Colour(255, 0, 0));
    warningLabel->setText("WARNING: Dont use this! Super alpha! It will crash and probably won't "
                          "load in the future or work now. And have fun!",
                          juce::NotificationType::dontSendNotification);

    addAndMakeVisible(warningLabel.get());

    lesserWarningLabel = std::make_unique<juce::Label>("Lesser Warning");
    lesserWarningLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
    lesserWarningLabel->setText(
        "We will copy the script to clipboard when you apply, just in case.",
        juce::NotificationType::dontSendNotification);
    addAndMakeVisible(lesserWarningLabel.get());
}

void FormulaModulatorEditor::applyCode()
{
    formulastorage->setFormula(mainDocument->getAllContent().toStdString());
    applyButton->setEnabled(false);
    editor->invalidateFrame();
    juce::SystemClipboard::copyTextToClipboard(formulastorage->formulaString);
}

void FormulaModulatorEditor::resized()
{
    auto w = getWidth() - 5;
    auto h = getHeight() - 5; // this is a hack obvs
    int m = 3, m2 = m * 2;
    warningLabel->setBounds(m, m, w - m2, 20);
    mainEditor->setBounds(m, 20 + m2, w - m2, h - 40 - m2 - m2);
    applyButton->setBounds(m, h - 20, 50, 20 - m);
    lesserWarningLabel->setBounds(m2 + 50, h - 20, w - 70, 20 - m);
}

WavetableEquationEditor::WavetableEquationEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                                 OscillatorStorage *os, Surge::GUI::Skin::ptr_t sk)
    : CodeEditorContainerWithApply(ed, s, sk), osc(os)
{
    mainDocument->insertText(0, osc->wavetable_formula);

    warningLabel = std::make_unique<juce::Label>("Warning");
    warningLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(14));
    warningLabel->setBounds(5, 5, 730, 20);
    warningLabel->setColour(juce::Label::textColourId, juce::Colour(255, 0, 0));
    warningLabel->setText("THIS DOES ABSOLUTELY NOTHING YET. Just a placeholder",
                          juce::NotificationType::dontSendNotification);

    addAndMakeVisible(warningLabel.get());
}

void WavetableEquationEditor::resized()
{
    auto w = getWidth() - 5;
    auto h = getHeight() - 5; // this is a hack obvs
    int m = 3;
    warningLabel->setBounds(m, m, w - m * 2, 20);

    mainEditor->setBounds(m, m * 2 + 20, w - 2 * m, h - 30 - 20 - 2 * m);
    applyButton->setBounds(m, h - 28, 100, 25);
}

void WavetableEquationEditor::applyCode()
{
    std::cout << "Would apply " << mainDocument->getAllContent().toStdString() << std::endl;
    osc->wavetable_formula = mainDocument->getAllContent().toStdString();
}