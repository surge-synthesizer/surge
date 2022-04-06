/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "DebugHelpers.h"
#include "plugin_type_extensions/SurgeSynthFlavorExtensions.h"
#include "version.h"
#include "sst/plugininfra/cpufeatures.h"

/*
 * This is a bit odd but - this is an editor concept with the lifetime of the processor
 */
#include "gui/UndoManager.h"

//==============================================================================
SurgeSynthProcessor::SurgeSynthProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                               .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Scene A", juce::AudioChannelSet::stereo(), false)
                               .withOutput("Scene B", juce::AudioChannelSet::stereo(), false))
{
    // Since we are using 'external clap' this is the one JUCE API we can't override
    std::string wrapperTypeString = getWrapperTypeDescription(wrapperType);

#if HAS_CLAP_JUCE_EXTENSIONS
    if (wrapperType == wrapperType_Undefined && is_clap)
        wrapperTypeString = std::string("Clap.v") + std::to_string(clap_version_major) + "." +
                            std::to_string(clap_version_minor);
#endif

    std::cout << "SurgeXT " << wrapperTypeString << "\n"
              << "  - Version      : " << Surge::Build::FullVersionStr << " with JUCE " << std::hex
              << JUCE_VERSION << std::dec << "\n"
              << "  - Build Info   : " << Surge::Build::BuildDate << " " << Surge::Build::BuildTime
              << " using " << Surge::Build::BuildCompiler << "\n";

    surge = std::make_unique<SurgeSynthesizer>(this);
    std::cout << "  - Data         : " << surge->storage.datapath << "\n"
              << "  - User Data    : " << surge->storage.userDataPath << std::endl;

    auto parent = std::make_unique<juce::AudioProcessorParameterGroup>("Root", "Root", "|");
    auto macroG = std::make_unique<juce::AudioProcessorParameterGroup>("macros", "Macros", "|");
    for (int mn = 0; mn < n_customcontrollers; ++mn)
    {
        auto nm = std::make_unique<SurgeMacroToJuceParamAdapter>(surge.get(), mn);
        macrosById.push_back(nm.get());
        macroG->addChild(std::move(nm));
    }
    parent->addChild(std::move(macroG));

    std::map<unsigned int, std::vector<std::unique_ptr<juce::AudioProcessorParameter>>> parByGroup;
    for (auto par : surge->storage.getPatch().param_ptr)
    {
        if (par)
        {
            parametermeta pm;
            surge->getParameterMeta(surge->idForParameter(par), pm);
            auto sja = std::make_unique<SurgeParamToJuceParamAdapter>(surge.get(), par);
            paramsByID[surge->idForParameter(par)] = sja.get();
            parByGroup[pm.clump].push_back(std::move(sja));
        }
    }
    for (auto &cv : parByGroup)
    {
        auto clump = cv.first;
        std::string id = std::string("SRG_GRP_") + std::to_string(clump);
        auto name = paramClumpName(clump);
        auto subg = std::make_unique<juce::AudioProcessorParameterGroup>(id, name, "|");
        for (auto &p : cv.second)
        {
            subg->addChild(std::move(p));
        }
        parent->addChild(std::move(subg));
    }

    auto vb = std::make_unique<SurgeBypassParameter>();
    bypassParameter = vb.get();
    parent->addChild(std::move(vb));

    addParameterGroup(std::move(parent));

    presetOrderToPatchList.clear();
    for (int i = 0; i < surge->storage.firstThirdPartyCategory; i++)
    {
        // Remap index to the corresponding category in alphabetical order.
        int c = surge->storage.patchCategoryOrdering[i];
        for (auto p : surge->storage.patchOrdering)
        {
            if (surge->storage.patch_list[p].category == c)
            {
                presetOrderToPatchList.push_back(p);
            }
        }
    }

    surge->hostProgram = juce::PluginHostType().getHostDescription();
    surge->juceWrapperType = wrapperTypeString;

    midiKeyboardState.addListener(this);

    SurgeSynthProcessorSpecificExtensions(this, surge.get());
}

SurgeSynthProcessor::~SurgeSynthProcessor() {}

