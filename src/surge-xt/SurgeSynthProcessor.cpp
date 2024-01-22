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

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "DebugHelpers.h"
#include "version.h"
#include "sst/plugininfra/cpufeatures.h"
#include "globals.h"
#include "UserDefaults.h"
#include "UnitConversions.h"
#include <any>

#if LINUX
// getCurrentPosition is deprecated in J7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

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
        wrapperTypeString = std::string("CLAP");
#endif

#if BUILD_IS_DEBUG
    std::ostringstream oss;
    oss << "Surge XT " << wrapperTypeString << "\n"
        << "  - Version      : " << Surge::Build::FullVersionStr << " with JUCE " << std::hex
        << JUCE_VERSION << std::dec << "\n"
        << "  - Build Info   : " << Surge::Build::BuildDate << " " << Surge::Build::BuildTime
        << " using " << Surge::Build::BuildCompiler << "\n";
#endif

    try
    {
        surge = std::make_unique<SurgeSynthesizer>(this);
    }
    catch (const std::exception &e)
    {
        surge.reset();
        fatalErrorMessage = e.what();
        return;
    }

#if BUILD_IS_DEBUG
    oss << "  - Data         : " << surge->storage.datapath.u8string() << "\n"
        << "  - User Data    : " << surge->storage.userDataPath.u8string() << std::endl;
    DBG(oss.str());
#endif

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

    memset(inputLatentBuffer, 0, sizeof(inputLatentBuffer));

    surge->hostProgram = juce::PluginHostType().getHostDescription();
    surge->juceWrapperType = wrapperTypeString;

    midiKeyboardState.addListener(this);

#if SURGE_HAS_OSC
    oscHandler.initOSC(this, surge);
#endif
}

SurgeSynthProcessor::~SurgeSynthProcessor()
{
    if (!surge)
    {
        return;
    }

    if (oscHandler.listening)
    {
        oscHandler.stopListening(false);
    }

    // This also deletes the patch change -> OSC out listener
    stopOSCOut(false);
}

//==============================================================================
const juce::String SurgeSynthProcessor::getName() const { return JucePlugin_Name; }

bool SurgeSynthProcessor::acceptsMidi() const { return true; }

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

/* OSC (Open Sound Control) */
bool SurgeSynthProcessor::initOSCIn(int port)
{
    if (port <= 0)
    {
        return false;
    }

    auto state = oscHandler.initOSCIn(port);

    surge->storage.oscListenerRunning = state;
    surge->storage.oscStartIn = true;

    return state;
}

bool SurgeSynthProcessor::changeOSCInPort(int new_port)
{
    if (oscHandler.listening)
    {
        surge->storage.oscListenerRunning = false;
        oscHandler.stopListening();
    }

    return initOSCIn(new_port);
}

bool SurgeSynthProcessor::initOSCOut(int port, std::string ipaddr)
{
    if (port <= 0)
    {
        return false;
    }

    auto state = oscHandler.initOSCOut(port, ipaddr);

    surge->storage.oscSending = state;
    surge->storage.oscStartOut = true;

    return state;
}

void SurgeSynthProcessor::stopOSCOut(bool updateOSCStartInStorage)
{
    oscHandler.stopSending(updateOSCStartInStorage);
}

void SurgeSynthProcessor::initOSCError(int port, std::string outIP)
{
    std::ostringstream msg;

    msg << "Surge XT was unable to connect to OSC port " << port;
    if (outIP != "")
    {
        msg << "; at IP Address " << outIP;
    }

    msg << "\n"
        << "Either it is not a valid port, or it may be in use by another application.";

    surge->storage.reportError(msg.str(), "OSC Initialization Error");
};

bool SurgeSynthProcessor::changeOSCOut(int new_port, std::string ipaddr)
{
    return initOSCOut(new_port, ipaddr);
}

// Called as 'patch loaded' listener; runs on the juce::MessageManager thread
void SurgeSynthProcessor::patch_load_to_OSC(fs::path fullPath)
{
    std::string pathStr = path_to_string(fullPath);
    if (surge->storage.oscSending && !pathStr.empty())
    {
        oscHandler.send("/patch/load", pathStr);
    }
}

// Called as 'param changed' listener; runs on the juce::MessageManager thread
void SurgeSynthProcessor::param_change_to_OSC(std::string paramPath, bool hasFloat, float value,
                                              std::string valStr)
{
    if (surge->storage.oscSending && !paramPath.empty())
    {
        if (hasFloat)
            oscHandler.send(paramPath, value, valStr);
        else
            oscHandler.send(paramPath, valStr);
    }
}

