/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2022 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_FORMULAMODULATOREDITOR_H
#define SURGE_XT_FORMULAMODULATOREDITOR_H

#include <JuceHeader.h>
#include "SurgeStorage.h"

class SurgeGUIEditor;

class FormulaModulatorEditor : public juce::Component,
                               public juce::CodeDocument::Listener,
                               public juce::Button::Listener
{
  public:
    FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s, FormulaModulatorStorage *fs);
    std::unique_ptr<juce::CodeDocument> mainDocument;
    std::unique_ptr<juce::CodeEditorComponent> mainEditor;
    std::unique_ptr<juce::Button> applyButton;
    std::unique_ptr<juce::Label> warningLabel;

    std::unique_ptr<juce::LuaTokeniser> tokenizer;

    void buttonClicked(juce::Button *button) override;
    void codeDocumentTextDeleted(int startIndex, int endIndex) override;
    void codeDocumentTextInserted(const juce::String &newText, int insertIndex) override;
    void resized() override;
    SurgeGUIEditor *editor;
    FormulaModulatorStorage *formulastorage;
};

#endif // SURGE_XT_FORMULAMODULATOREDITOR_H
