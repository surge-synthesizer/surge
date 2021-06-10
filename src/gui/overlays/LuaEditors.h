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

/*
 * This file contains all the various LUA editors as specialized
 * classes and shared function base classes.
 */
#ifndef SURGE_XT_LUAEDITORS_H
#define SURGE_XT_LUAEDITORS_H

#include <JuceHeader.h>
#include "SurgeStorage.h"
#include "SkinSupport.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{

/*
 * This is a base class that provides you an apply button, an editor, a document
 * a tokenizer, etc... which you need to layout with yoru other components by
 * having a superclass constructor and implementing resized; and where you have
 * to handle the apply condition with applyCode()
 */
class CodeEditorContainerWithApply : public juce::Component,
                                     public juce::CodeDocument::Listener,
                                     public juce::Button::Listener,
                                     public juce::KeyListener,
                                     public Surge::GUI::SkinConsumingComponent
{
  public:
    CodeEditorContainerWithApply(SurgeGUIEditor *ed, SurgeStorage *s, Surge::GUI::Skin::ptr_t sk);
    std::unique_ptr<juce::CodeDocument> mainDocument;
    std::unique_ptr<juce::CodeEditorComponent> mainEditor;
    std::unique_ptr<juce::Button> applyButton;
    std::unique_ptr<juce::LuaTokeniser> tokenizer;
    void buttonClicked(juce::Button *button) override;
    void codeDocumentTextDeleted(int startIndex, int endIndex) override;
    void codeDocumentTextInserted(const juce::String &newText, int insertIndex) override;
    bool keyPressed(const juce::KeyPress &key, Component *originatingComponent) override;

    void paint(juce::Graphics &g) override;
    SurgeGUIEditor *editor;
    SurgeStorage *storage;

    virtual void applyCode() = 0;
};

class FormulaModulatorEditor : public CodeEditorContainerWithApply
{
  public:
    FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s, FormulaModulatorStorage *fs,
                           Surge::GUI::Skin::ptr_t sk);
    std::unique_ptr<juce::Label> warningLabel;
    std::unique_ptr<juce::Label> lesserWarningLabel;

    void resized() override;
    void applyCode() override;

    FormulaModulatorStorage *formulastorage;
};

class WavetablePreviewComponent;

class WavetableEquationEditor : public CodeEditorContainerWithApply,
                                public juce::Slider::Listener,
                                public juce::ComboBox::Listener
{
  public:
    WavetableEquationEditor(SurgeGUIEditor *ed, SurgeStorage *s, OscillatorStorage *os,
                            Surge::GUI::Skin::ptr_t sk);
    ~WavetableEquationEditor() noexcept;

    std::unique_ptr<juce::Label> resolutionLabel;
    std::unique_ptr<juce::ComboBox> resolution;

    std::unique_ptr<juce::Label> framesLabel;
    std::unique_ptr<juce::TextEditor> frames;

    std::unique_ptr<juce::Slider> currentFrame;
    std::unique_ptr<WavetablePreviewComponent> renderer;

    std::unique_ptr<juce::Button> generate;

    void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override;
    void sliderValueChanged(juce::Slider *slider) override;

    void resized() override;
    void applyCode() override;

    void rerenderFromUIState();

    void buttonClicked(juce::Button *button) override;

    OscillatorStorage *osc;
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_LUAEDITORS_H
