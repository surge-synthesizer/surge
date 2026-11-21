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

#include "SurgeFXProcessor.h"
#include "SurgeFXEditor.h"
#include "sst/jucegui/components/RuledLabel.h"
#include "dsp/effects/DelayEffect.h"
#include "dsp/effects/airwindows/AirWindowsEffect.h"

#include <charconv>
#include <map>
#include "FxPresetAndClipboardManager.h"
#include "tinyxml/tinyxml.h"
#include "fmt/core.h"
#include "SurgeStorage.h"

namespace effectPanel = surge::fx::effectPanel;

struct PickerBase : public juce::Component
{
    PickerBase(SurgefxAudioProcessorEditor *ed, const std::string &accessibleName) : editor(ed)
    {
        setAccessible(true);
        setTitle(accessibleName);
        setDescription(accessibleName);
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void mouseEnter(const juce::MouseEvent &) override { repaint(); }
    void mouseExit(const juce::MouseEvent &) override { repaint(); }
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

    static void drawMenuArrow(juce::Graphics &g, juce::Rectangle<float> bounds)
    {
        const auto x = bounds.getRight() - 8.f;
        const auto y = bounds.getCentreY();
        juce::Path arrow;
        arrow.addTriangle(x - 3.f, y - 1.5f, x + 3.f, y - 1.5f, x, y + 2.f);
        g.fillPath(arrow);
    }

    virtual std::string currentLabel() const = 0;
    virtual void openMenu() = 0;
    virtual void step(int direction) = 0;

    bool keyPressed(const juce::KeyPress &p) override
    {
        if (p.getKeyCode() == juce::KeyPress::returnKey ||
            (p.getKeyCode() == juce::KeyPress::F10Key && p.getModifiers().isShiftDown()))
        {
            openMenu();
            return true;
        }

        if (p.getKeyCode() == juce::KeyPress::upKey)
        {
            step(-1);
            return true;
        }

        if (p.getKeyCode() == juce::KeyPress::downKey)
        {
            step(1);
            return true;
        }
        return false;
    }

    void mouseDown(const juce::MouseEvent &) override { openMenu(); }

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) override
    {
        if (wheel.deltaY == 0.f)
        {
            return;
        }

        stepMagnitude = (stepMagnitude == 0.f) ? std::abs(wheel.deltaY)
                                               : std::min(stepMagnitude, std::abs(wheel.deltaY));

        wheelAccumulator += wheel.deltaY;
        const float sign = std::signbit(wheel.deltaY) ? 1.f : -1.f;

        while (std::abs(wheelAccumulator) >= stepMagnitude * 0.75f)
        {
            wheelAccumulator += sign * stepMagnitude;
            step(static_cast<int>(sign));
        }
    }

    SurgefxAudioProcessorEditor *editor{nullptr};
    float wheelAccumulator{0.f};
    float stepMagnitude{0.f};

    struct AH : public juce::AccessibilityHandler
    {
        struct AHV : public juce::AccessibilityValueInterface
        {
            explicit AHV(PickerBase *s) : comp(s) {}
            PickerBase *comp;
            bool isReadOnly() const override { return true; }
            double getCurrentValue() const override { return 0.; }
            void setValue(double) override {}
            void setValueAsString(const juce::String &) override {}
            AccessibleValueRange getRange() const override { return {{0, 1}, 1}; }
            juce::String getCurrentValueAsString() const override { return comp->currentLabel(); }
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AHV);
        };

        explicit AH(PickerBase *s)
            : comp(s),
              juce::AccessibilityHandler(*s, juce::AccessibilityRole::button,
                                         juce::AccessibilityActions()
                                             .addAction(juce::AccessibilityActionType::press,
                                                        [this]() { comp->openMenu(); })
                                             .addAction(juce::AccessibilityActionType::showMenu,
                                                        [this]() { comp->openMenu(); }),
                                         AccessibilityHandler::Interfaces{std::make_unique<AHV>(s)})
        {
        }
        PickerBase *comp;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AH);
    };

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<AH>(this);
    }
};

struct Picker : public PickerBase
{
    Picker(SurgefxAudioProcessorEditor *ed) : PickerBase(ed, "FX Type") {}
    bool menuOpen{false};

    std::string currentLabel() const override
    {
        return fx_type_names[editor->processor.getEffectType()];
    }

    void paint(juce::Graphics &g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const bool active = menuOpen || isMouseOver();
        g.setColour(effectPanel::accent());
        g.fillRoundedRectangle(0.f, 4.f, 2.f, bounds.getHeight() - 8.f, 1.f);
        g.setColour(active ? effectPanel::accent() : effectPanel::text());
        g.setFont(effectPanel::font().withHeight(17.f).boldened());
        g.drawText(currentLabel(), bounds.withTrimmedLeft(9.f).withTrimmedRight(17.f),
                   juce::Justification::centredLeft, true);
        drawMenuArrow(g, bounds);
    }

    void openMenu() override { editor->showMenu(); }

    // Walk the editor's menu list to find the next/previous selectable FX entry
    // relative to the currently active effect type, wrapping around at the ends.
    void step(int direction) override
    {
        const auto &menuItems = editor->menu;
        const int currentType = editor->processor.getEffectType();

        // Collect indices of all FX entries in the menu vector.
        std::vector<int> fxIndices;

        for (int i = 0; i < (int)menuItems.size(); ++i)
        {
            if (menuItems[i].type == SurgefxAudioProcessorEditor::FxMenu::FX)
            {
                fxIndices.push_back(i);
            }
        }

        if (fxIndices.empty())
        {
            return;
        }

        // Find the position inside fxIndices that matches the current fx type.
        int currentPos = 0;

        for (int k = 0; k < (int)fxIndices.size(); ++k)
        {
            if (menuItems[fxIndices[k]].fxtype == currentType)
            {
                currentPos = k;
                break;
            }
        }

        // Step with wraparound.
        const int newPos = (currentPos + direction + (int)fxIndices.size()) % (int)fxIndices.size();
        const int newFxType = menuItems[fxIndices[newPos]].fxtype;

        if (newFxType > 0)
        {
            editor->setEffectType(newFxType);
        }
    }
};

struct PresetPicker : public PickerBase, juce::TextEditor::Listener
{
    PresetPicker(SurgefxAudioProcessorEditor *ed) : PickerBase(ed, "FX Preset") {}

    std::unique_ptr<juce::TextEditor> nameEditor;
    std::function<void(const std::string &)> onNameCommit;
    bool editingName{false};

    void paint(juce::Graphics &g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto active = hasKeyboardFocus(false) || isMouseOver() || editingName;
        g.setColour(effectPanel::surroundColour);
        g.fillRoundedRectangle(bounds, 3.f);
        g.setColour(active ? effectPanel::accent() : effectPanel::borderColour);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.f, 1.f);
        if (editingName)
            return;

