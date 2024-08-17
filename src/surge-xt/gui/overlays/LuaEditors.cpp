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
#include "SurgeImage.h"
#include "SurgeImageStore.h"
#include "widgets/MultiSwitch.h"
#include "widgets/NumberField.h"
#include "overlays/TypeinParamEditor.h"
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
                                                    editor->storage->getPatch(), editor->scene);

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
                                                    editor->storage->getPatch(), editor->scene);
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

  private:
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
        int margin = 2;
        int xpos = 10;
        int ypos = 1 + labelHeight + margin;
        int marginPos = xpos + margin;
        removeAllChildren();

        {
            int btnWidth = 100;

            codeL = newL("Code");
            codeL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*codeL);

            codeS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);
            codeS->setBounds(btnrect);
            codeS->setStorage(overlay->storage);
            codeS->setTitle("Code Selection");
            codeS->setDescription("Code Selection");
            codeS->setLabels({"Editor", "Prelude"});
            codeS->addListener(this);
            codeS->setTag(tag_select_tab);
            codeS->setHeightOfOneImage(buttonHeight);
            codeS->setRows(1);
            codeS->setColumns(2);
            codeS->setDraggable(true);
            codeS->setValue(overlay->getEditState().codeOrPrelude);
            codeS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*codeS);

            btnWidth = 60;

            applyS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(getWidth() / 2 - 30, ypos - 1, btnWidth, buttonHeight);
            applyS->setBounds(btnrect);
            applyS->setStorage(overlay->storage);
            applyS->setTitle("Apply");
            applyS->setDescription("Apply");
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

    int32_t controlModifierClicked(GUI::IComponentTagValue *pControl,
                                   const juce::ModifierKeys &mods, bool isDoubleClickEvent) override
    {
        int tag = pControl->getTag();

        switch (tag)
        {
        case tag_select_tab:
        case tag_code_apply:
        case tag_debugger_show:
        case tag_debugger_init:
        case tag_debugger_step:
        {
            auto contextMenu = juce::PopupMenu();

            auto msurl = editor->helpURLForSpecial("formula-editor");
            auto hurl = editor->fullyResolvedHelpURL(msurl);

            editor->addHelpHeaderTo("Formula Editor", hurl, contextMenu);

            contextMenu.showMenuAsync(editor->popupMenuOptions(this, false),
                                      Surge::GUI::makeEndHoverCallback(pControl));
        }
        break;
        default:
            break;
        }
        return 1;
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
            overlay->applyCode();
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
            overlay->debugPanel->initializeLfoDebugger();
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

  private:
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
    preludeDocument->insertText(0, Surge::LuaSupport::getFormulaPrelude());

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
    auto width = getWidth();
    auto height = getHeight();
    t.transformPoint(width, height);

    int controlHeight = 35;
    int debugPanelWidth = 0;
    int debugPanelMargin = 0;

    if (debugPanel->isVisible())
    {
        debugPanelWidth = 215;
        debugPanelMargin = 2;
    }
    auto edRect = juce::Rectangle<int>(2, 2, width - 4 - debugPanelMargin - debugPanelWidth,
                                       height - controlHeight - 4);
    mainEditor->setBounds(edRect);
    preludeDisplay->setBounds(edRect);
    if (debugPanel->isVisible())
    {
        debugPanel->setBounds(width - 4 - debugPanelWidth + debugPanelMargin, 2, debugPanelWidth,
                              height - 4 - controlHeight);
    }
    controlArea->setBounds(0, height - controlHeight, width, controlHeight);
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

struct WavetablePreviewComponent : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    WavetableScriptEditor *overlay{nullptr};
    SurgeGUIEditor *editor{nullptr};
    Surge::GUI::Skin::ptr_t skin;

    WavetablePreviewComponent(WavetableScriptEditor *ol, SurgeGUIEditor *ed,
                              Surge::GUI::Skin::ptr_t skin)
        : overlay(ol), editor(ed), skin(skin)

    {
    }

    void paint(juce::Graphics &g) override
    {
        auto height = getHeight();
        auto width = getWidth();
        auto middle = height * 0.5;
        int axisSpaceX = 18;

        juce::Rectangle<float> drawArea(axisSpaceX, 0, width - axisSpaceX, height);
        juce::Rectangle<float> vaxisArea(0, 0, axisSpaceX, height);

        // auto primaryFont = skin->fontManager->getLatoAtSize(9, juce::Font::bold);
        auto secondaryFont = skin->fontManager->getLatoAtSize(7);

        g.setColour(skin->getColor(Colors::MSEGEditor::Background));
        g.fillRect(drawArea);

        // Vertical axis
        std::vector<std::string> txt = {"1.0", "0.0", "-1.0"};
        g.setFont(secondaryFont);
        g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
        g.drawText(txt[0], vaxisArea.getX() - 3, 4, vaxisArea.getWidth(), 12,
                   juce::Justification::topRight);
        g.drawText(txt[1], vaxisArea.getX() - 3, middle - 12, vaxisArea.getWidth(), 12,
                   juce::Justification::bottomRight);
        g.drawText(txt[2], vaxisArea.getX() - 3, height - 14, vaxisArea.getWidth(), 12,
                   juce::Justification::centredRight);

        // Grid
        g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryHorizontal));
        for (float y : {0.25f, 0.75f})
            g.drawLine(drawArea.getX() - 8, height * y, width, height * y);

        g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical));
        for (float x : {0.25f, 0.5f, 0.75f})
            g.drawLine(drawArea.getX() + drawArea.getWidth() * x, 0,
                       drawArea.getX() + drawArea.getWidth() * x, height);

        // Borders
        g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
        g.drawLine(4, 0, width, 0);
        g.drawLine(4, height, width, height);
        g.drawLine(axisSpaceX, 0, axisSpaceX, height);
        g.drawLine(width, 0, width, height);
        g.drawLine(axisSpaceX, middle, width, middle);

        // Graph
        auto p = juce::Path();
        if (!points.empty())
        {
            float dx = (width - axisSpaceX) / float(points.size() - 1);
            for (int i = 0; i < points.size(); ++i)
            {
                float xp = dx * i;
                float yp = 0.5f * (1 - points[i]) * height;

                if (yp < 0.0f) // clamp to vertical bounds
                    yp = 0.0f;
                else if (yp > height)
                    yp = height;

                if (i == 0)
                    p.startNewSubPath(xp + axisSpaceX, middle);

                p.lineTo(xp + axisSpaceX, yp);

                if (i == points.size() - 1)
                    p.lineTo(xp + axisSpaceX, middle);
            }

            auto cg = juce::ColourGradient::vertical(
                skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
                skin->getColor(Colors::MSEGEditor::GradientFill::StartColor), drawArea);
            cg.addColour(0.5, skin->getColor(Colors::MSEGEditor::GradientFill::EndColor));

            g.setGradientFill(cg);
            g.fillPath(p);

            g.setColour(skin->getColor(Colors::MSEGEditor::Curve));
            g.strokePath(p, juce::PathStrokeType(1.0));
        }
    }

    void mouseDown(const juce::MouseEvent &event) override
    {
        lastDrag = event.getPosition().x + -event.getPosition().y;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void mouseDrag(const juce::MouseEvent &event) override
    {
        int currentDrag = event.getPosition().x + -event.getPosition().y;
        int delta = (currentDrag - lastDrag) * 2;
        lastDrag = currentDrag;

        int value = 0;
        if (delta > 0)
            value = 1;
        else if (delta < 0)
            value = -1;

        overlay->setCurrentFrame(value);
        repaint();
    }

    void mouseUp(const juce::MouseEvent &event) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    int frameNumber{1};
    std::vector<float> points;

  private:
    int lastDrag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetablePreviewComponent);
};

