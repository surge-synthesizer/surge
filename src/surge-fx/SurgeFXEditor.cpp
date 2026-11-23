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

namespace
{
// As in Shortcircuit, focus the value field after the popup has opened.
class MenuValueEntry : public juce::PopupMenu::CustomComponent
{
  public:
    MenuValueEntry(const juce::String &name, const juce::String &value,
                   std::function<bool(const juce::String &)> commit)
        : CustomComponent(false)
    {
        input.setTitle(name);
        input.setFont(effectPanel::font().withHeight(effectPanel::captionFontHeight));
        input.setColour(juce::TextEditor::backgroundColourId, effectPanel::panelColour);
        input.setColour(juce::TextEditor::textColourId, effectPanel::text());
        input.setColour(juce::TextEditor::focusedOutlineColourId, effectPanel::accent());
        input.setText(value, false);
        input.onReturnKey = [this, commit = std::move(commit)] {
            if (commit(input.getText()))
                triggerMenuItem();
            else
            {
                input.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::salmon);
                input.selectAll();
            }
        };
        input.onEscapeKey = [this] { triggerMenuItem(); };
        addAndMakeVisible(input);
    }

    void getIdealSize(int &width, int &height) override
    {
        width = 156;
        height = 22;
    }

    void resized() override { input.setBounds(getLocalBounds().reduced(3, 1)); }

    void visibilityChanged() override
    {
        if (!isVisible())
            return;
        juce::Timer::callAfterDelay(2, [safe = juce::Component::SafePointer(&input)] {
            if (safe && safe->isShowing())
            {
                safe->grabKeyboardFocus();
                safe->selectAll();
            }
        });
    }

  private:
    juce::TextEditor input;
};
} // namespace

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

    bool menuOpen{false};

    void paintFocus(juce::Graphics &g)
    {
        if (showFocusOutline && hasKeyboardFocus(false) && !menuOpen)
        {
            g.setColour(effectPanel::borderColour);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.f, 1.f);
        }
    }
    void focusGained(FocusChangeType cause) override
    {
        if (cause != focusChangedDirectly)
            showFocusOutline = cause == focusChangedByTabKey;
        repaint();
    }
    bool showFocusOutline{false};
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

    void mouseDown(const juce::MouseEvent &) override
    {
        showFocusOutline = false;
        repaint();
        openMenu();
    }

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

    std::string currentLabel() const override
    {
        return fx_type_names[editor->processor.getEffectType()];
    }

    void paint(juce::Graphics &g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        paintFocus(g);
        g.setColour(menuOpen ? effectPanel::accent() : effectPanel::text());
        g.fillRoundedRectangle(1.f, bounds.getCentreY() - 4.f, 8.f, 8.f, 1.f);
        g.setFont(effectPanel::font().withHeight(15.f));
        g.drawText(currentLabel(), bounds.withTrimmedLeft(13.f).withTrimmedRight(13.f),
                   juce::Justification::centredLeft, true);
        drawMenuArrow(g, bounds);
    }

    void openMenu() override { editor->showEffectMenu(); }

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

struct PanelGlyphButton : juce::Button
{
    using Glyph = sst::jucegui::components::GlyphPainter;