        g.setColour(effectPanel::text().withAlpha(0.66f));
        g.setFont(effectPanel::font().withHeight(10.f).boldened());
        g.drawText("PRESET", bounds.withTrimmedLeft(7.f).withWidth(36.f),
                   juce::Justification::centredLeft, false);
        g.setColour(active ? effectPanel::accent() : effectPanel::text());
        g.setFont(effectPanel::font().withHeight(12.f));
        g.drawText(currentLabel(), bounds.withTrimmedLeft(45.f).withTrimmedRight(17.f),
                   juce::Justification::centredLeft, true);
        drawMenuArrow(g, bounds);
    }

    // Turns the label into a text field. Enter hands the typed text to onCommit, while
    // Escape or clicking elsewhere cancels.
    void beginNameEdit(const std::string &initialText,
                       std::function<void(const std::string &)> onCommit)
    {
        if (!nameEditor)
        {
            auto textCol = effectPanel::accent();

            nameEditor = std::make_unique<juce::TextEditor>();
            nameEditor->setColour(juce::TextEditor::backgroundColourId,
                                  juce::Colours::transparentBlack);
            nameEditor->setColour(juce::TextEditor::outlineColourId,
                                  juce::Colours::transparentBlack);
            nameEditor->setColour(juce::TextEditor::focusedOutlineColourId,
                                  juce::Colours::transparentBlack);
            nameEditor->setColour(juce::TextEditor::textColourId, textCol);
            nameEditor->setColour(juce::TextEditor::highlightColourId, textCol.withAlpha(0.3f));
            nameEditor->setColour(juce::TextEditor::highlightedTextColourId, textCol);
            nameEditor->setColour(juce::CaretComponent::caretColourId, textCol);
            nameEditor->setJustification(juce::Justification::centred);
            nameEditor->setIndents(0, 0);
            nameEditor->setSelectAllWhenFocused(true);
            nameEditor->addListener(this);
            addChildComponent(*nameEditor);
        }

        onNameCommit = std::move(onCommit);
        editingName = true;

        resized();
        toFront(false);
        nameEditor->setText(initialText, juce::NotificationType::dontSendNotification);
        nameEditor->setVisible(true);
        editor->repaint();

        // Grab focus once the popup menu that started this has fully gone away, so it
        // can't take focus back
        juce::MessageManager::callAsync([w = juce::Component::SafePointer<PresetPicker>(this)]() {
            if (w && w->editingName)
            {
                w->nameEditor->grabKeyboardFocus();
            }
        });
    }

    void endNameEdit(bool commit)
    {
        if (!editingName)
        {
            return;
        }

        // Cleared before hiding, since hiding the focused editor reports a focus loss
        editingName = false;

        auto text = nameEditor->getText().toStdString();
        auto cb = std::move(onNameCommit);
        onNameCommit = nullptr;

        nameEditor->setVisible(false);
        repaint();

        if (commit && cb)
        {
            cb(text);
        }
    }

    void resized() override
    {
        if (nameEditor)
        {
            const auto font = effectPanel::font();
            nameEditor->setFont(font);
            nameEditor->applyFontToAllText(font);
            nameEditor->setBounds(getLocalBounds().reduced(4, 0));
        }
    }

    void textEditorReturnKeyPressed(juce::TextEditor &) override { endNameEdit(true); }
    void textEditorEscapeKeyPressed(juce::TextEditor &) override { endNameEdit(false); }
    void textEditorFocusLost(juce::TextEditor &) override { endNameEdit(false); }

    std::string currentLabel() const override
    {
        if (editor->currentPresetIndex < 0 || editor->currentPresets.empty() ||
            editor->currentPresetIndex >= (int)editor->currentPresets.size())
        {
            // A stored name that isn't in the list means the loaded settings came from a
            // preset that no longer exists, for example one that was just deleted
            return editor->processor.getCurrentPresetName().empty() ? "- Init -" : "Unsaved";
        }
        return editor->currentPresets[editor->currentPresetIndex].name;
    }

    void openMenu() override { editor->showPresetMenu(); }

    void mouseDown(const juce::MouseEvent &e) override
    {
        // Right-click is a shortcut for Save FX Preset As...
        if (e.mods.isPopupMenu())
        {
            editor->saveCurrentPreset();
            return;
        }

        PickerBase::mouseDown(e);
    }

    void step(int direction) override
    {
        // Keeps the mouse wheel from changing presets underneath the name being typed
        if (!editingName)
        {
            editor->stepPreset(direction);
        }
    }
};

//==============================================================================
void SurgefxAudioProcessorEditor::mouseEnter(const juce::MouseEvent &e)
{
    for (int i = 0; i < n_fx_params; ++i)
    {
        if (e.eventComponent == &fxParamKnobs[i])
        {
            hoveredParam = i;
            repaint();
            break;
        }
    }
}

void SurgefxAudioProcessorEditor::mouseExit(const juce::MouseEvent &e)
{
    if (hoveredParam >= 0 && e.eventComponent == &fxParamKnobs[hoveredParam])
    {
        hoveredParam = -1;
        repaint();
    }
}

//==============================================================================
SurgefxAudioProcessorEditor::SurgefxAudioProcessorEditor(SurgefxAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    processor.storage->addErrorListener(this);
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);

    makeMenu();

    surgeLookFeel.reset(new SurgeLookAndFeel());
    setLookAndFeel(surgeLookFeel.get());
    juce::LookAndFeel::setDefaultLookAndFeel(surgeLookFeel.get());

    picker = std::make_unique<Picker>(this);
    addAndMakeVisibleRecordOrder(picker.get());

    // Without this, the storage's default provider silently answers OK, so saving
    // an FX preset under an existing name would overwrite it without asking
    processor.storage->okCancelProvider =
        [w = juce::Component::SafePointer(this)](
            const std::string &msg, const std::string &title, SurgeStorage::OkCancel def,
            std::function<void(SurgeStorage::OkCancel)> callback) {
            if (!w)
            {
                callback(SurgeStorage::CANCEL);
                return;
            }

            w->awaitingSaveConfirmation = !w->pendingSaveName.empty();

            auto options = juce::MessageBoxOptions()
                               .withIconType(juce::MessageBoxIconType::QuestionIcon)
                               .withTitle(title)
                               .withMessage(msg)
                               .withButton("OK")
                               .withButton("Cancel")
                               .withAssociatedComponent(w);

            juce::AlertWindow::showAsync(options, [w, callback](int result) {
                const bool ok = result == 1;

                callback(ok ? SurgeStorage::OK : SurgeStorage::CANCEL);

                if (w && w->awaitingSaveConfirmation)
                {
                    w->awaitingSaveConfirmation = false;

                    if (ok)
                    {
                        w->finishPresetSave();
                    }

                    w->pendingSaveName.clear();
                }
            });
        };

    fxPresetManager = std::make_unique<Surge::Storage::FxUserPreset>();
    fxPresetManager->doPresetRescan(processor.storage.get());

    rebuildCurrentPresets();

    // The editor can be closed and reopened at any time, so pick up whichever preset the
    // processor says is loaded
    selectPresetByStoredName();
    lastSeenEffectType = processor.getEffectType();

    presetPicker = std::make_unique<PresetPicker>(this);
    addAndMakeVisibleRecordOrder(presetPicker.get());

    for (int i = 0; i < n_fx_params; ++i)
    {
        auto &knob = fxParamKnobs[i];
        const auto style = effectPanel::panelStyle();
        knob.setStyle(style);
        knob.addMouseListener(this, false);
        knob.binding.editText = [this, i](const std::string &value) {
            if (!processor.setParameterByString(i, value))
                return false;
            fxParamKnobs[i].update(processor.getFXStorageValue01(i), processor.getParamValue(i));
            repaint();
            return true;
        };
        knob.binding.edit = [this, i](float value, bool quantized) {
            processor.prepareParametersAbsentAudio();
            processor.setFXParamValue01(i, value, quantized);
            fxParamKnobs[i].update(processor.getFXStorageValue01(i), processor.getParamValue(i));
            if (processor.getEffectType() == fxt_nimbus && processor.getStorageParamIndex(i) == 0)
                resetLabels();
            else if (hoveredParam == i || fxParamKnobs[i].hasKeyboardFocus(false))
                repaint();
        };
        knob.onPopupMenu = [this, i](const auto &) { showParameterMenu(i); };
        knob.onEditValue = [this, i]() { showValueEditor(i); };
        knob.onBeginEdit = [this, i]() { processor.setUserEditingFXParam(i, true); };
        knob.onEndEdit = [this, i]() { processor.setUserEditingFXParam(i, false); };
        addAndMakeVisibleRecordOrder(&knob);

        fxParamLabels[i].setStyle(style);
        fxParamLabels[i].setAccessible(false); // The knob exposes the parameter name.
        addChildComponent(fxParamLabels[i]);

        using Glyph = sst::jucegui::components::GlyphPainter;
        fxDeactivated[i].binding.invert = true;
        fxDeactivated[i].setStyle(style);
        fxDeactivated[i].setGlyph(Glyph::SMALL_POWER_LIGHT);
        fxExtended[i].setStyle(style);
        fxExtended[i].setGlyph(Glyph::PLUS_MINUS);

        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      juce::NotificationType::dontSendNotification);
        fxTempoSync[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamTempoSync(i, this->fxTempoSync[i].getToggleState());
            this->processor.setFXStorageTempoSync(i, this->fxTempoSync[i].getToggleState());
            fxParamKnobs[i].update(processor.getFXStorageValue01(i), processor.getParamValue(i));
            this->processor.setUserEditingParamFeature(i, false);
        };
        fxTempoSync[i].onPopupMenu = [this, i](const auto &) {
            if (auto *param = processor.getHostParameterFor(i, SurgeFX::ParamKind::TempoSync))
            {
                showHostContextMenuFor(param);
            }
        };
        fxTempoSync[i].setTitle("Parameter " + std::to_string(i) + " Tempo Sync");

        fxDeactivated[i].setEnabled(processor.canDeactitvate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        juce::NotificationType::dontSendNotification);
        fxDeactivated[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamDeactivated(i, this->fxDeactivated[i].getToggleState());
            this->processor.setFXStorageDeactivated(i, this->fxDeactivated[i].getToggleState());

            // Special case - coupled dectivation
            this->resetLabels();
            this->processor.setUserEditingParamFeature(i, false);
        };
        fxDeactivated[i].onPopupMenu = [this, i](const auto &) {
            if (auto *param = processor.getHostParameterFor(i, SurgeFX::ParamKind::Deactivated))
            {
                showHostContextMenuFor(param);
            }
        };
        fxDeactivated[i].setTitle("Parameter " + std::to_string(i) + " Deactivate");
        addAndMakeVisibleRecordOrder(&(fxDeactivated[i]));

        fxExtended[i].setEnabled(processor.canExtend(i));
        fxExtended[i].setToggleState(processor.getFXStorageExtended(i),
                                     juce::NotificationType::dontSendNotification);
        fxExtended[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamExtended(i, this->fxExtended[i].getToggleState());
            this->processor.setFXStorageExtended(i, this->fxExtended[i].getToggleState());
            fxParamKnobs[i].update(processor.getFXStorageValue01(i), processor.getParamValue(i));
            this->processor.setUserEditingParamFeature(i, false);
        };
        fxExtended[i].onPopupMenu = [this, i](const auto &) {
            if (auto *param = processor.getHostParameterFor(i, SurgeFX::ParamKind::Extended))
            {
                showHostContextMenuFor(param);
            }
        };
        fxExtended[i].setTitle("Parameter " + std::to_string(i) + " Extended");
        addAndMakeVisibleRecordOrder(&fxExtended[i]);
        fxExtended[i].setVisible(false);

        fxAbsoluted[i].setEnabled(processor.canAbsolute(i));
        fxAbsoluted[i].setToggleState(processor.getFXParamAbsolute(i),
                                      juce::NotificationType::dontSendNotification);
        fxAbsoluted[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamAbsolute(i, this->fxAbsoluted[i].getToggleState());
            this->processor.setFXStorageAbsolute(i, this->fxAbsoluted[i].getToggleState());
            fxParamKnobs[i].update(processor.getFXStorageValue01(i), processor.getParamValue(i));
            this->processor.setUserEditingParamFeature(i, false);
        };
        fxAbsoluted[i].onPopupMenu = [this, i](const auto &) {
            if (auto *param = processor.getHostParameterFor(i, SurgeFX::ParamKind::Absolute))
            {
                showHostContextMenuFor(param);
            }
        };
        fxAbsoluted[i].setTitle("Parameter " + std::to_string(i) + " Absolute");
    }

    this->processor.setParameterChangeListener([this]() { this->triggerAsyncUpdate(); });

    setTitle("Surge XT Effects");
    setDescription("Surge XT Effects");
    resetLabels();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    auto dzf = Surge::Storage::getUserDefaultValue(
        processor.storage.get(), Surge::Storage::FXUnitDefaultZoom, effectPanel::defaultZoom);

    setSize(juce::roundToInt(0.01 * dzf * baseWidth), juce::roundToInt(0.01 * dzf * baseHeight));
    setResizable(true, true);
    getConstrainer()->setMinimumWidth(baseWidth * 0.75);
    getConstrainer()->setFixedAspectRatio(baseWidth * 1.0 / baseHeight);

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 25);
}

