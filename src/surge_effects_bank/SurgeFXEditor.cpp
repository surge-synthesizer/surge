/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeFXProcessor.h"
#include "SurgeFXEditor.h"

static std::vector<std::vector<std::string>> fxnm = {
    {"EQ", "Graphic EQ", "Resonator", "Exciter", "CHOW", "Distortion", "Neuron", "Tape"},
    {"Chorus", "Ensemble", "Flanger", "Phaser", "Rotary", "Delay", "Reverb 1", "Reverb 2"},
    {"Combulator", "Freq Shifter", "Nimbus", "Ring Modulator", "Treemonster", "Vocoder",
     "Airwindows", "Conditioner"}};

static std::vector<std::vector<int>> fxt = {
    {fxt_eq, fxt_geq11, fxt_resonator, fxt_exciter, fxt_chow, fxt_distortion, fxt_neuron, fxt_tape},
    {fxt_chorus4, fxt_ensemble, fxt_flanger, fxt_phaser, fxt_rotaryspeaker, fxt_delay, fxt_reverb,
     fxt_reverb2},
    {fxt_combulator, fxt_freqshift, fxt_nimbus, fxt_ringmod, fxt_treemonster, fxt_vocoder,
     fxt_airwindows, fxt_conditioner}};

static std::vector<std::vector<int>> fxgroup = {
    {0, 0, 0, 0, 1, 1, 1, 1}, {2, 2, 2, 2, 2, 3, 3, 3}, {4, 4, 4, 4, 4, 4, 5, 5}};

// https://piccianeri.com/wp-content/uploads/2016/10/Complementary-palette-f27400.jpg
static std::vector<juce::Colour> fxbuttoncolors = {
    juce::Colour(0xFF106060), juce::Colour(0xFFE03400), juce::Colour(0xFF104B60),
    juce::Colour(0xFFF06000), juce::Colour(0xFF103560), juce::Colour(0xFFFF9000)};