struct WavetableScriptControlArea : public juce::Component,
                                    public Surge::GUI::SkinConsumingComponent,
                                    public Surge::GUI::IComponentTagValue::Listener
{
    enum tags
    {
        tag_select_tab = 0x597500,
        tag_code_apply,
        tag_current_frame,
        tag_frames_value,
        tag_res_value,
        tag_generate_wt
    };

    WavetableScriptEditor *overlay{nullptr};
    SurgeGUIEditor *editor{nullptr};

    WavetableScriptControlArea(WavetableScriptEditor *ol, SurgeGUIEditor *ed)
        : overlay(ol), editor(ed)
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
        removeAllChildren();

        {
            int btnWidth = 100;

            int labelHeight = 12;
            int buttonHeight = 14;
            int numfieldWidth = 32;
            int numfieldHeight = 12;

            int margin = 2;
            int xpos = 10;
            int ypos = 1 + labelHeight + margin;
            int marginPos = xpos + margin;

            codeL = newL("Code");
            codeL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*codeL);

            codeS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);
            codeS->setBounds(btnrect);
            codeS->setStorage(overlay->storage);
            codeS->setTitle("Code Selection");
            codeS->setDescription("Code Selection");
            codeS->setLabels({"Editor", "Prelude"});
            codeS->addListener(this);
            codeS->setTag(tag_select_tab);
            codeS->setHeightOfOneImage(buttonHeight);
            codeS->setRows(1);
            codeS->setColumns(2);
            codeS->setDraggable(true);
            codeS->setValue(overlay->getEditState().codeOrPrelude);
            codeS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*codeS);

            btnWidth = 60;

            applyS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(getWidth() / 2 - 30, ypos - 1, btnWidth, buttonHeight);
            applyS->setBounds(btnrect);
            applyS->setStorage(overlay->storage);
            applyS->setTitle("Apply");
            applyS->setDescription("Apply");
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

            int bpos = getWidth() - marginPos - numfieldWidth * 3 - btnWidth - 10;
            auto images = skin->standardHoverAndHoverOnForIDB(IDB_MSEG_SNAPVALUE_NUMFIELD,
                                                              associatedBitmapStore);

            currentFrameL = newL("View");
            currentFrameL->setBounds(bpos - 3, 1, 100, labelHeight);
            addAndMakeVisible(*currentFrameL);

            currentFrameN = std::make_unique<Surge::Widgets::NumberField>();
            currentFrameN->setControlMode(Surge::Skin::Parameters::WTSE_FRAMES);
            currentFrameN->setIntValue(1);
            currentFrameN->addListener(this);
            currentFrameN->setTag(tag_current_frame);
            currentFrameN->setStorage(overlay->storage);
            currentFrameN->setTitle("Current Frame");
            currentFrameN->setDescription("Current Frame");
            currentFrameN->setSkin(skin, associatedBitmapStore);
            btnrect = juce::Rectangle<int>(bpos, ypos - 1, numfieldWidth, numfieldHeight);
            currentFrameN->setBounds(btnrect);
            currentFrameN->setBackgroundDrawable(images[0]);
            currentFrameN->setHoverBackgroundDrawable(images[1]);
            currentFrameN->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
            currentFrameN->setHoverTextColour(
                skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
            addAndMakeVisible(*currentFrameN);

            bpos += numfieldWidth + 5;

            framesL = newL("Frames");
            framesL->setBounds(bpos - 3, 1, 100, labelHeight);
            addAndMakeVisible(*framesL);

            framesN = std::make_unique<Surge::Widgets::NumberField>();
            framesN->setControlMode(Surge::Skin::Parameters::WTSE_FRAMES);
            framesN->setIntValue(overlay->osc->wavetable_formula_nframes);
            framesN->addListener(this);
            framesN->setTag(tag_frames_value);
            framesN->setStorage(overlay->storage);
            framesN->setTitle("Max Frame");
            framesN->setDescription("Max Frame");
            framesN->setSkin(skin, associatedBitmapStore);
            btnrect = juce::Rectangle<int>(bpos, ypos - 1, numfieldWidth, numfieldHeight);
            framesN->setBounds(btnrect);
            framesN->setBackgroundDrawable(images[0]);
            framesN->setHoverBackgroundDrawable(images[1]);
            framesN->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
            framesN->setHoverTextColour(skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
            framesN->onReturnPressed = [w = juce::Component::SafePointer(this)](auto tag, auto nf) {
                if (w)
                {
                    w->overlay->rerenderFromUIState();
                    return true;
                }
                return false;
            };
            addAndMakeVisible(*framesN);

            bpos += numfieldWidth + 5;

            resolutionL = newL("Samples");
            resolutionL->setBounds(bpos - 3, 1, 100, labelHeight);
            addAndMakeVisible(*resolutionL);

            resolutionN = std::make_unique<Surge::Widgets::NumberField>();
            resolutionN->setControlMode(Surge::Skin::Parameters::WTSE_RESOLUTION);
            resolutionN->setIntValue(overlay->osc->wavetable_formula_res_base);
            resolutionN->addListener(this);
            resolutionN->setTag(tag_res_value);
            resolutionN->setStorage(overlay->storage);
            resolutionN->setTitle("Samples");
            resolutionN->setDescription("Samples");
            resolutionN->setSkin(skin, associatedBitmapStore);
            btnrect = juce::Rectangle<int>(bpos, ypos - 1, numfieldWidth, numfieldHeight);
            resolutionN->setBounds(btnrect);
            resolutionN->setBackgroundDrawable(images[0]);
            resolutionN->setHoverBackgroundDrawable(images[1]);
            resolutionN->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
            resolutionN->setHoverTextColour(
                skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
            addAndMakeVisible(*resolutionN);

            bpos += numfieldWidth + 5;

            generateS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(bpos, ypos - 1, btnWidth, buttonHeight);
            generateS->setBounds(btnrect);
            generateS->setStorage(overlay->storage);
            generateS->setTitle("Generate");
            generateS->setDescription("Generate");
            generateS->setLabels({"Generate"});
            generateS->addListener(this);
            generateS->setTag(tag_generate_wt);
            generateS->setHeightOfOneImage(buttonHeight);
            generateS->setRows(1);
            generateS->setColumns(1);
            generateS->setDraggable(false);
            generateS->setSkin(skin, associatedBitmapStore);
            generateS->setEnabled(true);
            addAndMakeVisible(*generateS);
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

    int32_t controlModifierClicked(GUI::IComponentTagValue *pControl,
                                   const juce::ModifierKeys &mods, bool isDoubleClickEvent) override
    {
        int tag = pControl->getTag();

        std::vector<std::pair<std::string, float>> options;
        bool hasTypein = false;
        std::string menuName;

        switch (tag)
        {
        case tag_select_tab:
        case tag_code_apply:
        case tag_current_frame:
        case tag_res_value:
        case tag_generate_wt:
        {
            auto contextMenu = juce::PopupMenu();

            auto msurl = editor->helpURLForSpecial("wtse-editor");
            auto hurl = editor->fullyResolvedHelpURL(msurl);

            editor->addHelpHeaderTo("WTSE Editor", hurl, contextMenu);

            contextMenu.showMenuAsync(editor->popupMenuOptions(this, false),
                                      Surge::GUI::makeEndHoverCallback(pControl));
        }
        break;
        case tag_frames_value:
        {
            hasTypein = true;
            menuName = "WTSE Wavetable Frame Amount";

            auto addStop = [&options](int v) {
                options.push_back(
                    std::make_pair(std::to_string(v), Parameter::intScaledToFloat(v, 256, 1)));
            };

            addStop(10);
            addStop(16);
            addStop(20);
            addStop(32);
            addStop(50);
            addStop(64);
            addStop(100);
            addStop(128);
            addStop(200);
            addStop(256);
            break;
        }
        default:
            break;
        }

        if (!options.empty())
        {
            auto contextMenu = juce::PopupMenu();

            auto msurl = SurgeGUIEditor::helpURLForSpecial(overlay->storage, "wtse-editor");
            auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);
            auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(menuName, hurl);

            tcomp->setSkin(skin, associatedBitmapStore);
            auto hment = tcomp->getTitle();

            contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

            contextMenu.addSeparator();

            for (auto op : options)
            {
                auto val = op.second;

                contextMenu.addItem(op.first, true, (val == pControl->getValue()),
                                    [val, pControl, this]() {
                                        pControl->setValue(val);
                                        valueChanged(pControl);

                                        auto iv = pControl->asJuceComponent();

                                        if (iv)
                                        {
                                            iv->repaint();
                                        }
                                    });
            }

            if (hasTypein)
            {
                contextMenu.addSeparator();

                auto c = pControl->asJuceComponent();

                auto handleTypein = [c, pControl, this](const std::string &s) {
                    auto i = std::atoi(s.c_str());

                    if (i >= 1 && i <= 256)
                    {
                        pControl->setValue(Parameter::intScaledToFloat(i, 256, 1));
                        valueChanged(pControl);

                        if (c)
                        {
                            c->repaint();
                        }

                        return true;
                    }
                    return false;
                };

                auto val =
                    std::to_string(Parameter::intUnscaledFromFloat(pControl->getValue(), 256, 1));

                auto showTypein = [this, c, handleTypein, menuName, pControl, val]() {
                    if (!typeinEditor)
                    {
                        typeinEditor =
                            std::make_unique<Surge::Overlays::TypeinLambdaEditor>(handleTypein);
                        getParentComponent()->addChildComponent(*typeinEditor);
                    }

                    typeinEditor->callback = handleTypein;
                    typeinEditor->setMainLabel(menuName);
                    typeinEditor->setValueLabels("current: " + val, "");
                    typeinEditor->setSkin(skin, associatedBitmapStore);
                    typeinEditor->setEditableText(val);
                    typeinEditor->setReturnFocusTarget(c);

                    auto topOfControl = c->getParentComponent()->getY();
                    auto pb = c->getBounds();
                    auto cx = pb.getCentreX();

                    auto r = typeinEditor->getRequiredSize();
                    cx -= r.getWidth() / 2;
                    r = r.withBottomY(topOfControl).withX(cx);
                    typeinEditor->setBounds(r);

                    typeinEditor->setVisible(true);
                    typeinEditor->grabFocus();
                };

                contextMenu.addItem(Surge::GUI::toOSCase("Edit Value: ") + val, true, false,
                                    showTypein);
            }
            contextMenu.showMenuAsync(editor->popupMenuOptions(),
                                      Surge::GUI::makeEndHoverCallback(pControl));
        }
        return 1;
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
            overlay->applyCode();
            break;
        case tag_current_frame:
        {
            int currentFrame = currentFrameN->getIntValue();
            int maxFrames = framesN->getIntValue();
            if (currentFrame > maxFrames)
            {
                currentFrame = maxFrames;
                currentFrameN->setIntValue(currentFrame);
            }
            overlay->rendererComponent->frameNumber = currentFrame;
            overlay->rerenderFromUIState();
        }
        break;
        case tag_frames_value:
        {
            int currentFrame = currentFrameN->getIntValue();
            int maxFrames = framesN->getIntValue();
            if (currentFrame > maxFrames)
            {
                currentFrameN->setIntValue(maxFrames);
                overlay->rendererComponent->frameNumber = maxFrames;
            }
            overlay->osc->wavetable_formula_nframes = maxFrames;
            overlay->rerenderFromUIState();
        }
        break;
        case tag_res_value:
        {
            overlay->osc->wavetable_formula_res_base = resolutionN->getIntValue();
            overlay->rerenderFromUIState();
        }
        break;

        case tag_generate_wt:
            overlay->generateWavetable();
            break;
        default:
            break;
        }
    }

    std::unique_ptr<Surge::Overlays::TypeinLambdaEditor> typeinEditor;
    std::unique_ptr<juce::Label> codeL, currentFrameL, framesL, resolutionL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> codeS, applyS, generateS;
    std::unique_ptr<Surge::Widgets::NumberField> currentFrameN, framesN, resolutionN;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { rebuild(); }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableScriptControlArea);
};