juce::PopupMenu SurgefxAudioProcessorEditor::modifyHostMenu(juce::PopupMenu menu,
                                                            juce::AudioProcessorParameter *param)
{
    auto newMenu = juce::PopupMenu();

    const auto kind = processor.paramKindForIndex[param->getParameterIndex()];
    const auto slot = processor.paramSlotForIndex[param->getParameterIndex()];

    newMenu.addSectionHeader(processor.getMutableName(kind, slot));

    newMenu.addSeparator();

    // make things look a bit nicer for our friends from Image-Line
    if (juce::PluginHostType().isFruityLoops())
    {
        auto it = juce::PopupMenu::MenuItemIterator(menu);

        while (it.next())
        {
            auto txt = it.getItem().text;

            if (txt.startsWithChar('-'))
            {
                it.getItem().isSectionHeader = true;
                it.getItem().text = txt.fromFirstOccurrenceOf("-", false, false);
            }

            newMenu.addItem(it.getItem());
        }
    }

    // we really don't need that parameter name repeated in Reaper...
    if (juce::PluginHostType().isReaper())
    {
        auto it = juce::PopupMenu::MenuItemIterator(menu);

        while (it.next())
        {
            auto txt = it.getItem().text;
            bool include = true;

            if (txt.startsWithChar('[') && txt.endsWithChar(']'))
            {
                include = it.next();
            }

            if (include)
            {
                newMenu.addItem(it.getItem());
            }
        }
    }

    return newMenu;
}

SurgefxAudioProcessorEditor::~SurgefxAudioProcessorEditor()
{
    idleTimer->stopTimer();
    setLookAndFeel(nullptr);
    this->processor.setParameterChangeListener([]() {});
    this->processor.storage->removeErrorListener(this);
    this->processor.storage->clearOkCancelProvider();
}

void SurgefxAudioProcessorEditor::parentHierarchyChanged()
{
    for (auto *p = getParentComponent(); p != nullptr; p = p->getParentComponent())
    {
        if (auto dw = dynamic_cast<juce::DocumentWindow *>(p))
        {
            if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
            {
                dw->setColour(juce::DocumentWindow::backgroundColourId, juce::Colours::black);
            }
        }
    }

    AudioProcessorEditor::parentHierarchyChanged();
}

void SurgefxAudioProcessorEditor::resetLabels()
{
    processor.prepareParametersAbsentAudio();
    const auto &layout = effectPanel::layout(processor.getEffectType());

    auto st = [](auto &thing, const std::string &title) {
        thing.setTitle(title);
        if (auto *handler = thing.getAccessibilityHandler())
        {
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        }
    };

    for (int i = 0; i < n_fx_params; ++i)
    {
        auto nm = processor.getParamName(i) + " " + processor.getParamGroup(i);
        auto &knob = fxParamKnobs[i];
        knob.binding.name = juce::String(nm).trim().toStdString();
        knob.binding.defaultValue = processor.getFXDefaultValue01(i);
        knob.binding.discreteStep = processor.getFXParamDiscreteStep(i);
        knob.binding.bipolar = processor.isParamBipolar(i);
        knob.binding.acceptsText = processor.canSetParameterByString(i);
        const auto slot = processor.getStorageParamIndex(i);
        knob.setOpensChoiceMenu(std::any_of(layout.begin(), layout.end(), [slot](const auto &c) {
            return c.slot == slot && c.kind == effectPanel::Kind::choice;
        }));
        knob.binding.blocked = processor.getFXStorageAppearsDeactivated(i);
        knob.setEnabled(processor.getParamEnabled(i));
        st(knob, knob.binding.name);
        knob.update(processor.getFXStorageValue01(i), processor.getParamValue(i));

        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setAccessible(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      juce::NotificationType::dontSendNotification);
        st(fxTempoSync[i], nm + " Tempo Synced");

        fxExtended[i].setEnabled(processor.canExtend(i));
        fxExtended[i].setToggleState(processor.getFXStorageExtended(i),
                                     juce::NotificationType::dontSendNotification);
        fxExtended[i].setAccessible(processor.canExtend(i));
        st(fxExtended[i], nm + " Extended");

        fxAbsoluted[i].setEnabled(processor.canAbsolute(i));
        fxAbsoluted[i].setToggleState(processor.getFXStorageAbsolute(i),
                                      juce::NotificationType::dontSendNotification);
        fxAbsoluted[i].setAccessible(processor.canAbsolute(i));
        st(fxAbsoluted[i], nm + " Absolute");

        fxDeactivated[i].setEnabled(processor.canDeactitvate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        juce::NotificationType::dontSendNotification);
        fxDeactivated[i].setAccessible(processor.canDeactitvate(i));
        st(fxDeactivated[i], nm + " Enabled");
    }

    picker->repaint();
    if (presetPicker)
    {
        presetPicker->repaint();
    }

    resized();
    repaint();

    if (auto h = getAccessibilityHandler())
    {
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
    }
}

