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
#include "FxPresetAndClipboardManager.h"
#include "tinyxml/tinyxml.h"
#include "fmt/core.h"
#include "SurgeStorage.h"

static void drawUpDownArrows(juce::Graphics &g, juce::Rectangle<float> arrowRect,
                             juce::Colour colour)
{
    g.setColour(colour);

    const float w = arrowRect.getWidth();
    const float h = arrowRect.getHeight();
    const float cx = arrowRect.getCentreX();
    const float cy = arrowRect.getCentreY();
    const float sz = std::min(w * 0.25f, h * 0.25f);
    const float gap = sz * 0.8f;

    juce::Path up;
    up.addTriangle(cx - sz, cy - gap, cx + sz, cy - gap, cx, cy - gap - sz * 1.25f);
    g.fillPath(up);

    juce::Path dn;
    dn.addTriangle(cx - sz, cy + gap, cx + sz, cy + gap, cx, cy + gap + sz * 1.25f);
    g.fillPath(dn);
}

static constexpr float arrowZoneWidth = 24.f;
static constexpr float pickerFontSize = 20.f;

struct PickerBase : public juce::Component
{
    PickerBase(SurgefxAudioProcessorEditor *ed, const std::string &accessibleName) : editor(ed)
    {
        setAccessible(true);
        setTitle(accessibleName);
        setDescription(accessibleName);
        setWantsKeyboardFocus(true);
    }

    virtual std::string currentLabel() const = 0;
    virtual void openMenu() = 0;
    virtual void step(int direction) = 0;

    float arrowSplitX() const { return getWidth() - arrowZoneWidth * editor->getImpliedZoom(); }

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.f, 2.f);
        auto edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledEdge);
        auto textCol = findColour(SurgeLookAndFeel::SurgeColourIds::paramDisplay);

        g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledBg));
        g.fillRoundedRectangle(bounds, 5);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, 5, 1);

        const float splitX = arrowSplitX();
        g.setColour(edge.withAlpha(0.5f));
        g.drawLine(splitX, bounds.getY() + 4, splitX, bounds.getBottom() - 4, 1.f);

        g.setColour(textCol);
        g.setFont(SST_JUCE_FONT_OPTIONS(pickerFontSize * editor->getImpliedZoom()));
        auto labelRect = bounds.withWidth(splitX - bounds.getX() - 4.f);
        g.drawText(currentLabel(), labelRect.reduced(8, 3), juce::Justification::centred, true);

        drawUpDownArrows(g, bounds.withLeft(splitX), textCol);
    }

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

    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.x >= arrowSplitX())
        {

            const float mid = getHeight() * 0.5f;
            step(e.y < mid ? -1 : 1);
        }
        else
        {
            openMenu();
        }
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

struct PresetPicker : public PickerBase
{
    PresetPicker(SurgefxAudioProcessorEditor *ed) : PickerBase(ed, "FX Preset") {}

    std::string currentLabel() const override
    {
        if (editor->currentPresetIndex < 0 || editor->currentPresets.empty() ||
            editor->currentPresetIndex >= (int)editor->currentPresets.size())
        {
            return "- Init -";
        }
        return editor->currentPresets[editor->currentPresetIndex].name;
    }

    void openMenu() override { editor->showPresetMenu(); }
    void step(int direction) override { editor->stepPreset(direction); }
};

//==============================================================================
void SurgefxAudioProcessorEditor::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isAltDown())
    {
        for (int i = 0; i < n_fx_params; ++i)
        {
            if (&fxParamSliders[i] == e.eventComponent)
            {
                rubberbandParamIdx = i;
            }
        }

        rubberbandValue = (float)fxParamSliders[rubberbandParamIdx].getValue();
        rubberbandMode = true;
    }
}

