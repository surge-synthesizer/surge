/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "SurgeFXProcessor.h"
#include "SurgeFXEditor.h"
#include "DebugHelpers.h"
#include "UserDefaults.h"

//==============================================================================
SurgefxAudioProcessor::SurgefxAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true))
{
    storage.reset(new SurgeStorage());
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
    setupStorageRanges((Parameter *)fxstorage, &(fxstorage->p[n_fx_params - 1]));

    for (int i = 0; i < n_fx_params; ++i)
    {
        char lb[256], nm[256];
        snprintf(lb, 256, "fx_parm_%d", i);
        snprintf(nm, 256, "FX Parameter %d", i);

        addParameter(fxParams[i] =
                         new float_param_t(lb, nm, juce::NormalisableRange<float>(0.0, 1.0),
                                           fxstorage->p[fx_param_remap[i]].get_value_f01()));
        fxParams[i]->getTextHandler = [this, i](float f, int len) -> juce::String {
            return juce::String(getParamValueFor(i, f)).substring(0, len);
        };
        fxParams[i]->getTextToValue = [this, i](const juce::String &s) -> float {
            return getParameterValueForString(i, s.toStdString());
        };
        fxBaseParams[i] = fxParams[i];
    }

    addParameter(fxType =
                     new int_param_t("fxtype", "FX Type", fxt_delay, n_fx_types - 1, effectNum));
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
        char lb[256], nm[256];
        snprintf(lb, 256, "fx_temposync_%d", i);
        snprintf(nm, 256, "Feature Param %d", i);

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

bool SurgefxAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    bool inputValid = layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
                      layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();

    bool outputValid = layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();

    bool sidechainValid = layouts.getChannelSet(true, 1).isDisabled() ||
                          layouts.getChannelSet(true, 1) == juce::AudioChannelSet::stereo();

    return inputValid && outputValid && sidechainValid;
}

#define is_aligned(POINTER, BYTE_COUNT) (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

void SurgefxAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                         juce::MidiBuffer &midiMessages)
{
    if (resettingFx || !surge_effect)
        return;

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

            if (effectNum == fxt_vocoder && sideChainBus && sideChainBus->isEnabled())
            {
                auto sideL = sideChainInput.getReadPointer(0, outPos);
                auto sideR = sideChainInput.getReadPointer(1, outPos);

                memcpy(storage->audio_in_nonOS[0], sideL, BLOCK_SIZE * sizeof(float));
                memcpy(storage->audio_in_nonOS[1], sideR, BLOCK_SIZE * sizeof(float));
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
        char nm[256];
        snprintf(nm, 256, "fxp_%d", i);
        float val = *(fxParams[i]);
        xml->setAttribute(nm, val);

        auto &spar = fxstorage->p[fx_param_remap[i]];
        snprintf(nm, 256, "surgevaltype_%d", i);
        xml->setAttribute(nm, spar.valtype);

        snprintf(nm, 256, "surgeval_%d", i);

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
        snprintf(nm, 256, "fxp_param_features_%d", i);
        int pf = paramFeatureFromParam(&(fxstorage->p[fx_param_remap[i]]));
        xml->setAttribute(nm, pf);
    }
    xml->setAttribute("fxt", effectNum);

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

            for (int i = 0; i < n_fx_params; ++i)
            {
                char nm[256];
                if (streamingVersion == 1)
                {
                    snprintf(nm, 256, "fxp_%d", i);
                    float v = xmlState->getDoubleAttribute(nm, 0.0);
                    fxstorage->p[fx_param_remap[i]].set_value_f01(v);
                }
                else if (streamingVersion == 2)
                {
                    auto &spar = fxstorage->p[fx_param_remap[i]];
                    snprintf(nm, 256, "fxp_%d", i);
                    float v = xmlState->getDoubleAttribute(nm, 0.0);

                    snprintf(nm, 256, "surgevaltype_%d", i);
                    int type = xmlState->getIntAttribute(nm, vt_float);

                    snprintf(nm, 256, "surgeval_%d", i);

                    if (type == vt_int && xmlState->hasAttribute(nm))
                    {
                        // Unstream the int as an int. Bools and FLoats are
                        // fine since they will 01 consisntently and properly but
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
                snprintf(nm, 256, "fxp_temposync_%d", i);
                if (xmlState->hasAttribute(nm))
                {
                    bool b = xmlState->getBoolAttribute(nm, false);
                    fxstorage->p[fx_param_remap[i]].temposync = b;
                }

                // Modern unstream parameters
                snprintf(nm, 256, "fxp_param_features_%d", i);
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
    p->set_value_from_string_onto(s, v, errMsg);
    return v.f;
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

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgefxAudioProcessor(); }