WavetableScriptEditor::WavetableScriptEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                             OscillatorStorage *os, int oid, int scene,
                                             Surge::GUI::Skin::ptr_t skin)

    : CodeEditorContainerWithApply(ed, s, skin, false), osc(os), osc_id(oid), scene(scene),
      editor(ed)
{
    mainEditor->setScrollbarThickness(8);
    mainEditor->setTitle("Wavetable Code");
    mainEditor->setDescription("Wavetable Code");

    if (osc->wavetable_formula == "")
    {
        mainDocument->insertText(0, Surge::WavetableScript::defaultWavetableScript());
    }
    else
    {
        mainDocument->insertText(0, osc->wavetable_formula);
    }

    preludeDocument = std::make_unique<juce::CodeDocument>();
    preludeDocument->insertText(0, Surge::LuaSupport::getWTSEPrelude());

    preludeDisplay = std::make_unique<SurgeCodeEditorComponent>(*preludeDocument, tokenizer.get());
    preludeDisplay->setTabSize(4, true);
    preludeDisplay->setReadOnly(true);
    preludeDisplay->setScrollbarThickness(8);
    preludeDisplay->setTitle("Wavetable Prelude Code");
    preludeDisplay->setDescription("Wavetable Prelude Code");
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);

    controlArea = std::make_unique<WavetableScriptControlArea>(this, editor);
    addAndMakeVisible(*controlArea);
    addAndMakeVisible(*mainEditor);
    addChildComponent(*preludeDisplay);

    rendererComponent = std::make_unique<WavetablePreviewComponent>(this, editor, skin);
    addAndMakeVisible(*rendererComponent);

    switch (getEditState().codeOrPrelude)
    {
    case 0:
        showModulatorCode();
        break;
    case 1:
        showPreludeCode();
        break;
    }
}