void SurgefxAudioProcessorEditor::mouseUp(const juce::MouseEvent &e)
{
    if (rubberbandMode && rubberbandParamIdx > -1)
    {
        const auto idx = rubberbandParamIdx;

        processor.setFXParamValue01(idx, rubberbandValue, false);
        fxParamSliders[idx].setValue(rubberbandValue, juce::NotificationType::dontSendNotification);
        fxParamDisplay[idx].setDisplay(processor.getParamValue(idx));
        fxParamSliders[idx].setTextValue(processor.getParamValue(idx).c_str());

        rubberbandMode = false;
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

    fxPresetManager = std::make_unique<Surge::Storage::FxUserPreset>();
    fxPresetManager->doPresetRescan(processor.storage.get());

    rebuildCurrentPresets();

    currentPresetIndex = defaultPresetIndexForCurrentType();
    lastSeenEffectType = processor.getEffectType();

    presetPicker = std::make_unique<PresetPicker>(this);
    addAndMakeVisibleRecordOrder(presetPicker.get());

    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamSliders[i].setRange(0.0, 1.0, 0.0);
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i),
                                   juce::NotificationType::dontSendNotification);
        fxParamSliders[i].setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        fxParamSliders[i].setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0,
                                          0);
        fxParamSliders[i].setMouseDragSensitivity(750);
        fxParamSliders[i].setVelocityModeParameters(0.04, 1, 0.04, true,
                                                    juce::ModifierKeys::shiftModifier);
        fxParamSliders[i].setChangeNotificationOnlyOnRelease(false);
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i));
        fxParamSliders[i].addMouseListener(this, false);

        fxParamSliders[i].onValueChange = [i, this]() {
            const bool forceInteger = juce::ModifierKeys::getCurrentModifiers().isCtrlDown();

            processor.prepareParametersAbsentAudio();
            processor.setFXParamValue01(i, this->fxParamSliders[i].getValue(), forceInteger);

            if (forceInteger)
            {
                fxParamSliders[i].setValue(processor.getFXParamValue01(i),
                                           juce::NotificationType::dontSendNotification);
            }

            fxParamDisplay[i].setDisplay(processor.getParamValue(i));
            fxParamSliders[i].setTextValue(processor.getParamValue(i).c_str());
        };

        fxParamSliders[i].onDragStart = [i, this]() {
            this->processor.setUserEditingFXParam(i, true);
        };

        fxParamSliders[i].onDragEnd = [i, this]() {
            this->processor.setUserEditingFXParam(i, false);
        };

        fxParamSliders[i].setTitle("Parameter " + std::to_string(i) + " Knob");

        addAndMakeVisibleRecordOrder(&(fxParamSliders[i]));

        fxTempoSync[i].setOnOffImage(BinaryData::TS_Act_svg, BinaryData::TS_Act_svgSize,
                                     BinaryData::TS_Deact_svg, BinaryData::TS_Deact_svgSize);
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      juce::NotificationType::dontSendNotification);
        fxTempoSync[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamTempoSync(i, this->fxTempoSync[i].getToggleState());
            this->processor.setFXStorageTempoSync(i, this->fxTempoSync[i].getToggleState());
            fxParamDisplay[i].setDisplay(
                processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
            this->processor.setUserEditingParamFeature(i, false);
        };

        fxTempoSync[i].setTitle("Parameter " + std::to_string(i) + " Tempo Sync");
        addAndMakeVisibleRecordOrder(&(fxTempoSync[i]));

        fxDeactivated[i].setOnOffImage(BinaryData::DE_Act_svg, BinaryData::DE_Act_svgSize,
                                       BinaryData::DE_Deact_svg, BinaryData::DE_Deact_svgSize);
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
        fxDeactivated[i].setTitle("Parameter " + std::to_string(i) + " Deactivate");
        addAndMakeVisibleRecordOrder(&(fxDeactivated[i]));

        fxExtended[i].setOnOffImage(BinaryData::EX_Act_svg, BinaryData::EX_Act_svgSize,
                                    BinaryData::EX_Deact_svg, BinaryData::EX_Deact_svgSize);
        fxExtended[i].setEnabled(processor.canExtend(i));
        fxExtended[i].setToggleState(processor.getFXStorageExtended(i),
                                     juce::NotificationType::dontSendNotification);
        fxExtended[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamExtended(i, this->fxExtended[i].getToggleState());
            this->processor.setFXStorageExtended(i, this->fxExtended[i].getToggleState());
            fxParamDisplay[i].setDisplay(
                processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
            this->processor.setUserEditingParamFeature(i, false);
        };
        fxExtended[i].setTitle("Parameter " + std::to_string(i) + " Extended");
        addAndMakeVisibleRecordOrder(&(fxExtended[i]));

        fxAbsoluted[i].setOnOffImage(BinaryData::AB_Act_svg, BinaryData::AB_Act_svgSize,
                                     BinaryData::AB_Deact_svg, BinaryData::AB_Deact_svgSize);
        fxAbsoluted[i].setEnabled(processor.canAbsolute(i));
        fxAbsoluted[i].setToggleState(processor.getFXParamAbsolute(i),
                                      juce::NotificationType::dontSendNotification);
        fxAbsoluted[i].onClick = [i, this]() {
            this->processor.setUserEditingParamFeature(i, true);
            this->processor.setFXParamAbsolute(i, this->fxAbsoluted[i].getToggleState());
            this->processor.setFXStorageAbsolute(i, this->fxAbsoluted[i].getToggleState());
            fxParamDisplay[i].setDisplay(
                processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
            this->processor.setUserEditingParamFeature(i, false);
        };

        fxAbsoluted[i].setTitle("Parameter " + std::to_string(i) + " Absolute");
        addAndMakeVisibleRecordOrder(&(fxAbsoluted[i]));

        processor.prepareParametersAbsentAudio();
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].setDisplay(processor.getParamValue(i));
        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        fxParamDisplay[i].onOverlayEntered = [i, this](const std::string &s) {
            processor.setParameterByString(i, s);
        };

        addAndMakeVisibleRecordOrder(&(fxParamDisplay[i]));
    }

    fxNameLabel = std::make_unique<juce::Label>("fxlabel", "Surge XT Effects");
    fxNameLabel->setFont(SST_JUCE_FONT_OPTIONS(26));
    fxNameLabel->setColour(juce::Label::textColourId, juce::Colours::black);
    fxNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisibleRecordOrder(fxNameLabel.get());

    this->processor.setParameterChangeListener([this]() { this->triggerAsyncUpdate(); });

    setTitle("Surge XT Effects");
    setDescription("Surge XT Effects");
    resetLabels();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    auto dzf = Surge::Storage::getUserDefaultValue(processor.storage.get(),
                                                   Surge::Storage::FXUnitDefaultZoom, 100);

    setSize(0.01 * dzf * baseWidth, 0.01 * dzf * baseHeight);
    setResizable(true, true);
    getConstrainer()->setMinimumWidth(baseWidth * 0.75);
    getConstrainer()->setFixedAspectRatio(baseWidth * 1.0 / baseHeight);

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 25);
}

