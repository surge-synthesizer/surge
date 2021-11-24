/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeFXProcessor.h"
#include "SurgeFXEditor.h"
#include "FxPresetAndClipboardManager.h"
#include "tinyxml/tinyxml.h"

struct Picker : public juce::Component
{
    Picker(SurgefxAudioProcessorEditor *ed) : editor(ed) {}
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

    void mouseDown(const juce::MouseEvent &e) override { editor->showMenu(); }
    SurgefxAudioProcessorEditor *editor{nullptr};
};

//==============================================================================
SurgefxAudioProcessorEditor::SurgefxAudioProcessorEditor(SurgefxAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    makeMenu();
    surgeLookFeel.reset(new SurgeLookAndFeel());
    setLookAndFeel(surgeLookFeel.get());
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    int topSection = 80;

    setSize(600, 55 * 6 + 80 + topSection);
    setResizable(false, false); // For now

    fxNameLabel = std::make_unique<juce::Label>("fxlabel", "Surge XT Effects");
    fxNameLabel->setFont(28);
    fxNameLabel->setColour(juce::Label::textColourId,
                           surgeLookFeel->findColour(SurgeLookAndFeel::blue));
    fxNameLabel->setBounds(40, getHeight() - 40, 350, 38);
    fxNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fxNameLabel.get());

    int ypos0 = topSection - 5;
    int byoff = 7;

    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamSliders[i].setRange(0.0, 1.0, 0.0001);
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i),
                                   juce::NotificationType::dontSendNotification);
        fxParamSliders[i].setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        fxParamSliders[i].setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0,
                                          0);
        juce::Rectangle<int> position{(i / 6) * getWidth() / 2 + 5, (i % 6) * 60 + ypos0, 55, 55};
        fxParamSliders[i].setBounds(position);
        fxParamSliders[i].setChangeNotificationOnlyOnRelease(false);
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i));
        fxParamSliders[i].onValueChange = [i, this]() {
            this->processor.setFXParamValue01(i, this->fxParamSliders[i].getValue());
            fxParamDisplay[i].setDisplay(
                processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
        };
        fxParamSliders[i].onDragStart = [i, this]() {
            this->processor.setUserEditingFXParam(i, true);
        };
        fxParamSliders[i].onDragEnd = [i, this]() {
            this->processor.setUserEditingFXParam(i, false);
        };
        addAndMakeVisible(&(fxParamSliders[i]));

        int buttonSize = 19;
        int buttonMargin = 1;
        juce::Rectangle<int> tsPos{(i / 6) * getWidth() / 2 + 2 + 55,
                                   (i % 6) * 60 + ypos0 + byoff + buttonMargin, buttonSize,
                                   buttonSize};
        fxTempoSync[i].setOnOffImage(BinaryData::TS_Act_svg, BinaryData::TS_Act_svgSize,
                                     BinaryData::TS_Deact_svg, BinaryData::TS_Deact_svgSize);
        fxTempoSync[i].setBounds(tsPos);
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
        addAndMakeVisible(&(fxTempoSync[i]));

        juce::Rectangle<int> daPos{(i / 6) * getWidth() / 2 + 2 + 55,
                                   (i % 6) * 60 + ypos0 + byoff + 2 * buttonMargin + buttonSize,
                                   buttonSize, buttonSize};
        fxDeactivated[i].setOnOffImage(BinaryData::DE_Act_svg, BinaryData::DE_Act_svgSize,
                                       BinaryData::DE_Deact_svg, BinaryData::DE_Deact_svgSize);
        fxDeactivated[i].setBounds(daPos);
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
        addAndMakeVisible(&(fxDeactivated[i]));

        juce::Rectangle<int> exPos{(i / 6) * getWidth() / 2 + 2 + 55 + buttonMargin + buttonSize,
                                   (i % 6) * 60 + ypos0 + byoff + 1 * buttonMargin + 0 * buttonSize,
                                   buttonSize, buttonSize};
        fxExtended[i].setOnOffImage(BinaryData::EX_Act_svg, BinaryData::EX_Act_svgSize,
                                    BinaryData::EX_Deact_svg, BinaryData::EX_Deact_svgSize);

        fxExtended[i].setBounds(exPos);
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
        addAndMakeVisible(&(fxExtended[i]));

        juce::Rectangle<int> abPos{(i / 6) * getWidth() / 2 + 2 + 55 + buttonMargin + buttonSize,
                                   (i % 6) * 60 + ypos0 + byoff + 2 * buttonMargin + 1 * buttonSize,
                                   buttonSize, buttonSize};
        fxAbsoluted[i].setOnOffImage(BinaryData::AB_Act_svg, BinaryData::AB_Act_svgSize,
                                     BinaryData::AB_Deact_svg, BinaryData::AB_Deact_svgSize);

        fxAbsoluted[i].setBounds(abPos);
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
        addAndMakeVisible(&(fxAbsoluted[i]));

        juce::Rectangle<int> dispPos{
            (i / 6) * getWidth() / 2 + 4 + 55 + 2 * buttonMargin + 2 * buttonSize,
            (i % 6) * 60 + ypos0, getWidth() / 2 - 68 - 2 * buttonMargin - 2 * buttonSize, 55};
        fxParamDisplay[i].setBounds(dispPos);
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].setDisplay(processor.getParamValue(i));
        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));

        addAndMakeVisible(fxParamDisplay[i]);
    }

    picker = std::make_unique<Picker>(this);
    picker->setBounds(100, 10, getWidth() - 200, topSection - 30);
    addAndMakeVisible(*picker);

    this->processor.setParameterChangeListener([this]() { this->triggerAsyncUpdate(); });
}

