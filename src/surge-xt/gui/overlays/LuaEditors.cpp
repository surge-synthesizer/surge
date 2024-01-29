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

#include "LuaEditors.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "WavetableScriptEvaluator.h"
#include "LuaSupport.h"
#include "widgets/MultiSwitch.h"
#include "widgets/MenuCustomComponents.h"
#include <fmt/core.h>

namespace Surge
{
namespace Overlays
{

struct SurgeCodeEditorComponent : public juce::CodeEditorComponent
{
    SurgeCodeEditorComponent(juce::CodeDocument &d, juce::CodeTokeniser *t)
        : juce::CodeEditorComponent(d, t)
    {
    }

    void handleEscapeKey() override
    {
        juce::Component *c = this;
        while (c)
        {
            if (auto fm = dynamic_cast<FormulaModulatorEditor *>(c))
            {
                fm->escapeKeyPressed();
                return;
            }
            c = c->getParentComponent();
        }
    }
};
struct EditorColors
{
    static void setColorsFromSkin(juce::CodeEditorComponent *comp,
                                  const Surge::GUI::Skin::ptr_t &skin)
    {
        auto cs = comp->getColourScheme();

        cs.set("Bracket", skin->getColor(Colors::FormulaEditor::Lua::Bracket));
        cs.set("Comment", skin->getColor(Colors::FormulaEditor::Lua::Comment));
        cs.set("Error", skin->getColor(Colors::FormulaEditor::Lua::Error));
        cs.set("Float", skin->getColor(Colors::FormulaEditor::Lua::Number));
        cs.set("Integer", skin->getColor(Colors::FormulaEditor::Lua::Number));
        cs.set("Identifier", skin->getColor(Colors::FormulaEditor::Lua::Identifier));
        cs.set("Keyword", skin->getColor(Colors::FormulaEditor::Lua::Keyword));
        cs.set("Operator", skin->getColor(Colors::FormulaEditor::Lua::Interpunction));
        cs.set("Punctuation", skin->getColor(Colors::FormulaEditor::Lua::Interpunction));
        cs.set("String", skin->getColor(Colors::FormulaEditor::Lua::String));

        comp->setColourScheme(cs);

        comp->setColour(juce::CodeEditorComponent::backgroundColourId,
                        skin->getColor(Colors::FormulaEditor::Background));
        comp->setColour(juce::CodeEditorComponent::highlightColourId,
                        skin->getColor(Colors::FormulaEditor::Highlight));
        comp->setColour(juce::CodeEditorComponent::defaultTextColourId,
                        skin->getColor(Colors::FormulaEditor::Text));
        comp->setColour(juce::CodeEditorComponent::lineNumberBackgroundId,
                        skin->getColor(Colors::FormulaEditor::LineNumBackground));
        comp->setColour(juce::CodeEditorComponent::lineNumberTextId,
                        skin->getColor(Colors::FormulaEditor::LineNumText));

        comp->retokenise(0, -1);
    }
};

CodeEditorContainerWithApply::CodeEditorContainerWithApply(SurgeGUIEditor *ed, SurgeStorage *s,
                                                           Surge::GUI::Skin::ptr_t skin,
                                                           bool addComponents)
    : editor(ed), storage(s)
{
    applyButton = std::make_unique<juce::TextButton>("Apply");
    applyButton->setButtonText("Apply");
    applyButton->addListener(this);

    mainDocument = std::make_unique<juce::CodeDocument>();
    mainDocument->addListener(this);
    tokenizer = std::make_unique<juce::LuaTokeniser>();

    mainEditor = std::make_unique<SurgeCodeEditorComponent>(*mainDocument, tokenizer.get());
    mainEditor->setTabSize(4, true);
    mainEditor->addKeyListener(this);
    EditorColors::setColorsFromSkin(mainEditor.get(), skin);

    if (addComponents)
    {
        addAndMakeVisible(applyButton.get());
        addAndMakeVisible(mainEditor.get());
    }

    applyButton->setEnabled(false);
}

void CodeEditorContainerWithApply::buttonClicked(juce::Button *button)
{
    if (button == applyButton.get())
    {
        applyCode();
    }
}

void CodeEditorContainerWithApply::onSkinChanged()
{
    mainEditor->setFont(skin->getFont(Fonts::LuaEditor::Code));
    EditorColors::setColorsFromSkin(mainEditor.get(), skin);
}

void CodeEditorContainerWithApply::codeDocumentTextInserted(const juce::String &newText,
                                                            int insertIndex)
{
    applyButton->setEnabled(true);
    setApplyEnabled(true);
}

void CodeEditorContainerWithApply::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    applyButton->setEnabled(true);
    setApplyEnabled(true);
}

bool CodeEditorContainerWithApply::keyPressed(const juce::KeyPress &key, juce::Component *o)
{
    auto keyCode = key.getKeyCode();

    if (keyCode == juce::KeyPress::tabKey)
    {
        if (key.getModifiers().isShiftDown())
        {
            mainEditor->unindentSelection();
        }
        else
        {
            mainEditor->indentSelection();
        }

        return true;
    }
    else if (key.getModifiers().isCommandDown() && keyCode == juce::KeyPress::returnKey)
    {
        applyCode();

        return true;
    }
    else if (key.getModifiers().isCommandDown() && keyCode == 68) // Ctrl/Cmd+D
    {
        auto pos = mainEditor->getCaretPos();
        auto sel = mainEditor->getHighlightedRegion();
        auto txt = mainEditor->getTextInRange(sel);
        bool isMultiline = false;
        bool doSel = true;
        int offset = 0;

        // pos.setPositionMaintained(true);

        if (txt.isEmpty())
        {
            txt = pos.getLineText();
            doSel = false;
        }

        if (txt.containsChar('\n'))
        {
            int count = 0;

            // see if selection is multiline
            for (auto c : txt)
            {
                if (c == '\n' && count < 2)
                {
                    count++;
                }
            }

            // if we have any character after newline, we're still multiline
            if (!txt.endsWithChar('\n'))
            {
                count++;
            }

            isMultiline = count > 1;
            offset = -pos.getIndexInLine();
        }

        mainDocument->insertText(pos.movedBy(isMultiline ? 0 : offset), txt);

        // go back to original position
        mainEditor->moveCaretTo(pos, false);
        // move to latest position after insertion,
        // optionally reselecting the text if a selection existed
        mainEditor->moveCaretTo(pos.movedBy(txt.length()), doSel);

        return true;
    }
    else
    {
        return Component::keyPressed(key);
    }
}

void CodeEditorContainerWithApply::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }

struct ExpandingFormulaDebugger : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    bool isOpen{false};

    ExpandingFormulaDebugger(FormulaModulatorEditor *ed) : editor(ed)
    {
        debugTableDataModel = std::make_unique<DebugDataModel>();
        debugTable = std::make_unique<juce::TableListBox>("Debug", debugTableDataModel.get());
        debugTable->getHeader().addColumn("key", 1, 50);
        debugTable->getHeader().addColumn("value", 2, 50);
        debugTable->setHeaderHeight(0);
        debugTable->getHeader().setVisible(false);
        debugTable->setRowHeight(14);
        addAndMakeVisible(*debugTable);
    }

    FormulaModulatorEditor *editor{nullptr};

    pdata tp[n_scene_params];

    void initializeLfoDebugger()
    {
        auto lfodata = editor->lfos;

        tp[lfodata->delay.param_id_in_scene].i = lfodata->delay.val.i;
        tp[lfodata->attack.param_id_in_scene].i = lfodata->attack.val.i;
        tp[lfodata->hold.param_id_in_scene].i = lfodata->hold.val.i;
        tp[lfodata->decay.param_id_in_scene].i = lfodata->decay.val.i;
        tp[lfodata->sustain.param_id_in_scene].i = lfodata->sustain.val.i;
        tp[lfodata->release.param_id_in_scene].i = lfodata->release.val.i;

        tp[lfodata->magnitude.param_id_in_scene].i = lfodata->magnitude.val.i;
        tp[lfodata->rate.param_id_in_scene].i = lfodata->rate.val.i;
        tp[lfodata->shape.param_id_in_scene].i = lfodata->shape.val.i;
        tp[lfodata->start_phase.param_id_in_scene].i = lfodata->start_phase.val.i;
        tp[lfodata->deform.param_id_in_scene].i = lfodata->deform.val.i;
        tp[lfodata->trigmode.param_id_in_scene].i = lm_keytrigger;

        lfoDebugger = std::make_unique<LFOModulationSource>();
        lfoDebugger->assign(editor->storage, editor->lfos, tp, 0, nullptr, nullptr,
                            editor->formulastorage, true);

        if (editor->lfo_id < n_lfos_voice)
        {
            lfoDebugger->setIsVoice(true);
        }
        else
        {
            lfoDebugger->setIsVoice(false);
        }

        if (lfoDebugger->isVoice)
        {
            lfoDebugger->formulastate.velocity = 100;
        }

        lfoDebugger->attack();

        stepLfoDebugger();

        if (editor && editor->editor)
            editor->editor->enqueueAccessibleAnnouncement("Reset Debugger");
    }

    void refreshDebuggerView() { updateDebuggerWithOptionalStep(false); }

    void stepLfoDebugger() { updateDebuggerWithOptionalStep(true); }

    void updateDebuggerWithOptionalStep(bool doStep)
    {
        if (doStep)
        {
            Surge::Formula::setupEvaluatorStateFrom(lfoDebugger->formulastate,
                                                    editor->storage->getPatch());

            lfoDebugger->process_block();
        }
        else
        {
            auto &formulastate = lfoDebugger->formulastate;
            auto &localcopy = tp;
            auto lfodata = editor->lfos;
            auto storage = editor->storage;

            formulastate.rate = localcopy[lfodata->rate.param_id_in_scene].f;
            formulastate.amp = localcopy[lfodata->magnitude.param_id_in_scene].f;
            formulastate.phase = localcopy[lfodata->start_phase.param_id_in_scene].f;
            formulastate.deform = localcopy[lfodata->deform.param_id_in_scene].f;
            formulastate.tempo = storage->temposyncratio * 120.0;
            formulastate.songpos = storage->songpos;

            Surge::Formula::setupEvaluatorStateFrom(lfoDebugger->formulastate,
                                                    editor->storage->getPatch());
            float out[Surge::Formula::max_formula_outputs];
            Surge::Formula::valueAt(lfoDebugger->getIntPhase(), lfoDebugger->getPhase(), storage,
                                    lfoDebugger->fs, &formulastate, out, true);
        }

        auto st = Surge::Formula::createDebugDataOfModState(lfoDebugger->formulastate);

        if (debugTableDataModel && debugTable)
        {
            debugTableDataModel->setRows(st);
            debugTable->updateContent();
            debugTable->repaint();
        }

        if (editor && editor->editor)
            editor->editor->enqueueAccessibleAnnouncement("Stepped Debugger");
    }

    std::unique_ptr<juce::TableListBox> debugTable;
    struct DebugDataModel : public juce::TableListBoxModel,
                            public Surge::GUI::SkinConsumingComponent
    {
        std::vector<Surge::Formula::DebugRow> rows;
        void setRows(const std::vector<Surge::Formula::DebugRow> &r) { rows = r; }
        int getNumRows() override { return rows.size(); }

        void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                bool rowIsSelected) override
        {
            if (rowNumber % 2 == 0)
                g.fillAll(skin->getColor(Colors::FormulaEditor::Debugger::LightRow));
            else
                g.fillAll(skin->getColor(Colors::FormulaEditor::Debugger::Row));
        }

        std::string getText(int rowNumber, int columnId)
        {
            auto r = rows[rowNumber];

            if (columnId == 1)
            {
                return r.label;
            }
            else if (columnId == 2)
            {
                if (!r.hasValue)
                {
                    return "";
                }
                else if (auto fv = std::get_if<float>(&r.value))
                {
                    return fmt::format("{:.3f}", *fv);
                }
                else if (auto sv = std::get_if<std::string>(&r.value))
                {
                    return *sv;
                }
            }
            return "";
        }
        void paintCell(juce::Graphics &g, int rowNumber, int columnId, int w, int h,
                       bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= rows.size())
                return;

            auto r = rows[rowNumber];
            auto b = juce::Rectangle<int>(0, 0, w, h);
            g.setFont(skin->fontManager->getFiraMonoAtSize(9));
            if (r.isInternal)
                g.setColour(skin->getColor(Colors::FormulaEditor::Debugger::InternalText));
            else
                g.setColour(skin->getColor(Colors::FormulaEditor::Debugger::Text));

            if (columnId == 1)
            {
                b = b.withTrimmedLeft(r.depth * 10);
                g.drawText(getText(rowNumber, columnId), b, juce::Justification::centredLeft);
            }
            else if (columnId == 2)
            {
                g.drawText(getText(rowNumber, columnId), b, juce::Justification::centredRight);
            }
            else
            {
                g.setColour(juce::Colours::red);
                g.fillRect(b);
            }
        }

        struct DebugCell : juce::Label
        {
            int row{0}, col{0};
            DebugDataModel *model{nullptr};
            DebugCell(DebugDataModel *m) : model(m) {}
            void paint(juce::Graphics &g) override
            {
                model->paintCell(g, row, col, getWidth(), getHeight(), false);
            }

            void updateAccessibility()
            {
                setAccessible(true);
                setText(model->getText(row, col), juce::dontSendNotification);
            }

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugCell);
        };
        friend class DebugCell;

        Component *refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                           Component *existingComponentToUpdate) override
        {
            DebugCell *cell{nullptr};
            if (existingComponentToUpdate)
            {
                cell = dynamic_cast<DebugCell *>(existingComponentToUpdate);
                if (!cell)
                    delete existingComponentToUpdate;
            }
            if (!cell)
            {
                cell = new DebugCell(this);
            }
            cell->row = rowNumber;
            cell->col = columnId;
            cell->updateAccessibility();

            return cell;
        }
    };

    std::unique_ptr<DebugDataModel> debugTableDataModel;

    std::unique_ptr<juce::Label> dPhaseLabel;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { debugTableDataModel->setSkin(skin, associatedBitmapStore); }

    void setOpen(bool b)
    {
        isOpen = b;
        editor->getEditState().debuggerOpen = b;
        setVisible(b);
        editor->resized();
    }

    void resized() override
    {
        if (isOpen)
        {
            int margin = 4;

            debugTable->setBounds(getLocalBounds().reduced(margin));
            auto w = getLocalBounds().reduced(margin).getWidth() - 10;
            debugTable->getHeader().setColumnWidth(1, w / 2);
            debugTable->getHeader().setColumnWidth(2, w / 2);
        }
    }

    std::unique_ptr<LFOModulationSource> lfoDebugger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpandingFormulaDebugger);
};

