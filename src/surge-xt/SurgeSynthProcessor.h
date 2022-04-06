/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"
#include "util/LockFreeStack.h"

#include "juce_audio_processors/juce_audio_processors.h"

#if HAS_CLAP_JUCE_EXTENSIONS
#include "clap-juce-extensions/clap-juce-extensions.h"
#endif

#include <unordered_map>

#if MAC
#include <execinfo.h>
#endif

namespace Surge
{
namespace GUI
{
struct UndoManager;
}
} // namespace Surge

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

    static juce::String getMacroName(SurgeSynthesizer *s, int macro)
    {
        juce::String res = s->storage.getPatch().CustomControllerLabel[macro];
        return res;
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
    juce::String getName(int i) const override
    {
        juce::String res = SurgeParamToJuceInfo::getParameterName(s, p);
        res = res.substring(0, i);
        return res;
    }
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
                return p->value_to_normalized(onto.f);
            if (p->valtype == vt_int)
                return p->value_to_normalized((float)onto.i);
            if (p->valtype == vt_bool)
                return p->value_to_normalized((float)onto.b);
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

struct SurgeMacroToJuceParamAdapter : public juce::RangedAudioParameter
{
    explicit SurgeMacroToJuceParamAdapter(SurgeSynthesizer *s, long macroNum)
        : s(s), macroNum(macroNum),
          range(0.f, 1.f, 0.001f), juce::RangedAudioParameter(
                                       std::string("macro_") + std::to_string(macroNum),
                                       std::string("C: ") + std::to_string(macroNum), "")
    {
        setValueNotifyingHost(getValue());
    }

    // Oh this is all incorrect of course
    juce::String getName(int i) const override
    {
        juce::String res = "C" + std::to_string(macroNum) + ": ";
        res += SurgeParamToJuceInfo::getMacroName(s, macroNum);

        res = res.substring(0, i);
        return res;
    }
    float getValue() const override { return s->getMacroParameter01(macroNum); }
    float getValueForText(const juce::String &text) const override
    {
        auto tf = std::atof(text.toRawUTF8());
        return std::max(std::min(tf, 1.0), 0.0);
    }
    float getDefaultValue() const override { return 0.0; /* FIXME */ }
    void setValue(float f) override
    {
        if (f != getValue())
            s->setMacroParameter01(macroNum, f);
    }

    const juce::NormalisableRange<float> &getNormalisableRange() const override { return range; }
    juce::NormalisableRange<float> range;
    SurgeSynthesizer *s;
    long macroNum;
};

class SurgeSynthProcessor : public juce::AudioProcessor,
#if SURGE_JUCE_VST3_EXTENSIONS
                            public juce::VST3ClientExtensions,
#endif

#if HAS_CLAP_JUCE_EXTENSIONS
                            public clap_juce_extensions::clap_properties,
#endif

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
    bool canRemoveBusValue = false;
    bool canRemoveBus(bool isInput) const override { return canRemoveBusValue; }
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlockBypassed(juce::AudioBuffer<float> &buffer,
                              juce::MidiBuffer &midiMessages) override;
    bool priorCalLWasProcessBlockNotBypassed{true};
    int bypassCountdown{-1};

    void applyMidi(const juce::MidiMessageMetadata &);
    bool supportsMPE() const override { return true; }

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    std::atomic<float> standaloneTempo{120};
    struct midiR
    {
        enum Type
        {
            NOTE,
            MODWHEEL,
            PITCHWHEEL,
        } type{NOTE};
        int ch{0}, note{0}, vel{0};
        bool on{false};
        int cval{0};
        midiR() {}
        midiR(int c, int n, int v, bool o) : type(NOTE), ch(c), note(n), vel(v), on(o) {}
        midiR(Type type, int cval) : type(type), cval(cval) {}
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
    void surgeMacroUpdated(long macroNum, float d) override;

    std::unique_ptr<SurgeSynthesizer> surge;
    std::unordered_map<SurgeSynthesizer::ID, SurgeParamToJuceParamAdapter *> paramsByID;
    std::vector<SurgeMacroToJuceParamAdapter *> macrosById;

    std::string paramClumpName(int clumpid);
    juce::MidiKeyboardState midiKeyboardState;

#if SURGE_JUCE_VST3_EXTENSIONS
    bool getPluginHasMainInput() const override { return false; }
#endif

  private:
    std::vector<SurgeParamToJuceParamAdapter *> paramAdapters;

    std::vector<int> presetOrderToPatchList;
    int juceSidePresetId{0};

    int blockPos = 0;

    int checkNamesEvery = 0;

  public:
    std::unique_ptr<Surge::GUI::UndoManager> undoManager;

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeSynthProcessor)
};
