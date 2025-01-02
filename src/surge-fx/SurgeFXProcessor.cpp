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

#include "SurgeFXProcessor.h"
#include "FXOpenSoundControl.h"
#include "SurgeFXEditor.h"
#include "DebugHelpers.h"
#include "UserDefaults.h"
#include <fmt/core.h>

#if LINUX
// getCurrentPosition is deprecated in J7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

//==============================================================================
SurgefxAudioProcessor::SurgefxAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true))
{
    auto cfg = SurgeStorage::SurgeStorageConfig::fromDataPath("");
    cfg.createUserDirectory = false;
    cfg.scanWavetableAndPatches = false;

    storage.reset(new SurgeStorage(cfg));
    storage->userDefaultsProvider->addOverride(Surge::Storage::HighPrecisionReadouts, false);

    nonLatentBlockMode = !juce::PluginHostType().isFruityLoops();
    nonLatentBlockMode = Surge::Storage::getUserDefaultValue(
        storage.get(), Surge::Storage::FXUnitAssumeFixedBlock, nonLatentBlockMode);

    setLatencySamples(nonLatentBlockMode ? 0 : BLOCK_SIZE);

    effectNum = fxt_off;

    fxstorage = &(storage->getPatch().fx[0]);
    audio_thread_surge_effect.reset();
    resetFxType(effectNum, false);
    fxstorage->return_level.id = -1;
    setupStorageRanges(&(fxstorage->type), &(fxstorage->p[n_fx_params - 1]));

    for (int i = 0; i < n_fx_params; ++i)
    {
        std::string lb, nm;
        lb = fmt::format("fx_parm_{:d}", i);
        nm = fmt::format("FX Parameter {:d}", i);

        addParameter(fxParams[i] = new float_param_t(
                         juce::ParameterID(lb, 1), nm, juce::NormalisableRange<float>(0.0, 1.0),
                         fxstorage->p[fx_param_remap[i]].get_value_f01()));
        fxParams[i]->getTextHandler = [this, i](float f, int len) -> juce::String {
            return juce::String(getParamValueFor(i, f)).substring(0, len);
        };
        fxParams[i]->getTextToValue = [this, i](const juce::String &s) -> float {
            return getParameterValueForString(i, s.toStdString());
        };
        fxBaseParams[i] = fxParams[i];
    }

    addParameter(fxType = new int_param_t(juce::ParameterID("fxtype", 1), "FX Type", fxt_delay,
                                          n_fx_types - 1, effectNum));
    fxType->getTextHandler = [this](float f, int len) -> juce::String {
        auto i = 1 + (int)round(f * (n_fx_types - 2));
        if (i >= 1 && i < n_fx_types)
            return fx_type_names[i];
        return "";
    };

    fxType->getTextToValue = [](const juce::String &s) -> float { return 0; };
    fxBaseParams[n_fx_params] = fxType;

    for (int i = 0; i < n_fx_params; ++i)
    {
        // FIXME - is this even used here?!
        std::string lb, nm;
        lb = fmt::format("fx_temposync_{:d}", i);
        nm = fmt::format("Feature Param {:d}", i);

        paramFeatures[i] = paramFeatureFromParam(&(fxstorage->p[fx_param_remap[i]]));
    }

    for (int i = 0; i < n_fx_params + 1; ++i)
    {
        fxBaseParams[i]->addListener(this);
        changedParams[i] = false;
        isUserEditing[i] = false;
    }

    for (int i = 0; i < n_fx_params; ++i)
    {
        wasParamFeatureChanged[i] = false;
    }

    paramChangeListener = []() {};
    resettingFx = false;

    oscHandler.initOSC(this, storage);
}

SurgefxAudioProcessor::~SurgefxAudioProcessor() {}

//==============================================================================
const juce::String SurgefxAudioProcessor::getName() const { return JucePlugin_Name; }

bool SurgefxAudioProcessor::acceptsMidi() const { return false; }

bool SurgefxAudioProcessor::producesMidi() const { return false; }

bool SurgefxAudioProcessor::isMidiEffect() const { return false; }

double SurgefxAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int SurgefxAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int SurgefxAudioProcessor::getCurrentProgram() { return 0; }

void SurgefxAudioProcessor::setCurrentProgram(int index) {}

const juce::String SurgefxAudioProcessor::getProgramName(int index) { return "Default"; }