SurgefxAudioProcessorEditor::~SurgefxAudioProcessorEditor()
{
    idleTimer->stopTimer();
    setLookAndFeel(nullptr);
    this->processor.setParameterChangeListener([]() {});
    this->processor.storage->removeErrorListener(this);
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
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i),
                                   juce::NotificationType::dontSendNotification);
        fxParamSliders[i].setDoubleClickReturnValue(true, processor.getFXDefaultValue01(i),
                                                    juce::ModifierKeys());
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i) &&
                                     !processor.getFXStorageAppearsDeactivated(i));

        st(fxParamSliders[i], nm + " Knob");
        fxParamSliders[i].setTextValue(processor.getParamValue(i).c_str());

        fxParamDisplay[i].setDisplay(processor.getParamValue(i).c_str());
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].allowsTypein = processor.canSetParameterByString(i);

        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        fxParamDisplay[i].setAppearsDeactivated(processor.getFXStorageAppearsDeactivated(i));

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
        st(fxDeactivated[i], nm + " Deactivated");
    }

    picker->repaint();
    if (presetPicker)
    {
        presetPicker->repaint();
    }

    if (auto h = getAccessibilityHandler())
    {
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
    }
}

void SurgefxAudioProcessorEditor::setEffectType(int i)
{
    processor.resetFxType(i);
    blastToggleState(i - 1);

    // Rebuild preset list for the new FX type and reset selection
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
        if (cv[i])
        {
            if (i < n_fx_params)
            {
                fxParamSliders[i].setValue(fv[i], juce::NotificationType::dontSendNotification);
                fxParamDisplay[i].setDisplay(processor.getParamValueFor(i, fv[i]));
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
                    currentPresetIndex = defaultPresetIndexForCurrentType();
                    resetLabels();
                }
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

    if (currentPresets.empty())
    {
        p.addItem("No presets available", false, false, []() {});

        auto o =
            juce::PopupMenu::Options()
                .withTargetComponent(presetPicker.get())
                .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);
        p.showMenuAsync(o);
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

            p.addSectionHeader(preset.isFactory ? "Factory" : "User");
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

    auto o = juce::PopupMenu::Options()
                 .withTargetComponent(presetPicker.get())
                 .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);

    p.showMenuAsync(o);
}

