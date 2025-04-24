#include "ParameterPanel.h"

ParameterPanel::ParameterPanel(SurgefxAudioProcessor &p,
                               std::vector<juce::Component *> &accessibleOrder)
    : processor(p), styleSheet(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
                        sst::jucegui::style::StyleSheet::DARK)),
      accessibleOrderWeakRefs(accessibleOrder)
{
    sst::jucegui::style ::StyleSheet::initializeStyleSheets([]() {});

    using knobStyle = sst::jucegui::components::Knob::Styles;

    styleSheet->setColour(knobStyle::styleClass, knobStyle::handle, backgroundColour);
    styleSheet->setColour(knobStyle::styleClass, knobStyle::knobbase, backgroundColour);
    styleSheet->setColour(knobStyle::styleClass, knobStyle::value, surgeOrange);
    styleSheet->setColour(knobStyle::styleClass, knobStyle::value_hover, surgeOrange);

    for (int i = 0; i < n_fx_params; ++i)
    {
        auto knob = std::make_unique<sst::jucegui::components::Knob>();
        auto knobSource = std::make_unique<KnobSource>(processor, fxParamDisplay[i], i);

        knob->setStyle(styleSheet);
        knob->setModulationDisplay(sst::jucegui::components::Knob::Modulatable::NONE);

        auto paramName = processor.getParamName(i) + " " + processor.getParamGroup(i);
        knobSource->setValueFromGUI(processor.getFXStorageValue01(i));
        knobSource->setLabel(paramName + " Knob");

        knob->setSource(knobSource.get());

        addAndMakeVisibleRecordOrder(knob.get());
        knobs.push_back(std::move(knob));
        sources.push_back(std::move(knobSource));

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
                processor.getParamValueFromFloat(i, processor.getFXStorageValue01(i)));
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
            this->reset();
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
                processor.getParamValueFromFloat(i, processor.getFXStorageValue01(i)));
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
                processor.getParamValueFromFloat(i, processor.getFXStorageValue01(i)));
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
}

ParameterPanel::~ParameterPanel() {}

void ParameterPanel::paint(juce::Graphics &g) { g.fillAll(backgroundColour); }

void ParameterPanel::reset()
{
    knobs.clear();
    sources.clear();

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
        auto knob = std::make_unique<sst::jucegui::components::Knob>();
        auto knobSource = std::make_unique<KnobSource>(processor, fxParamDisplay[i], i);

        knob->setStyle(styleSheet);
        knob->setModulationDisplay(sst::jucegui::components::Knob::Modulatable::NONE);

        auto paramName = processor.getParamName(i) + " " + processor.getParamGroup(i);
        knobSource->setValueFromGUI(processor.getFXStorageValue01(i));
        auto name = paramName + " Knob";
        knobSource->setLabel(name);

        knob->setSource(knobSource.get());
        knob->setEnabled(processor.getParamEnabled(i));

        addAndMakeVisibleRecordOrder(knob.get());
        knobs.push_back(std::move(knob));
        sources.push_back(std::move(knobSource));

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
        st(fxTempoSync[i], name + " Tempo Synced");
        fxDeactivated[i].setEnabled(false);

        fxExtended[i].setEnabled(processor.canExtend(i));
        fxExtended[i].setToggleState(processor.getFXStorageExtended(i),
                                     juce::NotificationType::dontSendNotification);
        fxExtended[i].setAccessible(processor.canExtend(i));
        st(fxExtended[i], name + " Extended");
        fxAbsoluted[i].setEnabled(processor.canAbsolute(i));
        fxAbsoluted[i].setToggleState(processor.getFXStorageAbsolute(i),
                                      juce::NotificationType::dontSendNotification);
        fxAbsoluted[i].setAccessible(processor.canAbsolute(i));
        st(fxAbsoluted[i], name + " Absolute");
        fxDeactivated[i].setEnabled(processor.canDeactitvate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        juce::NotificationType::dontSendNotification);
        fxDeactivated[i].setAccessible(processor.canDeactitvate(i));
        st(fxDeactivated[i], name + " Deactivated");
    }
    resized();
}
void ParameterPanel::resized()
{
    int ypos0 = 5;
    int rowHeight = getHeight() / 6.0;
    int byoff = 7;

    int sliderOff = 5;
    if (getWidth() < baseWidth)
        sliderOff = 2;

    for (int i = 0; i < n_fx_params; ++i)
    {
        juce::Rectangle<int> position{(i / 6) * getWidth() / 2 + sliderOff,
                                      (i % 6) * rowHeight + ypos0, rowHeight - sliderOff,
                                      rowHeight - sliderOff};

        position = position.reduced(position.getWidth() * 0.10, position.getHeight() * 0.10);

        knobs.at(i).get()->setBounds(position);

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
}
