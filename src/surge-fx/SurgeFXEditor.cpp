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

struct Picker : public juce::Component
{
    Picker(SurgefxAudioProcessorEditor *ed) : editor(ed)
    {
        setAccessible(true);
        setTitle("Select FX Type");
        setDescription("Select FX Type");
        setWantsKeyboardFocus(true);
    }
    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.f, 2.f);
        auto edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledEdge);

        g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledBg));
        g.fillRoundedRectangle(bounds, 5);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, 5, 1);
        g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisplay));
        g.setFont(28);
        g.drawText(fx_type_names[editor->processor.getEffectType()], bounds.reduced(8, 3),
                   juce::Justification::centred);

        auto p = juce::Path();
        int sz = 15;
        int xp = -10;
        int yp = (getHeight() - sz) / 2;
        p.addTriangle(bounds.getTopRight().translated(xp - sz, yp),
                      bounds.getTopRight().translated(xp, yp),
                      bounds.getTopRight().translated(xp - sz / 2, yp + sz));
        g.fillPath(p);
    }

    bool keyPressed(const juce::KeyPress &p) override
    {
        if (p.getKeyCode() == juce::KeyPress::returnKey ||
            (p.getKeyCode() == juce::KeyPress::F10Key && p.getModifiers().isShiftDown()))
        {
            editor->showMenu();
            return true;
        }
        return false;
    }
    void mouseDown(const juce::MouseEvent &e) override { editor->showMenu(); }
    SurgefxAudioProcessorEditor *editor{nullptr};

    struct AH : public juce::AccessibilityHandler
    {
        struct AHV : public juce::AccessibilityValueInterface
        {
            explicit AHV(Picker *s) : comp(s) {}

            Picker *comp;

            bool isReadOnly() const override { return true; }
            double getCurrentValue() const override { return 0.; }

            void setValue(double) override {}
            void setValueAsString(const juce::String &newValue) override {}
            AccessibleValueRange getRange() const override { return {{0, 1}, 1}; }
            juce::String getCurrentValueAsString() const override
            {
                return fx_type_names[comp->editor->processor.getEffectType()];
            }

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AHV);
        };

        explicit AH(Picker *s)
            : comp(s),
              juce::AccessibilityHandler(*s, juce::AccessibilityRole::button,
                                         juce::AccessibilityActions()
                                             .addAction(juce::AccessibilityActionType::press,
                                                        [this]() { comp->editor->showMenu(); })
                                             .addAction(juce::AccessibilityActionType::showMenu,
                                                        [this]() { comp->editor->showMenu(); }),
                                         AccessibilityHandler::Interfaces{std::make_unique<AHV>(s)})
        {
        }

        Picker *comp;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AH);
    };

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<AH>(this);
    }
};

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

    picker = std::make_unique<Picker>(this);
    addAndMakeVisibleRecordOrder(picker.get());

    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamSliders[i].setRange(0.0, 1.0, 0.0001);
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i),
                                   juce::NotificationType::dontSendNotification);
        fxParamSliders[i].setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        fxParamSliders[i].setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0,
                                          0);
        fxParamSliders[i].setChangeNotificationOnlyOnRelease(false);
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i));
        fxParamSliders[i].onValueChange = [i, this]() {
            this->processor.prepareParametersAbsentAudio();
            this->processor.setFXParamValue01(i, this->fxParamSliders[i].getValue());
            fxParamDisplay[i].setDisplay(
                processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
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

        fxTempoSync[i].setTitle("Parameter " + std::to_string(i) + " TempoSync");
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

        fxAbsoluted[i].setTitle("Parameter " + std::to_string(i) + " Absoluted");
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
    fxNameLabel->setFont(28);
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
    // setResizable(false, false);

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 5);
}

SurgefxAudioProcessorEditor::~SurgefxAudioProcessorEditor()
{
    idleTimer->stopTimer();
    setLookAndFeel(nullptr);
    this->processor.setParameterChangeListener([]() {});
    this->processor.storage->removeErrorListener(this);
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
        fxParamDisplay[i].setDisplay(processor.getParamValue(i).c_str());
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].allowsTypein = processor.canSetParameterByString(i);

        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        fxParamDisplay[i].setAppearsDeactivated(processor.getFXStorageAppearsDeactivated(i));
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i) &&
                                     !processor.getFXStorageAppearsDeactivated(i));
        st(fxParamSliders[i], nm + " Knob");
        fxParamSliders[i].setTextValue(processor.getParamValue(i).c_str());

        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setAccessible(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      juce::NotificationType::dontSendNotification);
        st(fxTempoSync[i], nm + " Tempo Synced");
        fxDeactivated[i].setEnabled(false);

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

    int row = 0, col = 0;

    if (auto h = getAccessibilityHandler())
    {
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
    }
}

void SurgefxAudioProcessorEditor::setEffectType(int i)
{
    processor.resetFxType(i);
    blastToggleState(i - 1);
    resetLabels();
    picker->repaint();
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
                // My type has changed - blow out the toggle states by hand
                blastToggleState(processor.getEffectType() - 1);
                resetLabels();
            }
        }
}

void SurgefxAudioProcessorEditor::blastToggleState(int w) {}