//==============================================================================
SurgefxAudioProcessorEditor::SurgefxAudioProcessorEditor(SurgefxAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    surgeLookFeel.reset(new SurgeLookAndFeel());
    setLookAndFeel(surgeLookFeel.get());
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(650, 55 * 6 + 80 + 50 * fxt.size());
    setResizable(false, false); // For now

    fxNameLabel = std::make_unique<juce::Label>("fxlabel", "Surge FX Bank");
    fxNameLabel->setFont(28);
    fxNameLabel->setColour(juce::Label::textColourId,
                           surgeLookFeel->findColour(SurgeLookAndFeel::blue));
    fxNameLabel->setBounds(40, getHeight() - 40, 350, 38);
    fxNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fxNameLabel.get());

    int ypos0 = 50 * fxt.size() - 5;
    int byoff = 7;

    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamSliders[i].setRange(0.0, 1.0, 0.0001);
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i),
                                   NotificationType::dontSendNotification);
        fxParamSliders[i].setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        fxParamSliders[i].setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
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
                                      NotificationType::dontSendNotification);
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
                                        NotificationType::dontSendNotification);
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
                                     NotificationType::dontSendNotification);
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
                                      NotificationType::dontSendNotification);
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

    int en = processor.getEffectType();
    int maxButtonsPerRow = 0;

    for (auto &e : fxt)
    {
        maxButtonsPerRow = std::max(maxButtonsPerRow, (int)e.size());
    }

    int buttonsPerRow = maxButtonsPerRow;
    int i = 0;

    for (int row = 0; row < fxt.size(); ++row)
    {
        std::unordered_set<int> groupsPerRow;

        for (auto g : fxgroup[row])
        {
            groupsPerRow.insert(g);
        }

        auto nGroups = groupsPerRow.size();

        int bxsz = (getWidth() - 40) / buttonsPerRow;
        int bxmg = 10;
        int bysz = 38;
        int bymg = 10;

        int xPos = bxmg;

        if (nGroups > 2)
        {
            xPos -= bxmg / 3 * (nGroups - 2);
        }

        if (fxt[row].size() < buttonsPerRow)
        {
            xPos += bxsz * (buttonsPerRow - fxt[row].size()) * 0.5;
        }

        int currGrp = fxgroup[row][0];

        for (int col = 0; col < fxt[row].size(); ++col)
        {
            auto nm = fxnm[row][col];
            auto ty = fxt[row][col];

            selectType[i].setButtonText(nm);
            selectType[i].setColour(SurgeLookAndFeel::fxButtonFill,
                                    fxbuttoncolors[fxgroup[row][col]]);
            typeByButtonIndex[i] = ty;

            if (fxgroup[row][col] != currGrp)
            {
                currGrp = fxgroup[row][col];
                xPos += bxsz / 3;
            }

            juce::Rectangle<int> bpos{xPos, row * bysz + bymg, bxsz, bysz};
            xPos += bxsz;

            selectType[i].setRadioGroupId(FxTypeGroup);
            selectType[i].setBounds(bpos);
            selectType[i].setClickingTogglesState(true);
            selectType[i].onClick = [this, ty] { this->setEffectType(ty); };

            // gross but works just so we don't see those ellipses
            if (fxt[row][col] == fxt_treemonster || fxt[row][col] == fxt_rotaryspeaker)
            {
                bpos.setRight(bpos.getRight() + 4);
                selectType[i].setBounds(bpos);
            }
            if (fxt[row][col] == fxt_vocoder)
            {
                auto pos = bpos.getPosition();
                pos.setX(pos.x + 4);
                bpos.setPosition(pos);
                selectType[i].setBounds(bpos);
            }

            if (ty == en)
            {
                selectType[i].setToggleState(true, NotificationType::dontSendNotification);
            }
            else
            {
                selectType[i].setToggleState(false, NotificationType::dontSendNotification);
            }

            addAndMakeVisible(selectType[i]);

            ++i;
        }
    }

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
                                   NotificationType::dontSendNotification);
        fxParamDisplay[i].setDisplay(processor.getParamValue(i).c_str());
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        fxParamDisplay[i].setAppearsDeactivated(processor.getFXStorageAppearsDeactivated(i));
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i) &&
                                     !processor.getFXStorageAppearsDeactivated(i));
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      NotificationType::dontSendNotification);

        fxDeactivated[i].setEnabled(false);
        fxExtended[i].setEnabled(processor.canExtend(i));
        fxExtended[i].setToggleState(processor.getFXStorageExtended(i),
                                     NotificationType::dontSendNotification);
        fxAbsoluted[i].setEnabled(processor.canAbsolute(i));
        fxAbsoluted[i].setToggleState(processor.getFXStorageAbsolute(i),
                                      NotificationType::dontSendNotification);
        fxDeactivated[i].setEnabled(processor.canDeactitvate(i));
        fxDeactivated[i].setToggleState(processor.getFXStorageDeactivated(i),
                                        NotificationType::dontSendNotification);
    }

    int row = 0, col = 0;

    // not elegant but works
    for (int i = 0; i < fxt.size(); ++i)
    {
        for (int j = 0; j < fxt[row].size(); ++j)
        {
            if (fxt[i][j] == processor.getEffectType())
            {
                row = i;
                col = j;
            }
        }
    }

    auto nm = fxnm[row][col];

    fxNameLabel->setText(juce::String("Surge FX: " + nm), juce::dontSendNotification);
}

void SurgefxAudioProcessorEditor::setEffectType(int i)
{
    processor.resetFxType(i);
    blastToggleState(i - 1);
    resetLabels();
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
                fxParamSliders[i].setValue(fv[i], NotificationType::dontSendNotification);
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

void SurgefxAudioProcessorEditor::blastToggleState(int w)
{
    for (auto i = 0; i < n_fx_types; ++i)
    {
        selectType[i].setToggleState(typeByButtonIndex[i] == w + 1,
                                     NotificationType::dontSendNotification);
    }
}

//==============================================================================
void SurgefxAudioProcessorEditor::paint(Graphics &g)
{
    surgeLookFeel->paintComponentBackground(g, getWidth(), getHeight());
}

void SurgefxAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
