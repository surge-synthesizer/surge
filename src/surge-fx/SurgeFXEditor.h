/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "SurgeFXProcessor.h"
#include "SurgeLookAndFeel.h"

#include "juce_gui_basics/juce_gui_basics.h"

//==============================================================================
/**
 */
class SurgefxAudioProcessorEditor : public juce::AudioProcessorEditor, juce::AsyncUpdater
{
  public:
    SurgefxAudioProcessorEditor(SurgefxAudioProcessor &);
    ~SurgefxAudioProcessorEditor();

    struct FxMenu
    {
        enum Type
        {
            SECTION,
            FX
        } type;
        bool isBreak{false};
        std::string name;
        int fxtype{-1};
    };
    std::vector<FxMenu> menu;
    std::unique_ptr<juce::Component> picker;
    void makeMenu();
    void showMenu();

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

    void paramsChangedCallback();
    void setEffectType(int i);

    virtual void handleAsyncUpdate() override;

    enum RadioGroupIds
    {
        FxTypeGroup = 1776
    };

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgefxAudioProcessor &processor;

  private:
    juce::Slider fxParamSliders[n_fx_params];
    SurgeFXParamDisplay fxParamDisplay[n_fx_params];
    SurgeTempoSyncSwitch fxTempoSync[n_fx_params];
    SurgeTempoSyncSwitch fxDeactivated[n_fx_params];
    SurgeTempoSyncSwitch fxExtended[n_fx_params];
    SurgeTempoSyncSwitch fxAbsoluted[n_fx_params];

    void blastToggleState(int i);
    void resetLabels();

    std::unique_ptr<SurgeLookAndFeel> surgeLookFeel;
    std::unique_ptr<juce::Label> fxNameLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgefxAudioProcessorEditor)
};
