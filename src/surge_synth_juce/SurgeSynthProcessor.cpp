/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "DebugHelpers.h"
#include "SurgeSynthFlavorExtensions.h"
#include "version.h"
#include "CPUFeatures.h"

using namespace juce;

//==============================================================================
SurgeSynthProcessor::SurgeSynthProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", AudioChannelSet::stereo(), true)
                         .withOutput("Scene A", AudioChannelSet::stereo(), false)
                         .withOutput("Scene B", AudioChannelSet::stereo(), false))
{
    std::cout << "SurgeXT Startup\n"
              << "  - Version      : " << Surge::Build::FullVersionStr << "\n"
              << "  - Build Info   : " << Surge::Build::BuildArch << " "
              << Surge::Build::BuildCompiler << "\n"
              << "  - Build Time   : " << Surge::Build::BuildDate << " " << Surge::Build::BuildTime
              << "\n"
              << "  - JUCE Version : " << std::hex << JUCE_VERSION << std::dec << "\n"
#if SURGE_JUCE_ACCESSIBLE
              << "  - Accessiblity : Enabled\n"
#endif
#if SURGE_JUCE_HOST_CONTEXT
              << "  - JUCE Host Context Support Enabled\n"
#endif
              << "  - CPU          : " << Surge::CPUFeatures::cpuBrand() << std::endl;

    surge = std::make_unique<SurgeSynthesizer>(this);
    surge->storage.initializePatchDb(); // In the UI branch we want the patch DB running

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
    auto parent = std::make_unique<AudioProcessorParameterGroup>();
    for (auto &cv : parByGroup)
    {
        auto clump = cv.first;
        std::string id = std::string("SRG_GRP_") + std::to_string(clump);
        auto name = paramClumpName(clump);
        auto subg = std::make_unique<AudioProcessorParameterGroup>(id, name, "|");
        for (auto &p : cv.second)
        {
            subg->addChild(std::move(p));
        }
        parent->addChild(std::move(subg));
    }
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
    surge->juceWrapperType = getWrapperTypeDescription(wrapperType);

    midiKeyboardState.addListener(this);

    SurgeSynthProcessorSpecificExtensions(this, surge.get());
}

SurgeSynthProcessor::~SurgeSynthProcessor() {}

//==============================================================================
const String SurgeSynthProcessor::getName() const { return JucePlugin_Name; }

bool SurgeSynthProcessor::acceptsMidi() const { return false; }

bool SurgeSynthProcessor::producesMidi() const { return false; }

bool SurgeSynthProcessor::isMidiEffect() const { return false; }

double SurgeSynthProcessor::getTailLengthSeconds() const { return 2.0; }

int SurgeSynthProcessor::getNumPrograms() { return surge->storage.patch_list.size() + 1; }

int SurgeSynthProcessor::getCurrentProgram()
{
    if (surge->patchid < 0 || surge->patchid > surge->storage.patch_list.size())
        return 0;

    return surge->patchid + 1;
}

void SurgeSynthProcessor::setCurrentProgram(int index)
{
    if (index > 0 && index <= presetOrderToPatchList.size())
    {
        surge->patchid_queue = presetOrderToPatchList[index - 1];
    }
}

const String SurgeSynthProcessor::getProgramName(int index)
{
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
}

void SurgeSynthProcessor::changeProgramName(int index, const String &newName) {}

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

    auto outputValid = (mocs == AudioChannelSet::stereo()) || mocs.isDisabled();
    auto inputValid = (mics == AudioChannelSet::stereo()) || (mics == AudioChannelSet::mono()) ||
                      (mics.isDisabled());

    /*
     * Check the 6 output shape
     */
    auto c1 = layouts.getNumChannels(false, 1);
    auto c2 = layouts.getNumChannels(false, 2);
    auto sceneOut = (c1 == 0 && c2 == 0) || (c1 == 2 && c2 == 2);

    return outputValid && inputValid && sceneOut;
}