    PanelGlyphButton(const juce::String &label, Glyph::GlyphType icon) : Button(label), glyph(icon)
    {
        setTooltip(label);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void focusGained(FocusChangeType cause) override
    {
        if (cause != focusChangedDirectly)
            showFocusOutline = cause == focusChangedByTabKey;
        Button::focusGained(cause);
    }

    void mouseDown(const juce::MouseEvent &event) override
    {
        showFocusOutline = false;
        repaint();
        Button::mouseDown(event);
    }

    void paintButton(juce::Graphics &g, bool highlighted, bool down) override
    {
        const bool active = isToggleable() ? getToggleState() : highlighted || down;
        // The settings SVG has generous padding around the gear.
        const int size = glyph == Glyph::SETTINGS ? 26 : 18;
        Glyph::paintGlyph(g, getLocalBounds().withSizeKeepingCentre(size, size), glyph,
                          active ? effectPanel::accent() : effectPanel::text());
        if (showFocusOutline && hasKeyboardFocus(false))
        {
            g.setColour(effectPanel::borderColour);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f), 3.f, 1.f);
        }
    }

    Glyph::GlyphType glyph;
    bool showFocusOutline{false};
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
        if (editingName)
        {
            g.setColour(effectPanel::surroundColour);
            g.fillRoundedRectangle(bounds, 3.f);
            g.setColour(effectPanel::accent());
            g.drawRoundedRectangle(bounds.reduced(0.5f), 3.f, 1.f);
            return;
        }

        paintFocus(g);
        g.setColour(menuOpen ? effectPanel::accent() : effectPanel::text());
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse(bounds.getCentreX() + 6.f * i - 1.5f, bounds.getCentreY() - 1.5f, 3.f,
                          3.f);
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

        editor->resized();
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
        editor->resized();
        editor->repaint();

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

    buildEffectMenu();

    lookAndFeel.setStyle(effectPanel::panelStyle());
    popupLayer.setInterceptsMouseClicks(false, true);
    popupLayer.setAlwaysOnTop(true);
    addAndMakeVisible(popupLayer);

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

    tempoSyncPicker =
        std::make_unique<PanelGlyphButton>("Tempo sync all", PanelGlyphButton::Glyph::METRONOME);
    tempoSyncPicker->setClickingTogglesState(true);
    tempoSyncPicker->onClick = [this] { toggleTempoSync(); };
    addAndMakeVisibleRecordOrder(tempoSyncPicker.get());

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
                refreshParameterControls();
            else if (hoveredParam == i || fxParamKnobs[i].hasKeyboardFocus(false))
                repaint();
        };
        knob.onPopupMenu = [this, i](const auto &) { showParameterMenu(i); };
        knob.onBeginEdit = [this, i]() { processor.setUserEditingFXParam(i, true); };
        knob.onEndEdit = [this, i]() { processor.setUserEditingFXParam(i, false); };
        addAndMakeVisibleRecordOrder(&knob);

        fxParamLabels[i].setStyle(style);
        fxParamLabels[i].setAccessible(false); // The knob exposes the parameter name.
        fxParamLabels[i].setInterceptsMouseClicks(false, false);
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
            refreshTempoSyncButton();
            repaint();
        };
        fxTempoSync[i].onPopupMenu = [this, i](const auto &) {
            if (auto *param = processor.getHostParameterFor(i, SurgeFX::ParamKind::TempoSync))
            {
                showHostContextMenuFor(param);
            }
        };
        fxTempoSync[i].setTitle("Parameter " + std::to_string(i) + " Tempo Sync");

        fxDeactivated[i].setEnabled(processor.canDeactivate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        juce::NotificationType::dontSendNotification);
        fxDeactivated[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamDeactivated(i, this->fxDeactivated[i].getToggleState());
            this->processor.setFXStorageDeactivated(i, this->fxDeactivated[i].getToggleState());

            // Special case - coupled dectivation
            this->refreshParameterControls();
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
    settingsButton =
        std::make_unique<PanelGlyphButton>("Settings", PanelGlyphButton::Glyph::SETTINGS);
    settingsButton->onClick = [this] { showSettingsMenu(); };
    addAndMakeVisibleRecordOrder(settingsButton.get());
    refreshParameterControls();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setZoomPercent(effectPanel::defaultZoom);
    setResizable(false, false); // Resize only through the editor's zoom controls.

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 25);
}