void SurgefxAudioProcessorEditor::setEffectType(int i)
{
    processor.resetFxType(i);
    blastToggleState(i - 1);

    // Rebuild preset list for the new FX type and reset selection. The settings are the new
    // type's defaults now, not the previously loaded preset.
    processor.setCurrentPresetName("");
    rebuildCurrentPresets();
    currentPresetIndex = defaultPresetIndexForCurrentType();
    lastSeenEffectType = i;

    resetLabels();
    picker->repaint();

    if (presetPicker)
    {
        presetPicker->repaint();
    }
}

void SurgefxAudioProcessorEditor::handleAsyncUpdate() { paramsChangedCallback(); }

void SurgefxAudioProcessorEditor::paramsChangedCallback()
{
    bool cv[n_fx_params + 1];
    float fv[n_fx_params + 1];

    processor.copyChangeValues(cv, fv);

    for (int i = 0; i < n_fx_params + 1; ++i)
    {
        if (cv[i])
        {
            if (i < n_fx_params)
            {
                fxParamKnobs[i].update(fv[i], processor.getParamValueFor(i, fv[i]));
                if (processor.getEffectType() == fxt_nimbus &&
                    processor.getStorageParamIndex(i) == 0)
                    resetLabels();
                else if (hoveredParam == i || fxParamKnobs[i].hasKeyboardFocus(false))
                    repaint();
            }
            else
            {
                // changedParams[n_fx_params] is set unconditionally by
                // updateJuceParamsFromStorage() on every call - including
                // calls from loadFxPreset() which never changes the effect type.
                // Compare against the last type we actually saw so we
                // only rebuild/reset the preset selection on a real type
                // change (e.g. external host automation of the FX type
                // parameter), not on every preset load.
                const int newType = processor.getEffectType();

                if (newType != lastSeenEffectType)
                {
                    lastSeenEffectType = newType;
                    blastToggleState(newType - 1);
                    rebuildCurrentPresets();

                    // A host type change has already dropped the stored name, while a state
                    // restore has just set the one to select
                    selectPresetByStoredName();
                }

                // The processor also signals metadata changes without a type change,
                // e.g. when Airwindows finishes configuring a different algorithm.
                resetLabels();
            }
        }

        bool anyDeactivationChanged{false};

        auto syncToggle = [](surge::fx::ParameterOption &sw, bool newState) -> bool {
            const bool old = sw.getToggleState();
            sw.setToggleState(newState, juce::NotificationType::dontSendNotification);
            return old != newState;
        };

        if (i < n_fx_params)
        {
            bool displayDirty = false;

            displayDirty |= syncToggle(fxTempoSync[i], processor.getFXStorageTempoSync(i));
            displayDirty |= syncToggle(fxExtended[i], processor.getFXStorageExtended(i));
            displayDirty |= syncToggle(fxAbsoluted[i], processor.getFXStorageAbsolute(i));

            if (syncToggle(fxDeactivated[i], processor.getFXStorageDeactivated(i)))
            {
                anyDeactivationChanged = true;
            }

            if (displayDirty)
            {
                fxParamKnobs[i].update(processor.getFXStorageValue01(i),
                                       processor.getParamValue(i));
            }
        }

        if (anyDeactivationChanged)
        {
            resetLabels();
        }
    }
}

void SurgefxAudioProcessorEditor::blastToggleState(int w) {}

void SurgefxAudioProcessorEditor::rebuildCurrentPresets()
{
    if (!fxPresetManager)
    {
        return;
    }

    currentPresets.clear();

    const int fxType = processor.getEffectType();
    auto storage = processor.storage.get();
    auto fxConfiguration = storage->getSnapshotSection("fx");

    for (auto c = fxConfiguration->FirstChildElement(); c; c = c->NextSiblingElement())
    {
        if (std::string(c->Value()) != "type")
        {
            continue;
        }

        int t = -1;
        c->QueryIntAttribute("i", &t);

        if (t != fxType)
        {
            continue;
        }

        for (auto s = c->FirstChildElement("snapshot"); s; s = s->NextSiblingElement("snapshot"))
        {
            Surge::Storage::FxUserPreset::Preset preset;
            preset.type = fxType;
            preset.isFactory = true;

            if (fxPresetManager->readFromXMLSnapshot(preset, s))
            {
                currentPresets.push_back(preset);
            }
        }

        break;
    }

    // Scanned .srgfx presets (factory, then user) come after the configuration.xml
    // Init presets above. Both report isFactory=true, so showPresetMenu()'s
    // existing grouping naturally keeps Init presets at the top of the same
    // "Factory" section, ahead of the scanned ones - no separate header needed.
    if (fxPresetManager)
    {
        auto scanned = fxPresetManager->getPresetsForSingleType(fxType);
        currentPresets.insert(currentPresets.end(), scanned.begin(), scanned.end());
    }
}

void SurgefxAudioProcessorEditor::stepPreset(int direction)
{
    if (currentPresets.empty())
    {
        return;
    }

    const int n = (int)currentPresets.size();

    if (currentPresetIndex < 0)
    {
        currentPresetIndex = (direction > 0) ? 0 : n - 1;
    }
    else
    {
        currentPresetIndex = (currentPresetIndex + direction + n) % n;
    }

    loadCurrentPreset();
}

void SurgefxAudioProcessorEditor::loadCurrentPreset()
{
    if (currentPresetIndex < 0 || currentPresetIndex >= (int)currentPresets.size())
    {
        return;
    }

    const auto &preset = currentPresets[currentPresetIndex];

    // rebuildCurrentPresets() only ever populates currentPresets with presets
    // whose type matches the currently loaded effect
    // (see getPresetsForSingleType(processor.getEffectType())), so this can never
    // cross an FX-type boundary.
    processor.loadFxPreset(preset);
    processor.setCurrentPresetName(preset.name);
    blastToggleState(processor.getEffectType() - 1);
    resetLabels();

    if (presetPicker)
    {
        presetPicker->repaint();
    }
}

