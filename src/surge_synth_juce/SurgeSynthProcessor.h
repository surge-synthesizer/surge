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
#include "LockFreeStack.h"

#include <functional>
#include <unordered_map>

#if MAC
#include <execinfo.h>
#endif

//==============================================================================
/**
 */

struct SurgeParamToJuceInfo
{
    static juce::String getParameterName(SurgeSynthesizer *s, Parameter *p)
    {
        char txt[TXT_SIZE];
        s->getParameterName(s->idForParameter(p), txt);
        return juce::String(txt);
    }
};

/*
 * Obviously buckets of work to do here
 */
struct SurgeParamToJuceParamAdapter : juce::RangedAudioParameter
{
    explicit SurgeParamToJuceParamAdapter(SurgeSynthesizer *s, Parameter *p)
        : s(s), p(p), range(0.f, 1.f, 0.001f), juce::RangedAudioParameter(
                                                   p->get_storage_name(),
                                                   SurgeParamToJuceInfo::getParameterName(s, p), "")
    {
        setValueNotifyingHost(getValue());
    }

    // Oh this is all incorrect of course
    float getValue() const override { return s->getParameter01(s->idForParameter(p)); }
    float getDefaultValue() const override { return 0.0; /* FIXME */ }
    void setValue(float f) override
    {
        if (f != getValue())
            s->setParameter01(s->idForParameter(p), f, true);
    }
    int getNumSteps() const override { return RangedAudioParameter::getNumSteps(); }
    float getValueForText(const juce::String &text) const override
    {
        pdata onto;
        if (p->set_value_from_string_onto(text.toStdString(), onto))
        {
            if (p->valtype == vt_float)
                return onto.f;
            if (p->valtype == vt_int)
                return onto.i;
            if (p->valtype == vt_bool)
                return onto.b;
        }
        return 0;
    }
    juce::String getCurrentValueAsText() const override
    {
        char txt[TXT_SIZE];
        p->get_display(txt);
        return txt;
    }
    juce::String getText(float normalisedValue, int i) const override
    {
        char txt[TXT_SIZE];
        p->get_display(txt, true, normalisedValue);
        return txt;
    }
    bool isMetaParameter() const override { return true; }
    const juce::NormalisableRange<float> &getNormalisableRange() const override { return range; }
    juce::NormalisableRange<float> range;
    SurgeSynthesizer *s;
    Parameter *p;
};

class SurgeSynthProcessor : public juce::AudioProcessor,
                            public SurgeSynthesizer::PluginLayer,
                            public juce::MidiKeyboardState::Listener
{
  public:
    //==============================================================================
    SurgeSynthProcessor();
    ~SurgeSynthProcessor();

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    std::atomic<float> standaloneTempo{120};
    struct midiR
    {
        midiR() {}
        midiR(int c, int n, int v, bool o) : ch(c), note(n), vel(v), on(o) {}
        int ch{0}, note{0}, vel{0};
        bool on{false};
    };
    LockFreeStack<midiR, 4096> midiFromGUI;
    bool isAddingFromMidi{false};
    void handleNoteOn(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber,
                      float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber,
                       float velocity) override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    void surgeParameterUpdated(const SurgeSynthesizer::ID &id, float value) override;

    std::unique_ptr<SurgeSynthesizer> surge;
    std::unordered_map<SurgeSynthesizer::ID, SurgeParamToJuceParamAdapter *> paramsByID;

    std::string paramClumpName(int clumpid);
    juce::MidiKeyboardState midiKeyboardState;

  private:
    std::vector<SurgeParamToJuceParamAdapter *> paramAdapters;

    std::vector<int> presetOrderToPatchList;
    int blockPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeSynthProcessor)
};