juce::PopupMenu SurgefxAudioProcessorEditor::modifyHostMenu(juce::PopupMenu menu,
                                                            juce::AudioProcessorParameter *param)
{
    const juce::PluginHostType host;
    if (!host.isFruityLoops() && !host.isReaper())
        return menu;

    auto newMenu = juce::PopupMenu();

    const auto kind = processor.paramKindForIndex[param->getParameterIndex()];
    const auto slot = processor.paramSlotForIndex[param->getParameterIndex()];
    newMenu.addSectionHeader(processor.getMutableName(kind, slot));
    newMenu.addSeparator();

    // make things look a bit nicer for our friends from Image-Line
    if (host.isFruityLoops())
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
    else if (host.isReaper())
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

void SurgefxAudioProcessorEditor::refreshParameterControls()
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
        knob.setAlwaysQuantize(knob.binding.discreteStep > 0.f);
        knob.binding.bipolar = processor.isParamBipolar(i);
        knob.binding.acceptsText = processor.canSetParameterByString(i);
        const auto slot = processor.getStorageParamIndex(i);
        using Kind = effectPanel::Kind;
        using Presentation = surge::fx::ParameterKnob::Presentation;
        const auto display = std::ranges::find_if(layout, [slot](const auto &c) {
            return c.slot == slot && (c.kind == Kind::choice || c.kind == Kind::fader);
        });
        auto presentation = Presentation::rotary;
        if (display != layout.end())
            presentation =
                display->kind == Kind::choice ? Presentation::choice : Presentation::fader;
        knob.setPresentation(presentation);
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

        fxDeactivated[i].setEnabled(processor.canDeactivate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        juce::NotificationType::dontSendNotification);
        fxDeactivated[i].setAccessible(processor.canDeactivate(i));
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

    // Rebuild preset list for the new FX type and reset selection. The settings are the new
    // type's defaults now, not the previously loaded preset.
    processor.setCurrentPresetName("");
    rebuildCurrentPresets();
    currentPresetIndex = defaultPresetIndexForCurrentType();
    lastSeenEffectType = i;

    refreshParameterControls();
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
                    refreshParameterControls();
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
                    rebuildCurrentPresets();

                    // A host type change has already dropped the stored name, while a state
                    // restore has just set the one to select
                    selectPresetByStoredName();
                }

                // The processor also signals metadata changes without a type change,
                // e.g. when Airwindows finishes configuring a different algorithm.
                refreshParameterControls();
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

            if (syncToggle(fxTempoSync[i], processor.getFXStorageTempoSync(i)))
            {
                refreshTempoSyncButton();
                displayDirty = true;
            }
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
            refreshParameterControls();
        }
    }
}

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
    refreshParameterControls();

    if (presetPicker)
    {
        presetPicker->repaint();
    }
}

void SurgefxAudioProcessorEditor::showPresetMenu()
{
    if (presetPicker->menuOpen)
        return;

    auto p = juce::PopupMenu();
    p.addSectionHeader(presetPicker->currentLabel());

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

    p.addSeparator();
    if (currentPresets.empty())
        p.addItem("No presets available", false, false, [] {});

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

            firstSection = false;

            p.addSectionHeader(preset.isFactory ? "FACTORY" : "USER");
            lastIsFactory = preset.isFactory;
            currentSubMenuPath.clear();
        }

        if (preset.subPath.empty())
        {
            flushSubMenu();

            addItem(p);
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
    presetPicker->menuOpen = true;
    presetPicker->repaint();
    p.showMenuAsync(
        menuOptions(presetPicker.get())
            .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),
        [safe = juce::Component::SafePointer(this)](int) {
            if (safe)
            {
                safe->presetPicker->menuOpen = false;
                safe->presetPicker->repaint();
            }
        });
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
int SurgefxAudioProcessorEditor::getStatusParameter() const
{
    if (hoveredParam >= 0 && fxParamKnobs[hoveredParam].isVisible())
        return hoveredParam;

    for (int i = 0; i < n_fx_params; ++i)
        if (fxParamKnobs[i].hasKeyboardFocus(false))
            return i;
    return -1;
}