struct FormulaControlArea : public juce::Component,
                            public Surge::GUI::SkinConsumingComponent,
                            public Surge::GUI::IComponentTagValue::Listener
{
    enum tags
    {
        tag_select_tab = 0x575200,
        tag_code_apply,
        tag_debugger_show,
        tag_debugger_init,
        tag_debugger_step
    };

    FormulaModulatorEditor *overlay{nullptr};
    SurgeGUIEditor *editor{nullptr};

    FormulaControlArea(FormulaModulatorEditor *ol, SurgeGUIEditor *ed) : overlay(ol), editor(ed)
    {
        setAccessible(true);
        setTitle("Controls");
        setDescription("Controls");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    }

    void resized() override
    {
        if (skin)
        {
            rebuild();
        }
    }

    void rebuild()
    {
        int labelHeight = 12;
        int buttonHeight = 14;
        int numfieldHeight = 12;
        int margin = 2;
        int xpos = 10;

        removeAllChildren();
        auto h = getHeight();

        {
            int marginPos = xpos + margin;
            int btnWidth = 100;
            int ypos = 1 + labelHeight + margin;

            codeL = newL("Code");
            codeL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*codeL);

            codeS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

            codeS->setBounds(btnrect);
            codeS->setStorage(overlay->storage);
            codeS->setTitle("Code Selection");
            codeS->setDescription("Code Selection");
            codeS->setLabels({"Modulator", "Prelude"});
            codeS->addListener(this);
            codeS->setTag(tag_select_tab);
            codeS->setHeightOfOneImage(buttonHeight);
            codeS->setRows(1);
            codeS->setColumns(2);
            codeS->setDraggable(true);
            codeS->setValue(overlay->getEditState().codeOrPrelude);
            codeS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*codeS);
            marginPos += btnWidth + margin;

            applyS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(getWidth() / 2 - 30, ypos - 1, 60, buttonHeight);

            applyS->setBounds(btnrect);
            applyS->setTitle("Apply");
            applyS->setDescription("Apply");
            applyS->setStorage(overlay->storage);
            applyS->setLabels({"Apply"});
            applyS->addListener(this);
            applyS->setTag(tag_code_apply);
            applyS->setHeightOfOneImage(buttonHeight);
            applyS->setRows(1);
            applyS->setColumns(1);
            applyS->setDraggable(true);
            applyS->setSkin(skin, associatedBitmapStore);
            applyS->setEnabled(false);
            addAndMakeVisible(*applyS);
            xpos += 60 + 10;
        }

        // Debugger Controls from the left
        {
            debugL = newL("Debugger");
            debugL->setBounds(getWidth() - 10 - 100, 1, 100, labelHeight);
            debugL->setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(*debugL);

            int btnWidth = 60;
            int bpos = getWidth() - 10 - btnWidth;
            int ypos = 1 + labelHeight + margin;

            auto ma = [&](const std::string &l, tags t) {
                auto res = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
                auto btnrect = juce::Rectangle<int>(bpos, ypos - 1, btnWidth, buttonHeight);

                res->setBounds(btnrect);
                res->setStorage(overlay->storage);
                res->setLabels({l});
                res->addListener(this);
                res->setTag(t);
                res->setHeightOfOneImage(buttonHeight);
                res->setRows(1);
                res->setColumns(1);
                res->setDraggable(false);
                res->setSkin(skin, associatedBitmapStore);
                res->setValue(0);
                return res;
            };

            auto isOpen = overlay->debugPanel->isOpen;
            showS = ma(isOpen ? "Hide" : "Show", tag_debugger_show);
            addAndMakeVisible(*showS);
            bpos -= btnWidth + margin;

            stepS = ma("Step", tag_debugger_step);
            stepS->setVisible(isOpen);
            addChildComponent(*stepS);
            bpos -= btnWidth + margin;

            initS = ma("Init", tag_debugger_init);
            initS->setVisible(isOpen);
            addChildComponent(*initS);
            bpos -= btnWidth + margin;
        }
    }

    std::unique_ptr<juce::Label> newL(const std::string &s)
    {
        auto res = std::make_unique<juce::Label>(s, s);
        res->setText(s, juce::dontSendNotification);
        res->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
        res->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        return res;
    }

    int32_t controlModifierClicked(GUI::IComponentTagValue *c, const juce::ModifierKeys &mods,
                                   bool isDoubleClickEvent) override
    {
        auto tag = (tags)(c->getTag());

        switch (tag)
        {
        case tag_select_tab:
        case tag_code_apply:
        case tag_debugger_show:
        case tag_debugger_init:
        case tag_debugger_step:
        {
            juce::PopupMenu contextMenu;

            auto msurl = editor->helpURLForSpecial("formula-editor");
            auto hurl = editor->fullyResolvedHelpURL(msurl);

            editor->addHelpHeaderTo("Formula Editor", hurl, contextMenu);

            contextMenu.showMenuAsync(editor->popupMenuOptions(this, false),
                                      Surge::GUI::makeEndHoverCallback(c));
        }
        break;
        default:
            break;
        }

        return 0;
    }

    void valueChanged(GUI::IComponentTagValue *c) override
    {
        auto tag = (tags)(c->getTag());

        switch (tag)
        {
        case tag_select_tab:
        {
            int m = c->getValue();

            if (m > 0.5)
            {
                overlay->showPreludeCode();
            }
            else
            {
                overlay->showModulatorCode();
            }
        }
        break;
        case tag_code_apply:
        {
            overlay->applyCode();
        }
        break;
        case tag_debugger_show:
        {
            if (overlay->debugPanel->isOpen)
            {
                overlay->debugPanel->setOpen(false);
                showS->setLabels({"Show"});
                stepS->setVisible(false);
                initS->setVisible(false);
            }
            else
            {
                overlay->debugPanel->setOpen(true);
                showS->setLabels({"Hide"});
                stepS->setVisible(true);
                initS->setVisible(true);
            }

            repaint();
        }
        case tag_debugger_init:
        {
            overlay->debugPanel->initializeLfoDebugger();
        }
        break;
        case tag_debugger_step:
        {
            if (!overlay->debugPanel->lfoDebugger)
            {
                overlay->debugPanel->initializeLfoDebugger();
            }

            overlay->debugPanel->stepLfoDebugger();
        }
        break;
        default:
            break;
        }
    }

    std::unique_ptr<juce::Label> codeL, debugL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> codeS, applyS, showS, initS, stepS;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { rebuild(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormulaControlArea);
};

