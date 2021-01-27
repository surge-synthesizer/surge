/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "DebugHelpers.h"

using namespace juce;

//==============================================================================
SurgeSynthProcessor::SurgeSynthProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", AudioChannelSet::stereo(), true))
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
    if (index != 0)
    {
        std::cout << "Setting patchid_queue to " << presetOrderToPatchList[index - 1] << std::endl;
        surge->patchid_queue = presetOrderToPatchList[index - 1];
    }
}

const String SurgeSynthProcessor::getProgramName(int index)
{
    if (index == 0)
        return "INIT OR DROPPED";
    index--;
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
    for (const MidiMessageMetadata it : midiMessages)
    {
        MidiMessage m = it.getMessage();
        if (m.isNoteOn())
        {
            surge->playNote(0, m.getNoteNumber(), m.getVelocity(), 0);
        }
        else if (m.isNoteOff())
        {
            surge->releaseNote(0, m.getNoteNumber(), m.getVelocity());
        }
    }

    auto mainInputOutput = getBusBuffer(buffer, false, 0);

    for (int i = 0; i < buffer.getNumSamples(); i += BUFFER_COPY_CHUNK)
    {
        auto outL = mainInputOutput.getWritePointer(0, i);
        auto outR = mainInputOutput.getWritePointer(1, i);

        if (blockPos == 0)
            surge->process();

        memcpy(outL, &(surge->output[0][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));
        memcpy(outR, &(surge->output[1][blockPos]), BUFFER_COPY_CHUNK * sizeof(float));

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
    std::unique_ptr<XmlElement> xml(new XmlElement("surgesynth"));
    copyXmlToBinary(*xml, destData);
}

void SurgeSynthProcessor::setStateInformation(const void *data, int sizeInBytes) {}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgeSynthProcessor(); }
