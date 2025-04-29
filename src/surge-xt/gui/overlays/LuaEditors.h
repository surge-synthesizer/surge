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

/*
 * This file contains all the various LUA editors as specialized
 * classes and shared function base classes.
 */
#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_LUAEDITORS_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_LUAEDITORS_H

#include <juce_gui_extra/juce_gui_extra.h>

#include "WavetableScriptEvaluator.h"

#include "SurgeStorage.h"
#include "SkinSupport.h"

#include "OverlayComponent.h"
#include "RefreshableOverlay.h"
#include "util/LuaTokeniserSurge.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{

class TextfieldButton : public juce::Component
{

  protected:
    std::unique_ptr<juce::XmlElement> xml;
    bool isSelectable = false;
    bool selected = false;
    bool isMouseDown = false;
    bool isMouseOver = false;
    bool enabled = true;
    void (*callback)(TextfieldButton &f);

  public:
    bool isSelected() { return selected; };
    TextfieldButton(juce::String &svg, int r);
    void loadSVG(juce::String &svg);
    void setSelectable();
    void select(bool v);
    void setEnabled(bool v);

    void updateGraphicState();
    void paint(juce::Graphics &g) override;
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;
    std::function<void()> onClick;
    int row;

  private:
    std::unique_ptr<juce::Drawable> svgGraphics;
};

class Textfield : public juce::TextEditor
{

  private:
    juce::Colour colour;
    juce::String header;

  protected:
    juce::String title;

  public:
    Textfield(int r);
    virtual void focusLost(FocusChangeType) override;
    void paint(juce::Graphics &g) override;
    void setColour(int colourID, juce::Colour newColour);
    void setHeader(juce::String h);
    void setHeaderColor(juce::Colour c);
    int row;
    std::function<void()> onFocusLost;
};

class TextfieldPopup : public juce::Component,
                       public juce::TextEditor::Listener,
                       public juce::KeyListener,
                       public Surge::GUI::SkinConsumingComponent
{
  public:
    static constexpr int STYLE_MARGIN = 4;

    static constexpr int STYLE_ROW_MARGIN = 4;
    static constexpr int STYLE_TEXT_HEIGHT = 17;

    static constexpr int STYLE_BUTTON_MARGIN = 2;
    static constexpr int STYLE_BUTTON_SIZE = 14;
    int STYLE_MARGIN_BETWEEN_TEXT_AND_BUTTONS = 40;
    std::function<void()> onFocusLost;

  protected:
    juce::CodeEditorComponent *ed;
    Surge::GUI::Skin::ptr_t currentSkin;
    std::unique_ptr<juce::Label> labelResult;

    std::unique_ptr<TextfieldButton> button[8];
    std::unique_ptr<Textfield> textfield[8];
    int buttonOffset[8] = {0};

    juce::String header;
    int buttonCount = 0;
    int textfieldCount = 0;

  private:
    int textWidth = 120;

  public:
    int rowsVisible = 1;
    virtual bool keyPressed(const juce::KeyPress &key,
                            juce::Component *originatingComponent) override;
    virtual void paint(juce::Graphics &g) override;
    virtual void onClick(std::unique_ptr<TextfieldButton> &btn);
    TextfieldPopup(juce::CodeEditorComponent &editor, Surge::GUI::Skin::ptr_t);
    virtual void textEditorEscapeKeyPressed(juce::TextEditor &) override;
    void resize();
    void setHeader(juce::String);
    void createButton(juce::String svg, int row);
    bool getButtonSelected(int row) { return button[row]->isSelected(); }
    void setButtonSelected(int row, bool value) { button[row]->select(value); }

    void setText(int id, std ::string str);
    juce::String getText(int id);
    void createTextfield(int row);
    void showRows(int rows);
    void setButtonOffsetAtRow(int row, int offset);

    void setTextWidth(int w);
    virtual void show();
    virtual void hide();
};

class CodeEditorSearch : public TextfieldPopup
{
  private:
    virtual void setHighlightColors();
    virtual void removeHighlightColors();

    bool active = false;
    int result[512] = {0};
    int resultCurrent = 0;
    int resultTotal = 0;
    bool saveCaretStartPositionLock;
    juce::String latestSearch;
    juce::CodeDocument::Position startCaretPosition;