void SurgefxAudioProcessor::changeProgramName(int index, const juce::String &newName) {}

void SurgefxAudioProcessor::tryLazyOscStartupFromStreamedState()
{
    if ((!oscHandler.listening && oscStartIn && oscPortIn > 0))
    {
        oscHandler.tryOSCStartup();
    }
    oscCheckStartup = false;
}

//==============================================================================
/* OSC (Open Sound Control) */
bool SurgefxAudioProcessor::initOSCIn(int port)
{
    if (port <= 0)
    {
        return false;
    }

    auto state = oscHandler.initOSCIn(port);

    oscReceiving.store(state);
    oscStartIn = true;

    return state;
}

bool SurgefxAudioProcessor::changeOSCInPort(int new_port)
{
    oscReceiving.store(false);
    oscHandler.stopListening();
    return initOSCIn(new_port);
}

void SurgefxAudioProcessor::initOSCError(int port, std::string outIP)
{
    std::ostringstream msg;

    msg << "Surge XT was unable to connect to OSC port " << port;
    if (!outIP.empty())
    {
        msg << " at IP Address " << outIP;
    }

    msg << ".\n"
        << "Either it is not a valid port, or it is already used by Surge XT or another "
           "application.";

    storage->reportError(msg.str(), "OSC Initialization Error");
};

//==============================================================================
void SurgefxAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    storage->setSamplerate(sr);
    storage->songpos = 0.;

    setLatencySamples(nonLatentBlockMode ? 0 : BLOCK_SIZE);

    if (effectNum == fxt_off)
    {
        resetFxType(fxt_delay, true);
    }
}

void SurgefxAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void SurgefxAudioProcessor::reset()
{
    if (surge_effect)
    {
        surge_effect->init();
    }
}

bool SurgefxAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    bool inputValid = layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
                      layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();

    bool outputValid = layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() ||
                       layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();

    bool sidechainValid = layouts.getChannelSet(true, 1).isDisabled() ||
                          layouts.getChannelSet(true, 1) == juce::AudioChannelSet::stereo();

    return inputValid && outputValid && sidechainValid;
}

#define is_aligned(POINTER, BYTE_COUNT) (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

void SurgefxAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                         juce::MidiBuffer &midiMessages)
{
    audioRunning = true;
    if (resettingFx || !surge_effect)
        return;

    if (oscCheckStartup)
    {
        tryLazyOscStartupFromStreamedState();
    }

    juce::ScopedNoDenormals noDenormals;

    float thisBPM = 120.0;
    bool havePlayhead{false};
    auto playhead = getPlayHead();
    if (playhead)
    {
        juce::AudioPlayHead::CurrentPositionInfo cp;
        playhead->getCurrentPosition(cp);
        thisBPM = cp.bpm;
        havePlayhead = true;
    }

    if (storage && thisBPM != lastBPM)
    {
        lastBPM = thisBPM;
        storage->temposyncratio = thisBPM / 120.0;
        storage->temposyncratio_inv = 1.f / storage->temposyncratio;
    }

    if (surge_effect->checkHasInvalidatedUI())
    {
        resetFxParams(true);
    }

    auto sampl = buffer.getNumSamples();
    if (nonLatentBlockMode && ((sampl & ~(BLOCK_SIZE - 1)) != sampl))
    {
        nonLatentBlockMode = false;
        input_position = 0;
        output_position = 0;
        memset(output_buffer, 0, sizeof(output_buffer));
        memset(input_buffer, 0, sizeof(input_buffer));
        memset(sidechain_buffer, 0, sizeof(sidechain_buffer));

        setLatencySamples(BLOCK_SIZE);
        updateHostDisplay(ChangeDetails().withLatencyChanged(true));
    }

    auto mib = getBus(true, 0);
    if (mib->isEnabled() && !(mib->getNumberOfChannels() == 1 || mib->getNumberOfChannels() == 2))
    {
        m_audioValid = false;
        m_audioValidMessage = "Enabled Input is neither mono nor stereo";
        return;
    }
    if (!mib->isEnabled())
    {
        m_audioValid = false;
        m_audioValidMessage = "Input is not enabled";
        return;
    }
    auto mob = getBus(false, 0);
    if (mob->isEnabled() && (mob->getNumberOfChannels() != 2))
    {
        m_audioValid = false;
        m_audioValidMessage =
            "Enabled Output is not stereo " + std::to_string(mib->getNumberOfChannels());
        return;
    }
    if (!mob->isEnabled())
    {
        m_audioValid = false;
        m_audioValidMessage = "Output is not enabled";
        return;
    }
    m_audioValid = true;

    auto mainInput = getBusBuffer(buffer, true, 0);

    int inChanL = 0;
    int inChanR = 1;
    if (mainInput.getNumChannels() == 1)
        inChanR = 0;

    auto mainOutput = getBusBuffer(buffer, false, 0);
    auto sideChainInput = getBusBuffer(buffer, true, 1);

    // FIXME: Check: has type changed?
    int pt = *fxType;

    if (effectNum != pt)
    {
        effectNum = pt;
        resetFxType(effectNum);
    }

    if (audio_thread_surge_effect.get() != surge_effect.get())
    {
        audio_thread_surge_effect = surge_effect;
    }

    if (nonLatentBlockMode)
    {
        auto sideChainBus = getBus(true, 1);

        for (int outPos = 0; outPos < buffer.getNumSamples() && !resettingFx; outPos += BLOCK_SIZE)
        {
            auto outL = mainOutput.getWritePointer(0, outPos);
            auto outR = mainOutput.getWritePointer(1, outPos);

            if ((effectNum == fxt_vocoder || effectNum == fxt_ringmod) && sideChainBus &&
                sideChainBus->isEnabled())
            {
                auto sideL = sideChainInput.getReadPointer(0, outPos);
                auto sideR = sideChainInput.getReadPointer(1, outPos);

                memcpy(storage->audio_in_nonOS[0], sideL, BLOCK_SIZE * sizeof(float));
                memcpy(storage->audio_in_nonOS[1], sideR, BLOCK_SIZE * sizeof(float));

                if (effectNum == fxt_ringmod)
                {
                    halfbandIN.process_block_U2(storage->audio_in_nonOS[0],
                                                storage->audio_in_nonOS[1], storage->audio_in[0],
                                                storage->audio_in[1], BLOCK_SIZE_OS);
                }
            }

            for (int i = 0; i < n_fx_params; ++i)
            {
                fxstorage->p[fx_param_remap[i]].set_value_f01(*fxParams[i]);
                paramFeatureOntoParam(&(fxstorage->p[fx_param_remap[i]]), paramFeatures[i]);
            }
            copyGlobaldataSubset(storage_id_start, storage_id_end);

            auto inL = mainInput.getReadPointer(inChanL, outPos);
            auto inR = mainInput.getReadPointer(inChanR, outPos);

            if (is_aligned(outL, 16) && is_aligned(outR, 16) && inL == outL && inR == outR)
            {
                audio_thread_surge_effect->process_ringout(outL, outR, true);
            }
            else
            {
                float bufferL alignas(16)[BLOCK_SIZE], bufferR alignas(16)[BLOCK_SIZE];

                memcpy(bufferL, inL, BLOCK_SIZE * sizeof(float));
                memcpy(bufferR, inR, BLOCK_SIZE * sizeof(float));

                audio_thread_surge_effect->process_ringout(bufferL, bufferR, true);

                memcpy(outL, bufferL, BLOCK_SIZE * sizeof(float));
                memcpy(outR, bufferR, BLOCK_SIZE * sizeof(float));
            }
        }
    }
    else
    {
        auto outL = mainOutput.getWritePointer(0, 0);
        auto outR = mainOutput.getWritePointer(1, 0);

        auto inL = mainInput.getReadPointer(inChanL, 0);
        auto inR = mainInput.getReadPointer(inChanR, 0);

        const float *sideL = nullptr, *sideR = nullptr;

        auto sideChainBus = getBus(true, 1);

        if (effectNum == fxt_vocoder && sideChainBus && sideChainBus->isEnabled())
        {
            sideL = sideChainInput.getReadPointer(0, 0);
            sideR = sideChainInput.getReadPointer(1, 0);
        }

        for (int smp = 0; smp < buffer.getNumSamples(); smp++)
        {
            input_buffer[0][input_position] = inL[smp];
            input_buffer[1][input_position] = inR[smp];
            if (effectNum == fxt_vocoder && sideL && sideR)
            {
                sidechain_buffer[0][input_position] = sideL[smp];
                sidechain_buffer[1][input_position] = sideR[smp];
            }
            else
            {
                sidechain_buffer[0][input_position] = 0.f;
                sidechain_buffer[1][input_position] = 0.f;
            }
            input_position++;

            if (input_position == BLOCK_SIZE)
            {
                memcpy(storage->audio_in_nonOS[0], sidechain_buffer[0], BLOCK_SIZE * sizeof(float));
                memcpy(storage->audio_in_nonOS[1], sidechain_buffer[1], BLOCK_SIZE * sizeof(float));

                for (int i = 0; i < n_fx_params; ++i)
                {
                    fxstorage->p[fx_param_remap[i]].set_value_f01(*fxParams[i]);
                    paramFeatureOntoParam(&(fxstorage->p[fx_param_remap[i]]), paramFeatures[i]);
                }
                copyGlobaldataSubset(storage_id_start, storage_id_end);

                audio_thread_surge_effect->process_ringout(input_buffer[0], input_buffer[1], true);
                memcpy(output_buffer, input_buffer, 2 * BLOCK_SIZE * sizeof(float));
                input_position = 0;
                output_position = 0;
            }

            if (output_position >= 0 && output_position < BLOCK_SIZE) // that < should never happen
            {
                outL[smp] = output_buffer[0][output_position];
                outR[smp] = output_buffer[1][output_position];
                output_position++;
            }
            else
            {
                outL[smp] = 0;
                outR[smp] = 0;
            }
        }
    }

    // While chasing that mac error put this in
    bool doHardClip{true};
    if (doHardClip)
    {
        auto outL = mainOutput.getWritePointer(0, 0);
        auto outR = mainOutput.getWritePointer(1, 0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            outL[i] = std::clamp(outL[i], -2.f, 2.f);
            outR[i] = std::clamp(outR[i], -2.f, 2.f);
        }
    }

    if (oscReceiving.load())
        processBlockOSC();
}

