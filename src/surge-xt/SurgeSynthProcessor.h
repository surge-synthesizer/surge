/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_SURGE_XT_SURGESYNTHPROCESSOR_H
#define SURGE_SRC_SURGE_XT_SURGESYNTHPROCESSOR_H

#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"
#include "util/LockFreeStack.h"

#include "osc/OpenSoundControl.h"

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
        // technically this branch is checked inside the extended by fx group but
        // lets make it explicit here also
        if (p->ctrlgroup == cg_FX)
        {
            s->getParameterNameExtendedByFXGroup(s->idForParameter(p), txt);
        }
        else
        {
            s->getParameterName(s->idForParameter(p), txt);
        }

        return juce::String(txt);
    }

    static juce::String getMacroName(SurgeSynthesizer *s, int macro)
    {
        juce::String res = s->storage.getPatch().CustomControllerLabel[macro];
        return res;
    }
};

struct SurgeBaseParam : juce::RangedAudioParameter
#if HAS_CLAP_JUCE_EXTENSIONS
    ,
                        clap_juce_extensions::clap_juce_parameter_capabilities
#endif

{
    template <typename... Args>
    SurgeBaseParam(Args &&...args) : juce::RangedAudioParameter(std::forward<Args>(args)...)
    {
    }

    virtual void applyMonophonicModulation(double value)
    {
        std::cout << "Base Class Mono Mod - Override " << value << std::endl;
    }
    virtual void applyPolyphonicModulation(int32_t note_id, int16_t key, int16_t channel,
                                           double value)
    {
        std::cout << "BASE CLASS POLY MOD - Override for note_id=" << note_id << " key=" << key
                  << " channel=" << channel << " value=" << value << std::endl;
    }
};

struct SurgeParamToJuceParamAdapter : SurgeBaseParam
{
    explicit SurgeParamToJuceParamAdapter(SurgeSynthesizer *s, Parameter *p)
        : s(s), p(p), range(0.f, 1.f, 0.001f),
          SurgeBaseParam(juce::ParameterID(p->get_storage_name(),
                                           1), // This "1" needs thought if we add params
                         SurgeParamToJuceInfo::getParameterName(s, p),
                         juce::AudioProcessorParameterWithIDAttributes())
    {
        setValueNotifyingHost(getValue());
    }

    std::atomic<bool> inEditGesture{false};

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
        auto matches = (f == getValue());
        if (!matches && !inEditGesture)
        {
            s->setParameter01(s->idForParameter(p), f, true);
        }
        /*
         * In LIVE 11.1 and 11.2 this will fire and matches will be false
        else if (inEditGesture)
        {
            std::cout << ">>> VST3 SUPPRESSED >>> " << f << " " << (matches ? "match" : "DIFFERENT")
                      << std::endl;
        }
         */
    }

    int getNumSteps() const override { return RangedAudioParameter::getNumSteps(); }
    float getValueForText(const juce::String &text) const override
    {
        pdata onto;
        std::string errMsg;
        if (p->set_value_from_string_onto(text.toStdString(), onto, errMsg))
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
    juce::String getCurrentValueAsText() const override { return p->get_display(); }
    juce::String getText(float normalisedValue, int i) const override
    {
        return p->get_display(true, normalisedValue);
    }
    bool isMetaParameter() const override { return true; }
    const juce::NormalisableRange<float> &getNormalisableRange() const override { return range; }
    juce::NormalisableRange<float> range;
    SurgeSynthesizer *s;
    Parameter *p;

#if HAS_CLAP_JUCE_EXTENSIONS
    bool supportsMonophonicModulation() override { return p->can_be_nondestructively_modulated(); }
    bool supportsPolyphonicModulation() override
    {
        return p->can_be_nondestructively_modulated() && p->per_voice_processing;
    }
    void applyPolyphonicModulation(int32_t note_id, int16_t key, int16_t channel,
                                   double value) override
    {
        s->applyParameterPolyphonicModulation(p, note_id, key, channel, value);
    }
    void applyMonophonicModulation(double value) override
    {
        s->applyParameterMonophonicModulation(p, value);
    }

#endif
};

struct SurgeMacroToJuceParamAdapter : public SurgeBaseParam
{
    explicit SurgeMacroToJuceParamAdapter(SurgeSynthesizer *s, long macroNum)
        : s(s), macroNum(macroNum), range(0.f, 1.f, 0.001f),
          SurgeBaseParam(juce::ParameterID(std::string("macro_") + std::to_string(macroNum), 1),
                         std::string("M") + std::to_string(macroNum + 1),
                         juce::AudioProcessorParameterWithIDAttributes())
    {
        setValueNotifyingHost(getValue());
    }