void SurgefxAudioProcessorEditor::paint(juce::Graphics &g)
{
    const auto panel = getLocalBounds().toFloat();
    effectPanel::paint(g, panel);
    if (panel.isEmpty())
        return;

    g.addTransform(effectPanel::panelTransform(panel));
    g.setColour(effectPanel::borderColour);
    const auto ruleStart = static_cast<float>(effectPanel::gutter);
    const auto ruleEnd = ruleStart + effectPanel::contentWidth;
    if (!presetPicker->editingName)
    {
        const auto right =
            tempoSyncPicker->isVisible() ? tempoSyncPicker->getX() : presetPicker->getX();
        if (right - picker->getRight() > 12)
            g.drawHorizontalLine(effectPanel::headerArea.getCentreY(), picker->getRight() + 6.f,
                                 right - 6.f);
    }
    g.drawHorizontalLine(effectPanel::statusArea.getY(), ruleStart, ruleEnd);
    auto statusBounds = effectPanel::statusArea.withRight(settingsButton->getX() - 4);
    juce::String status;
    g.setFont(effectPanel::font().withHeight(11.f));
    if (!processor.m_audioValid && !processor.m_audioValidMessage.empty())
    {
        status = processor.m_audioValidMessage;
    }
    else if (const auto param = getStatusParameter(); param >= 0)
    {
        status =
            juce::String(processor.getParamName(param)) + ": " + fxParamKnobs[param].binding.text;
        using Glyph = sst::jucegui::components::GlyphPainter;
        const auto active = [](const auto &option) {
            return option.isEnabled() && option.getToggleState();
        };
        const auto drawGlyph = [&](Glyph::GlyphType glyph) {
            const auto area = statusBounds.removeFromRight(15).withSizeKeepingCentre(12, 12);
            Glyph::paintGlyph(g, area, glyph, effectPanel::accent());
        };
        if (active(fxDeactivated[param]))
        {
            const bool linked =
                processor.getEffectType() == fxt_delay &&
                processor.getStorageParamIndex(param) == DelayEffect::dly_time_right;
            drawGlyph(linked ? Glyph::LINK : Glyph::SMALL_POWER_LIGHT_OFF);
        }
        if (active(fxAbsoluted[param]))
        {
            g.setColour(effectPanel::accent());
            g.drawText("|x|", statusBounds.removeFromRight(15), juce::Justification::centred);
        }
        if (active(fxExtended[param]))
            drawGlyph(Glyph::PLUS_MINUS);
        if (active(fxTempoSync[param]))
            drawGlyph(Glyph::METRONOME);
    }
    g.setColour(status.isEmpty() ? effectPanel::text().withAlpha(0.55f) : effectPanel::accent());
    g.drawText(status.isEmpty() ? juce::String("SURGE XT  /  EFFECTS") : status, statusBounds,
               juce::Justification::centredLeft, true);

    if (processor.getEffectType() == fxt_off)
    {
        g.setColour(effectPanel::text());
        g.setFont(effectPanel::font().boldened());
        g.drawText("No effect selected", effectPanel::gutter, 117, effectPanel::contentWidth, 22,
                   juce::Justification::centred);
        g.setColour(effectPanel::text().withAlpha(0.8f));
        g.setFont(effectPanel::font());
        g.drawText("Choose an effect from the menu above", effectPanel::gutter, 141,
                   effectPanel::contentWidth, 18, juce::Justification::centred);
    }
}