// Pull incoming OSC events from ring buffer
void SurgefxAudioProcessor::processBlockOSC()
{
    while (true)
    {
        auto om = oscRingBuf.pop();
        if (!om.has_value())
            break;
        switch (om->type)
        {
        case SurgefxAudioProcessor::FX_PARAM:
        {
            prepareParametersAbsentAudio();
            setFXParamValue01(om->p_index, om->fval);
            // this order does matter
            changedParamsValue[om->p_index] = om->fval;
            changedParams[om->p_index] = true;
            triggerAsyncUpdate();
        }
        break;
        }
    }
}

//==============================================================================
bool SurgefxAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *SurgefxAudioProcessor::createEditor()
{
    return new SurgefxAudioProcessorEditor(*this);
}

//==============================================================================
void SurgefxAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    std::unique_ptr<juce::XmlElement> xml(new juce::XmlElement("surgefx"));
    xml->setAttribute("streamingVersion", (int)2);

    for (int i = 0; i < n_fx_params; ++i)
    {
        juce::String nm = fmt::format("fxp_{:d}", i);
        float val = *(fxParams[i]);

        xml->setAttribute(nm, val);

        auto &spar = fxstorage->p[fx_param_remap[i]];
        nm = fmt::format("surgevaltype_{:d}", i);

        xml->setAttribute(nm, spar.valtype);

        nm = fmt::format("surgeval_{:d}", i);

        switch (spar.valtype)
        {
        case vt_bool:
            xml->setAttribute(nm, spar.val.b);
            break;
        case vt_int:
            if (spar.ctrltype == ct_none)
            {
                xml->setAttribute(nm, 0);
            }
            else
            {
                xml->setAttribute(nm, spar.val.i);
            }
            break;
        default:
        case vt_float:
            xml->setAttribute(nm, spar.val.f);
            break;
        }

        nm = fmt::format("fxp_param_features_{:d}", i);

        int pf = paramFeatureFromParam(&(fxstorage->p[fx_param_remap[i]]));
        xml->setAttribute(nm, pf);
    }

    xml->setAttribute("fxt", effectNum);
    xml->setAttribute("oscpin", oscPortIn);
    xml->setAttribute("oscin", oscStartIn);

    copyXmlToBinary(*xml, destData);
}

void SurgefxAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        int paramFeaturesCache[n_fx_params];
        if (xmlState->hasTagName("surgefx"))
        {
            auto streamingVersion = xmlState->getIntAttribute("streamingVersion", (int)2);
            if (streamingVersion > 2 || streamingVersion < 0)
                streamingVersion = 1; // assume some corrupted ancient session

            effectNum = xmlState->getIntAttribute("fxt", fxt_delay);
            resetFxType(effectNum, false);

            oscPortIn = xmlState->getIntAttribute("oscpin", 0);
            oscStartIn = xmlState->getBoolAttribute("oscin", false);
            // start OSC, if variables merit it
            oscCheckStartup = true;

            for (int i = 0; i < n_fx_params; ++i)
            {
                std::string nm;

                if (streamingVersion == 1)
                {
                    nm = fmt::format("fxp_{:d}", i);
                    float v = xmlState->getDoubleAttribute(nm, 0.0);
                    fxstorage->p[fx_param_remap[i]].set_value_f01(v);
                }
                else if (streamingVersion == 2)
                {
                    auto &spar = fxstorage->p[fx_param_remap[i]];
                    nm = fmt::format("fxp_{:d}", i);
                    float v = xmlState->getDoubleAttribute(nm, 0.0);

                    nm = fmt::format("surgevaltype_{:d}", i);
                    int type = xmlState->getIntAttribute(nm, vt_float);

                    nm = fmt::format("surgeval_{:d}", i);

                    if (type == vt_int && xmlState->hasAttribute(nm))
                    {
                        // Unstream the int as an int. Bools and FLoats are
                        // fine since they will 01 consistently and properly but
                        // this means things like add an airwindow and we survive
                        // going forward
                        int ival = xmlState->getIntAttribute(nm, 0);
                        spar.val.i = ival;
                    }
                    else
                    {
                        spar.set_value_f01(v);
                    }
                }

                // Legacy unstream temposync
                nm = fmt::format("fxp_temposync_{:d}", i);

                if (xmlState->hasAttribute(nm))
                {
                    bool b = xmlState->getBoolAttribute(nm, false);
                    fxstorage->p[fx_param_remap[i]].temposync = b;
                }

                // Modern unstream parameters
                nm = fmt::format("fxp_param_features_{:d}", i);

                if (xmlState->hasAttribute(nm))
                {
                    int pf = xmlState->getIntAttribute(nm, 0);
                    paramFeaturesCache[i] = pf;
                }
            }

            resetFxParams(true);

            for (int i = 0; i < n_fx_params; ++i)
            {
                paramFeatureOntoParam(&(fxstorage->p[fx_param_remap[i]]), paramFeaturesCache[i]);
            }

            updateJuceParamsFromStorage();
        }
    }
}

void SurgefxAudioProcessor::reorderSurgeParams()
{
    if (surge_effect.get())
    {
        for (auto i = 0; i < n_fx_params; ++i)
            fx_param_remap[i] = i;

        std::vector<std::pair<int, int>> orderTrack;
        for (auto i = 0; i < n_fx_params; ++i)
        {
            if (fxstorage->p[i].posy_offset && fxstorage->p[i].ctrltype != ct_none)
            {
                orderTrack.push_back(std::pair<int, int>(i, i * 2 + fxstorage->p[i].posy_offset));
            }
            else
            {
                orderTrack.push_back(std::pair<int, int>(i, 10000));
            }
        }
        std::sort(orderTrack.begin(), orderTrack.end(),
                  [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
                      return a.second < b.second;
                  });

        int idx = 0;
        for (auto a : orderTrack)
        {
            fx_param_remap[idx++] = a.first;
        }
    }
    else
    {
        for (int i = 0; i < n_fx_params; ++i)
        {
            fx_param_remap[i] = i;
        }
    }

    // I hate having to use this API so much...
    for (auto i = 0; i < n_fx_params; ++i)
    {
        if (fxstorage->p[fx_param_remap[i]].ctrltype == ct_none)
        {
            group_names[i] = "-";
        }
        else
        {
            int fpos = fxstorage->p[fx_param_remap[i]].posy / 10 +
                       fxstorage->p[fx_param_remap[i]].posy_offset;
            for (auto j = 0; j < n_fx_params && surge_effect->group_label(j); ++j)
            {
                if (surge_effect->group_label(j) &&
                    surge_effect->group_label_ypos(j) <= fpos // constants for SurgeGUIEditor. Sigh.
                )
                {
                    group_names[i] = surge_effect->group_label(j);
                }
            }
        }
    }
}