WavetableScriptEditor::~WavetableScriptEditor() = default;

DAWExtraStateStorage::EditorState::WavetableScriptEditState &WavetableScriptEditor::getEditState()
{
    return storage->getPatch().dawExtraState.editor.wavetableScriptEditState[scene][osc_id];
}

void WavetableScriptEditor::onSkinChanged()
{
    CodeEditorContainerWithApply::onSkinChanged();
    preludeDisplay->setFont(skin->getFont(Fonts::LuaEditor::Code));
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);
    controlArea->setSkin(skin, associatedBitmapStore);
    rendererComponent->setSkin(skin, associatedBitmapStore); // FIXME
}

void WavetableScriptEditor::applyCode()
{
    osc->wavetable_formula = mainDocument->getAllContent().toStdString();
    osc->wavetable_formula_res_base = controlArea->resolutionN->getIntValue();
    osc->wavetable_formula_nframes = controlArea->framesN->getIntValue();

    editor->repaintFrame();
    rerenderFromUIState();
    setApplyEnabled(false);
    mainEditor->grabKeyboardFocus();
}

void WavetableScriptEditor::forceRefresh()
{
    mainDocument->replaceAllContent(osc->wavetable_formula);
    editor->repaintFrame();
}

void WavetableScriptEditor::setApplyEnabled(bool b)
{
    if (controlArea)
    {
        controlArea->applyS->setEnabled(b);
        controlArea->applyS->repaint();
    }
}