//==============================================================================
const juce::String SurgeSynthProcessor::getName() const { return JucePlugin_Name; }

bool SurgeSynthProcessor::acceptsMidi() const { return false; }

bool SurgeSynthProcessor::producesMidi() const { return false; }

bool SurgeSynthProcessor::isMidiEffect() const { return false; }

double SurgeSynthProcessor::getTailLengthSeconds() const { return 2.0; }

#undef SURGE_JUCE_PRESETS

int SurgeSynthProcessor::getNumPrograms()
{
#ifdef SURGE_JUCE_PRESETS
    return surge->storage.patch_list.size() + 1;
#else
    return 1;
#endif
}

int SurgeSynthProcessor::getCurrentProgram()
{
#ifdef SURGE_JUCE_PRESETS
    return juceSidePresetId;
#else
    return 0;
#endif
}

void SurgeSynthProcessor::setCurrentProgram(int index)
{
#ifdef SURGE_JUCE_PRESETS
    if (index > 0 && index <= presetOrderToPatchList.size())
    {
        juceSidePresetId = index;
        surge->patchid_queue = presetOrderToPatchList[index - 1];
    }
#endif
}

const juce::String SurgeSynthProcessor::getProgramName(int index)
{
#ifdef SURGE_JUCE_PRESETS
    if (index == 0)
        return "INIT OR DROPPED";
    index--;
    if (index < 0 || index >= presetOrderToPatchList.size())
    {
        return "RANGE ERROR";
    }
    auto patch = surge->storage.patch_list[presetOrderToPatchList[index]];
    auto res = surge->storage.patch_category[patch.category].name + " / " + patch.name;
    return res;
#else
    return "";
#endif
}

void SurgeSynthProcessor::changeProgramName(int index, const juce::String &newName) {}

//==============================================================================
void SurgeSynthProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    surge->setSamplerate(sr);
    surge->audio_processing_active = true;
}

void SurgeSynthProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool SurgeSynthProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    auto mocs = layouts.getMainOutputChannelSet();
    auto mics = layouts.getMainInputChannelSet();

    auto outputValid = (mocs == juce::AudioChannelSet::stereo()) ||
                       (mocs == juce::AudioChannelSet::mono()) || (mocs.isDisabled());
    auto inputValid = (mics == juce::AudioChannelSet::stereo()) ||
                      (mics == juce::AudioChannelSet::mono()) || (mics.isDisabled());

    /*
     * Check the 6 output shape
     */
    auto c1 = layouts.getNumChannels(false, 1);
    auto c2 = layouts.getNumChannels(false, 2);
    auto sceneOut = (c1 == 0 && c2 == 0) || (c1 == 2 && c2 == 2);

    return outputValid && inputValid && sceneOut;
}

void SurgeSynthProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                       juce::MidiBuffer &midiMessages)
{
    auto fpuguard = sst::plugininfra::cpufeatures::FPUStateGuard();

    priorCallWasProcessBlockNotBypassed = true;

    // Make sure we have a main output
    auto mb = getBus(false, 0);
    if (mb->getNumberOfChannels() != 2 || !mb->isEnabled())
    {
        // We have to have a stereo output
        return;
    }

    if (bypassParameter->getValue() > 0.5)
    {
        if (priorCallWasProcessBlockNotBypassed)
        {
            surge->allNotesOff();
            bypassCountdown = 8; // let us fade out by doing a halted process
        }
        if (bypassCountdown == 0)
        {
            return;
        }
        bypassCountdown--;
        surge->audio_processing_active = false;
        priorCallWasProcessBlockNotBypassed = false;
        midiMessages.clear(); // but don't send notes. We are all notes off
    }

    if (!surge->audio_processing_active)
    {
        // I am just becoming active. There may be lingering notes from when I was
        // deactivated so
        surge->allNotesOff();
    }
    surge->audio_processing_active = true;

    auto playhead = getPlayHead();
    if (playhead)
    {
        juce::AudioPlayHead::CurrentPositionInfo cp;
        playhead->getCurrentPosition(cp);
        surge->time_data.tempo = cp.bpm;

        // isRecording should always imply isPlaying but better safe than sorry
        if (cp.isPlaying || cp.isRecording)
            surge->time_data.ppqPos = cp.ppqPosition;

        surge->time_data.timeSigNumerator = cp.timeSigNumerator;
        surge->time_data.timeSigDenominator = cp.timeSigDenominator;
        surge->resetStateFromTimeData();
    }
    else
    {
        surge->time_data.tempo = standaloneTempo;
        surge->time_data.timeSigNumerator = 4;
        surge->time_data.timeSigDenominator = 4;
        surge->resetStateFromTimeData();
    }

    midiR rec;
    while (midiFromGUI.pop(rec))
    {
        if (rec.type == midiR::NOTE)
        {
            if (rec.on)
                surge->playNote(rec.ch, rec.note, rec.vel, 0);
            else
                surge->releaseNote(rec.ch, rec.note, rec.vel);
        }
        if (rec.type == midiR::PITCHWHEEL)
        {
            surge->pitchBend(rec.ch, rec.cval);
        }
        if (rec.type == midiR::MODWHEEL)
        {
            surge->channelController(rec.ch, 1, rec.cval);
        }
    }

    auto mainOutput = getBusBuffer(buffer, false, 0);

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto sceneAOutput = getBusBuffer(buffer, false, 1);
    auto sceneBOutput = getBusBuffer(buffer, false, 2);

    auto midiIt = midiMessages.findNextSamplePosition(0);
    int nextMidi = -1;
    if (midiIt != midiMessages.cend())
    {
        nextMidi = (*midiIt).samplePosition;
    }

    for (int i = 0; i < buffer.getNumSamples(); i++)
    {
        while (i == nextMidi)
        {
            applyMidi(*midiIt);
            midiIt++;
            if (midiIt == midiMessages.cend())
            {
                nextMidi = -1;
            }
            else
            {
                nextMidi = (*midiIt).samplePosition;
            }
        }
        auto outL = mainOutput.getWritePointer(0, i);
        auto outR = mainOutput.getWritePointer(1, i);

        if (blockPos == 0 && mainInput.getNumChannels() > 0)
        {
            auto inL = mainInput.getReadPointer(0, i);
            auto inR = inL;                     // assume mono
            if (mainInput.getNumChannels() > 1) // unless its not
            {
                inR = mainInput.getReadPointer(1, i);
            }
            surge->process_input = true;
            memcpy(&(surge->input[0][0]), inL, BLOCK_SIZE * sizeof(float));
            memcpy(&(surge->input[1][0]), inR, BLOCK_SIZE * sizeof(float));
        }
        else
        {
            surge->process_input = false;
        }
        if (blockPos == 0)
        {
            surge->process();
            surge->time_data.ppqPos +=
                (double)BLOCK_SIZE * surge->time_data.tempo / (60. * samplerate);
        }
        *outL = surge->output[0][blockPos];
        *outR = surge->output[1][blockPos];

        if (surge->activateExtraOutputs && sceneAOutput.getNumChannels() == 2 &&
            sceneBOutput.getNumChannels() == 2)
        {
            auto sAL = sceneAOutput.getWritePointer(0, i);
            auto sAR = sceneAOutput.getWritePointer(1, i);
            auto sBL = sceneBOutput.getWritePointer(0, i);
            auto sBR = sceneBOutput.getWritePointer(1, i);

            if (sAL && sAR)
            {
                *sAL = surge->sceneout[0][0][blockPos];
                *sAR = surge->sceneout[0][1][blockPos];
            }
            if (sBL && sBR)
            {
                *sBL = surge->sceneout[1][0][blockPos];
                *sBR = surge->sceneout[1][1][blockPos];
            }
        }

        blockPos = (blockPos + 1) & (BLOCK_SIZE - 1);
    }

    // This should, in theory, never happen, but better safe than sorry
    while (midiIt != midiMessages.cend())
    {
        applyMidi(*midiIt);
        midiIt++;
    }

    if (checkNamesEvery++ > 10)
    {
        checkNamesEvery = 0;
        if (std::atomic_exchange(&parameterNameUpdated, false))
        {
            updateHostDisplay();
        }
    }
}