void SurgefxAudioProcessor::resetFxType(int type, bool updateJuceParams)
{
    resettingFx = true;
    input_position = 0;
    output_position = -1;
    effectNum = type;
    fxstorage->type.val.i = effectNum;

    for (int i = 0; i < n_fx_params; ++i)
        fxstorage->p[i].set_type(ct_none);

    surge_effect.reset(spawn_effect(effectNum, storage.get(), &(storage->getPatch().fx[0]),
                                    storage->getPatch().globaldata));
    if (surge_effect)
    {
        copyGlobaldataSubset(storage_id_start, storage_id_end);

        surge_effect->init();
        surge_effect->init_ctrltypes();
        surge_effect->init_default_values();
    }
    resetFxParams(updateJuceParams);
}

void SurgefxAudioProcessor::resetFxParams(bool updateJuceParams)
{
    reorderSurgeParams();

    /*
    ** TempoSync etc settings may linger so whack them all to false again
    */
    for (int i = 0; i < n_fx_params; ++i)
        paramFeatureOntoParam(&(fxstorage->p[i]), 0);

    if (updateJuceParams)
    {
        updateJuceParamsFromStorage();
    }

    updateHostDisplay();
    resettingFx = false;
}

void SurgefxAudioProcessor::updateJuceParamsFromStorage()
{
    SupressGuard sg(&supressParameterUpdates);
    for (int i = 0; i < n_fx_params; ++i)
    {
        *(fxParams[i]) = fxstorage->p[fx_param_remap[i]].get_value_f01();
        fxParams[i]->mutableName = getParamGroup(i) + " " + getParamName(i);
        paramFeatures[i] = paramFeatureFromParam(&(fxstorage->p[fx_param_remap[i]]));
    }
    *(fxType) = effectNum;

    for (int i = 0; i < n_fx_params; ++i)
    {
        changedParamsValue[i] = fxstorage->p[fx_param_remap[i]].get_value_f01();
        changedParams[i] = true;
    }
    changedParamsValue[n_fx_params] = effectNum;
    changedParams[n_fx_params] = true;

    triggerAsyncUpdate();
}

float SurgefxAudioProcessor::getParameterValueForString(int i, const std::string &s)
{
    if (!canSetParameterByString(i))
        return 0;

    auto *p = &(fxstorage->p[fx_param_remap[i]]);

    pdata v;
    // TODO: range error reporting
    std::string errMsg;
    auto res = p->set_value_from_string_onto(s, v, errMsg);
    if (res)
    {
        return p->value_to_normalized(v.f);
    }
    else
    {
        return 0;
    }
}
void SurgefxAudioProcessor::setParameterByString(int i, const std::string &s)
{
    auto *p = &(fxstorage->p[fx_param_remap[i]]);
    // TODO: range error reporting
    std::string errMsg;
    p->set_value_from_string(s, errMsg);
    *(fxParams[i]) = fxstorage->p[fx_param_remap[i]].get_value_f01();
    changedParamsValue[i] = fxstorage->p[fx_param_remap[i]].get_value_f01();
    triggerAsyncUpdate();
}

bool SurgefxAudioProcessor::canSetParameterByString(int i)
{
    auto *p = &(fxstorage->p[fx_param_remap[i]]);
    return p->can_setvalue_from_string();
}

void SurgefxAudioProcessor::copyGlobaldataSubset(int start, int end)
{
    for (int i = start; i < end; ++i)
    {
        storage->getPatch().globaldata[i].i = storage->getPatch().param_ptr[i]->val.i;
    }
}

void SurgefxAudioProcessor::setupStorageRanges(Parameter *start, Parameter *endIncluding)
{
    int min_id = 100000, max_id = -1;
    Parameter *oap = start;
    while (oap <= endIncluding)
    {
        if (oap->id >= 0)
        {
            if (oap->id > max_id)
                max_id = oap->id;
            if (oap->id < min_id)
                min_id = oap->id;
        }
        oap++;
    }

    storage_id_start = min_id;
    storage_id_end = max_id + 1;
}

void SurgefxAudioProcessor::prepareParametersAbsentAudio()
{
    if (!audioRunning)
    {
        if (getEffectType() == fxt_airwindows)
        {
            /*
             * Airwindows needs to set up its internal state with a process
             * See #6897
             */
            float dL alignas(16)[BLOCK_SIZE], dR alignas(16)[BLOCK_SIZE];
            memset(dL, 0, sizeof(dL));
            memset(dR, 0, sizeof(dR));
            surge_effect->process_ringout(dL, dR);
        }
    }
}
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgefxAudioProcessor(); }