//==============================================================================
void SurgefxAudioProcessorEditor::paint(juce::Graphics &g)
{
    surgeLookFeel->paintComponentBackground(g, getWidth(), getHeight(), getImpliedZoom());
}

void SurgefxAudioProcessorEditor::resized()
{
    auto z = getImpliedZoom();

    const float pickerY = 16 * z;
    const float pickerH = (topSection - 36.f) * z;
    const float sideMargin = 9 * z;
    const float pickerGap = 18 * z;
    const float totalPickerW = getWidth() - 2.f * sideMargin - pickerGap;
    const float halfW = totalPickerW * 0.5f;

    picker->setBounds(
        juce::Rectangle<float>(sideMargin, pickerY, halfW, pickerH).toNearestIntEdges());

    if (presetPicker)
    {
        presetPicker->setBounds(
            juce::Rectangle<float>(sideMargin + halfW + pickerGap, pickerY, halfW, pickerH)
                .toNearestIntEdges());
    }

    const auto ypos0 = (topSection - 5.f) * z;
    const auto rowHeight = (getHeight() - topSection * z - 40.f * z - 10.f * z) / 6.f;
    const auto byoff = 7 * z;
    const auto sliderOff = (getWidth() < baseWidth) ? 2.f * z : 5.f * z;
    const auto buttonSize = (getWidth() < baseWidth) ? 17.f * z : 19.f * z;
    const auto buttonMargin = std::max(1, (int)(1.f * z));

    for (int i = 0; i < n_fx_params; ++i)
    {
        juce::Rectangle<float> position{(i / 6) * getWidth() / 2.f + sliderOff,
                                        (i % 6) * rowHeight + ypos0, rowHeight - sliderOff,
                                        rowHeight - sliderOff};
        fxParamSliders[i].setBounds(position.toNearestIntEdges());

        juce::Rectangle<float> tsPos{(i / 6) * getWidth() / 2.f + 2.f * z + rowHeight - 5.f * z,
                                     (i % 6) * rowHeight + ypos0 + byoff + buttonMargin, buttonSize,
                                     buttonSize};
        fxTempoSync[i].setBounds(tsPos.toNearestIntEdges());

        juce::Rectangle<float> daPos{(i / 6) * getWidth() / 2.f + 2.f * z + rowHeight - 5.f * z,
                                     (i % 6) * rowHeight + ypos0 + byoff + 2.f * buttonMargin +
                                         buttonSize,
                                     buttonSize, buttonSize};
        fxDeactivated[i].setBounds(daPos.toNearestIntEdges());

        juce::Rectangle<float> exPos{
            (i / 6) * getWidth() / 2.f + 2.f * z + rowHeight - 5.f * z + buttonMargin + buttonSize,
            (i % 6) * rowHeight + ypos0 + byoff + buttonMargin, buttonSize, buttonSize};
        fxExtended[i].setBounds(exPos.toNearestIntEdges());

        juce::Rectangle<float> abPos{
            (i / 6) * getWidth() / 2.f + 2.f * z + rowHeight - 5.f * z + buttonMargin + buttonSize,
            (i % 6) * rowHeight + ypos0 + byoff + 2.f * buttonMargin + buttonSize, buttonSize,
            buttonSize};
        fxAbsoluted[i].setBounds(abPos.toNearestIntEdges());

        juce::Rectangle<float> dispPos{(i / 6) * getWidth() / 2.f + 4.f * z + rowHeight - 5.f * z +
                                           2.f * buttonMargin + 2.f * buttonSize,
                                       (i % 6) * rowHeight + ypos0,
                                       getWidth() / 2.f - rowHeight - 8.f * z - 2.f * buttonMargin -
                                           2.f * buttonSize,
                                       rowHeight - 5.f * z};
        fxParamDisplay[i].setBounds(dispPos.toNearestIntEdges());
    }

    juce::Rectangle<float> titleRect{34.f * z, getHeight() - (40.f * z), 350.f * z, 40.f * z};

    fxNameLabel->setFont(SST_JUCE_FONT_OPTIONS(26 * z));
    fxNameLabel->setBounds(titleRect.toNearestIntEdges());
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
    auto p = juce::PopupMenu();

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
            p.addItem(m.name, [this, m]() {
                if (m.fxtype > 0)
                {
                    setEffectType(m.fxtype);
                }
            });
        }
    }

    p.addSeparator();

    auto sm = juce::PopupMenu();
    sm.addItem(Surge::GUI::toOSCase("Zero Latency Mode"), true, processor.nonLatentBlockMode,
               [this]() { toggleLatencyMode(); });
    auto oscm = makeOSCMenu();
    sm.addSubMenu("OSC", oscm);
    p.addSubMenu("Options", sm);

    std::vector<int> zoomTos = {{75, 100, 125, 150, 200}};
    std::string lab;
    auto dzf = Surge::Storage::getUserDefaultValue(processor.storage.get(),
                                                   Surge::Storage::FXUnitDefaultZoom, 100);

    auto zm = juce::PopupMenu();
    auto zoomTo = [this](int zf) { setSize(baseWidth * zf * 0.01, baseHeight * zf * 0.01); };

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
        p.addSubMenu("Zoom", zm);
    }

    auto o = juce::PopupMenu::Options()
                 .withTargetComponent(picker.get())
                 .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);

    p.showMenuAsync(o);
}