void SurgefxAudioProcessorEditor::resized()
{
    const auto panel = getLocalBounds().toFloat();
    if (panel.isEmpty())
        return;

    const auto transform = effectPanel::panelTransform(panel);
    popupLayer.setBounds(0, 0, baseWidth, baseHeight);
    popupLayer.setTransform(transform);
    settingsButton->setTransform(transform);
    settingsButton->setBounds(
        effectPanel::statusArea.withTrimmedLeft(effectPanel::contentWidth - 24));
    auto header = effectPanel::headerArea;
    presetPicker->setTransform(transform);
    presetPicker->setBounds(presetPicker->editingName ? header : header.removeFromRight(26));
    tempoSyncPicker->setTransform(transform);
    const bool hasTempoSync =
        std::ranges::any_of(fxTempoSync, [](const auto &option) { return option.isEnabled(); });
    tempoSyncPicker->setVisible(hasTempoSync && !presetPicker->editingName);
    if (tempoSyncPicker->isVisible())
    {
        header.removeFromRight(3);
        tempoSyncPicker->setBounds(header.removeFromRight(24));
    }
    refreshTempoSyncButton();
    picker->setTransform(transform);
    const auto titleWidth = juce::GlyphArrangement::getStringWidthInt(
        effectPanel::font().withHeight(15.f), picker->currentLabel());
    picker->setBounds(header.removeFromLeft(juce::jlimit(48, 160, titleWidth + 29)));

    using Kind = effectPanel::Kind;
    const auto effect = processor.getEffectType();
    const auto &layout = effectPanel::layout(effect);
    const auto origin = effectPanel::contentOrigin;
    if (panelLabelEffect != effect)
    {
        panelLabelEffect = effect;
        panelLabels.clear();
        for (const auto &control : layout)
        {
            if (control.kind != Kind::group)
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

    std::array<int, n_fx_params> slotToParam;
    slotToParam.fill(-1);
    int lastSlot = 0;
    for (int i = 0; i < n_fx_params; ++i)
    {
        if (!processor.getParamEnabled(i))
            continue;
        const auto slot = processor.getStorageParamIndex(i);
        if (slot < 0 || slot >= n_fx_params)
            continue;
        slotToParam[slot] = i;
        lastSlot = std::max(lastSlot, slot);
    }
    const bool roomyAirwindows = effect == fxt_airwindows && lastSlot <= 6;

    std::array<bool, n_fx_params> visibleKnobs{}, visibleNames{}, visibleLights{}, visibleExtend{};
    size_t labelIndex = 0;
    for (const auto &control : layout)
    {
        const auto pixels = control.bounds.toNearestIntEdges() + origin;
        if (control.kind == Kind::group)
        {
            auto &label = *panelLabels[labelIndex++];
            label.setTransform(transform);
            label.setBounds(pixels);
            continue;
        }
        if (control.slot < 0 || control.slot >= n_fx_params)
            continue;
        const int i = slotToParam[control.slot];
        if (i < 0)
            continue;

        switch (control.kind)
        {
        case Kind::knob:
        case Kind::fader:
        case Kind::choice:
        {
            auto controlBounds = pixels;
            if (roomyAirwindows && control.slot > 0)
            {
                const auto slot = control.slot - 1;
                controlBounds =
                    juce::Rectangle<int>(36 + 112 * (slot % 2), 69 + 61 * (slot / 2), 40, 40) +
                    origin;
            }
            const auto size = std::min(controlBounds.getWidth(), controlBounds.getHeight());
            auto &knob = fxParamKnobs[i];
            knob.setTransform(transform);
            knob.setBounds(control.kind == Kind::knob
                               ? controlBounds.withSizeKeepingCentre(size, size)
                               : controlBounds);
            knob.repaint();
            visibleKnobs[i] = true;

            auto name =
                control.useParamName ? processor.getParamName(i) : control.label.toStdString();
            if (!name.empty())
            {
                auto &label = fxParamLabels[i];
                label.setText(name);
                label.setTransform(transform);
                if (knob.isFader())
                {
                    label.setFontHeightOverride(10);
                    label.setJustification(juce::Justification::centred);
                    label.setBounds(pixels.withY(pixels.getBottom() + 3)
                                        .withHeight(effectPanel::captionHeight));
                }
                else if (effect == fxt_airwindows)
                {
                    label.setFontHeightOverride(effectPanel::captionFontHeight);
                    if (roomyAirwindows)
                    {
                        label.setJustification(juce::Justification::centred);
                        label.setBounds(controlBounds.getCentreX() - 42, controlBounds.getBottom(),
                                        84, effectPanel::captionHeight);
                    }
                    else
                    {
                        label.setJustification(juce::Justification::centredLeft);
                        const auto labelLeft = pixels.getRight() + 3;
                        const auto columnEnd = (control.slot % 2 == 1 ? 112 : 224) + origin.x;
                        label.setBounds(labelLeft, pixels.getY(), columnEnd - labelLeft - 4,
                                        pixels.getHeight());
                    }
                }
                else
                {
                    label.setFontHeightOverride(effectPanel::captionFontHeight);
                    label.setJustification(juce::Justification::centred);
                    label.setBounds(effectPanel::captionBounds(layout, control) + origin);
                }
                visibleNames[i] = true;
            }
            break;
        }
        case Kind::power:
            fxDeactivated[i].setTransform(transform);
            fxDeactivated[i].setBounds(pixels);
            visibleLights[i] = true;
            break;
        case Kind::extend:
            fxExtended[i].setTransform(transform);
            fxExtended[i].setBounds(pixels);
            visibleExtend[i] = true;
            break;
        case Kind::group:
            break; // Already placed above.
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

void SurgefxAudioProcessorEditor::buildEffectMenu()
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
        }
        menu.push_back(men);

        c = c->NextSiblingElement();
    }
}

void SurgefxAudioProcessorEditor::showEffectMenu()
{
    if (picker->menuOpen)
        return;

    auto p = juce::PopupMenu();
    auto safe = juce::Component::SafePointer(this);

    for (const auto &m : menu)
    {
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

    auto o = menuOptions(picker.get())
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

void SurgefxAudioProcessorEditor::toggleTempoSync()
{
    const bool enable = tempoSyncPicker->getToggleState();
    for (auto &option : fxTempoSync)
    {
        if (!option.isEnabled() || option.getToggleState() == enable)
            continue;
        option.setToggleState(enable, juce::dontSendNotification);
        option.onClick();
    }
}

void SurgefxAudioProcessorEditor::refreshTempoSyncButton()
{
    const bool any =
        std::ranges::any_of(fxTempoSync, [](const auto &option) { return option.isEnabled(); });
    const bool all = std::ranges::all_of(fxTempoSync, [](const auto &option) {
        return !option.isEnabled() || option.getToggleState();
    });
    tempoSyncPicker->setToggleState(any && all, juce::dontSendNotification);
}

void SurgefxAudioProcessorEditor::setZoomPercent(int percent)
{
    const auto scale = effectPanel::nativeScale * percent * 0.01;
    setSize(juce::roundToInt(baseWidth * scale), juce::roundToInt(baseHeight * scale));
}

int SurgefxAudioProcessorEditor::getZoomPercent() const
{
    return juce::roundToInt(100.0 * getWidth() / (effectPanel::nativeScale * baseWidth));
}

void SurgefxAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSubMenu("OSC", makeOSCMenu());

    juce::PopupMenu zm;
    const auto zoomFactor = getZoomPercent();
    for (int percent : {75, effectPanel::defaultZoom, 125, 150, 200})
    {
        zm.addItem(fmt::format("Zoom to {}%", percent), true, percent == zoomFactor,
                   [this, percent] { setZoomPercent(percent); });
    }

    menu.addSubMenu("Zoom", zm);

    menu.addItem(Surge::GUI::toOSCase("Zero Latency Mode"), true, processor.nonLatentBlockMode,
                 [this]() { toggleLatencyMode(); });
    menu.showMenuAsync(
        menuOptions(settingsButton.get())
            .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::upwards));
}

juce::PopupMenu::Options SurgefxAudioProcessorEditor::menuOptions(juce::Component *anchor)
{
    // Six Sines parents menus inside its zoom transform. Use the same logical
    // coordinates here without transforming the host-facing editor bounds.
    // The editor is too narrow for adjacent menu columns.
    auto options =
        juce::PopupMenu::Options().withParentComponent(&popupLayer).withMaximumNumColumns(1);
    return anchor ? options.withTargetComponent(anchor) : options;
}

void SurgefxAudioProcessorEditor::showParameterMenu(int i)
{
    const int effectType = processor.getEffectType();
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(processor.getParamName(i)) + ": " +
                          juce::String(processor.getParamValue(i)));
    auto safe = juce::Component::SafePointer(this);
    if (fxParamKnobs[i].isChoice() && !fxParamKnobs[i].isEditingBlocked())
    {
        const auto select = [safe, i, effectType](float normalized) {
            if (!safe || safe->processor.getEffectType() != effectType)
                return;
            auto &knob = safe->fxParamKnobs[i];
            if (!knob.isChoice() || knob.isEditingBlocked())
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
    if (!fxParamKnobs[i].isEditingBlocked())
    {
        const auto reset = [safe, i, effectType]() {
            if (!safe || safe->processor.getEffectType() != effectType)
                return;
            auto &knob = safe->fxParamKnobs[i];
            if (knob.isEditingBlocked())
                return;
            knob.onBeginEdit();
            knob.binding.setValueFromGUI(knob.binding.getDefaultValue());
            knob.onEndEdit();
        };
        if (fxParamKnobs[i].binding.acceptsText && !fxParamKnobs[i].isChoice())
        {
            menu.addSeparator();
            menu.addCustomItem(-1,
                               std::make_unique<MenuValueEntry>(
                                   processor.getParamName(i), processor.getParamValue(i),
                                   [safe, i, effectType, reset](const juce::String &value) {
                                       if (!safe || safe->processor.getEffectType() != effectType)
                                           return true;
                                       if (value.trim().isEmpty())
                                       {
                                           reset();
                                           return true;
                                       }
                                       return safe->fxParamKnobs[i].binding.trySetValueAsString(
                                           value.toStdString());
                                   }));
            menu.addSeparator();
        }
        menu.addItem("Set to Default", reset);
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
            {
                auto items = modifyHostMenu(hostMenu->getEquivalentPopupMenu(), param);
                if (items.getNumItems() > 0)
                {
                    menu.addSeparator();
                    menu.addItem("Host Automation...", [safe, i, effectType,
                                                        items = std::move(items)]() mutable {
                        if (!safe || safe->processor.getEffectType() != effectType)
                            return;
                        auto &knob = safe->fxParamKnobs[i];
                        auto options = safe->menuOptions(knob.isShowing() ? &knob : nullptr);
                        items.showMenuAsync(options.withMinimumWidth(baseWidth - 32));
                    });
                }
            }
        }
    }
    auto *anchor = fxParamKnobs[i].isVisible() ? static_cast<juce::Component *>(&fxParamKnobs[i])
                                               : picker.get();
    menu.showMenuAsync(menuOptions(anchor), [safe, i, effectType](int) {
        if (!safe || safe->processor.getEffectType() != effectType)
            return;

        // The inline editor takes focus from the knob. Put it back unless the user
        // or a host menu action moved focus outside the FX editor.
        auto *focused = juce::Component::getCurrentlyFocusedComponent();
        auto &knob = safe->fxParamKnobs[i];
        if (knob.isShowing() &&
            (!focused || focused == safe.getComponent() || safe->isParentOf(focused)))
            knob.grabKeyboardFocus();
    });
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
            menu.showMenuAsync(menuOptions());
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
    auto zoomTo = [this](int zf) { setZoomPercent(zf); };
    auto zoomFactor = getZoomPercent();

    if (key.getTextCharacter() == '/' && (key.getModifiers().isShiftDown()))
    {
        zoomTo(effectPanel::defaultZoom);
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
        refreshParameterControls();
    }
}