FormulaModulatorEditor::FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s, LFOStorage *ls,
                                               FormulaModulatorStorage *fs, int lid, int scene,
                                               Surge::GUI::Skin::ptr_t skin)
    : CodeEditorContainerWithApply(ed, s, skin, false), lfos(ls), scene(scene), formulastorage(fs),
      lfo_id(lid), editor(ed)
{
    mainEditor->setScrollbarThickness(8);
    mainEditor->setTitle("Formula Modulator Code");
    mainEditor->setDescription("Formula Modulator Code");

    mainDocument->insertText(0, fs->formulaString);

    preludeDocument = std::make_unique<juce::CodeDocument>();
    preludeDocument->insertText(0, Surge::LuaSupport::getSurgePrelude());

    preludeDisplay = std::make_unique<SurgeCodeEditorComponent>(*preludeDocument, tokenizer.get());
    preludeDisplay->setTabSize(4, true);
    preludeDisplay->setReadOnly(true);
    preludeDisplay->setScrollbarThickness(8);
    preludeDisplay->setTitle("Formula Modulator Prelude Code");
    preludeDisplay->setDescription("Formula Modulator Prelude Code");
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);

    controlArea = std::make_unique<FormulaControlArea>(this, editor);
    addAndMakeVisible(*controlArea);
    addAndMakeVisible(*mainEditor);
    addChildComponent(*preludeDisplay);

    debugPanel = std::make_unique<ExpandingFormulaDebugger>(this);
    debugPanel->setVisible(false);
    addChildComponent(*debugPanel);

    switch (getEditState().codeOrPrelude)
    {
    case 0:
        showModulatorCode();
        break;
    case 1:
        showPreludeCode();
        break;
    }

    if (getEditState().debuggerOpen)
    {
        debugPanel->setOpen(true);
        debugPanel->initializeLfoDebugger();
        repaint();
    }
}

