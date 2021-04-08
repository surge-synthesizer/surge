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

using namespace juce;

//==============================================================================
SurgeSynthProcessor::SurgeSynthProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", AudioChannelSet::stereo(), true)
                         .withOutput("Scene A", AudioChannelSet::stereo(), false)
                         .withOutput("Scene B", AudioChannelSet::stereo(), false))
{
    surge = std::make_unique<SurgeSynthesizer>(this);

    for (auto par : surge->storage.getPatch().param_ptr)
    {
        if (par)
        {
            parametermeta pm;
            surge->getParameterMeta(surge->idForParameter(par), pm);
            // FIXME: use pm.clump to make as set of
            // https://docs.juce.com/master/classAudioProcessorParameterGroup.html
            auto sja = new SurgeParamToJuceParamAdapter(surge.get(), par);
            addParameter(sja);
            paramsByID[surge->idForParameter(par)] = sja;
        }
    }

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
}

void SurgeSynthProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool SurgeSynthProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    // the sidechain can take any layout, the main bus needs to be the same on the input and output
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet() &&
           !layouts.getMainInputChannelSet().isDisabled() &&
           layouts.getMainInputChannelSet() == AudioChannelSet::stereo();
}

#define is_aligned(POINTER, BYTE_COUNT) (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

void SurgeSynthProcessor::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages)
{
    // FIXME obvioulsy
    float thisBPM = 120.0;
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
        surge->time_data.tempo = 120;
        surge->time_data.timeSigNumerator = 4;
        surge->time_data.timeSigDenominator = 4;
        surge->resetStateFromTimeData();
    }

    for (const MidiMessageMetadata it : midiMessages)
    {
        MidiMessage m = it.getMessage();
        if (m.isNoteOn())
        {
            surge->playNote(m.getChannel(), m.getNoteNumber(), m.getVelocity(), 0);
        }
        else if (m.isNoteOff())
        {
            surge->releaseNote(m.getChannel(), m.getNoteNumber(), m.getVelocity());
        }
        else if (m.isChannelPressure())
        {
            surge->channelAftertouch(m.getChannel(), m.getChannelPressureValue());
        }
        else if (m.isAftertouch())
        {
            surge->polyAftertouch(m.getChannel(), m.getNoteNumber(), m.getAfterTouchValue());
        }
        else if (m.isPitchWheel())
        {
            surge->pitchBend(m.getChannel(), m.getPitchWheelValue() - 8192);
        }
        else if (m.isController())
        {
            surge->channelController(m.getChannel(), m.getControllerNumber(),
                                     m.getControllerValue());
        }
        else if (m.isProgramChange())
        {
            // Implement program change in 1.9
        }
        else
        {
            // std::cout << "Ignoring message " << std::endl;
        }
    }

    auto mainInputOutput = getBusBuffer(buffer, true, 0);
    auto sceneAOutput = getBusBuffer(buffer, false, 1);
    auto sceneBOutput = getBusBuffer(buffer, false, 2);
    for (int i = 0; i < buffer.getNumSamples(); i += BUFFER_COPY_CHUNK)
    {
        auto outL = mainInputOutput.getWritePointer(0, i);
        auto outR = mainInputOutput.getWritePointer(1, i);

        surge->time_data.ppqPos += (double)BLOCK_SIZE * surge->time_data.tempo / (60. * samplerate);

        if (blockPos == 0)
        {
            auto inL = mainInputOutput.getReadPointer(0, i);
            auto inR = mainInputOutput.getReadPointer(1, i);
            surge->process_input = true;
            memcpy(&(surge->input[0][0]), inL, BLOCK_SIZE * sizeof(float));
            memcpy(&(surge->input[1][0]), inR, BLOCK_SIZE * sizeof(float));
            surge->process();
        }

        memcpy(outL, &(surge->output[0][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));
        memcpy(outR, &(surge->output[1][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));

        if (surge->activateExtraOutputs && sceneAOutput.getNumChannels() == 2 &&
            sceneBOutput.getNumChannels() == 2)
        {
            auto sAL = sceneAOutput.getWritePointer(0, i);
            auto sAR = sceneAOutput.getWritePointer(1, i);
            auto sBL = sceneBOutput.getWritePointer(0, i);
            auto sBR = sceneBOutput.getWritePointer(1, i);

            if (sAL && sAR)
            {
                memcpy(sAL, &(surge->sceneout[0][0][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));
                memcpy(sAR, &(surge->sceneout[0][1][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));
            }
            if (sBL && sBR)
            {
                memcpy(sBL, &(surge->sceneout[1][0][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));
                memcpy(sBR, &(surge->sceneout[1][1][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));
            }
        }

        blockPos += BUFFER_COPY_CHUNK;

        if (blockPos >= BLOCK_SIZE)
            blockPos = 0;
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
    spar->setValueNotifyingHost(f);
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgeSynthProcessor(); }