void WavetableScriptEditor::resized()
{
    auto t = getTransform().inverted();
    auto width = getWidth();
    auto height = getHeight();
    t.transformPoint(width, height);

    int itemWidth = 100;
    int topHeight = 20;
    int controlHeight = 35;
    int rendererHeight = 125;

    auto edRect =
        juce::Rectangle<int>(2, 2, width - 4, height - controlHeight - rendererHeight - 6);
    mainEditor->setBounds(edRect);
    preludeDisplay->setBounds(edRect);
    controlArea->setBounds(0, height - controlHeight - rendererHeight - 2, width,
                           controlHeight + rendererHeight + 2);
    rendererComponent->setBounds(2, height - rendererHeight, width - 2, rendererHeight);

    rerenderFromUIState();
}

void WavetableScriptEditor::showModulatorCode()
{
    preludeDisplay->setVisible(false);
    mainEditor->setVisible(true);
    getEditState().codeOrPrelude = 0;
}

void WavetableScriptEditor::showPreludeCode()
{
    preludeDisplay->setVisible(true);
    mainEditor->setVisible(false);
    getEditState().codeOrPrelude = 1;
}

void WavetableScriptEditor::escapeKeyPressed()
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

void WavetableScriptEditor::rerenderFromUIState()
{
    auto resi = controlArea->resolutionN->getIntValue();
    auto nfr = controlArea->framesN->getIntValue();
    auto cfr = rendererComponent->frameNumber;

    auto respt = 32;
    for (int i = 1; i < resi; ++i)
        respt *= 2;

    rendererComponent->points = Surge::WavetableScript::evaluateScriptAtFrame(
        storage, mainDocument->getAllContent().toStdString(), respt, cfr, nfr);
    rendererComponent->repaint();
}