FormulaModulatorEditor::~FormulaModulatorEditor() = default;

DAWExtraStateStorage::EditorState::FormulaEditState &FormulaModulatorEditor::getEditState()
{
    return storage->getPatch().dawExtraState.editor.formulaEditState[scene][lfo_id];
}
void FormulaModulatorEditor::onSkinChanged()
{
    CodeEditorContainerWithApply::onSkinChanged();
    preludeDisplay->setFont(skin->getFont(Fonts::LuaEditor::Code));
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);
    controlArea->setSkin(skin, associatedBitmapStore);
    debugPanel->setSkin(skin, associatedBitmapStore);
}

void FormulaModulatorEditor::applyCode()
{
    editor->undoManager()->pushFormula(scene, lfo_id, *formulastorage);
    formulastorage->setFormula(mainDocument->getAllContent().toStdString());
    storage->getPatch().isDirty = true;
    updateDebuggerIfNeeded();
    editor->repaintFrame();
    juce::SystemClipboard::copyTextToClipboard(formulastorage->formulaString);
    setApplyEnabled(false);
    mainEditor->grabKeyboardFocus();
}

void FormulaModulatorEditor::updateDebuggerIfNeeded()
{
    {
        if (debugPanel->isOpen)
        {
            bool anyUpdate{false};
            auto lfodata = lfos;

#define CK(x)                                                                                      \
    {                                                                                              \
        auto &r = debugPanel->tp[lfodata->x.param_id_in_scene];                                    \
                                                                                                   \
        if (r.i != lfodata->x.val.i)                                                               \
        {                                                                                          \
            r.i = lfodata->x.val.i;                                                                \
            anyUpdate = true;                                                                      \
        }                                                                                          \
    }

            CK(rate);
            CK(magnitude);
            CK(start_phase);
            CK(deform);

            if (debugPanel->lfoDebugger->formulastate.tempo != storage->temposyncratio * 120)
            {
                anyUpdate = true;
            }

#undef CK

#define CKENV(x, y)                                                                                \
    {                                                                                              \
        auto &tgt = debugPanel->lfoDebugger->formulastate.x;                                       \
        auto src = lfodata->y.value_to_normalized(lfodata->y.val.f);                               \
                                                                                                   \
        if (tgt != src)                                                                            \
        {                                                                                          \
            tgt = src;                                                                             \
            anyUpdate = true;                                                                      \
        }                                                                                          \
    }
            CKENV(del, delay);
            CKENV(a, attack);
            CKENV(h, hold);
            CKENV(dec, decay);
            CKENV(s, sustain);
            CKENV(r, release);

#undef CKENV

            if (anyUpdate)
            {
                debugPanel->refreshDebuggerView();
                editor->repaintFrame();
            }
        }
    }
    updateDebuggerCounter = (updateDebuggerCounter + 1) & 31;
}