void SurgeSynthProcessor::paramChangeToListeners(Parameter *p, bool isSpecialCase,
                                                 int specialCaseType, int macroNum, float fval,
                                                 std::string newValue)
{
    std::string valStr = "";
    for (auto &it : this->paramChangeListeners)
    {
        if (isSpecialCase)
        {
            switch (specialCaseType)
            {
            case SCT_MACRO:
            {
                std::ostringstream oss;
                oss << "/param/macro/" << macroNum + 1;
                (it.second)(oss.str(), true, fval, "");
            }
            break;

            case SCT_FX_DEACT:
            {
                std::ostringstream oss, oss2;
                oss << "/param/fx/<s>/<n>/deactivate";
                oss2 << newValue << "(new mask)";
                (it.second)(oss.str(), true, fval, oss2.str());
            }
            break;

            default:
                break;
            }
        }
        else
        {
            float val = -1.;
            valStr = p->get_display(false, 0.0);
            switch (p->valtype)
            {
            case vt_int:
                val = (float)p->val.i;
                break;

            case vt_bool:
                val = (float)p->val.b;
                break;

            case vt_float:
            {
                std::ostringstream oss;
                val = p->value_to_normalized(p->val.f);
            }
            break;

            default:
                break;
            }
            (it.second)(p->oscName, true, val, valStr);
        }
    }
}

//==============================================================================

void SurgeSynthProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    if (!surge)
    {
        return;
    }

    surge->setSamplerate(sr);
    oscCheckStartup = true;

    // It used to be we would set audio processing active true here *but* REAPER calls this for
    // inactive muted channels so we didn't load if that was the case. Set it true only
    // if we actually have an audio process going! See #6173
}

void SurgeSynthProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any spare memory, etc.
}

bool SurgeSynthProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    auto mocs = layouts.getMainOutputChannelSet();
    auto mics = layouts.getMainInputChannelSet();

    auto outputValid = (mocs == juce::AudioChannelSet::stereo()) || (mocs.isDisabled());
    auto inputValid = (mics == juce::AudioChannelSet::stereo()) ||
                      (mics == juce::AudioChannelSet::mono()) || (mics.isDisabled());

    // Check the 6 output shape
    auto c1 = layouts.getNumChannels(false, 1);
    auto c2 = layouts.getNumChannels(false, 2);
    auto sceneOut = (c1 == 0 || c1 == 2) && (c2 == 0 || c2 == 2);

    return outputValid && inputValid && sceneOut;
}

void SurgeSynthProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                       juce::MidiBuffer &midiMessages)
{
    auto fpuguard = sst::plugininfra::cpufeatures::FPUStateGuard();

    if (!surge)
    {
        buffer.clear();
        return;
    }

#if SURGE_HAS_OSC
    if (oscCheckStartup)
    {
        tryLazyOscStartupFromStreamedState();
    }

#endif

    priorCallWasProcessBlockNotBypassed = true;

    // Make sure we have a main output
    auto mb = getBus(false, 0);

    if (mb->getNumberOfChannels() != 2 || !mb->isEnabled())
    {
        // We have to have a stereo output
        if (!warnedAboutBadConfig)
        {
            std::ostringstream msg;
            msg << "Surge XT was not configured to have stereo output, which is required.\n"
                << "The main output bus has " << mb->getNumberOfChannels() << " channels\n"
                << "and is " << (mb->isEnabled() ? "enabled" : "disabled") << ".";
            surge->storage.reportError(msg.str(), "Bus Configuration Error");
            warnedAboutBadConfig = true;
        }
        return;
    }

    if (buffer.getNumChannels() < 2 && mb->isEnabled())
    {
        if (!warnedAboutBadConfig)
        {
            std::ostringstream msg;
            msg << "Surge XT did not receive a stereo processing buffer, which is required.\n"
                << "The provided buffer has " << buffer.getNumChannels() << " channels.\n"
                << "This can happen with certain Bluetooth headsets on macOS,\n"
                << "when they are used as both an input and an output.";
            surge->storage.reportError(msg.str(), "Bus Configuration Error");
            warnedAboutBadConfig = true;
        }
        return;
    }

    if (bypassParameter->getValue() > 0.5)
    {
        if (priorCallWasProcessBlockNotBypassed)
        {
            surge->stopSound();
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
        surge->stopSound();
    }

    surge->audio_processing_active = true;

    processBlockPlayhead();
    processBlockMidiFromGUI();
    processBlockOSC();
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

    const float *incL{nullptr}, *incR{nullptr};
    if (mainInput.getNumChannels() > 0)
    {
        incL = mainInput.getReadPointer(0);
        incR = incL;
    }
    if (mainInput.getNumChannels() > 1)
    {
        incR = mainInput.getReadPointer(1);
    }

    auto sc = buffer.getNumSamples();
    if (!inputIsLatent && (sc & ~(BLOCK_SIZE - 1)) != sc)
    {
        surge->storage.reportError(
            fmt::format("Incoming audio input block is not a multiple of {sz} samples.\n"
                        "If audio input is used, it will be delayed by {sz} samples, in order to "
                        "compensate.\n"
                        "This can be avoided by setting the DAW to use fixed buffer sizes, if "
                        "possible.",
                        fmt::arg("sz", BLOCK_SIZE)),
            "Audio Input Latency Activated", SurgeStorage::AUDIO_INPUT_LATENCY_WARNING, false);
        inputIsLatent = true;
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

        if (blockPos == 0 && incL && incR)
        {
            surge->process_input = true;

            if (inputIsLatent)
            {
                memcpy(&(surge->input[0][0]), inputLatentBuffer[0], BLOCK_SIZE * sizeof(float));
                memcpy(&(surge->input[1][0]), inputLatentBuffer[1], BLOCK_SIZE * sizeof(float));
            }
            else
            {
                auto inL = incL + i;
                auto inR = incR + i;

                memcpy(&(surge->input[0][0]), inL, BLOCK_SIZE * sizeof(float));
                memcpy(&(surge->input[1][0]), inR, BLOCK_SIZE * sizeof(float));
            }
        }
        else
        {
            surge->process_input = false;
        }

        if (blockPos == 0)
        {
            surge->process();
            surge->time_data.ppqPos +=
                (double)BLOCK_SIZE * surge->time_data.tempo / (60. * surge->storage.samplerate);
        }

        if (inputIsLatent && incL && incR)
        {
            inputLatentBuffer[0][blockPos] = incL[i];
            inputLatentBuffer[1][blockPos] = incR[i];
        }

        *outL = surge->output[0][blockPos];
        *outR = surge->output[1][blockPos];

        if (surge->activateExtraOutputs)
        {
            if (sceneAOutput.getNumChannels() == 2)
            {
                auto sAL = sceneAOutput.getWritePointer(0, i);
                auto sAR = sceneAOutput.getWritePointer(1, i);

                if (sAL && sAR)
                {
                    *sAL = surge->sceneout[0][0][blockPos];
                    *sAR = surge->sceneout[0][1][blockPos];
                }
            }

            if (sceneBOutput.getNumChannels() == 2)
            {
                auto sBL = sceneBOutput.getWritePointer(0, i);
                auto sBR = sceneBOutput.getWritePointer(1, i);

                if (sBL && sBR)
                {
                    *sBL = surge->sceneout[1][0][blockPos];
                    *sBR = surge->sceneout[1][1][blockPos];
                }
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

    processBlockPostFunction();
}

void SurgeSynthProcessor::processBlockPlayhead()
{
    auto playhead = getPlayHead();

    if (playhead && !(wrapperType == wrapperType_Standalone))
    {
        juce::AudioPlayHead::CurrentPositionInfo cp;
        playhead->getCurrentPosition(cp);
        surge->time_data.tempo = cp.bpm;

        // isRecording should always imply isPlaying but better safe than sorry
        if (cp.isPlaying || cp.isRecording)
        {
            surge->time_data.ppqPos = cp.ppqPosition;
        }

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
}

void SurgeSynthProcessor::processBlockMidiFromGUI()
{
    midiR rec;

    while (midiFromGUI.pop(rec))
    {
        if (rec.type == midiR::NOTE)
        {
            if (rec.on)
                surge->playNote(rec.ch, rec.note, rec.vel, 0, non_clap_noteid++);
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
        if (rec.type == midiR::SUSPEDAL)
        {
            surge->channelController(rec.ch, 64, rec.cval);
        }
    }
}

void SurgeSynthProcessor::processBlockOSC()
{
    auto messages = oscRingBuf.popall();
    for (const auto &om : messages)
    {
        switch (om.type)
        {
        case SurgeSynthProcessor::NOTEX_PITCH:
            surge->setNoteExpression(SurgeVoice::PITCH, om.noteid, -1, -1, om.fval);
            break;

        case SurgeSynthProcessor::NOTEX_VOL:
            surge->setNoteExpression(SurgeVoice::VOLUME, om.noteid, -1, -1, om.fval);
            break;

        case SurgeSynthProcessor::NOTEX_PAN:
            surge->setNoteExpression(SurgeVoice::PAN, om.noteid, -1, -1, om.fval);
            break;

        case SurgeSynthProcessor::NOTEX_PRES:
            surge->setNoteExpression(SurgeVoice::PRESSURE, om.noteid, -1, -1, om.fval);
            break;

        case SurgeSynthProcessor::NOTEX_TIMB:
            surge->setNoteExpression(SurgeVoice::TIMBRE, om.noteid, -1, -1, om.fval);
            break;

        case SurgeSynthProcessor::PARAMETER:
        {
            float pval = om.fval;
            if (om.param->valtype == vt_int)
                pval = Parameter::intScaledToFloat(pval, om.param->val_max.i, om.param->val_min.i);
            surge->setParameter01(surge->idForParameter(om.param), pval, true);
            surge->storage.getPatch().isDirty = true;

            // Special cases: A few control types require a rebuild and
            // SGE Value Callbacks would do it as would the VST3 param handler
            // so put them here for now. Bit of a hack...
            auto ct = om.param->ctrltype;
            if (ct == ct_bool_solo || ct == ct_bool_mute || ct == ct_scenesel)
            {
                surge->refresh_editor = true;
            }
        }
        break;

        case SurgeSynthProcessor::MACRO:
        {
            surge->setMacroParameter01(om.ival, om.fval);
        }
        break;

        case SurgeSynthProcessor::MNOTE:
        {
            if (om.on)
                surge->playNote(0, om.mnote, om.vel, 0, om.noteid);
            else
                surge->releaseNoteByHostNoteID(om.noteid, om.vel);
        }
        break;

        case SurgeSynthProcessor::FREQNOTE:
        {
            if (om.on)
                surge->playNoteByFrequency(om.fval, om.vel, om.noteid);
            else
            {
                surge->releaseNoteByHostNoteID(om.noteid, om.vel);
            }
        }
        break;

        case SurgeSynthProcessor::ALLNOTESOFF:
        {
            surge->allNotesOff();
        }
        break;

        case SurgeSynthProcessor::ALLSOUNDOFF:
        {
            surge->allSoundOff();
        }
        break;

        case SurgeSynthProcessor::MOD:
        {
            surge->setModDepth01(om.param->id, (modsources)om.ival, om.scene, om.index, om.fval);
        }
        break;

        case SurgeSynthProcessor::MOD_MUTE:
        {
            bool mute = om.fval > 0.0;
            surge->muteModulation(om.param->id, (modsources)om.ival, om.scene, om.index, mute);
        }
        break;

        case SurgeSynthProcessor::FX_DISABLE:
        {
            int selected_mask = om.ival;
            int curmask = surge->storage.getPatch().fx_disable.val.i;
            int msk = selected_mask;
            int newDisabledMask = 0;
            if (om.on == 0) // set selected bit to zero
            {
                msk = ~(msk & 0) ^ selected_mask; // all bits to 1 except selected bit
                newDisabledMask = curmask & msk;
            }
            else // set selected bit to one
            {
                newDisabledMask = curmask | msk;
            }
            surge->storage.getPatch().fx_disable.val.i = newDisabledMask;
            surge->fx_suspend_bitmask = newDisabledMask;
            surge->storage.getPatch().isDirty = true;
            surge->refresh_editor = true;
            // std::cout << "newDisabledMask: " << std::bitset<16>(newDisabledMask) << std::endl;
        }
        break;

        default:
            break;
        }
    }
}

void SurgeSynthProcessor::processBlockPostFunction()
{
    if (checkNamesEvery++ > 10)
    {
        checkNamesEvery = 0;
        if (std::atomic_exchange(&parameterNameUpdated, false))
        {
            updateHostDisplay(
                juce::AudioProcessorListener::ChangeDetails().withParameterInfoChanged(true));
        }
        if (std::atomic_exchange(&surge->patchChanged, false))
        {
            updateHostDisplay(juce::AudioProcessorListener::ChangeDetails()
                                  .withParameterInfoChanged(true)
                                  .withProgramChanged(true)
                                  .withNonParameterStateChanged(true));
        }
    }
}

#if HAS_CLAP_JUCE_EXTENSIONS
clap_process_status SurgeSynthProcessor::clap_direct_process(const clap_process *process) noexcept
{
    auto fpuguard = sst::plugininfra::cpufeatures::FPUStateGuard();

    if (process->audio_outputs_count == 0 || process->audio_outputs_count > 3)
        return CLAP_PROCESS_ERROR;
    if (process->audio_outputs[0].channel_count > 2 || process->audio_outputs[0].channel_count == 0)
        return CLAP_PROCESS_ERROR;

    if (!surge->audio_processing_active)
    {
        // I am just becoming active. There may be lingering notes from when I was
        // deactivated so
        surge->stopSound();
    }
    surge->audio_processing_active = true;

    processBlockPlayhead();
    processBlockMidiFromGUI();
    processBlockOSC();

    auto ev = process->in_events;
    auto evtsz = ev->size(ev);
    auto currev = 0;
    auto nextevtime = -1;

    if (evtsz > 0)
    {
        auto evt = ev->get(ev, 0);
        nextevtime = evt->time;
    }

    // TO DO: Sidechain Input

    float *outL{nullptr}, *outR{nullptr}, *inL{nullptr}, *inR{nullptr};
    outL = process->audio_outputs[0].data32[0];
    outR = outL;
    if (process->audio_outputs[0].channel_count == 2)
        outR = process->audio_outputs[0].data32[1];

    float *sceneAL{nullptr}, *sceneAR{nullptr}, *sceneBL{nullptr}, *sceneBR{nullptr};
    bool haveSceneOut{false};

    if (process->audio_inputs_count == 1)
    {
        surge->process_input = true;
        inL = process->audio_inputs[0].data32[0];
        inR = inL;
        if (process->audio_inputs[0].channel_count == 2)
            inR = process->audio_inputs[0].data32[1];
    }
    if (process->audio_outputs_count == 3 && process->audio_outputs[1].channel_count == 2 &&
        process->audio_outputs[2].channel_count == 2)
    {
        haveSceneOut = true;
        sceneAL = process->audio_outputs[1].data32[0];
        sceneAR = process->audio_outputs[1].data32[1];
        sceneBL = process->audio_outputs[2].data32[0];
        sceneBR = process->audio_outputs[2].data32[1];

        if (!sceneAL || !sceneAR || !sceneBL || !sceneBR)
            haveSceneOut = false;
    }

    for (int s = 0; s < process->frames_count; ++s)
    {
        if (blockPos == 0)
        {
            while (nextevtime >= 0 && nextevtime < s + BLOCK_SIZE && currev < evtsz)
            {
                auto evt = ev->get(ev, currev);

                process_clap_event(evt);

                currev++;
                if (currev < evtsz)
                {
                    nextevtime = ev->get(ev, currev)->time;
                }
                else
                {
                    nextevtime = -1;
                }
            }
        }

        if (blockPos == 0)
        {
            if (inL && inR)
            {
                memcpy(&(surge->input[0][0]), inL, BLOCK_SIZE * sizeof(float));
                memcpy(&(surge->input[1][0]), inR, BLOCK_SIZE * sizeof(float));
                inL += BLOCK_SIZE;
                inR += BLOCK_SIZE;
            }
            surge->process();
            surge->time_data.ppqPos +=
                (double)BLOCK_SIZE * surge->time_data.tempo / (60. * surge->storage.samplerate);

            if (surge->hostNoteEndedDuringBlockCount > 0)
            {
                auto ov = process->out_events;
                for (int v = 0; v < surge->hostNoteEndedDuringBlockCount; ++v)
                {
                    auto evt = clap_event_note();
                    evt.header.size = sizeof(clap_event_note);
                    evt.header.type = (uint16_t)CLAP_EVENT_NOTE_END;
                    evt.header.time = s;
                    evt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    evt.header.flags = 0;

                    evt.port_index = 0;
                    evt.channel = surge->endedHostNoteOriginalChannel[v];
                    evt.key = surge->endedHostNoteOriginalKey[v];
                    evt.note_id = surge->endedHostNoteIds[v];
                    evt.velocity = 0.0;

                    ov->try_push(ov, reinterpret_cast<const clap_event_header *>(&evt));
                }
            }
        }
        *outL = surge->output[0][blockPos];
        *outR = surge->output[1][blockPos];
        outL++;
        outR++;

        if (haveSceneOut)
        {
            *sceneAL = surge->sceneout[0][0][blockPos];
            *sceneAR = surge->sceneout[0][1][blockPos];
            *sceneBL = surge->sceneout[1][0][blockPos];
            *sceneBR = surge->sceneout[1][1][blockPos];

            sceneAL++;
            sceneAR++;
            sceneBL++;
            sceneBR++;
        }

        // TO DO: Scene Output

        blockPos = (blockPos + 1) & (BLOCK_SIZE - 1);
    }

    // just in case
    while (currev < evtsz)
    {
        auto evt = ev->get(ev, currev);
        process_clap_event(evt);
        currev++;
    }

    processBlockPostFunction();
    return CLAP_PROCESS_CONTINUE;
}

void SurgeSynthProcessor::clap_direct_paramsFlush(const clap_input_events *in,
                                                  const clap_output_events *out) noexcept
{
    uint32_t sz = in->size(in);
    for (uint32_t i = 0; i < sz; ++i)
    {
        auto ev = in->get(in, i);
        process_clap_event(ev);
        if (ev->type == CLAP_EVENT_PARAM_VALUE)
        {
            auto pevt = reinterpret_cast<const clap_event_param_value *>(ev);
            auto jp = static_cast<JUCEParameterVariant *>(pevt->cookie);
            if (!jp) // unlikely
                jp = findParameterByParameterId(pevt->param_id);
            jp->processorParam->setValue(pevt->value);
        }
    }
    if (!is_clap_processing)
    {
        // Setting params can change internal state so give the synth a chance to react
        // although the spec is unclear whether this can be called when processing, so don't
        // call surge process if we are!
        surge->process();
    }
}

void SurgeSynthProcessor::process_clap_event(const clap_event_header_t *evt)
{
    if (evt->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return;

    switch (evt->type)
    {
    case CLAP_EVENT_NOTE_ON:
    {
        auto nevt = reinterpret_cast<const clap_event_note *>(evt);

        if (nevt->velocity != 0)
            surge->playNote(nevt->channel, nevt->key, 127 * nevt->velocity, 0, nevt->note_id);
        else
            surge->releaseNote(nevt->channel, nevt->key, 127 * nevt->velocity, nevt->note_id);

        {
            // We need to remember to update the virtual keyboard
            auto m = juce::MidiMessage::noteOn(nevt->channel + 1, nevt->key, (float)nevt->velocity);
            juce::ScopedValueSetter<bool> midiAdd(isAddingFromMidi, true);
            midiKeyboardState.processNextMidiEvent(m);
        }
    }
    break;
    case CLAP_EVENT_NOTE_CHOKE:
    {
        auto nevt = reinterpret_cast<const clap_event_note *>(evt);
        surge->chokeNote(nevt->channel, nevt->key, 127 * nevt->velocity, nevt->note_id);

        {
            // We need to remember to update the virtual keyboard
            auto m = juce::MidiMessage::noteOff(nevt->channel + 1, nevt->key);
            juce::ScopedValueSetter<bool> midiAdd(isAddingFromMidi, true);
            midiKeyboardState.processNextMidiEvent(m);
        }
    }
    break;
    case CLAP_EVENT_NOTE_OFF:
    {
        auto nevt = reinterpret_cast<const clap_event_note *>(evt);
        surge->releaseNote(nevt->channel, nevt->key, 127 * nevt->velocity, nevt->note_id);

        {
            // We need to remember to update the virtual keyboard
            auto m = juce::MidiMessage::noteOff(nevt->channel + 1, nevt->key);
            juce::ScopedValueSetter<bool> midiAdd(isAddingFromMidi, true);
            midiKeyboardState.processNextMidiEvent(m);
        }
    }
    break;
    case CLAP_EVENT_MIDI:
    {
        auto mevt = reinterpret_cast<const clap_event_midi *>(evt);
        auto sz = juce::MidiMessage::getMessageLengthFromFirstByte(mevt->data[0]);
        jassert(sz <= 3 && sz > 0);
        applyMidi(juce::MidiMessageMetadata(mevt->data, sz, mevt->header.time));
    }
    break;
    case CLAP_EVENT_PARAM_VALUE:
    {
        auto pevt = reinterpret_cast<const clap_event_param_value *>(evt);
        auto jp = static_cast<JUCEParameterVariant *>(pevt->cookie);
        if (!jp) // unlikely
            jp = findParameterByParameterId(pevt->param_id);
        jp->processorParam->setValue(pevt->value);
    }
    break;
    case CLAP_EVENT_PARAM_MOD:
    {
        auto pevt = reinterpret_cast<const clap_event_param_mod *>(evt);
        auto jv = static_cast<JUCEParameterVariant *>(pevt->cookie);
        if (!jv) // unlikely
            jv = findParameterByParameterId(pevt->param_id);
        auto jp = static_cast<SurgeBaseParam *>(jv->processorParam);
        if ((pevt->note_id == -1 && pevt->channel == -1 && pevt->key == -1) ||
            !jp->supportsPolyphonicModulation())
        {
            jassert(jp->supportsMonophonicModulation());
            jp->applyMonophonicModulation(pevt->amount);
        }
        else
        {
            jassert(jp->supportsPolyphonicModulation());
            jp->applyPolyphonicModulation(pevt->note_id, pevt->key, pevt->channel, pevt->amount);
        }
    }
    break;

    case CLAP_EVENT_NOTE_EXPRESSION:
    {
        auto pevt = reinterpret_cast<const clap_event_note_expression *>(evt);
        SurgeVoice::NoteExpressionType net = SurgeVoice::UNKNOWN;
        switch (pevt->expression_id)
        {
        // with 0 < x <= 4, plain = 20 * log(x)
        case CLAP_NOTE_EXPRESSION_VOLUME:
            net = SurgeVoice::VOLUME;
            break;
        // pan, 0 left, 0.5 center, 1 right
        case CLAP_NOTE_EXPRESSION_PAN:
            net = SurgeVoice::PAN;
            break;
        // relative tuning in semitone, from -120 to +120
        case CLAP_NOTE_EXPRESSION_TUNING:
            net = SurgeVoice::PITCH;
            break;

            // 0..1
        case CLAP_NOTE_EXPRESSION_BRIGHTNESS:
            net = SurgeVoice::TIMBRE;
            break;
        case CLAP_NOTE_EXPRESSION_PRESSURE:
            net = SurgeVoice::PRESSURE;
            break;
        case CLAP_NOTE_EXPRESSION_VIBRATO:
        case CLAP_NOTE_EXPRESSION_EXPRESSION:
            break;
        }
        if (net != SurgeVoice::UNKNOWN)
            surge->setNoteExpression(net, pevt->note_id, pevt->key, pevt->channel, pevt->value);
    }
    break;

    case CLAP_EVENT_TRANSPORT:
    case CLAP_EVENT_NOTE_END:
    default:
    {
        DBG("Unknown CLAP Message type in Surge XT Direct " << (int)(evt->type));
        // In theory I should never get this.
        // jassertfalse
    }
    break;
    }
}
#endif

void SurgeSynthProcessor::applyMidi(const juce::MidiMessageMetadata &it)
{
    applyMidi(it.getMessage());
}
void SurgeSynthProcessor::applyMidi(const juce::MidiMessage &m)
{
    const int ch = m.getChannel() - 1;

    juce::ScopedValueSetter<bool> midiAdd(isAddingFromMidi, true);
    midiKeyboardState.processNextMidiEvent(m);

    if (m.isNoteOn())
    {
        // no note ids coming from juce- or ui- land
        if (m.getVelocity() != 0)
            surge->playNote(ch, m.getNoteNumber(), m.getVelocity(), 0, -1);
        else
            surge->releaseNote(ch, m.getNoteNumber(), m.getVelocity(), -1);
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
    if (surge)
    {
        return new SurgeSynthEditor(*this);
    }
    else
    {
        return new SurgeSynthStartupErrorEditor(*this);
    }
}

//==============================================================================
void SurgeSynthProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    if (!surge)
        return;

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
    if (!surge)
        return;

    surge->enqueuePatchForLoad(data, sizeInBytes);
    surge->processAudioThreadOpsWhenAudioEngineUnavailable();
    if (surge->audio_processing_active)
    {
        oscCheckStartup = true;
    }
    else
    {
        tryLazyOscStartupFromStreamedState();
    }
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

void SurgeSynthProcessor::reset() { blockPos = 0; }

void SurgeSynthProcessor::tryLazyOscStartupFromStreamedState()
{
    if ((!oscHandler.listening && surge->storage.oscStartIn && surge->storage.oscPortIn > 0) ||
        (!oscHandler.sendingOSC && surge->storage.oscStartOut && surge->storage.oscPortOut > 0))
    {
        oscHandler.tryOSCStartup();
    }
    oscCheckStartup = false;
}

#if HAS_CLAP_JUCE_EXTENSIONS

uint32_t SurgeSynthProcessor::remoteControlsPageCount() noexcept
{
    return 5; // macros + scene a and b mixer and filters
}

bool SurgeSynthProcessor::remoteControlsPageFill(
    uint32_t pageIndex, juce::String &sectionName, uint32_t &pageID, juce::String &pageName,
    std::array<juce::AudioProcessorParameter *, CLAP_REMOTE_CONTROLS_COUNT> &params) noexcept
{
    if (pageIndex < 0 || pageIndex >= remoteControlsPageCount())
        return false;

    pageID = pageIndex + 2054;
    for (auto &p : params)
        p = nullptr;
    switch (pageIndex)
    {
    case 0:
    {
        sectionName = "Global";
        pageName = "Macros";
        for (int i = 0; i < CLAP_REMOTE_CONTROLS_COUNT && i < macrosById.size(); ++i)
            params[i] = macrosById[i];
    }
    break;
    case 1:
    case 3:
    {
        int scene = 0;
        sectionName = "Scene A";
        pageName = "Scene A Mixer";
        if (pageIndex == 3)
        {
            sectionName = "Scene B";
            pageName = "Scene B Mixer";
            scene = 1;
        }
        auto &sc = surge->storage.getPatch().scene[scene];
        params[0] = paramsByID[surge->idForParameter(&sc.level_o1)];
        params[1] = paramsByID[surge->idForParameter(&sc.level_o2)];
        params[2] = paramsByID[surge->idForParameter(&sc.level_o3)];
        params[3] = nullptr;
        params[4] = paramsByID[surge->idForParameter(&sc.level_noise)];
        params[5] = paramsByID[surge->idForParameter(&sc.level_ring_12)];
        params[6] = paramsByID[surge->idForParameter(&sc.level_ring_23)];
        params[7] = paramsByID[surge->idForParameter(&sc.level_pfg)];
    }
    break;
    case 2:
    case 4:
    {
        int scene = 0;
        sectionName = "Scene A";
        pageName = "Scene A Filters";
        if (pageIndex == 4)
        {
            scene = 1;
            sectionName = "Scene B";
            pageName = "Scene B Filters";
        }
        auto &sc = surge->storage.getPatch().scene[scene];
        params[0] = paramsByID[surge->idForParameter(&sc.filterunit[0].cutoff)];
        params[1] = paramsByID[surge->idForParameter(&sc.filterunit[0].resonance)];
        params[2] = paramsByID[surge->idForParameter(&sc.filterunit[1].cutoff)];
        params[3] = paramsByID[surge->idForParameter(&sc.filterunit[1].resonance)];
        params[4] = paramsByID[surge->idForParameter(&sc.wsunit.drive)];
        params[5] = nullptr;
        params[6] = paramsByID[surge->idForParameter(&sc.filter_balance)];
        params[7] = paramsByID[surge->idForParameter(&sc.feedback)];
    }
    break;
    }
    return true;
}

#endif

#if HAS_CLAP_JUCE_EXTENSIONS

bool SurgeSynthProcessor::presetLoadFromLocation(uint32_t location_kind, const char *location,
                                                 const char * /*load_key*/) noexcept
{
    if (location_kind != CLAP_PRESET_DISCOVERY_LOCATION_FILE)
        return false;

    {
        std::lock_guard<std::mutex> mg(surge->patchLoadSpawnMutex);
        strncpy(surge->patchid_file, location, sizeof(surge->patchid_file));
        surge->has_patchid_file = true;
    }
    surge->processAudioThreadOpsWhenAudioEngineUnavailable();
    return true;
}

const void *JUCE_CALLTYPE clapJuceExtensionCustomFactory(const char *f)
{
    if (strcmp(f, CLAP_PRESET_DISCOVERY_FACTORY_ID) == 0 ||
        strcmp(f, CLAP_PRESET_DISCOVERY_FACTORY_ID_COMPAT) == 0)
    {
        return SurgeSynthProcessor::getSurgePresetDiscoveryFactory();
    }

    return nullptr;
}
#endif

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SurgeSynthProcessor(); }

#if 0
void *JUCE_CALLTYPE clapJuceExtensionCustomFactory(const char *)
{
    // ToDo: Implement preset discovery here
    // See #6930
    return nullptr;
}
#endif