SurgefxAudioProcessorEditor::~SurgefxAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    this->processor.setParameterChangeListener([]() {});
}

void SurgefxAudioProcessorEditor::resetLabels()
{
    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i),
                                   juce::NotificationType::dontSendNotification);
        fxParamDisplay[i].setDisplay(processor.getParamValue(i).c_str());
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        fxParamDisplay[i].setAppearsDeactivated(processor.getFXStorageAppearsDeactivated(i));
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i) &&
                                     !processor.getFXStorageAppearsDeactivated(i));
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      juce::NotificationType::dontSendNotification);

        fxDeactivated[i].setEnabled(false);
        fxExtended[i].setEnabled(processor.canExtend(i));
        fxExtended[i].setToggleState(processor.getFXStorageExtended(i),
                                     juce::NotificationType::dontSendNotification);
        fxAbsoluted[i].setEnabled(processor.canAbsolute(i));
        fxAbsoluted[i].setToggleState(processor.getFXStorageAbsolute(i),
                                      juce::NotificationType::dontSendNotification);
        fxDeactivated[i].setEnabled(processor.canDeactitvate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        juce::NotificationType::dontSendNotification);
    }

    int row = 0, col = 0;

    std::string nm = fx_type_names[processor.getEffectType()];

    fxNameLabel->setText(juce::String("Surge FX: " + nm), juce::dontSendNotification);
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
                fxParamDisplay[i].setDisplay(processor.getParamValue(i));
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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
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
            p.addColumnBreak();
        if (m.type == FxMenu::SECTION)
            p.addSectionHeader(m.name);
        if (m.type == FxMenu::FX)
            p.addItem(m.name, [this, m]() {
                if (m.fxtype > 0)
                    setEffectType(m.fxtype);
            });
    }
    auto o = juce::PopupMenu::Options();
    auto r = juce::Rectangle<int>().withPosition(
        localPointToGlobal(picker->getBounds().getBottomLeft()));
    o = o.withTargetScreenArea(r).withPreferredPopupDirection(
        juce::PopupMenu::Options::PopupDirection::downwards);
    p.showMenuAsync(o);
}