/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeFXProcessor.h"
#include "SurgeFXEditor.h"

static std::vector<std::string> fxnm =
{ 
  "chorus", "flanger", "phaser", "rotary",
  "delay", "reverb1", "reverb2",

  "eq", "dist", "cond", "freq", "ring", "voco", "airwin"
};
static std::vector<int> fxt =
{ 
  fxt_chorus4, fxt_flanger, fxt_phaser, fxt_rotaryspeaker,
  fxt_delay, fxt_reverb, fxt_reverb2,

  fxt_eq, fxt_distortion, fxt_conditioner, fxt_freqshift, fxt_ringmod, fxt_vocoder, fxt_airwindows
};

//==============================================================================
SurgefxAudioProcessorEditor::SurgefxAudioProcessorEditor (SurgefxAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    surgeLookFeel.reset(new SurgeLookAndFeel() );
    setLookAndFeel(surgeLookFeel.get());
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (500, 55 * 6 + 150);
    setResizable(false, false); // For now

    for( int i=0; i<n_fx_params; ++ i )
    {
        fxParamSliders[i].setRange(0.0, 1.0, 0.001 );
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i), NotificationType::dontSendNotification);
        fxParamSliders[i].setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        fxParamSliders[i].setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0 );
        juce::Rectangle<int> position { ( i / 6 ) * getWidth()/2 + 5, ( i % 6 ) * 60 + 100, 55, 55 };
        fxParamSliders[i].setBounds(position);
        fxParamSliders[i].setChangeNotificationOnlyOnRelease(false);
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i));
        fxParamSliders[i].onValueChange = [i, this]() {
            this->processor.setFXParamValue01(i, this->fxParamSliders[i].getValue() );
            fxParamDisplay[i].setDisplay(processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
        };
        fxParamSliders[i].onDragStart = [i,this]() {
            this->processor.setUserEditingFXParam(i,true);
        };
        fxParamSliders[i].onDragEnd = [i,this]() {
            this->processor.setUserEditingFXParam(i,false);
        };
        addAndMakeVisible(&(fxParamSliders[i]));

        juce::Rectangle<int> tsPos {  ( i / 6 ) * getWidth() / 2 + 2 + 55,
                ( i % 6 ) * 60 + 100 + 12,
                13,
                55 -24 };
        fxTempoSync[i].setBounds(tsPos);
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i), NotificationType::dontSendNotification);
        fxTempoSync[i].onClick = [i, this]() {
            this->processor.setUserEditingTemposync(i, true);
            this->processor.setFXParamTempoSync(i, this->fxTempoSync[i].getToggleState() );
            this->processor.setFXStorageTempoSync(i, this->fxTempoSync[i].getToggleState() );
            fxParamDisplay[i].setDisplay(processor.getParamValueFromFloat(i, this->fxParamSliders[i].getValue()));
            this->processor.setUserEditingTemposync(i, false);
        };

        addAndMakeVisible(&(fxTempoSync[i]));
        
        
        juce::Rectangle<int> dispPos { ( i / 6 ) * getWidth() / 2 + 4 + 55 + 15,
                ( i % 6 ) * 60 + 100,
                getWidth() / 2 - 68 - 15,
                55 };
        fxParamDisplay[i].setBounds(dispPos);
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].setDisplay(processor.getParamValue(i));
        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        
        addAndMakeVisible(fxParamDisplay[i]);
    }
    
    int en = processor.getEffectType();
    int buttonsPerRow = 7;
    for( int i=0; i<n_fx; ++i )
    {
        selectType[i].setButtonText(fxnm[i]);
        int bxsz = (getWidth()-20)/buttonsPerRow;
        int bxmg = 10;
        int bysz = 40;
        int bymg = 10;
        juce::Rectangle<int> bpos { ( i % buttonsPerRow ) * bxsz + bxmg, (i/buttonsPerRow) * bysz + bymg, bxsz, bysz };
        selectType[i].setRadioGroupId(FxTypeGroup);
        selectType[i].setBounds(bpos);
        selectType[i].setClickingTogglesState(true);
        selectType[i].onClick = [this,i] { this->setEffectType(fxt[i]); };
        if( fxt[i] == en )
        {
            selectType[i].setToggleState(true,  NotificationType::dontSendNotification);
        }
        else
        {
            selectType[i].setToggleState(false,  NotificationType::dontSendNotification);
        }
        addAndMakeVisible(selectType[i]);
    }
    

    this->processor.setParameterChangeListener([this]() { this->triggerAsyncUpdate(); } );
}

SurgefxAudioProcessorEditor::~SurgefxAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    this->processor.setParameterChangeListener([](){});
}

void SurgefxAudioProcessorEditor::resetLabels()
{
    for( int i=0; i<n_fx_params; ++i )
    {
        fxParamSliders[i].setValue(processor.getFXStorageValue01(i), NotificationType::dontSendNotification);
        fxParamDisplay[i].setDisplay(processor.getParamValue(i).c_str());
        fxParamDisplay[i].setGroup(processor.getParamGroup(i).c_str());
        fxParamDisplay[i].setName(processor.getParamName(i).c_str());
        fxParamDisplay[i].setEnabled(processor.getParamEnabled(i));
        fxParamSliders[i].setEnabled(processor.getParamEnabled(i));
        fxTempoSync[i].setEnabled(processor.canTempoSync(i));
        fxTempoSync[i].setToggleState(processor.getFXStorageTempoSync(i), NotificationType::dontSendNotification);
    }
}

void SurgefxAudioProcessorEditor::setEffectType(int i)
{
    processor.resetFxType(i);
    blastToggleState(i-1);
    resetLabels();
}

void SurgefxAudioProcessorEditor::handleAsyncUpdate() {
    paramsChangedCallback();
}

void SurgefxAudioProcessorEditor::paramsChangedCallback() {
    bool cv[n_fx_params+1];
    float fv[n_fx_params+1];
    processor.copyChangeValues(cv, fv);
    for( int i=0; i<n_fx_params+1; ++i )
        if( cv[i] )
        {
            if( i < n_fx_params )
            {
                fxParamSliders[i].setValue(fv[i], NotificationType::dontSendNotification);
                fxParamDisplay[i].setDisplay(processor.getParamValue(i));
            }
            else
            {
                // My type has changed - blow out the toggle states by hand
                blastToggleState(processor.getEffectType()-1);
                resetLabels();
            }
        }
}

void SurgefxAudioProcessorEditor::blastToggleState(int w)
{
    for( auto i=0; i<n_fx; ++i )
    {
        selectType[i].setToggleState( fxt[i] == w + 1, NotificationType::dontSendNotification );
    }
}

//==============================================================================
void SurgefxAudioProcessorEditor::paint (Graphics& g)
{
    surgeLookFeel->paintComponentBackground(g, getWidth(), getHeight() );
}

void SurgefxAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
