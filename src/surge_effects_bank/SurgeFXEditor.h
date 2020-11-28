/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "SurgeFXProcessor.h"
#include "SurgeLookAndFeel.h"

//==============================================================================
/**
*/
class SurgefxAudioProcessorEditor  : public AudioProcessorEditor, AsyncUpdater
{
public:
    SurgefxAudioProcessorEditor (SurgefxAudioProcessor&);
    ~SurgefxAudioProcessorEditor();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

    void paramsChangedCallback();
    void setEffectType(int i);

    virtual void handleAsyncUpdate() override;
    
    enum RadioGroupIds {
        FxTypeGroup = 1776
    };
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgefxAudioProcessor& processor;

    Slider fxParamSliders[n_fx_params];
    SurgeFXParamDisplay fxParamDisplay[n_fx_params];
    SurgeTempoSyncSwitch fxTempoSync[n_fx_params];

    constexpr static int n_fx = 14;
    TextButton selectType[n_fx]; // this had better match the list of fxnames in the constructor
    Slider fxTypeSlider;

    void blastToggleState(int i);
    void resetLabels();
    
    std::unique_ptr<SurgeLookAndFeel> surgeLookFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SurgefxAudioProcessorEditor)
};