void SurgeSynthProcessor::applyMidi(const juce::MidiMessageMetadata &it)
{
    auto m = it.getMessage();
    const int ch = m.getChannel() - 1;
    juce::ScopedValueSetter<bool> midiAdd(isAddingFromMidi, true);
    midiKeyboardState.processNextMidiEvent(m);

    if (m.isNoteOn())
    {
        surge->playNote(ch, m.getNoteNumber(), m.getVelocity(), 0);
    }
    else if (m.isNoteOff())
    {
        surge->releaseNote(ch, m.getNoteNumber(), m.getVelocity());
    }
    else if (m.isChannelPressure())
    {
        surge->channelAftertouch(ch, m.getChannelPressureValue());
    }
    else if (m.isAftertouch())
    {
        surge->polyAftertouch(ch, m.getNoteNumber(), m.getAfterTouchValue());
    }
    else if (m.isPitchWheel())
    {
        surge->pitchBend(ch, m.getPitchWheelValue() - 8192);
    }
    else if (m.isController())
    {
        surge->channelController(ch, m.getControllerNumber(), m.getControllerValue());
    }
    else if (m.isProgramChange())
    {
        // apparently this is not enough to actually execute SurgeSynthesizer::programChange
        // in VST3 case
        surge->programChange(ch, m.getProgramChangeNumber());
    }
    else
    {
        // std::cout << "Ignoring message " << std::endl;
    }
}

//==============================================================================
bool SurgeSynthProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *SurgeSynthProcessor::createEditor()
{
    return new SurgeSynthEditor(*this);
}

//==============================================================================
void SurgeSynthProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    surge->populateDawExtraState();
    auto sse = dynamic_cast<SurgeSynthEditor *>(getActiveEditor());
    if (sse)
    {
        sse->populateForStreaming(surge.get());
    }

    void *data = nullptr; // surgeInstance owns this on return
    unsigned int stateSize = surge->saveRaw(&data);
    destData.setSize(stateSize);
    destData.copyFrom(data, 0, stateSize);
}

void SurgeSynthProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    surge->enqueuePatchForLoad(data, sizeInBytes);
    surge->processThreadunsafeOperations();
}

void SurgeSynthProcessor::surgeParameterUpdated(const SurgeSynthesizer::ID &id, float f)
{
    auto spar = paramsByID[id];
    if (spar)
    {
        spar->setValueNotifyingHost(f);
    }
}

void SurgeSynthProcessor::surgeMacroUpdated(const long id, float f)
{
    auto spar = macrosById[id];
    if (spar)
        spar->setValueNotifyingHost(f);
}

std::string SurgeSynthProcessor::paramClumpName(int clumpid)
{
    switch (clumpid)
    {
    case 1:
        return "Macros";
    case 2:
        return "Global & FX";
    case 3:
        return "A Common";
    case 4:
        return "A Oscillators";
    case 5:
        return "A Mixer";
    case 6:
        return "A Filters";
    case 7:
        return "A Envelopes";
    case 8:
        return "A LFOs";
    case 9:
        return "B Common";
    case 10:
        return "B Oscillators";
    case 11:
        return "B Mixer";
    case 12:
        return "B Filters";
    case 13:
        return "B Envelopes";
    case 14:
        return "B LFOs";
    }
    return "";
}

void SurgeSynthProcessor::handleNoteOn(juce::MidiKeyboardState *source, int midiChannel,
                                       int midiNoteNumber, float velocity)
{
    if (!isAddingFromMidi)
        midiFromGUI.push(midiR(midiChannel - 1, midiNoteNumber, (int)(127.f * velocity), true));
}

void SurgeSynthProcessor::handleNoteOff(juce::MidiKeyboardState *source, int midiChannel,
                                        int midiNoteNumber, float velocity)
{
    if (!isAddingFromMidi)
        midiFromGUI.push(midiR(midiChannel - 1, midiNoteNumber, (int)(127.f * velocity), false));
}

juce::AudioProcessorParameter *SurgeSynthProcessor::getBypassParameter() const
{
    return bypassParameter;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgeSynthProcessor(); }