  public:
    bool resultHasChanged = false;
    virtual void search(bool moveCaret);
    virtual juce::String getSearchQuery();
    virtual bool isActive();
    virtual void show() override;
    virtual void hide() override;

    virtual void onClick(std::unique_ptr<TextfieldButton> &btn) override;

    virtual void textEditorTextChanged(juce::TextEditor &textEditor) override;
    virtual void mouseDown(const juce::MouseEvent &event) override;
    virtual void focusLost(FocusChangeType) override;
    virtual bool keyPressed(const juce::KeyPress &key,
                            juce::Component *originatingComponent) override;
    virtual void textEditorEscapeKeyPressed(juce::TextEditor &) override;
    virtual void textEditorReturnKeyPressed(juce::TextEditor &) override;
    virtual void replaceResults(bool all);
    virtual void replaceCurrentResult(juce::String newText);

    CodeEditorSearch(juce::CodeEditorComponent &editor, Surge::GUI::Skin::ptr_t);

    virtual void saveCaretStartPosition(bool onlyReadCaretPosition);
    virtual void showResult(int increase, bool moveCaret);
    virtual void setCurrentResult(int res);
    int getCurrentResult() { return resultCurrent; };
    virtual int *getResult();
    virtual int getResultTotal();
    virtual void showReplace(bool b);
};

class GotoLine : public TextfieldPopup
{
  public:
    virtual bool keyPressed(const juce::KeyPress &key,
                            juce::Component *originatingComponent) override;
    GotoLine(juce::CodeEditorComponent &editor, Surge::GUI::Skin::ptr_t);
    virtual void show() override;
    virtual void hide() override;
    void focusLost(FocusChangeType) override;
    int currentLine;
    int getCurrentLine();
    virtual void onClick(std::unique_ptr<TextfieldButton> &btn) override;

  private:
    int startScroll;
    juce::CodeDocument::Position startCaretPosition;
};

class SurgeCodeEditorComponent : public juce::CodeEditorComponent
{
  public:
    bool keyPressed(const juce::KeyPress &key) override;
    virtual void handleEscapeKey() override;
    virtual void handleReturnKey() override;
    virtual void caretPositionMoved() override;

    virtual void paint(juce::Graphics &) override;
    virtual void paintOverChildren(juce::Graphics &g) override;

    virtual void setSearch(CodeEditorSearch &s);
    virtual void setGotoLine(GotoLine &s);
    virtual void addPopupMenuItems(juce::PopupMenu &menuToAddTo,
                                   const juce::MouseEvent *mouseClickEvent) override;

    void focusLost(juce::Component::FocusChangeType e) override;
    std::function<void()> onFocusLost;

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &d) override;
    SurgeCodeEditorComponent(juce::CodeDocument &d, juce::CodeTokeniser *t,
                             Surge::GUI::Skin::ptr_t &skin);

    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void findWordAt(juce ::CodeDocument::Position &pos, juce ::CodeDocument::Position &from,
                    juce::CodeDocument::Position &to);

  private:
    std::unique_ptr<juce::Image> searchMapCache;
    Surge::GUI::Skin::ptr_t currentSkin;
    CodeEditorSearch *search = nullptr;
    GotoLine *gotoLine = nullptr;
};

/*
 * This is a base class that provides you an apply button, an editor, a document
 * a tokenizer, etc... which you need to layout with yoru other components by
 * having a superclass constructor and implementing resized; and where you have
 * to handle the apply condition with applyCode()
 */