juce::PopupMenu SurgefxAudioProcessorEditor::makeOSCMenu()
{
    auto oscSubMenu = juce::PopupMenu();

    if (processor.oscReceiving.load())
    {
        oscSubMenu.addItem(Surge::GUI::toOSCase("Stop OSC Connections"),
                           [this]() { processor.oscHandler.stopListening(); });
    }
    else
    {
        oscSubMenu.addItem(Surge::GUI::toOSCase("Start OSC Connections"), [this]() {
            if (processor.oscPortIn > 0)
            {
                if (!processor.initOSCIn(processor.oscPortIn))
                {
                    processor.initOSCError(processor.oscPortIn);
                }
            }
        });
    }

    std::string iport =
        (processor.oscPortIn == 0) ? "not used" : std::to_string(processor.oscPortIn);

    oscSubMenu.addItem(Surge::GUI::toOSCase("Change OSC Input Port (current: " + iport + ")..."),
                       [w = juce::Component::SafePointer(this), iport]() {
                           if (!w)
                               return;
                           w->promptForTypeinValue(
                               "Enter a new OSC input port number:", iport, [w](const auto &a) {
                                   int newPort;
                                   try
                                   {
                                       newPort = std::stoi(a);
                                   }
                                   catch (...)
                                   {
                                       std::ostringstream msg;
                                       msg << "Entered value is not a number. Please try again!";
                                       w->processor.storage->reportError(msg.str(), "Input Error");
                                       return;
                                   }

                                   if (newPort > 65535 || newPort < 0)
                                   {
                                       std::ostringstream msg;
                                       msg << "Port number must be between 0 and 65535!";
                                       w->processor.storage->reportError(
                                           msg.str(), "Port Number Out Of Range");
                                       return;
                                   }

                                   if (newPort == 0)
                                   {
                                       w->processor.oscHandler.stopListening();
                                       w->processor.oscPortIn = newPort;
                                   }
                                   else if (w->processor.changeOSCInPort(newPort))
                                   {
                                       w->processor.oscPortIn = newPort;
                                   }
                                   else
                                   {
                                       w->processor.initOSCError(newPort);
                                   }
                               });
                       });

    oscSubMenu.addItem(
        Surge::GUI::toOSCase("FX OSC Message Format"), [w = juce::Component::SafePointer(this)]() {
            if (!w)
                return;

            std::string form_str =
                "'/fx/param/<n> <val>'; replace <n> with 1 - 12 and <val> with 0.0 - 1.0 ";
            w->processor.storage->reportError(form_str, "OSC Message Format");
        });

    return oscSubMenu;
}

struct SurgefxAudioProcessorEditor::PromptOverlay : juce::Component, juce::TextEditor::Listener
{
    std::string prompt;
    std::unique_ptr<juce::TextEditor> ed;
    std::function<void(const std::string &)> cb{nullptr};

    PromptOverlay()
    {
        ed = std::make_unique<juce::TextEditor>();
        ed->setFont(SST_JUCE_FONT_OPTIONS(28));
        ed->setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
        ed->setJustification(juce::Justification::centred);
        ed->addListener(this);
        ed->setWantsKeyboardFocus(true);
        addAndMakeVisible(*ed);
    }