void WavetableScriptEditor::setCurrentFrame(int value)
{
    int frameNumber = rendererComponent->frameNumber;
    int maxFrames = controlArea->framesN->getIntValue();
    frameNumber += value;

    if (frameNumber < 1)
        frameNumber = 1;
    else if (frameNumber > maxFrames)
        frameNumber = maxFrames;

    rendererComponent->frameNumber = frameNumber;
    controlArea->currentFrameN->setIntValue(frameNumber);
}

void WavetableScriptEditor::generateWavetable()
{
    auto resi = controlArea->resolutionN->getIntValue();
    auto nfr = controlArea->framesN->getIntValue();
    auto respt = 32;

    for (int i = 1; i < resi; ++i)
        respt *= 2;

    std::cout << "Generating wavetable with " << respt << " samples and " << nfr << " frames"
              << std::endl;

    wt_header wh;
    float *wd = nullptr;
    Surge::WavetableScript::constructWavetable(storage, mainDocument->getAllContent().toStdString(),
                                               respt, nfr, wh, &wd);
    storage->waveTableDataMutex.lock();
    osc->wt.BuildWT(wd, wh, wh.flags & wtf_is_sample);
    osc->wavetable_display_name = "Scripted Wavetable";
    storage->waveTableDataMutex.unlock();

    delete[] wd;
    editor->repaintFrame();
}

std::optional<std::pair<std::string, std::string>>
WavetableScriptEditor::getPreCloseChickenBoxMessage()
{
    if (controlArea->applyS->isEnabled())
    {
        return std::make_pair("Close Wavetable Script Editor",
                              "Do you really want to close the wavetable editor? Any "
                              "changes that were not applied will be lost!");
    }
    return std::nullopt;
}

} // namespace Overlays
} // namespace Surge