class CodeEditorContainerWithApply : public OverlayComponent,
                                     public juce::CodeDocument::Listener,
                                     public juce::Button::Listener,
                                     public juce::KeyListener,
                                     public Surge::GUI::SkinConsumingComponent
{
  public:
    CodeEditorContainerWithApply(SurgeGUIEditor *ed, SurgeStorage *s, Surge::GUI::Skin::ptr_t sk,
                                 bool addComponents = false);
    ~CodeEditorContainerWithApply();
    std::unique_ptr<juce::CodeDocument> mainDocument;
    std::unique_ptr<SurgeCodeEditorComponent> mainEditor;
    std::unique_ptr<juce::Button> applyButton;
    std::unique_ptr<LuaTokeniserSurge> tokenizer;
    std::unique_ptr<CodeEditorSearch> search;
    std::unique_ptr<GotoLine> gotoLine;

    void buttonClicked(juce::Button *button) override;
    void codeDocumentTextDeleted(int startIndex, int endIndex) override;
    void codeDocumentTextInserted(const juce::String &newText, int insertIndex) override;
    void removeTrailingWhitespaceFromDocument();
    bool autoCompleteDeclaration(juce::KeyPress keypress, std::string start, std::string end);
    bool keyPressed(const juce::KeyPress &key, Component *originatingComponent) override;

    DAWExtraStateStorage::EditorState::CodeEditorState *state;

    void initState(DAWExtraStateStorage::EditorState::CodeEditorState &state);
    void saveState();
    void loadState();

    virtual void setApplyEnabled(bool) {}

    void paint(juce::Graphics &g) override;
    SurgeGUIEditor *editor;
    SurgeStorage *storage;

    virtual void onSkinChanged() override;

    virtual void applyCode() = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CodeEditorContainerWithApply);
};

struct ExpandingFormulaDebugger;
struct FormulaControlArea;

struct FormulaModulatorEditor : public CodeEditorContainerWithApply, public RefreshableOverlay
{
    FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s, LFOStorage *lfos,
                           FormulaModulatorStorage *fs, int lfoid, int scene,
                           Surge::GUI::Skin::ptr_t sk);
    ~FormulaModulatorEditor();

    void resized() override;
    void onSkinChanged() override;
    void applyCode() override;
    void forceRefresh() override;
    void setApplyEnabled(bool b) override;
    void showModulatorCode();
    void showPreludeCode();
    void escapeKeyPressed();

    void updateDebuggerIfNeeded();

    std::unique_ptr<juce::CodeDocument> preludeDocument;
    std::unique_ptr<SurgeCodeEditorComponent> preludeDisplay;
    std::unique_ptr<FormulaControlArea> controlArea;

    std::unique_ptr<ExpandingFormulaDebugger> debugPanel;

    LFOStorage *lfos{nullptr};
    FormulaModulatorStorage *formulastorage{nullptr};
    SurgeGUIEditor *editor{nullptr};
    int lfo_id, scene;
    int32_t updateDebuggerCounter{0};

    DAWExtraStateStorage::EditorState::FormulaEditState &getEditState();

    bool shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p) override
    {
        return false;
    }

    std::optional<std::pair<std::string, std::string>> getPreCloseChickenBoxMessage() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormulaModulatorEditor);
};

struct WavetablePreviewComponent;
struct WavetableScriptControlArea;

struct WavetableScriptEditor : public CodeEditorContainerWithApply, public RefreshableOverlay
{
    WavetableScriptEditor(SurgeGUIEditor *ed, SurgeStorage *s, OscillatorStorage *os, int oscid,
                          int scene, Surge::GUI::Skin::ptr_t sk);
    ~WavetableScriptEditor();

    void resized() override;
    void onSkinChanged() override;
    void applyCode() override;
    void forceRefresh() override;
    void setApplyEnabled(bool b) override;
    void showModulatorCode();
    void showPreludeCode();
    void escapeKeyPressed();

    void generateWavetable();
    void rerenderFromUIState();
    void setCurrentFrame(int value);

    int lastRes{-1}, lastFrames{-1}, lastFrame{-1}, lastRm{-1};

    void setupEvaluator();

    std::unique_ptr<Surge::WavetableScript::LuaWTEvaluator> evaluator;
    std::unique_ptr<juce::CodeDocument> preludeDocument;
    std::unique_ptr<juce::CodeEditorComponent> preludeDisplay;
    std::unique_ptr<WavetableScriptControlArea> controlArea;

    std::unique_ptr<WavetablePreviewComponent> rendererComponent;

    OscillatorStorage *osc;
    SurgeGUIEditor *editor{nullptr};
    int osc_id, scene;

    DAWExtraStateStorage::EditorState::WavetableScriptEditState &getEditState();

    bool shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p) override
    {
        return false;
    }

    std::optional<std::pair<std::string, std::string>> getPreCloseChickenBoxMessage() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableScriptEditor);
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_LUAEDITORS_H