void SurgefxAudioProcessorEditor::showPresetMenu()
{
    constexpr int itemsPerColumn = 25;

    auto p = juce::PopupMenu();
    p.addSectionHeader(presetPicker->currentLabel());

    auto addFunctionsAndShow = [this, &p]() {
        p.addColumnBreak();
        p.addSectionHeader("FUNCTIONS");

        p.addItem(Surge::GUI::toOSCase("Save FX Preset As..."),
                  [w = juce::Component::SafePointer(this)]() {
                      if (w)
                      {
                          w->saveCurrentPreset();
                      }
                  });

        if (currentPresetIndex >= 0 && currentPresetIndex < (int)currentPresets.size() &&
            !currentPresets[currentPresetIndex].isFactory &&
            !currentPresets[currentPresetIndex].file.empty())
        {
            p.addItem(Surge::GUI::toOSCase("Delete FX Preset"),
                      [w = juce::Component::SafePointer(this)]() {
                          if (w)
                          {
                              w->deleteCurrentPreset();
                          }
                      });
        }

        p.addSeparator();

        p.addItem(Surge::GUI::toOSCase("Refresh FX Preset List"),
                  [w = juce::Component::SafePointer(this)]() {
                      if (w)
                      {
                          w->refreshPresetList();
                      }
                  });

        auto o =
            juce::PopupMenu::Options()
                .withTargetComponent(presetPicker.get())
                .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);

        p.showMenuAsync(o);
    };

    if (currentPresets.empty())
    {
        p.addItem("No presets available", false, false, []() {});
        addFunctionsAndShow();
        return;
    }

    // Group by (isFactory, subPath), then list presets within each group.
    // Factory presets come first (already sorted that way by the scanner),
    // and within each factory/user bucket, presets are grouped by subPath.
    bool lastIsFactory = true;
    bool firstSection = true;
    juce::PopupMenu currentSubMenu;
    fs::path currentSubMenuPath;
    bool haveOpenSubMenu = false;

    auto flushSubMenu = [&]() {
        if (haveOpenSubMenu)
        {
            const bool subMenuIsTicked =
                currentPresetIndex >= 0 && currentPresetIndex < (int)currentPresets.size() &&
                currentPresets[currentPresetIndex].subPath == currentSubMenuPath;
            const bool subMenuHasItems = currentSubMenu.getNumItems() > 0;

            p.addSubMenu(currentSubMenuPath.string(), std::move(currentSubMenu), subMenuHasItems,
                         juce::Image{}, subMenuIsTicked, 0);

            currentSubMenu = juce::PopupMenu();
            haveOpenSubMenu = false;
        }
    };

    int flatItemsInColumn = 0;

    for (int idx = 0; idx < (int)currentPresets.size(); ++idx)
    {
        const auto &preset = currentPresets[idx];

        // Section header when crossing the factory/user boundary or when subPath changes
        const bool isCurrent = (idx == currentPresetIndex);
        auto addItem = [&](juce::PopupMenu &menu) {
            menu.addItem(preset.name, true, isCurrent, [this, idx]() {
                currentPresetIndex = idx;
                loadCurrentPreset();
            });
        };

        if (firstSection || preset.isFactory != lastIsFactory)
        {
            flushSubMenu();

            if (!firstSection)
            {
                p.addColumnBreak();
            }

            firstSection = false;
            flatItemsInColumn = 0;

            p.addSectionHeader(preset.isFactory ? "FACTORY" : "USER");
            lastIsFactory = preset.isFactory;
            currentSubMenuPath.clear();
        }

        if (preset.subPath.empty())
        {
            flushSubMenu();

            if (flatItemsInColumn >= itemsPerColumn)
            {
                p.addColumnBreak();
                p.addSectionHeader("#GHOST#");
                flatItemsInColumn = 0;
            }

            addItem(p);
            ++flatItemsInColumn;
        }
        else
        {
            if (!haveOpenSubMenu || preset.subPath != currentSubMenuPath)
            {
                flushSubMenu();
                currentSubMenuPath = preset.subPath;
                haveOpenSubMenu = true;
            }

            addItem(currentSubMenu);
        }
    }

    flushSubMenu();
    addFunctionsAndShow();
}

void SurgefxAudioProcessorEditor::saveCurrentPreset()
{
    if (!presetPicker)
    {
        return;
    }

    // Start from the current preset, subfolders included, so overwriting it is just Enter
    std::string initialText;

    if (currentPresetIndex >= 0 && currentPresetIndex < (int)currentPresets.size())
    {
        const auto &preset = currentPresets[currentPresetIndex];

        initialText = path_to_string(preset.subPath);
        std::replace(initialText.begin(), initialText.end(), '\\', '/');

        if (!initialText.empty())
        {
            initialText += "/";
        }

        initialText += preset.name;
    }

    presetPicker->beginNameEdit(initialText,
                                [w = juce::Component::SafePointer(this)](const std::string &name) {
                                    if (!w || name.empty())
                                    {
                                        return;
                                    }

                                    // Match how saveFxIn() lays the file out: anything before the
                                    // last separator becomes subfolders, which is exactly the
                                    // subPath the rescan reports back
                                    auto sp = string_to_path(name);

                                    w->pendingSaveSubPath = sp.parent_path();
                                    w->pendingSaveName = path_to_string(sp.filename());
                                    w->awaitingSaveConfirmation = false;

                                    w->processor.saveFxPreset(*w->fxPresetManager, name);

                                    // If the preset already existed, the save waits for the
                                    // overwrite confirmation, which finishes it from
                                    // okCancelProvider instead
                                    if (!w->awaitingSaveConfirmation)
                                    {
                                        w->finishPresetSave();
                                        w->pendingSaveName.clear();
                                    }
                                });
}

void SurgefxAudioProcessorEditor::finishPresetSave()
{
    // saveFxIn() has already rescanned. If the save failed (for example because of an
    // invalid name), the saved preset won't be found and the previous selection stays.
    rebuildCurrentPresetsKeepingSelection();

    auto idx = findPresetIndex(false, pendingSaveSubPath, pendingSaveName);

    if (idx >= 0)
    {
        currentPresetIndex = idx;
        processor.setCurrentPresetName(currentPresets[idx].name);

        if (presetPicker)
        {
            presetPicker->repaint();
        }
        repaint();
    }
}

void SurgefxAudioProcessorEditor::deleteCurrentPreset()
{
    if (currentPresetIndex < 0 || currentPresetIndex >= (int)currentPresets.size())
    {
        return;
    }

    const auto file = currentPresets[currentPresetIndex].file;

    if (currentPresets[currentPresetIndex].isFactory || file.empty())
    {
        return;
    }

    // Shown relative to the user FX presets folder, which is all that tells presets apart
    auto shownPath =
        path_to_string(string_to_path(file).lexically_relative(processor.storage->userFXPath));

    if (shownPath.empty())
    {
        shownPath = file;
    }

    auto options =
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Delete FX Preset")
            .withMessage("Do you really want to delete\n" + shownPath + "?\nThis cannot be undone!")
            .withButton("OK")
            .withButton("Cancel")
            .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(
        options, [w = juce::Component::SafePointer(this), file](int result) {
            if (!w || result != 1)
            {
                return;
            }

            try
            {
                fs::remove(string_to_path(file));
            }
            catch (const fs::filesystem_error &e)
            {
                std::ostringstream oss;
                oss << "Experienced filesystem error while deleting FX preset " << e.what();
                w->processor.storage->reportError(oss.str(), "Filesystem Error");
                return;
            }

            // The deleted preset is no longer found, so this clears the selection. Its name stays
            // stored, which is what makes the picker show the settings as Unsaved.
            w->refreshPresetList();
        });
}

void SurgefxAudioProcessorEditor::refreshPresetList()
{
    fxPresetManager->doPresetRescan(processor.storage.get(), true);
    rebuildCurrentPresetsKeepingSelection();
}

void SurgefxAudioProcessorEditor::rebuildCurrentPresetsKeepingSelection()
{
    const bool hadCurrent =
        currentPresetIndex >= 0 && currentPresetIndex < (int)currentPresets.size();
    const auto previous =
        hadCurrent ? currentPresets[currentPresetIndex] : Surge::Storage::FxUserPreset::Preset{};

    rebuildCurrentPresets();

    currentPresetIndex =
        hadCurrent ? findPresetIndex(previous.isFactory, previous.subPath, previous.name) : -1;

    if (presetPicker)
    {
        presetPicker->repaint();
    }
    repaint();
}

void SurgefxAudioProcessorEditor::selectPresetByStoredName()
{
    const auto name = processor.getCurrentPresetName();

    if (name.empty())
    {
        currentPresetIndex = defaultPresetIndexForCurrentType();
        return;
    }

    currentPresetIndex = -1;

    for (int i = 0; i < (int)currentPresets.size(); ++i)
    {
        if (currentPresets[i].name == name)
        {
            currentPresetIndex = i;
            break;
        }
    }
}

int SurgefxAudioProcessorEditor::findPresetIndex(bool isFactory, const fs::path &subPath,
                                                 const std::string &name) const
{
    for (int i = 0; i < (int)currentPresets.size(); ++i)
    {
        const auto &p = currentPresets[i];

        if (p.isFactory == isFactory && p.subPath == subPath && p.name == name)
        {
            return i;
        }
    }

    return -1;
}

//==============================================================================
juce::String SurgefxAudioProcessorEditor::getPanelStatus() const
{
    if (!processor.m_audioValid && !processor.m_audioValidMessage.empty())
        return processor.m_audioValidMessage;

    int param = hoveredParam;
    if (param < 0 || !fxParamKnobs[param].isVisible())
    {
        param = -1;
        for (int i = 0; i < n_fx_params; ++i)
            if (fxParamKnobs[i].hasKeyboardFocus(false))
                param = i;
    }
    if (param < 0)
        return {};
    return juce::String(processor.getParamName(param)) + ": " + fxParamKnobs[param].binding.text;
}