void FormulaModulatorEditor::forceRefresh()
{
    mainDocument->replaceAllContent(formulastorage->formulaString);
    editor->repaintFrame();
}

void FormulaModulatorEditor::setApplyEnabled(bool b)
{
    if (controlArea)
    {
        controlArea->applyS->setEnabled(b);
        controlArea->applyS->repaint();
    }
}

void FormulaModulatorEditor::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    t.transformPoint(w, h);

    int controlHeight = 35;
    int debugPanelWidth = 0;
    int debugPanelMargin = 0;
    if (debugPanel->isVisible())
    {
        debugPanelWidth = 200;
        debugPanelMargin = 2;
    }
    auto edRect = juce::Rectangle<int>(2, 2, w - 4 - debugPanelMargin - debugPanelWidth,
                                       h - controlHeight - 4);
    mainEditor->setBounds(edRect);
    preludeDisplay->setBounds(edRect);
    if (debugPanel->isVisible())
    {
        debugPanel->setBounds(w - 4 - debugPanelWidth + debugPanelMargin, 2, debugPanelWidth,
                              h - 4 - controlHeight);
    }
    controlArea->setBounds(0, h - controlHeight, w, controlHeight);
}

void FormulaModulatorEditor::showModulatorCode()
{
    preludeDisplay->setVisible(false);
    mainEditor->setVisible(true);
    getEditState().codeOrPrelude = 0;
}

