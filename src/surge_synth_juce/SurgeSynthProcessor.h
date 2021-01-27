/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"

#include <functional>

#if MAC
#include <execinfo.h>
#endif

//==============================================================================
/**
*/

struct SurgeParamToJuceParamAdapter : juce::RangedAudioParameter {
   explicit SurgeParamToJuceParamAdapter(SurgeSynthesizer *s, Parameter *p) : s(s), p(p),
   range(0.f, 1.f, 0.1f ) ,
   juce::RangedAudioParameter(p->get_storage_name(), p->get_storage_name(), "")
   {

   }

   // Oh this is all incorrect of course
   float getValue() const override { return s->getParameter01(s->idForParameter(p)); }
   float getDefaultValue() const override { return p->val_default.f; }
   void setValue(float f) override {
      s->setParameter01(s->idForParameter(p), f, true );
   }
   float getValueForText(const juce::String& text) const override
   {
      return 0;
   }
   const juce::NormalisableRange<float>& getNormalisableRange() const override
   {
      return range;
   }
   juce::NormalisableRange<float> range;
   SurgeSynthesizer *s;
   Parameter *p;
};

class SurgeSynthProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SurgeSynthProcessor();
    ~SurgeSynthProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    std::unique_ptr<SurgeSynthesizer> surge;
 private:
    std::vector<SurgeParamToJuceParamAdapter *> paramAdapters;

    std::vector<int> presetOrderToPatchList;

    static constexpr int BUFFER_COPY_CHUNK=4;
    int blockPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SurgeSynthProcessor)
};