void SurgefxAudioProcessorEditor::paint(juce::Graphics &g)
{
    const auto panel = getLocalBounds().toFloat();
    effectPanel::paint(g, panel);
    if (panel.isEmpty())
        return;

    g.addTransform(effectPanel::panelTransform(panel));
    g.setColour(effectPanel::borderColour);
    g.drawHorizontalLine(49, 9.f, 189.f);
    g.drawHorizontalLine(effectPanel::statusArea.getY(), 9.f, 189.f);
    const auto status = getPanelStatus();
    g.setFont(effectPanel::font().withHeight(11.f));
    g.setColour(status.isEmpty() ? effectPanel::text().withAlpha(0.55f) : effectPanel::accent());
    g.drawText(status.isEmpty() ? juce::String("SURGE XT  /  EFFECTS") : status,
               effectPanel::statusArea, juce::Justification::centredLeft, true);

    if (processor.getEffectType() == fxt_off)
    {
        g.setColour(effectPanel::text());
        g.setFont(effectPanel::font().boldened());
        g.drawText("No effect selected", 8, 117, 182, 22, juce::Justification::centred);
        g.setColour(effectPanel::text().withAlpha(0.8f));
        g.setFont(effectPanel::font());
        g.drawText("Choose an effect from the menu above", 8, 141, 182, 18,
                   juce::Justification::centred);
    }
}

void SurgefxAudioProcessorEditor::resized()
{
    if (valueEditor)
        valueEditor->setVisible(false);

    const auto panel = getLocalBounds().toFloat();
    if (panel.isEmpty())
        return;

    const auto transform = effectPanel::panelTransform(panel);
    picker->setTransform(transform);
    picker->setBounds(effectPanel::titleArea);
    presetPicker->setTransform(transform);
    presetPicker->setBounds(effectPanel::presetArea);

    const auto effect = processor.getEffectType();
    const auto &layout = effectPanel::layout(effect);
    if (panelLabelEffect != effect)
    {
        panelLabelEffect = effect;
        panelLabels.clear();
        for (const auto &control : layout)
        {
            if (control.kind != effectPanel::Kind::group)
                continue;

            auto widget = std::make_unique<sst::jucegui::components::RuledLabel>();
            widget->setStyle(effectPanel::panelStyle());
            widget->setText(control.label.toStdString());
            widget->setInterceptsMouseClicks(false, false);
            widget->setAccessible(false); // Group names are part of each knob's accessible title.
            addAndMakeVisible(*widget);
            panelLabels.push_back(std::move(widget));
        }
    }

    int lastAirwindowsSlot = 0;
    if (effect == fxt_airwindows)
        for (int i = 0; i < n_fx_params; ++i)
            if (processor.getParamEnabled(i))
                lastAirwindowsSlot =
                    std::max(lastAirwindowsSlot, processor.getStorageParamIndex(i));
    const bool roomyAirwindows = effect == fxt_airwindows && lastAirwindowsSlot <= 6;

    std::array<bool, n_fx_params> visibleKnobs{}, visibleNames{}, visibleLights{}, visibleExtend{};
    size_t labelIndex = 0;
    for (const auto &control : layout)
    {
        const auto pixels = control.bounds.toNearestIntEdges() + effectPanel::contentOrigin;
        if (control.kind == effectPanel::Kind::group)
        {
            auto &label = *panelLabels[labelIndex++];
            label.setTransform(transform);
            label.setBounds(pixels);
            continue;
        }
        for (int i = 0; i < n_fx_params; ++i)
        {
            if (processor.getStorageParamIndex(i) != control.slot || !processor.getParamEnabled(i))
                continue;

            if (control.kind == effectPanel::Kind::knob ||
                control.kind == effectPanel::Kind::fader ||
                control.kind == effectPanel::Kind::choice)
            {
                auto controlBounds = pixels;
                if (roomyAirwindows && control.slot > 0)
                {
                    const auto slot = control.slot - 1;
                    controlBounds =
                        juce::Rectangle<int>(26 + 90 * (slot % 2), 80 + 58 * (slot / 2), 40, 40) +
                        effectPanel::contentOrigin;
                }
                const auto size = std::min(controlBounds.getWidth(), controlBounds.getHeight());
                auto &knob = fxParamKnobs[i];
                knob.drawFader = control.kind == effectPanel::Kind::fader;
                knob.setTransform(transform);
                knob.setBounds(knob.opensChoiceMenu || knob.drawFader
                                   ? controlBounds
                                   : controlBounds.withSizeKeepingCentre(size, size));
                knob.repaint();
                visibleKnobs[i] = true;

                auto name =
                    control.useParamName ? processor.getParamName(i) : control.label.toStdString();
                if (!name.empty())
                {
                    auto &label = fxParamLabels[i];
                    label.setText(name);
                    label.setTransform(transform);
                    if (knob.drawFader)
                    {
                        label.setFontHeightOverride(10);
                        label.setJustification(juce::Justification::centred);
                        label.setBounds(pixels.withY(pixels.getBottom()).withHeight(15));
                    }
                    else if (effect == fxt_airwindows)
                    {
                        label.setFontHeightOverride(effectPanel::captionFontHeight);
                        if (roomyAirwindows)
                        {
                            label.setJustification(juce::Justification::centred);
                            label.setBounds(controlBounds.getCentreX() - 42,
                                            controlBounds.getBottom(), 84,
                                            effectPanel::captionHeight);
                        }
                        else
                        {
                            label.setJustification(juce::Justification::centredLeft);
                            label.setBounds(pixels.getRight() + 3, pixels.getY(), 64,
                                            pixels.getHeight());
                        }
                    }
                    else
                    {
                        label.setFontHeightOverride(effectPanel::captionFontHeight);
                        label.setJustification(juce::Justification::centred);
                        label.setBounds(effectPanel::captionBounds(layout, control) +
                                        effectPanel::contentOrigin);
                    }
                    visibleNames[i] = true;
                }
            }
            else if (control.kind == effectPanel::Kind::power)
            {
                fxDeactivated[i].setTransform(transform);
                fxDeactivated[i].setBounds(pixels);
                visibleLights[i] = true;
            }
            else if (control.kind == effectPanel::Kind::extend)
            {
                fxExtended[i].setTransform(transform);
                fxExtended[i].setBounds(pixels);
                visibleExtend[i] = true;
            }
            break;
        }
    }

    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamKnobs[i].setVisible(visibleKnobs[i]);
        fxParamLabels[i].setVisible(visibleNames[i]);
        fxDeactivated[i].setVisible(visibleLights[i]);
        fxExtended[i].setVisible(visibleExtend[i]);
    }
}

int SurgefxAudioProcessorEditor::findLargestFittingZoomBetween(int zoomLow, int zoomHigh,
                                                               int zoomQuanta,
                                                               int percentageOfScreenAvailable,
                                                               float baseW, float baseH)
{
    // Here is a very crude implementation
    int result = zoomHigh;
    auto screenDim = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea;
    float sx = screenDim.getWidth() * percentageOfScreenAvailable / 100.0;
    float sy = screenDim.getHeight() * percentageOfScreenAvailable / 100.0;

    while (result > zoomLow)
    {
        if (result * baseW / 100.0 <= sx && result * baseH / 100.0 <= sy)
            break;
        result -= zoomQuanta;
    }
    if (result < zoomLow)
        result = zoomLow;

    return result;
}

void SurgefxAudioProcessorEditor::makeMenu()
{
    auto storage = processor.storage.get();
    auto fxConfiguration = storage->getSnapshotSection("fx");
    auto c = fxConfiguration->FirstChildElement();
    while (c)
    {
        FxMenu men;
        auto a = c->Attribute("name");
        if (!a)
        {
            a = c->Attribute("label");
        }
        if (a)
        {
            men.name = a;
        }
        if (std::string(c->Value()) == "type")
        {
            men.type = FxMenu::FX;
            auto t = -1;
            c->QueryIntAttribute("i", &t);
            men.fxtype = t;
            if (t == fxt_audio_input || t == fxt_convolution)
            {
                c = c->NextSiblingElement();
                continue;
            }
        }
        else
        {
            men.type = FxMenu::SECTION;
            int bk = 0;
            c->QueryIntAttribute("columnbreak", &bk);
            men.isBreak = bk;
        }
        menu.push_back(men);

        c = c->NextSiblingElement();
    }
}