void FormulaModulatorEditor::showPreludeCode()
{
    preludeDisplay->setVisible(true);
    mainEditor->setVisible(false);
    getEditState().codeOrPrelude = 1;
}

void FormulaModulatorEditor::escapeKeyPressed()
{
    auto c = getParentComponent();
    while (c)
    {
        if (auto olw = dynamic_cast<OverlayWrapper *>(c))
        {
            olw->onClose();
            return;
        }
        c = c->getParentComponent();
    }
}

std::optional<std::pair<std::string, std::string>>
FormulaModulatorEditor::getPreCloseChickenBoxMessage()
{
    if (controlArea->applyS->isEnabled())
    {
        return std::make_pair("Close Formula Editor",
                              "Do you really want to close the formula editor? Any "
                              "changes that were not applied will be lost!");
    }
    return std::nullopt;
}

struct WavetablePreviewComponent : juce::Component
{
    WavetablePreviewComponent(SurgeStorage *s, OscillatorStorage *os, Surge::GUI::Skin::ptr_t skin)
        : storage(s), osc(os), skin(skin)
    {
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0, 0, 0));
        g.setFont(skin->fontManager->getFiraMonoAtSize(9));

        g.setColour(juce::Colour(230, 230, 255)); // could be a skin->getColor of course
        auto s1 = std::string("Frame : ") + std::to_string(frameNumber);
        auto s2 = std::string("Res   : ") + std::to_string(points.size());
        g.drawSingleLineText(s1, 3, 18);
        g.drawSingleLineText(s2, 3, 30);

        g.setColour(juce::Colour(160, 160, 160));
        auto h = getHeight();
        auto w = getWidth();

        auto t = h * 0.05;
        auto m = h * 0.5;
        auto b = h * 0.95;
        g.drawLine(0, t, w, t);
        g.drawLine(0, m, w, m);
        g.drawLine(0, b, w, b);

        auto p = juce::Path();
        auto dx = 1.0 / (points.size() - 1);
        for (int i = 0; i < points.size(); ++i)
        {
            float xp = dx * i * w;
            float yp = -((points[i] + 1) * 0.5) * (b - t) + b;
            if (i == 0)
                p.startNewSubPath(xp, yp);
            else
                p.lineTo(xp, yp);
        }
        g.setColour(juce::Colour(255, 180, 0));
        g.strokePath(p, juce::PathStrokeType(1.0));
    }

    int frameNumber = 0;
    std::vector<float> points;

    SurgeStorage *storage;
    OscillatorStorage *osc;
    Surge::GUI::Skin::ptr_t skin;
};