    void setInitVal(const std::string &s)
    {
        ed->clear();
        ed->setText(s, juce::NotificationType::dontSendNotification);
        ed->applyFontToAllText(SST_JUCE_FONT_OPTIONS(28));
        ed->applyColourToAllText(juce::Colours::white);
        ed->repaint();
    }

    void resized() override
    {
        auto teh = 35;
        auto h = (getHeight() - teh) * 0.5;
        ed->setBounds(10, h, getWidth() - 20, teh);
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.9f));
        g.setColour(juce::Colours::white);
        g.setFont(SST_JUCE_FONT_OPTIONS(28));
        g.drawMultiLineText(prompt, 0, 50, getWidth(), juce::Justification::centred);
    }

    void textEditorReturnKeyPressed(juce::TextEditor &editor) override
    {
        if (cb)
            cb(ed->getText().toStdString());
        dismiss();
    }

    void textEditorEscapeKeyPressed(juce::TextEditor &editor) override { dismiss(); }

    void visibilityChanged() override
    {
        if (isVisible())
            ed->setWantsKeyboardFocus(true);
    }

    void dismiss()
    {
        cb = nullptr;
        setVisible(false);
    }
};

void SurgefxAudioProcessorEditor::promptForTypeinValue(const std::string &prompt,
                                                       const std::string &initValue,
                                                       std::function<void(const std::string &)> cb)
{
    if (promptOverlay && promptOverlay->isVisible())
        return;

    if (!promptOverlay)
        promptOverlay = std::make_unique<PromptOverlay>();

    promptOverlay->prompt = prompt;
    promptOverlay->cb = cb;
    promptOverlay->setInitVal(initValue);

    promptOverlay->setBounds(getLocalBounds());
    addAndMakeVisible(*promptOverlay);
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

void SurgefxAudioProcessorEditor::changeOSCInputPort()
{
    // TODO: fill this out
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
        const auto iter = std::find(editor->accessibleOrderWeakRefs.cbegin(),
                                    editor->accessibleOrderWeakRefs.cend(), from);
        if (iter == editor->accessibleOrderWeakRefs.cend())
            return nullptr;

        switch (dir)
        {
        case 1:
            if (iter != std::prev(editor->accessibleOrderWeakRefs.cend()))
            {
                return *std::next(iter);
            }
            break;
        case -1:
            if (iter != editor->accessibleOrderWeakRefs.cbegin())
            {
                return *std::prev(iter);
            }
            break;
        }
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
    std::vector<juce::Component *> getAllComponents(juce::Component *parentComponent) override
    {
        return editor->accessibleOrderWeakRefs;
    }
    SurgefxAudioProcessorEditor *editor{nullptr};
};

std::unique_ptr<juce::ComponentTraverser> SurgefxAudioProcessorEditor::createFocusTraverser()
{
    return std::make_unique<FxFocusTrav>(this);
}

bool SurgefxAudioProcessorEditor::keyPressed(const juce::KeyPress &key)
{
    auto zoomTo = [this](int zf) { setSize(baseWidth * zf * 0.01, baseHeight * zf * 0.01); };
    auto zoomFactor = (int)std::round(100.0 * getWidth() / baseWidth);

    if (key.getTextCharacter() == '/' && (key.getModifiers().isShiftDown()))
    {
        auto dzf = Surge::Storage::getUserDefaultValue(processor.storage.get(),
                                                       Surge::Storage::FXUnitDefaultZoom, 100);
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

        if (!processor.m_audioValid)
        {
            fxNameLabel->setFont(SST_JUCE_FONT_OPTIONS(18 * getImpliedZoom()));
            fxNameLabel->setText(processor.m_audioValidMessage,
                                 juce::NotificationType::dontSendNotification);
        }
        else
        {
            fxNameLabel->setFont(SST_JUCE_FONT_OPTIONS(26 * getImpliedZoom()));
            fxNameLabel->setText("Surge XT Effects", juce::NotificationType::dontSendNotification);
        }
    }

    auto pending = processor.consumePendingPresetName();

    if (!pending.empty())
    {
        rebuildCurrentPresets();
        currentPresetIndex = -1;

        for (int i = 0; i < (int)currentPresets.size(); ++i)
        {
            if (currentPresets[i].name == pending)
            {
                currentPresetIndex = i;
                break;
            }
        }

        resetLabels();
    }
}