    juce::String getName(int i) const override
    {
        juce::String res = "M" + std::to_string(macroNum + 1) + ": ";
        res += SurgeParamToJuceInfo::getMacroName(s, macroNum);

        res = res.substring(0, i);
        return res;
    }
    float getValue() const override
    {
        /*
         * So why the target here? When we setValue we want the immediate
         * getValue to return the same thing, but setValue sets the target
         * So basically hide the smoothing from the externalizaion of the
         * parameter to meet th econstraint
         */
        return s->getMacroParameterTarget01(macroNum);
    }
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
    juce::String getText(float normalisedValue, int i) const override
    {
        return std::to_string(s->getMacroParameter01(macroNum));
    }

#if HAS_CLAP_JUCE_EXTENSIONS
    bool supportsMonophonicModulation() override { return true; }
    void applyMonophonicModulation(double value) override
    {
        s->applyMacroMonophonicModulation(macroNum, value);
    }
#endif

    const juce::NormalisableRange<float> &getNormalisableRange() const override { return range; }
    juce::NormalisableRange<float> range;
    SurgeSynthesizer *s;
    long macroNum;
};

struct SurgeBypassParameter : public juce::RangedAudioParameter
{
    explicit SurgeBypassParameter()
        : value(0.f), range(0.f, 1.f, 0.01f),
          juce::RangedAudioParameter(juce::ParameterID("surgext-bypass", 1), "Bypass Surge XT",
                                     juce::AudioProcessorParameterWithIDAttributes())
    {
        setValueNotifyingHost(getValue());
    }

    juce::String getName(int i) const override { return "Bypass Surge XT"; }
    float value{0.f};
    float getValue() const override { return value; }
    float getValueForText(const juce::String &text) const override
    {
        auto tf = std::atof(text.toRawUTF8());
        return std::max(std::min(tf, 1.0), 0.0);
    }
    float getDefaultValue() const override { return 0.0; }
    void setValue(float f) override { value = f; }
    juce::String getText(float normalisedValue, int i) const override
    {
        return (value > 0.5 ? "Bypassed" : "Active");
    }
    bool isAutomatable() const override { return false; }

    const juce::NormalisableRange<float> &getNormalisableRange() const override { return range; }
    juce::NormalisableRange<float> range;
};

class SurgeSynthProcessor : public juce::AudioProcessor,
                            public juce::VST3ClientExtensions,

#if HAS_CLAP_JUCE_EXTENSIONS
                            public clap_juce_extensions::clap_properties,
                            public clap_juce_extensions::clap_juce_audio_processor_capabilities,
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
    bool priorCallWasProcessBlockNotBypassed{true};
    int bypassCountdown{-1};

    void processBlockPlayhead();
    void processBlockMidiFromGUI();
    void processBlockOSC();
    void processBlockPostFunction();

    void applyMidi(const juce::MidiMessageMetadata &);
    void applyMidi(const juce::MidiMessage &);
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
            SUSPEDAL,
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
    // Open Sound Control
    enum oscToAudio_type
    {
        MNOTE,
        FREQNOTE,
        PARAMETER,
        MACRO,
        NOTEX_PITCH,
        NOTEX_VOL,
        NOTEX_PAN,
        NOTEX_TIMB,
        NOTEX_PRES,
        ALLNOTESOFF,
        ALLSOUNDOFF,
        MOD,
        FX_DISABLE,
        MOD_MUTE
    };

    struct oscToAudio
    {
        oscToAudio_type type;
        Parameter *param;
        float fval{0.0};
        int ival{-1};
        char mnote{0}, vel{0};
        bool on{false};
        int32_t noteid{-1};
        int scene{0}, index{0};

        oscToAudio() {}
        oscToAudio(oscToAudio_type omtype) : type(omtype) {}
        oscToAudio(int mask, int on) : type(FX_DISABLE), ival(mask), on(on) {}
        oscToAudio(int macnum, float f) : type(MACRO), ival(macnum), fval(f) {}
        oscToAudio(Parameter *p, float f) : type(PARAMETER), param(p), fval(f) {}
        oscToAudio(Parameter *p, int modulator, int sc, int idx, float depth)
            : type(MOD), param(p), ival(modulator), scene(sc), index(idx), fval(depth)
        {
        }
        oscToAudio(Parameter *p, int modulator, int sc, int idx, float depth, bool mute)
            : type(MOD_MUTE), param(p), ival(modulator), scene(sc), index(idx), fval(depth)
        {
        }
        oscToAudio(float freq, char velocity, bool noteon, int32_t nid)
            : type(FREQNOTE), fval(freq), vel(velocity), on(noteon), noteid(nid)
        {
        }
        oscToAudio(char note, char velocity, bool noteon, int32_t nid)
            : type(MNOTE), mnote(note), vel(velocity), on(noteon), noteid(nid)
        {
        }
        oscToAudio(oscToAudio_type type, int32_t nid, float f) : type(type), noteid(nid), fval(f) {}
    };
    sst::cpputils::SimpleRingBuffer<oscToAudio, 4096> oscRingBuf;

    Surge::OSC::OpenSoundControl oscHandler;
    std::atomic<bool> oscCheckStartup{false};
    void tryLazyOscStartupFromStreamedState();

    bool initOSCIn(int port);
    bool initOSCOut(int port, std::string ipaddr);
    bool changeOSCInPort(int newport);
    bool changeOSCOut(int newport, std::string ipaddr);
    void stopOSCOut(bool updateOSCStartInStorage = true);
    void initOSCError(int port, std::string outIP = "");

    void patch_load_to_OSC(fs::path newpath);
    void param_change_to_OSC(std::string paramPath, bool hasFloat, float value, std::string valStr);
    enum specialCaseType
    {
        SCT_MACRO,
        SCT_FX_DEACT
    };

    void paramChangeToListeners(Parameter *p, bool isSpecialCase = false, int specialCaseType = -1,
                                int macronum = 0, float fval = 0.0, std::string newValue = "");

    // --- 'param change' listener(s) ----
    // Listeners are notified whenever a parameter finishes changing, along with the new value.
    // Listeners should do any significant work on their own thread; for example, the OSC
    // paramChangeListener calls OSCSender::send(), which runs on a juce::MessageManager thread.
    //
    // Be sure to delete any added listeners in the destructor of the class that added them.
    std::unordered_map<std::string, std::function<void(const std::string &, const bool, const float,
                                                       const std::string &)>>
        paramChangeListeners;
    void addParamChangeListener(std::string key,
                                std::function<void(const std::string &, const bool, const float,
                                                   const std::string &)> const &l)
    {
        paramChangeListeners.insert({key, l});
    }
    void deleteParamChangeListener(std::string key) { paramChangeListeners.erase(key); }
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

    SurgeBypassParameter *bypassParameter{nullptr};
    juce::AudioProcessorParameter *getBypassParameter() const override;

    std::string paramClumpName(int clumpid);
    juce::MidiKeyboardState midiKeyboardState;
    void reset() override;

    bool getPluginHasMainInput() const override { return false; }

