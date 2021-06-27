/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "SurgeSynthProcessor.h"

class SurgeGUIEditor;
class SurgeJUCELookAndFeel;

//==============================================================================
/**
 */
class SurgeSynthEditor : public juce::AudioProcessorEditor,
                         public juce::AsyncUpdater,
                         public juce::FileDragAndDropTarget
{
  public:
    SurgeSynthEditor(SurgeSynthProcessor &);
    ~SurgeSynthEditor();

    static constexpr int extraYSpaceForVirtualKeyboard = 50;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    void parentHierarchyChanged() override;

    void paramsChangedCallback();
    void setEffectType(int i);

    void handleAsyncUpdate() override;

    void populateForStreaming(SurgeSynthesizer *s);
    void populateFromStreaming(SurgeSynthesizer *s);

    void beginParameterEdit(Parameter *p);
    void endParameterEdit(Parameter *p);

    struct IdleTimer : juce::Timer
    {
        IdleTimer(SurgeSynthEditor *ed) : ed(ed) {}
        ~IdleTimer() = default;
        void timerCallback() override;
        SurgeSynthEditor *ed;
    };
    void idle();
    std::unique_ptr<IdleTimer> idleTimer;

    bool drawExtendedControls{false};
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    std::unique_ptr<juce::Label> tempoLabel;
    std::unique_ptr<juce::TextEditor> tempoTypein;

    /* Drag and drop */
    bool isInterestedInFileDrag(const juce::StringArray &files) override;
    void filesDropped(const juce::StringArray &files, int, int) override;

    juce::PopupMenu hostMenuFor(Parameter *p);

    friend class SurgeGUIEditor;

  private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgeSynthProcessor &processor;

    std::unique_ptr<SurgeGUIEditor> adapter;
    std::unique_ptr<juce::Drawable> logo;

    std::unique_ptr<SurgeJUCELookAndFeel> surgeLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeSynthEditor)
};