void SurgefxAudioProcessorEditor::showMenu()
{
    if (picker->menuOpen)
        return;

    auto p = juce::PopupMenu();
    auto safe = juce::Component::SafePointer(this);

    for (const auto &m : menu)
    {
        if (m.isBreak)
        {
            p.addColumnBreak();
        }

        if (m.type == FxMenu::SECTION)
        {
            p.addSectionHeader(m.name);
        }

        if (m.type == FxMenu::FX)
        {
            p.addItem(m.name, [safe, type = m.fxtype]() {
                if (safe && type > 0)
                    safe->setEffectType(type);
            });
        }
    }

    p.addSeparator();
    addSettingsMenuItems(p);

    auto o = juce::PopupMenu::Options()
                 .withTargetComponent(picker.get())
                 .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);

    picker->menuOpen = true;
    picker->repaint();
    p.showMenuAsync(o, [safe](int) {
        if (safe)
        {
            safe->picker->menuOpen = false;
            safe->picker->repaint();
        }
    });
}

void SurgefxAudioProcessorEditor::addSettingsMenuItems(juce::PopupMenu &menu)
{
    menu.addSubMenu("OSC", makeOSCMenu());

    std::vector<int> zoomTos = {{75, 100, 125, 150, 200}};
    std::string lab;
    auto dzf = Surge::Storage::getUserDefaultValue(
        processor.storage.get(), Surge::Storage::FXUnitDefaultZoom, effectPanel::defaultZoom);

    auto zm = juce::PopupMenu();
    auto zoomTo = [this](int zf) {
        setSize(juce::roundToInt(baseWidth * zf * 0.01), juce::roundToInt(baseHeight * zf * 0.01));
    };

    auto zoomFactor = (int)std::round(100.0 * getWidth() / baseWidth);

    for (auto s : zoomTos) // These are somewhat arbitrary reasonable defaults
    {
        lab = fmt::format("Zoom to {:d}%", s);

        bool ticked = (s == zoomFactor);

        zm.addItem(lab, true, ticked, [this, s, zoomTo]() { zoomTo(s); });
    }

    zm.addSeparator();

    lab = fmt::format("Zoom to Default ({:d}%)", dzf);

    zm.addItem(Surge::GUI::toOSCase(lab), [this, dzf, zoomTo]() { zoomTo(dzf); });

    if ((int)zoomFactor != dzf)
    {
        lab = fmt::format("Set Current Zoom Level ({:d}%) as Default", (int)zoomFactor);

        zm.addItem(Surge::GUI::toOSCase(lab), [this, zoomFactor]() {
            Surge::Storage::updateUserDefaultValue(processor.storage.get(),
                                                   Surge::Storage::FXUnitDefaultZoom, zoomFactor);
        });
    }

    if (isResizable())
    {
        menu.addSubMenu("Zoom", zm);
    }

    menu.addItem(Surge::GUI::toOSCase("Zero Latency Mode"), true, processor.nonLatentBlockMode,
                 [this]() { toggleLatencyMode(); });
}

void SurgefxAudioProcessorEditor::showValueEditor(int i)
{
    auto &knob = fxParamKnobs[i];
    if (!knob.isVisible() || knob.opensChoiceMenu || knob.isEditingBlocked() ||
        !knob.binding.acceptsText)
        return;

    if (!valueEditor)
    {
        valueEditor = std::make_unique<juce::TextEditor>();
        auto &input = *valueEditor;
        input.setFont(effectPanel::font().withHeight(13.f));
        input.setColour(juce::TextEditor::backgroundColourId, effectPanel::panelColour);
        input.setColour(juce::TextEditor::textColourId, effectPanel::text());
        input.setColour(juce::TextEditor::highlightColourId,
                        effectPanel::accent().withAlpha(0.35f));
        input.setColour(juce::TextEditor::highlightedTextColourId, effectPanel::text());
        input.setJustification(juce::Justification::centred);
        input.setSelectAllWhenFocused(true);
        input.onFocusLost = [this]() { valueEditor->setVisible(false); };
        addChildComponent(input);
    }

    auto &input = *valueEditor;
    input.setTitle(juce::String(processor.getParamName(i)));
    input.setColour(juce::TextEditor::focusedOutlineColourId, effectPanel::accent());
    input.setText(juce::String(processor.getParamValue(i)), false);
    const auto effectType = processor.getEffectType();
    const auto close = [this, i, effectType]() {
        valueEditor->setVisible(false);
        auto &control = fxParamKnobs[i];
        if (processor.getEffectType() == effectType && control.isShowing() && control.isEnabled())
            control.grabKeyboardFocus();
    };
    input.onEscapeKey = close;
    input.onReturnKey = [this, i, effectType, close]() {
        if (!valueEditor->isVisible())
            return;
        if (processor.getEffectType() != effectType || fxParamKnobs[i].isEditingBlocked() ||
            fxParamKnobs[i].binding.trySetValueAsString(valueEditor->getText().toStdString()))
        {
            close();
            return;
        }
        valueEditor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::salmon);
        valueEditor->selectAll();
    };

    const juce::Rectangle<int> panel(baseWidth, baseHeight);
    auto bounds = juce::Rectangle<int>(84, 22).withCentre(knob.getBounds().getCentre());
    input.setTransform(effectPanel::panelTransform(getLocalBounds().toFloat()));
    input.setBounds(bounds.constrainedWithin(panel.reduced(4)));
    input.setVisible(true);
    input.toFront(false);
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer(&input)]() {
        if (safe && safe->isShowing())
            safe->grabKeyboardFocus();
    });
}

void SurgefxAudioProcessorEditor::showParameterMenu(int i)
{
    const int effectType = processor.getEffectType();
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(processor.getParamName(i)) + ": " +
                          juce::String(processor.getParamValue(i)));
    auto safe = juce::Component::SafePointer(this);
    if (fxParamKnobs[i].opensChoiceMenu && !fxParamKnobs[i].isEditingBlocked())
    {
        const auto select = [safe, i, effectType](float normalized) {
            if (!safe || safe->processor.getEffectType() != effectType)
                return;
            auto &knob = safe->fxParamKnobs[i];
            if (!knob.opensChoiceMenu || knob.isEditingBlocked())
                return;
            knob.onBeginEdit();
            knob.binding.setValueFromGUI(normalized);
            knob.onEndEdit();
        };
        if (effectType != fxt_airwindows)
        {
            if (const auto range = processor.getFXParamChoiceRange(i))
            {
                const auto [first, last] = *range;
                const auto selected =
                    Parameter::intUnscaledFromFloat(processor.getFXStorageValue01(i), last, first);
                for (int value = first; value <= last; ++value)
                {
                    const auto normalized = Parameter::intScaledToFloat(value, last, first);
                    menu.addItem(processor.getParamValueFor(i, normalized), true, selected == value,
                                 [select, normalized]() { select(normalized); });
                }
                menu.addSeparator();
            }
        }
        else
        {
            const auto &algorithms = AirWindowsEffect::fxreg();
            const auto last = static_cast<int>(algorithms.size()) - 1;
            const auto value = processor.getFXStorageValue01(i);
            std::map<std::string, juce::PopupMenu> groups;
            for (int index : AirWindowsEffect::fxregOrdering())
            {
                if (index < 0 ||
                    index > last) // The display ordering also contains heading markers.
                    continue;
                const auto &algorithm = algorithms[index];
                const auto normalized = Parameter::intScaledToFloat(index, last);
                const auto selected = std::abs(value - normalized) < 0.5f / (last + 1);
                groups[algorithm.groupName].addItem(algorithm.name, true, selected,
                                                    [select, normalized]() { select(normalized); });
            }
            for (auto &[group, choices] : groups)
                menu.addSubMenu(juce::String(group.empty() ? "Other" : group), std::move(choices));
            menu.addSeparator();
        }
    }
    if (fxParamKnobs[i].binding.acceptsText && !fxParamKnobs[i].opensChoiceMenu &&
        !fxParamKnobs[i].isEditingBlocked())
    {
        menu.addItem("Enter value", [safe, i, effectType]() {
            if (safe && safe->processor.getEffectType() == effectType)
                safe->showValueEditor(i);
        });
    }

    auto addOption = [&](const char *name, surge::fx::ParameterOption *option) {
        if (!option->isEnabled())
            return;
        menu.addItem(name, true, option->getToggleState(), [safe, effectType, option]() {
            if (safe && safe->processor.getEffectType() == effectType && option->isEnabled())
            {
                option->setToggleState(!option->getToggleState(), juce::dontSendNotification);
                option->onClick();
            }
        });
    };
    addOption("Tempo sync", &fxTempoSync[i]);
    addOption("Extended range", &fxExtended[i]);
    addOption("Absolute", &fxAbsoluted[i]);
    addOption(effectType == fxt_delay &&
                      processor.getStorageParamIndex(i) == DelayEffect::dly_time_right
                  ? "Link right delay time"
                  : "Deactivate",
              &fxDeactivated[i]);

    if (auto *context = getHostContext())
    {
        if (auto *param = processor.getHostParameterFor(i, SurgeFX::ParamKind::Knob))
        {
            if (auto hostMenu = context->getContextMenuForParameter(param))
                menu.addSubMenu("Host automation", hostMenu->getEquivalentPopupMenu());
        }
    }
    auto *anchor = fxParamKnobs[i].isVisible() ? static_cast<juce::Component *>(&fxParamKnobs[i])
                                               : picker.get();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(anchor));
}

