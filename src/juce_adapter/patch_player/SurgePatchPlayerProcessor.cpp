/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

/*
** TODOS
** - MPE, polyphony control
** - Tuning
** - The macros exposed as DAW parameters at least, but maybe as UI parameters?
** - Midi Activity
** - Write UserInteractionsJuce
*/

#include "SurgePatchPlayerProcessor.h"
#include "SurgePatchPlayerEditor.h"
#include "JuceAdapterToSurgeSynth.h"

//==============================================================================
SurgePatchPlayerAudioProcessor::SurgePatchPlayerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true )
                       )
#endif
{
   proxy = std::make_unique<JUCEPluginLayerProxy>();
   surge = std::make_unique<SurgeSynthesizer>( proxy.get() );
}

SurgePatchPlayerAudioProcessor::~SurgePatchPlayerAudioProcessor()
{
}

//==============================================================================
const juce::String SurgePatchPlayerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SurgePatchPlayerAudioProcessor::acceptsMidi() const
{
   return true;
}

bool SurgePatchPlayerAudioProcessor::producesMidi() const
{
   return false;
}

bool SurgePatchPlayerAudioProcessor::isMidiEffect() const
{
   return false;
}

double SurgePatchPlayerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SurgePatchPlayerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SurgePatchPlayerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SurgePatchPlayerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SurgePatchPlayerAudioProcessor::getProgramName (int index)
{
    return {};
}

void SurgePatchPlayerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SurgePatchPlayerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
   surge->setSamplerate(sampleRate);
}

void SurgePatchPlayerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SurgePatchPlayerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   // This is the place where you check if the layout is supported.
   // In this template code we only support mono or stereo.
   if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
       && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
      return false;
   
   // This checks if the input layout matches the output layout
   return true;
}
#endif

void SurgePatchPlayerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto *playHead = getPlayHead();
    Surge::JuceShared::playheadToTimeData< juce::AudioPlayHead, SurgeSynthesizer >( playHead, surge.get(), buffer.getNumSamples() );
   
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This should really interleave with the loop below
    for( const auto &mmd : midiMessages )
    {
       auto msg = mmd.getMessage();
       Surge::JuceShared::messageToSynth<juce::MidiMessage, SurgeSynthesizer>( msg, surge.get() );
    }

    auto mainOutput = getBusBuffer(buffer, false, 0);

    // Obviously we could do this block wise if we didn't want to interleave
    auto outL = mainOutput.getWritePointer(0, 0);
    auto outR = mainOutput.getWritePointer(1, 0);

    for( int i=0; i<buffer.getNumSamples(); ++i )
    {
       if( blockpos == 0 )
       {
          surge->process();
       }
       outL[i] = surge->output[0][blockpos];
       outR[i] = surge->output[1][blockpos];
       blockpos ++;
       if (blockpos >= BLOCK_SIZE)
         blockpos = 0;
    }

    vu[0] = surge->vu_peak[0];
    vu[1] = surge->vu_peak[1];
}

//==============================================================================
bool SurgePatchPlayerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SurgePatchPlayerAudioProcessor::createEditor()
{
    return new SurgePatchPlayerAudioProcessorEditor (*this);
}

//==============================================================================
void SurgePatchPlayerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SurgePatchPlayerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SurgePatchPlayerAudioProcessor();
}