void SurgeSynthProcessor::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages)
{
    auto fpuguard = Surge::CPUFeatures::FPUStateGuard();

    auto playhead = getPlayHead();
    if (playhead)
    {
        juce::AudioPlayHead::CurrentPositionInfo cp;
        playhead->getCurrentPosition(cp);
        surge->time_data.tempo = cp.bpm;

        if (cp.isPlaying || cp.isRecording || cp.isLooping)
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

    for (const MidiMessageMetadata it : midiMessages)
    {
        MidiMessage m = it.getMessage();
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
            // Implement program change in XT
        }
        else
        {
            // std::cout << "Ignoring message " << std::endl;
        }
    }

    midiR rec;
    while (midiFromGUI.pop(rec))
    {
        if (rec.on)
            surge->playNote(rec.ch, rec.note, rec.vel, 0);
        else
            surge->releaseNote(rec.ch, rec.note, rec.vel);
    }

    // Make sure we have a main output
    auto mb = getBus(false, 0);
    if (mb->getNumberOfChannels() != 2 || !mb->isEnabled())
    {
        // We have to have a stereo output
        return;
    }
    auto mainOutput = getBusBuffer(buffer, false, 0);

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto sceneAOutput = getBusBuffer(buffer, false, 1);
    auto sceneBOutput = getBusBuffer(buffer, false, 2);
    for (int i = 0; i < buffer.getNumSamples(); i++)
    {
        auto outL = mainOutput.getWritePointer(0, i);
        auto outR = mainOutput.getWritePointer(1, i);

        surge->time_data.ppqPos += (double)BLOCK_SIZE * surge->time_data.tempo / (60. * samplerate);

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
}

//==============================================================================
bool SurgeSynthProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor *SurgeSynthProcessor::createEditor() { return new SurgeSynthEditor(*this); }

//==============================================================================
void SurgeSynthProcessor::getStateInformation(MemoryBlock &destData)
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
    // FIXME - casting away constness is gross
    surge->loadRaw(data, sizeInBytes, false);

    surge->loadFromDawExtraState();
    auto sse = dynamic_cast<SurgeSynthEditor *>(getActiveEditor());
    if (sse)
    {
        sse->populateFromStreaming(surge.get());
    }
}

void SurgeSynthProcessor::surgeParameterUpdated(const SurgeSynthesizer::ID &id, float f)
{
    auto spar = paramsByID[id];
    if (spar)
        spar->setValueNotifyingHost(f);
}

std::string SurgeSynthProcessor::paramClumpName(int clumpid)
{
    switch (clumpid)
    {
    case 1:
        return "Macro Parameters";
    case 2:
        return "Global / FX";
    case 3:
        return "Scene A Common";
    case 4:
        return "Scene A Osc";
    case 5:
        return "Scene A Osc Mixer";
    case 6:
        return "Scene A Filters";
    case 7:
        return "Scene A Envelopes";
    case 8:
        return "Scene A LFOs";
    case 9:
        return "Scene B Common";
    case 10:
        return "Scene B Osc";
    case 11:
        return "Scene B Osc Mixer";
    case 12:
        return "Scene B Filters";
    case 13:
        return "Scene B Envelopes";
    case 14:
        return "Scene B LFOs";
    }
    return "";
}

void SurgeSynthProcessor::handleNoteOn(MidiKeyboardState *source, int midiChannel,
                                       int midiNoteNumber, float velocity)
{
    if (!isAddingFromMidi)
        midiFromGUI.push(midiR(midiChannel, midiNoteNumber, velocity, true));
}

void SurgeSynthProcessor::handleNoteOff(MidiKeyboardState *source, int midiChannel,
                                        int midiNoteNumber, float velocity)
{
    if (!isAddingFromMidi)
        midiFromGUI.push(midiR(midiChannel, midiNoteNumber, velocity, false));
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgeSynthProcessor(); }
