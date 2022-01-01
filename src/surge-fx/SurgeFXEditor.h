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
    struct AccSlider : public juce::Slider
    {
        AccSlider() { setWantsKeyboardFocus(true); }
        bool keyPressed(const juce::KeyPress &key) override
        {
            float amt = 0.05;
            if (key.getModifiers().isShiftDown())
                amt = 0.01;
            if (key.getKeyCode() == juce::KeyPress::upKey)
            {
                setValue(std::clamp(getValue() + amt, 0., 1.),
                         juce::NotificationType::sendNotification);
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::downKey)
            {
                setValue(std::clamp(getValue() - amt, 0., 1.),
                         juce::NotificationType::sendNotification);
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::homeKey)
            {
                setValue(1., juce::NotificationType::sendNotification);
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::endKey)
            {
                setValue(0., juce::NotificationType::sendNotification);
                return true;
            }
            return false;
        }
    };
    AccSlider fxParamSliders[n_fx_params];
    SurgeFXParamDisplay fxParamDisplay[n_fx_params];
    SurgeTempoSyncSwitch fxTempoSync[n_fx_params];
    SurgeTempoSyncSwitch fxDeactivated[n_fx_params];
    SurgeTempoSyncSwitch fxExtended[n_fx_params];
    SurgeTempoSyncSwitch fxAbsoluted[n_fx_params];

    void blastToggleState(int i);
    void resetLabels();

    std::unique_ptr<SurgeLookAndFeel> surgeLookFeel;
    std::unique_ptr<juce::Label> fxNameLabel;

    void addAndMakeVisibleRecordOrder(juce::Component *c)
    {
        accessibleOrderWeakRefs.push_back(c);
        addAndMakeVisible(c);
    }

  public:
    std::vector<juce::Component *> accessibleOrderWeakRefs;

  public:
    std::unique_ptr<juce::ComponentTraverser> createFocusTraverser() override;

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgefxAudioProcessorEditor)
};