//==============================================================================
void SurgefxAudioProcessorEditor::paint(juce::Graphics &g)
{
    surgeLookFeel->paintComponentBackground(g, getWidth(), getHeight());
}

void SurgefxAudioProcessorEditor::resized()
{
    picker->setBounds(100, 10, getWidth() - 200, topSection - 30);

    int ypos0 = topSection - 5;
    int rowHeight = (getHeight() - topSection - 40 - 10) / 6.0;
    int byoff = 7;

    int sliderOff = 5;
    if (getWidth() < baseWidth)
        sliderOff = 2;
    for (int i = 0; i < n_fx_params; ++i)
    {
        juce::Rectangle<int> position{(i / 6) * getWidth() / 2 + sliderOff,
                                      (i % 6) * rowHeight + ypos0, rowHeight - sliderOff,
                                      rowHeight - sliderOff};
        fxParamSliders[i].setBounds(position);

        int buttonSize = 19;
        if (getWidth() < baseWidth)
            buttonSize = 17;
        int buttonMargin = 1;
        juce::Rectangle<int> tsPos{(i / 6) * getWidth() / 2 + 2 + rowHeight - 5,
                                   (i % 6) * rowHeight + ypos0 + byoff + buttonMargin, buttonSize,
                                   buttonSize};
        fxTempoSync[i].setBounds(tsPos);

        juce::Rectangle<int> daPos{(i / 6) * getWidth() / 2 + 2 + rowHeight - 5,
                                   (i % 6) * rowHeight + ypos0 + byoff + 2 * buttonMargin +
                                       buttonSize,
                                   buttonSize, buttonSize};
        fxDeactivated[i].setBounds(daPos);

        juce::Rectangle<int> exPos{
            (i / 6) * getWidth() / 2 + 2 + rowHeight - 5 + buttonMargin + buttonSize,
            (i % 6) * rowHeight + ypos0 + byoff + 1 * buttonMargin + 0 * buttonSize, buttonSize,
            buttonSize};
        fxExtended[i].setBounds(exPos);

        juce::Rectangle<int> abPos{
            (i / 6) * getWidth() / 2 + 2 + rowHeight - 5 + buttonMargin + buttonSize,
            (i % 6) * rowHeight + ypos0 + byoff + 2 * buttonMargin + 1 * buttonSize, buttonSize,
            buttonSize};
        fxAbsoluted[i].setBounds(abPos);

        juce::Rectangle<int> dispPos{
            (i / 6) * getWidth() / 2 + 4 + rowHeight - 5 + 2 * buttonMargin + 2 * buttonSize,
            (i % 6) * rowHeight + ypos0,
            getWidth() / 2 - rowHeight - 8 - 2 * buttonMargin - 2 * buttonSize, rowHeight - 5};
        fxParamDisplay[i].setBounds(dispPos);
    }

    fxNameLabel->setFont(28);
    fxNameLabel->setBounds(40, getHeight() - 40, 350, 38);
}

int SurgefxAudioProcessorEditor::findLargestFittingZoomBetween(
    int zoomLow,                     // bottom of range
    int zoomHigh,                    // top of range
    int zoomQuanta,                  // step size
    int percentageOfScreenAvailable, // How much to shrink actual screen
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
            if (t == fxt_audio_input) // skip the "Audio In" effect
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

        // TODO: use this condition once we have a storable zoom factor
        bool ticked = (s == zoomFactor);

        // TODO: make it actually work!
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

    auto o = juce::PopupMenu::Options();

    auto r = juce::Rectangle<int>().withPosition(
        localPointToGlobal(picker->getBounds().getBottomLeft()));

    o = o.withTargetScreenArea(r).withPreferredPopupDirection(
        juce::PopupMenu::Options::PopupDirection::downwards);

    p.showMenuAsync(o);
}

juce::PopupMenu SurgefxAudioProcessorEditor::makeOSCMenu()
{
    auto oscSubMenu = juce::PopupMenu();

    if (processor.oscReceiving)
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
            w->processor.storage->reportError(form_str, "OSC Message Format:");
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
        ed->setFont(juce::Font(28));
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
        ed->applyFontToAllText(juce::Font(28));
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
        g.setFont(28);
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

    std::cout << "Showing " << prompt << std::endl;
}

void SurgefxAudioProcessorEditor::onSurgeError(const std::string &msg, const std::string &title,
                                               const SurgeStorage::ErrorType &errorType)
{
    /*
     * We could be cleverer than this but for now lets just do this
     */
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
           "to take "
           "effect!\n\n"
        << (clm ? "The processing latency is now 32 samples, and variable size audio "
                  "buffers are "
                  "supported."
                : "The processing latency is now disabled, so fixed size buffers of at "
                  "least "
                  "32 samples are required. Note that some DAWs (particularly FL Studio) "
                  "use "
                  "variable size buffers by default, so in this mode you have to adjust "
                  "the plugin "
                  "processing options in your DAW to send fixed size audio buffers.");

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
            fxNameLabel->setFont(18);
            fxNameLabel->setText(processor.m_audioValidMessage,
                                 juce::NotificationType::dontSendNotification);
        }
        else
        {
            fxNameLabel->setFont(28);
            fxNameLabel->setText("Surge XT Effects", juce::NotificationType::dontSendNotification);
        }
    }
}