/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeFXProcessor.h"
#include "SurgeFXEditor.h"

static std::vector<std::vector<std::string>> fxnm = {
    {"Eq", "GEq", "Reson", "Exciter", "Chow", "Dist", "Neuron"},
    {"Chorus", "Ens", "Flanger", "Phaser", "Rotary", "Delay", "Reverb1", "Reverb2"},
    {"Combs", "Freq", "Nimbus", "Ring Mod", "Vocoder", "Airwin", "Cond"}};
static std::vector<std::vector<int>> fxt = {
    {fxt_eq, fxt_geq11, fxt_resonator, fxt_exciter, fxt_chow, fxt_distortion, fxt_neuron},
    {fxt_chorus4, fxt_ensemble, fxt_flanger, fxt_phaser, fxt_rotaryspeaker, fxt_delay, fxt_reverb,
     fxt_reverb2},
    {fxt_combulator, fxt_freqshift, fxt_nimbus, fxt_ringmod, fxt_vocoder, fxt_airwindows,
     fxt_conditioner}};
static std::vector<std::vector<int>> fxgroup = {
    {0, 0, 0, 0, 1, 1, 1}, {2, 2, 2, 2, 2, 3, 3, 3}, {4, 4, 4, 4, 4, 5, 5}};

// https://piccianeri.com/wp-content/uploads/2016/10/Complementary-palette-f27400.jpg
static std::vector<juce::Colour> fxbuttoncolors = {
    juce::Colour(0xFF038294), juce::Colour(0xFFFFAB5E), juce::Colour(0xFF026775),
    juce::Colour(0xFF964800), juce::Colour(0xFF43a4b1), juce::Colour(0xFFF27400)};

//==============================================================================
SurgefxAudioProcessorEditor::SurgefxAudioProcessorEditor(SurgefxAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    surgeLookFeel.reset(new SurgeLookAndFeel());
    setLookAndFeel(surgeLookFeel.get());
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(540, 55 * 6 + 80 + 50 * fxt.size());
    setResizable(false, false); // For now

    fxNameLabel = std::make_unique<juce::Label>("fxlabel", "Surge FX");
    fxNameLabel->setFont(28);
    fxNameLabel->setColour(juce::Label::textColourId,
                           surgeLookFeel->findColour(SurgeLookAndFeel::blue));
    fxNameLabel->setBounds(40, getHeight() - 40, 300, 38);
    fxNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fxNameLabel.get());

    int ypos0 = 50 * fxt.size() - 10;
    for (int i = 0; i < n_fx_params; ++i)
    {
        fxParamSliders[i].setRange(0.0, 1.0, 0.001);
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

        juce::Rectangle<int> tsPos{(i / 6) * getWidth() / 2 + 2 + 55, (i % 6) * 60 + ypos0 + 12, 13,
                                   55 - 24};
        fxTempoSync[i].setBounds(tsPos);
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      NotificationType::dontSendNotification);
        fxTempoSync[i].onClick = [i, this]() {
            this->processor.setUserEditingTemposync(i, true);
            this->processor.setFXParamTempoSync(i, this->fxTempoSync[i].getToggleState());
            this->processor.setFXStorageTempoSync(i, this->fxTempoSync[i].getToggleState());
            fxParamDisplay[i].setDisplay(
                processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
            this->processor.setUserEditingTemposync(i, false);
        };

        addAndMakeVisible(&(fxTempoSync[i]));

        juce::Rectangle<int> dispPos{(i / 6) * getWidth() / 2 + 4 + 55 + 15, (i % 6) * 60 + ypos0,
                                     getWidth() / 2 - 68 - 15, 55};
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
        maxButtonsPerRow = std::max(maxButtonsPerRow, (int)e.size());

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
        int bysz = 40;
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
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i));
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i),
                                      NotificationType::dontSendNotification);
    }
    fxNameLabel->setText(juce::String("SurgeFX : ") + fx_type_names[processor.getEffectType()],
                         juce::dontSendNotification);
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