void SurgefxAudioProcessorEditor::showHostContextMenuFor(juce::AudioProcessorParameter *param)
{
    if (!param)
    {
        return;
    }

    if (auto *hostContext = getHostContext())
    {
        if (auto menuInfo = hostContext->getContextMenuForParameter(param))
        {
            auto menu = menuInfo->getEquivalentPopupMenu();

            menu = modifyHostMenu(menu, param);
            menu.showMenuAsync(juce::PopupMenu::Options{});
        }
    }
}

juce::PopupMenu SurgefxAudioProcessorEditor::makeOSCMenu()
{
    juce::PopupMenu menu;
    auto safe = juce::Component::SafePointer(this);
    const bool listening = processor.oscReceiving.load();
    menu.addItem("Enable OSC server", listening || processor.oscPortIn > 0, listening, [safe]() {
        if (!safe)
            return;
        auto &processor = safe->processor;
        if (processor.oscReceiving.load())
            processor.oscHandler.stopListening();
        else if (!processor.initOSCIn(processor.oscPortIn))
            processor.initOSCError(processor.oscPortIn);
    });

    menu.addItem("Input OSC port (" + juce::String(processor.oscPortIn) + ")", [safe]() {
        if (safe)
            safe->showOSCInputPortDialog();
    });

    menu.addItem("FX Message format", [safe]() {
        if (safe)
            juce::NativeMessageBox::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon, "OSC message format",
                "Example: /fx/param/1 0.5\nParameters 1–12, values 0–1.", safe.getComponent());
    });
    return menu;
}

void SurgefxAudioProcessorEditor::showOSCInputPortDialog()
{
    if (oscPortDialog)
        return;

    oscPortDialog = std::make_unique<juce::AlertWindow>(
        "OSC input port", "Enter a port from 1 to 65535, or 0 to disable the OSC server.",
        juce::MessageBoxIconType::NoIcon, this);
    auto &dialog = *oscPortDialog;
    dialog.addTextEditor("port", juce::String(processor.oscPortIn), "Port");
    dialog.addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto *input = dialog.getTextEditor("port");
    input->setTitle("OSC input port");
    dialog.getButton("Apply")->onClick = [this, input]() {
        const auto text = input->getText().trim().toStdString();
        const auto end = text.data() + text.size();
        unsigned int port = 0;
        const auto [ptr, error] = std::from_chars(text.data(), end, port);
        if (error != std::errc{} || ptr != end || port > 65535)
            oscPortDialog->setMessage("Enter a whole number from 0 to 65535.");
        else if (!processor.changeOSCInPort(static_cast<int>(port)))
            oscPortDialog->setMessage("Could not open port " + juce::String(port) +
                                      ". Your previous setting is unchanged.");
        else
        {
            oscPortDialog->exitModalState(1);
            return;
        }

        input->grabKeyboardFocus();
        input->selectAll();
    };

    dialog.setVisible(true);
    dialog.enterModalState(
        true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer(this)](int) {
            if (safe)
                safe->oscPortDialog.reset();
        }));
    input->grabKeyboardFocus();
    input->selectAll();
}

void SurgefxAudioProcessorEditor::onSurgeError(const std::string &msg, const std::string &title,
                                               const SurgeStorage::ErrorType &errorType)
{
    // We could be cleverer than this but for now lets just do this
    juce::MessageManager::callAsync([msg, title]() {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::AlertIconType::WarningIcon, title,
                                               msg, "OK");
    });
}

void SurgefxAudioProcessorEditor::toggleLatencyMode()
{
    auto clm = processor.nonLatentBlockMode;
    Surge::Storage::updateUserDefaultValue(processor.storage.get(),
                                           Surge::Storage::FXUnitAssumeFixedBlock, !clm);
    processor.nonLatentBlockMode = !clm;

    std::ostringstream oss;
    oss << "Please restart the DAW transport or reload your DAW project for this setting "
           "to take effect!\n\n"
        << (clm ? "The processing latency is now 32 samples, "
                  "and variable size audio buffers are supported."
                : "The processing latency is now disabled, so fixed size buffers of at "
                  "least 32 samples are required. Note that some DAWs (particularly FL Studio) "
                  "use variable size buffers by default, so in this mode you have to adjust "
                  "the plugin processing options in your DAW to send fixed size audio buffers.");

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                           "Latency Setting Changed", oss.str());
}

struct FxFocusTrav : public juce::ComponentTraverser
{
    FxFocusTrav(SurgefxAudioProcessorEditor *ed) : editor(ed) {}
    juce::Component *getDefaultComponent(juce::Component *parentComponent) override
    {
        return editor->picker.get();
    }
    juce::Component *searchDir(juce::Component *from, int dir)
    {
        const auto visible = getAllComponents(editor);
        const auto iter = std::find(visible.begin(), visible.end(), from);
        if (iter == visible.end())
            return nullptr;
        if (dir > 0 && std::next(iter) != visible.end())
            return *std::next(iter);
        if (dir < 0 && iter != visible.begin())
            return *std::prev(iter);
        return nullptr;
    }
    juce::Component *getNextComponent(juce::Component *current) override
    {
        return searchDir(current, 1);
    }
    juce::Component *getPreviousComponent(juce::Component *current) override
    {
        return searchDir(current, -1);
    }
    std::vector<juce::Component *> getAllComponents(juce::Component *) override
    {
        std::vector<juce::Component *> visible;
        for (auto *component : editor->accessibleOrderWeakRefs)
            if (component->isVisible() && component->isAccessible())
                visible.push_back(component);
        return visible;
    }
    SurgefxAudioProcessorEditor *editor{nullptr};
};

std::unique_ptr<juce::ComponentTraverser> SurgefxAudioProcessorEditor::createFocusTraverser()
{
    return std::make_unique<FxFocusTrav>(this);
}

bool SurgefxAudioProcessorEditor::keyPressed(const juce::KeyPress &key)
{
    auto zoomTo = [this](int zf) {
        setSize(juce::roundToInt(baseWidth * zf * 0.01), juce::roundToInt(baseHeight * zf * 0.01));
    };
    auto zoomFactor = (int)std::round(100.0 * getWidth() / baseWidth);

    if (key.getTextCharacter() == '/' && (key.getModifiers().isShiftDown()))
    {
        auto dzf = Surge::Storage::getUserDefaultValue(
            processor.storage.get(), Surge::Storage::FXUnitDefaultZoom, effectPanel::defaultZoom);
        zoomTo(dzf);
        return true;
    }
    int incrZoom = 0;
    if (key.getTextCharacter() == '+')
    {
        incrZoom = key.getModifiers().isShiftDown() ? 25 : 10;
    }
    if (key.getTextCharacter() == '-')
    {
        incrZoom = key.getModifiers().isShiftDown() ? -25 : -10;
    }

    if (incrZoom != 0)
    {
        auto nzf = std::clamp(zoomFactor + incrZoom, 75, 250);
        zoomTo(nzf);
        return true;
    }

    return false;
}

void SurgefxAudioProcessorEditor::IdleTimer::timerCallback() { ed->idle(); }

void SurgefxAudioProcessorEditor::idle()
{
    if (processor.m_audioValid != priorValid)
    {
        priorValid = processor.m_audioValid;

        repaint();
    }

    auto pending = processor.consumePendingPresetName();

    if (!pending.empty())
    {
        rebuildCurrentPresets();
        selectPresetByStoredName();
        resetLabels();
    }
}
