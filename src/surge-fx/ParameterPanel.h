#pragma once

#include <memory> // Include memory header for std::unique_ptr

#include "SurgeFXProcessor.h"
#include "SurgeLookAndFeel.h"
#include "KnobSource.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <sst/jucegui/style/StyleSheet.h>
#include <sst/jucegui/components/Knob.h>

#include "SurgeStorage.h"
#include "Effect.h"
#include "FXOpenSoundControl.h"
#include <atomic>
#include "sst/filters/HalfRateFilter.h"

#include "juce_audio_processors/juce_audio_processors.h"

class ParameterPanel : public juce::Component
{
  public:
    ParameterPanel(SurgefxAudioProcessor &);

    ~ParameterPanel() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void setProjectFont(juce::Font font) { projectFont = font; }
    void reset();

    SurgeFXParamDisplay fxParamDisplay[n_fx_params];
    std::vector<std::unique_ptr<KnobSource>> sources;
    std::shared_ptr<sst::jucegui::style::StyleSheet> styleSheet;

  private:
    static constexpr int topSection = 80;
    static constexpr int baseWidth = 600;

    SurgefxAudioProcessor &processor;

    SurgeTempoSyncSwitch fxTempoSync[n_fx_params];
    SurgeTempoSyncSwitch fxDeactivated[n_fx_params];
    SurgeTempoSyncSwitch fxExtended[n_fx_params];
    SurgeTempoSyncSwitch fxAbsoluted[n_fx_params];

    std::vector<std::unique_ptr<sst::jucegui::components::Knob>> knobs;

    juce::Colour backgroundColour = juce::Colour(205, 206, 212);
    juce::Colour surgeOrange = juce::Colour(255, 144, 0);

    juce::Font projectFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel);
};