WavetableEquationEditor::WavetableEquationEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                                 OscillatorStorage *os,
                                                 Surge::GUI::Skin::ptr_t skin)
    : CodeEditorContainerWithApply(ed, s, skin, true), osc(os)
{
    if (osc->wavetable_formula == "")
    {
        mainDocument->insertText(0, Surge::WavetableScript::defaultWavetableFormula());
    }
    else
    {
        mainDocument->insertText(0, osc->wavetable_formula);
    }

    resolutionLabel = std::make_unique<juce::Label>("resLabl");
    resolutionLabel->setFont(skin->fontManager->getLatoAtSize(10));
    resolutionLabel->setText("Resolution:", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(resolutionLabel.get());

    resolution = std::make_unique<juce::ComboBox>("res");
    int id = 1, grid = 32;
    while (grid <= 4096)
    {
        resolution->addItem(std::to_string(grid), id);
        id++;
        grid *= 2;
    }
    resolution->setSelectedId(os->wavetable_formula_res_base,
                              juce::NotificationType::dontSendNotification);
    addAndMakeVisible(resolution.get());

    framesLabel = std::make_unique<juce::Label>("frmLabl");
    framesLabel->setFont(skin->fontManager->getLatoAtSize(10));
    framesLabel->setText("Frames:", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(framesLabel.get());

    frames = std::make_unique<juce::TextEditor>("frm");
    frames->setFont(skin->fontManager->getLatoAtSize(10));
    frames->setText(std::to_string(osc->wavetable_formula_nframes),
                    juce::NotificationType::dontSendNotification);
    addAndMakeVisible(frames.get());

    generate = std::make_unique<juce::TextButton>("gen");
    generate->setButtonText("Generate");
    generate->addListener(this);
    addAndMakeVisible(generate.get());

    renderer = std::make_unique<WavetablePreviewComponent>(storage, osc, skin);
    addAndMakeVisible(renderer.get());

    currentFrame = std::make_unique<juce::Slider>("currF");
    currentFrame->setSliderStyle(juce::Slider::LinearVertical);
    currentFrame->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    currentFrame->setRange(0.0, 10.0);
    currentFrame->addListener(this);
    addAndMakeVisible(currentFrame.get());
}

WavetableEquationEditor::~WavetableEquationEditor() noexcept = default;

void WavetableEquationEditor::resized()
{
    auto w = getWidth() - 5;
    auto h = getHeight() - 5; // this is a hack obvs
    int m = 3;

    int topH = 20;
    int itemW = 100;

    int renderH = 150;

    resolutionLabel->setBounds(m, m, itemW, topH);
    resolution->setBounds(m * 2 + itemW, m, itemW, topH);
    framesLabel->setBounds(m * 3 + 2 * itemW, m, itemW, topH);
    frames->setBounds(m * 4 + 3 * itemW, m, itemW, topH);

    generate->setBounds(w - m - itemW, m, itemW, topH);
    applyButton->setBounds(w - 2 * m - 2 * itemW, m, itemW, topH);

    mainEditor->setBounds(m, m * 2 + topH, w - 2 * m, h - topH - renderH - 3 * m);

    currentFrame->setBounds(m, h - m - renderH, 32, renderH);
    renderer->setBounds(m + 32, h - m - renderH, w - 2 * m - 32, renderH);

    rerenderFromUIState();
}

void WavetableEquationEditor::applyCode()
{
    osc->wavetable_formula = mainDocument->getAllContent().toStdString();
    osc->wavetable_formula_res_base = resolution->getSelectedId();
    osc->wavetable_formula_nframes = std::atoi(frames->getText().toRawUTF8());

    applyButton->setEnabled(false);
    rerenderFromUIState();

    editor->repaintFrame();
}

void WavetableEquationEditor::rerenderFromUIState()
{
    auto resi = resolution->getSelectedId();
    auto nfr = std::atoi(frames->getText().toRawUTF8());
    auto cfr = (int)round(nfr * currentFrame->getValue() / 10.0);

    auto respt = 32;
    for (int i = 1; i < resi; ++i)
        respt *= 2;

    renderer->points = Surge::WavetableScript::evaluateScriptAtFrame(
        mainDocument->getAllContent().toStdString(), respt, cfr, nfr);
    renderer->frameNumber = cfr;
    renderer->repaint();
}

void WavetableEquationEditor::comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged)
{
    rerenderFromUIState();
}
void WavetableEquationEditor::sliderValueChanged(juce::Slider *slider) { rerenderFromUIState(); }
void WavetableEquationEditor::buttonClicked(juce::Button *button)
{
    if (button == generate.get())
    {
        std::cout << "GENERATE" << std::endl;
        auto resi = resolution->getSelectedId();
        auto nfr = std::atoi(frames->getText().toRawUTF8());
        auto respt = 32;
        for (int i = 1; i < resi; ++i)
            respt *= 2;

        wt_header wh;
        float *wd = nullptr;
        Surge::WavetableScript::constructWavetable(mainDocument->getAllContent().toStdString(),
                                                   respt, nfr, wh, &wd);
        storage->waveTableDataMutex.lock();
        osc->wt.BuildWT(wd, wh, wh.flags & wtf_is_sample);
        osc->wavetable_display_name = "Scripted Wavetable";
        storage->waveTableDataMutex.unlock();

        delete[] wd;
        editor->repaintFrame();

        return;
    }
    CodeEditorContainerWithApply::buttonClicked(button);
}

} // namespace Overlays
} // namespace Surge