#if HAS_CLAP_JUCE_EXTENSIONS
    bool isInputMain(int index) override { return false; }
    bool supportsDirectProcess() override { return true; }
    clap_process_status clap_direct_process(const clap_process *process) noexcept override;
    bool supportsDirectParamsFlush() override { return true; }
    void clap_direct_paramsFlush(const clap_input_events * /*in*/,
                                 const clap_output_events * /*out*/) noexcept override;
    void process_clap_event(const clap_event_header_t *evt);
    bool supportsVoiceInfo() override { return true; }
    bool voiceInfoGet(clap_voice_info *info) override
    {
        info->voice_capacity = 128;
        info->voice_count = 128;
        info->flags = CLAP_VOICE_INFO_SUPPORTS_OVERLAPPING_NOTES;
        return true;
    }
    bool supportsRemoteControls() const noexcept override { return true; }
    uint32_t remoteControlsPageCount() noexcept override;
    bool
    remoteControlsPageFill(uint32_t /*pageIndex*/, juce::String & /*sectionName*/,
                           uint32_t & /*pageID*/, juce::String & /*pageName*/,
                           std::array<juce::AudioProcessorParameter *, CLAP_REMOTE_CONTROLS_COUNT>
                               & /*params*/) noexcept override;

    bool supportsPresetLoad() const noexcept override { return true; }
    bool presetLoadFromLocation(uint32_t /*location_kind*/, const char * /*location*/,
                                const char * /*load_key*/) noexcept override;
#endif

  private:
    std::vector<SurgeParamToJuceParamAdapter *> paramAdapters;

    std::vector<int> presetOrderToPatchList;
    int juceSidePresetId{0};

    int blockPos = 0;

    int checkNamesEvery = 0;

    int32_t non_clap_noteid{1};

    // For non-block-size uniform blocks we need to lag input
    float inputLatentBuffer alignas(16)[2][BLOCK_SIZE];

  public:
    bool inputIsLatent{false};

    std::string fatalErrorMessage{false};

  private:
    // Have we warned about bad configurations
    bool warnedAboutBadConfig{false};

  public:
    std::unique_ptr<Surge::GUI::UndoManager> undoManager;

#if HAS_CLAP_JUCE_EXTENSIONS
    static const void *getSurgePresetDiscoveryFactory();
#endif

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeSynthProcessor)
};

#if HAS_CLAP_JUCE_EXTENSIONS
extern const void *JUCE_CALLTYPE clapJuceExtensionCustomFactory(const char *);
#endif

#endif // SURGE_SRC_SURGE_XT_SURGESYNTHPROCESSOR_H
