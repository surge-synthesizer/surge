/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once


#include "efvg/escape_from_vstgui.h"

#include <JuceHeader.h>

#include "SurgeSynthProcessor.h"

//==============================================================================
/**
*/
class SurgeSynthEditor  : public juce::AudioProcessorEditor, juce::AsyncUpdater,
                         EscapeFromVSTGUI::JuceVSTGUIEditorAdapter
{
public:
    SurgeSynthEditor (SurgeSynthProcessor&);
    ~SurgeSynthEditor();

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void paramsChangedCallback();
    void setEffectType(int i);

    virtual void handleAsyncUpdate() override;

    void controlBeginEdit( VSTGUI::CControl *c) override {}
    void controlEndEdit( VSTGUI::CControl *c) override {}

    struct IdleTimer : juce::Timer {
      IdleTimer(SurgeSynthEditor *ed) : ed(ed) {}
      ~IdleTimer() = default;
      void timerCallback() override { ed->idle(); }
      SurgeSynthEditor *ed;
    };
    void idle();
    std::unique_ptr<IdleTimer> idleTimer;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgeSynthProcessor& processor;

    std::unique_ptr<SurgeGUIEditor> adapter;

    std::unique_ptr<VSTGUI::CFrame> vstguiFrame;
    std::unique_ptr<juce::Drawable> logo;

    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SurgeSynthEditor)
};
