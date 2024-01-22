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

#include "OpenSoundControl.h"
#include "Parameter.h"
#include "SurgeSynthProcessor.h"
#include "SurgeStorage.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include "UnitConversions.h"
#include "Tunings.h"

namespace Surge
{
namespace OSC
{

OpenSoundControl::OpenSoundControl() {}

OpenSoundControl::~OpenSoundControl()
{
    if (listening)
    {
        stopListening(false);
    }
}

void OpenSoundControl::initOSC(SurgeSynthProcessor *ssp,
                               const std::unique_ptr<SurgeSynthesizer> &surge)
{
    // Init. pointers to synth and synth processor
    synth = surge.get();
    sspPtr = ssp;
}

void OpenSoundControl::tryOSCStartup()
{
    if (!synth || !sspPtr)
    {
        return;
    }

    bool startOSCInNow = synth->storage.getPatch().dawExtraState.oscStartIn;

    if (startOSCInNow)
    {
        int defaultOSCInPortPort = synth->storage.getPatch().dawExtraState.oscPortIn;

        if (defaultOSCInPortPort > 0)
        {
            if (!initOSCIn(defaultOSCInPortPort))
            {
                sspPtr->initOSCError(defaultOSCInPortPort);
            }
        }
    }

    bool startOSCOutNow = synth->storage.getPatch().dawExtraState.oscStartOut;

    if (startOSCOutNow)
    {
        int defaultOSCOutPort = synth->storage.getPatch().dawExtraState.oscPortOut;
        std::string defaultOSCOutIPAddr = synth->storage.getPatch().dawExtraState.oscIPAddrOut;

        if (defaultOSCOutPort > 0)
        {
            if (!initOSCOut(defaultOSCOutPort, defaultOSCOutIPAddr))
                sspPtr->initOSCError(defaultOSCOutPort, defaultOSCOutIPAddr);
        }
    }
}

/* ----- OSC Receiver  ----- */

bool OpenSoundControl::initOSCIn(int port)
{
    if (port < 1)
    {
        return false;
    }

    if (connect(port))
    {
        addListener(this);
        listening = true;
        iportnum = port;
        synth->storage.oscListenerRunning = true;
        synth->storage.oscStartIn = true;

#ifdef DEBUG
        std::cout << "Surge: Listening for OSC on port " << port << "." << std::endl;
#endif
        return true;
    }

    return false;
}

void OpenSoundControl::stopListening(bool updateOSCStartInStorage)
{
    if (!listening)
    {
        return;
    }

    removeListener(this);
    listening = false;

    if (synth)
    {
        synth->storage.oscListenerRunning = false;

        if (updateOSCStartInStorage)
        {
            synth->storage.oscStartIn = false;
        }
        // remove patch change and param change listeners
    }

#ifdef DEBUG
    std::cout << "Surge: Stopped listening for OSC." << std::endl;
#endif
}

// Concatenates OSC message data strings separated by spaces into one string (with spaces)
std::string OpenSoundControl::getWholeString(const juce::OSCMessage &om)
{
    std::string dataStr = "";
    for (int i = 0; i < om.size(); i++)
    {
        dataStr += om[i].getString().toStdString();
        if (i < (om.size() - 1))
            dataStr += " ";
    }
    return dataStr;
}

int OpenSoundControl::getNoteID(const juce::OSCMessage &om, int pos)
{
    // note id supplied by sender
    {
        if (!om[pos].isFloat32())
        {
            sendNotFloatError("fnote or note expression", "noteID");
            return -1;
        }
        float msg2 = om[pos].getFloat32();
        if (msg2 < 0. || msg2 > (float)(std::numeric_limits<int>::max()))
        {
            sendError("NoteID must be between 0 and " +
                      std::to_string(std::numeric_limits<int>::max()));
            return -1;
        }
        else
        {
            return (static_cast<int>(msg2));
        }
    }
}

void OpenSoundControl::oscMessageReceived(const juce::OSCMessage &message)
{
    std::string addr = message.getAddressPattern().toString().toStdString();
    if (addr.at(0) != '/')
    {
        sendError("Bad OSC message format.");
        return;
    }

    std::istringstream split(addr);
    // Scan past first '/'
    std::string throwaway;
    std::getline(split, throwaway, '/');

    std::string address1, address2, address3, address4, address5;
    std::getline(split, address1, '/');
    bool querying = false;

    // Queries (for params)
    if (address1 == "q")
    {
        querying = true;
        // remove the '/q' from the address
        std::getline(split, address1, '/');
        addr = addr.substr(2);
        // Currently, only params may be queried
        if ((address1 != "param") && (address1 != "all_params") && (address1 != "all_mods"))
        {
            sendError("No query available for '" + address1 + "'");
            return;
        }
        if (address1 == "all_params")
        {
            OpenSoundControl::sendAllParams();
            return;
        }
        if (address1 == "all_mods")
        {
            OpenSoundControl::sendAllModulators();
            return;
        }
    }

    // Note expressions
    if (address1 == "ne")
    {
        if (message.size() != 2)
        {
            sendDataCountError("note expression", "2");
        }
        if (!message[0].isFloat32())
        {
            sendNotFloatError("ne", "value");
            return;
        }
        int noteID = getNoteID(message, 0);
        if (noteID == -1)
        {
            sendError("Note expressions require a valid noteID.");
            return;
        }
        float val = message[1].getFloat32();

        std::getline(split, address2, '/');
        if (address2 == "volume")
        {
            if (val < 0.0 || val > 4.0)
            {
                sendError("Note expression (volume) '" + std::to_string(val) +
                          "' is out of range (0.0 - 4.0).");
                return;
            }
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::NOTEX_VOL, noteID, val));
        }
        else if (address2 == "pitch")
        {
            if (val < -120.0 || val > 120.0)
            {
                sendError("Note expression (pitch) '" + std::to_string(val) +
                          "' is out of range (-120.0 - 120.0).");
                return;
            }
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::NOTEX_PITCH, noteID, val));
        }
        else if (address2 == "pan")
        {
            if (val < 0.0 || val > 1.0)
            {
                sendError("Note expression (pan) '" + std::to_string(val) +
                          "' is out of range (0.0 - 1.0).");
                return;
            }
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::NOTEX_PAN, noteID, val));
        }
        else if (address2 == "timbre")
        {
            if (val < 0.0 || val > 1.0)
            {
                sendError("Note expression (timbre) '" + std::to_string(val) +
                          "' is out of range (0.0 - 1.0).");
                return;
            }
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::NOTEX_TIMB, noteID, val));
        }
        else if (address2 == "pressure")
        {
            if (val < 0.0 || val > 1.0)
            {
                sendError("Note expression (pressure) '" + std::to_string(val) +
                          "' is out of range (0.0 - 1.0).");
                return;
            }
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::NOTEX_PRES, noteID, val));
        }
    }

    // 'Frequency' notes
    else if (address1 == "fnote")
    // Play a note at the given frequency and velocity
    {
        int32_t noteID = 0;
        std::getline(split, address2, '/'); // check for '/rel'

        if (message.size() < 2 || message.size() > 3)
        {
            sendDataCountError("fnote", "2 or 3");
            return;
        }
        if (!message[0].isFloat32())
        {
            sendNotFloatError("fnote", "frequency");
            return;
        }
        if (!message[1].isFloat32())
        {
            sendNotFloatError("fnote", "velocity");
            return;
        }
        if (message.size() == 3)
        {
            noteID = getNoteID(message, 2);
            if (noteID == -1)
                return;
        }

        float frequency = message[0].getFloat32();
        // Future enhancement: keep velocity as float as long as possible
        int velocity = static_cast<int>(message[1].getFloat32() + 0.5);
        constexpr float MAX_MIDI_FREQ = 12543.854;

        bool noteon = (address2 != "rel") && (velocity != 0);

        // (if not a note off-by-noteid) ensure freq. is in MIDI note range
        if (noteon || noteID == 0)
        {
            if (frequency < Tunings::MIDI_0_FREQ || frequency > MAX_MIDI_FREQ)
            {
                sendError("Frequency '" + std::to_string(frequency) + "' is out of range. (" +
                          std::to_string(Tunings::MIDI_0_FREQ) + " - " +
                          std::to_string(MAX_MIDI_FREQ) + ").");
                return;
            }
        }

        // check velocity range
        if (velocity < 0 || velocity > 127)
        {
            sendError("Velocity '" + std::to_string(velocity) + "' is out of range (0 - 127).");
            return;
        }

        // Make a noteID from frequency if not supplied
        if (noteID == 0)
            noteID = int(frequency * 10000);

        // queue packet to audio thread
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
            frequency, static_cast<char>(velocity), noteon, noteID));
    }

    // "MIDI-style" notes
    else if (address1 == "mnote")
    // OSC equivalent of MIDI note
    {
        int32_t noteID = 0;

        if (message.size() < 2 || message.size() > 3)
        {
            sendDataCountError("mnote", "2 or 3");
            return;
        }

        std::getline(split, address2, '/'); // check for '/rel'

        if (!message[0].isFloat32() || !message[1].isFloat32())
        {
            sendError("Invalid data type for OSC MIDI-style note and/or velocity (must be a "
                      "float between 0 - 127).");
            return;
        }

        if (message.size() == 3)
        {
            noteID = getNoteID(message, 2);
            if (noteID == -1)
                return;
        }

        int note = static_cast<int>(message[0].getFloat32());
        int velocity = static_cast<int>(message[1].getFloat32());
        bool noteon = (address2 != "rel") && (velocity != 0);

        // check note and velocity ranges (if not a release w/ id)
        if (noteon || noteID == 0)
        {
            if (note < 0 || note > 127)
            {
                sendError("Note '" + std::to_string(note) + "' is out of range (0 - 127).");
                return;
            }
        }

        if (velocity < 0 || velocity > 127)
        {
            sendError("Velocity '" + std::to_string(velocity) + "' is out of range (0 - 127).");
            return;
        }

        if (noteID == 0)
            noteID = int(note);

        // Send packet to audio thread
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
            static_cast<char>(note), static_cast<char>(velocity), noteon, noteID));
    }

    // All notes off
    else if (address1 == "allnotesoff")
    {
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::ALLNOTESOFF));
    }

    else if (address1 == "allsoundoff")
    {
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::ALLSOUNDOFF));
    }

    // Parameters
    else if (address1 == "param")
    {
        float val = 0;

        if ((message.size() < 1) && !querying)
        {
            sendDataCountError("param", "1 or more");
            return;
        }
        else if (!querying)
        {
            if (!message[0].isFloat32())
            {
                // Not a valid data value
                sendNotFloatError("param", "");
                return;
            }
            val = message[0].getFloat32();
        }

        // Special case for /param/macro/
        std::getline(split, address2, '/'); // check for '/macro'
        if (address2 == "macro")
        {
            int macnum;
            std::getline(split, address3, '/'); // get macro num
            try
            {
                macnum = stoi(address3);
            }
            catch (...)
            {
                sendError("OSC /param/macro: Invalid macro number: " + address3);
                return;
            }
            if ((macnum <= 0) || (macnum > n_customcontrollers))
            {
                sendError("OSC /param/macro: Invalid macro number: " + address3);
                return;
            }
            if (querying)
            {
                OpenSoundControl::sendMacro(macnum - 1);
            }
            else
                sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(--macnum, val));
        }

        // Special case for /param/fx/<s>/<n>/deactivate, which is not a true 'parameter'
        else if ((address2 == "fx") && (hasEnding(addr, "deactivate")))
        {
            int fxidx = 0, fxslot = 0;
            std::string slot_type = "";
            std::string shortOSCname = "fx/";
            std::string tmp = "";
            std::getline(split, slot_type, '/');
            shortOSCname += slot_type + '/';
            std::getline(split, tmp, '/');
            try
            {
                fxidx = stoi(tmp);
            }
            catch (const std::exception &e)
            {
                sendError("Bad format FX index.");
                return;
            }
            shortOSCname += tmp;

            // find the shortOSCname
            auto found = std::find(std::begin(fxslot_shortoscname), std::end(fxslot_shortoscname),
                                   shortOSCname);
            if (found == std::end(fxslot_shortoscname))
            {
                sendError("Bad slot/index selector.");
                return;
            }

            fxslot = std::distance(std::begin(fxslot_shortoscname), found);
            int selected_mask = 1 << fxslot;

            if (querying)
            {
                int deac_mask = synth->storage.getPatch().fx_disable.val.i;
                bool isDeact = (deac_mask & selected_mask) > 0;
                std::string deactivated = isDeact ? "deactivated" : "activated";
                std::string addr = "/param/" + shortOSCname + "/deactivate";
                float val = (float)isDeact;
                if (!this->juceOSCSender.send(
                        juce::OSCMessage(juce::String(addr), val, juce::String(deactivated))))
                    sendFailed();
            }
            else
            {
                if (!message[0].isFloat32())
                {
                    // Not a valid data value
                    sendNotFloatError("fx deactivate", "on/off");
                    return;
                }

                int onoff = message[0].getFloat32();
                if (!((onoff == 0) || (onoff == 1)))
                {
                    sendError("FX deactivate value must be 0 or 1.");
                    return;
                }

                // Send packet to audio thread
                sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(selected_mask, onoff));
            }
        }

        // all the other /param messages
        else
        {
            auto *p = synth->storage.getPatch().parameterFromOSCName(addr);
            if (p == NULL)
            {
                sendError("No parameter with OSC address of " + addr);
                // Not a valid OSC address
                return;
            }

            if (querying)
            {
                sendParameter(p);
            }
            else
            {
                sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(p, val));
            }
        }

#ifdef DEBUG_VERBOSE
        std::cout << "Parameter OSC name:" << p->get_osc_name() << "  ";
        std::cout << "Parameter full name:" << p->get_full_name() << std::endl;
#endif
    }

    // Patch changing
    else if (address1 == "patch")
    {
        std::getline(split, address2, '/');
        if (address2 == "load")
        {
            std::string dataStr = getWholeString(message) + ".fxp";
            {
                std::lock_guard<std::mutex> mg(synth->patchLoadSpawnMutex);
                strncpy(synth->patchid_file, dataStr.c_str(), FILENAME_MAX);
                synth->has_patchid_file = true;
            }
            synth->processAudioThreadOpsWhenAudioEngineUnavailable();
        }
        else if (address2 == "save")
        {
            // Run this on the juce messenger thread
            juce::MessageManager::getInstance()->callAsync([this, message]() {
                std::string dataStr = getWholeString(message);
                if (dataStr.empty())
                    synth->savePatch(false, true);
                else
                {
                    dataStr += ".fxp";
                    fs::path ppath = fs::path(dataStr);
                    synth->savePatchToPath(ppath);
                }
            });
        }
        else if (address2 == "random")
        {
            synth->selectRandomPatch();
        }
        else if (address2 == "incr")
        {
            synth->jogPatch(true);
        }
        else if (address2 == "decr")
        {
            synth->jogPatch(false);
        }
        else if (address2 == "incr_category")
        {
            synth->jogCategory(true);
        }
        else if (address2 == "decr_category")
        {
            synth->jogCategory(false);
        }
    }

    // Tuning switching
    else if (address1 == "tuning")
    {
        fs::path path = getWholeString(message);
        fs::path def_path;

        std::getline(split, address2, '/');
        // Tuning files path control
        if (address2 == "path")
        {
            std::string dataStr = getWholeString(message);
            if ((dataStr != "_reset") && (!fs::exists(dataStr)))
            {
                sendError("An OSC 'tuning/path/...' message was received with a path which does "
                          "not exist: the default path will not change.");
                return;
            }

            fs::path ppath = fs::path(dataStr);
            std::getline(split, address3, '/');

            if (address3 == "scl")
            {
                if (dataStr == "_reset")
                {
                    ppath = synth->storage.datapath;
                    ppath /= "tuning_library/SCL";
                }
                Surge::Storage::updateUserDefaultPath(&(synth->storage),
                                                      Surge::Storage::LastSCLPath, ppath);
            }
            else if (address3 == "kbm")
            {
                if (dataStr == "_reset")
                {
                    ppath = synth->storage.datapath;
                    ppath /= "tuning_library/KBM Concert Pitch";
                }
                Surge::Storage::updateUserDefaultPath(&(synth->storage),
                                                      Surge::Storage::LastKBMPath, ppath);
            }
        }
        // Tuning file selection
        else if (address2 == "scl")
        {
            if (path.is_relative())
            {
                def_path = Surge::Storage::getUserDefaultPath(
                    &(synth->storage), Surge::Storage::LastSCLPath,
                    synth->storage.datapath / "tuning_library" / "SCL");
                def_path /= path;
                def_path += ".scl";
            }
            else
            {
                def_path = path;
                def_path += ".scl";
            }
            synth->storage.loadTuningFromSCL(def_path);
        }
        // KBM mapping file selection
        else if (address2 == "kbm")
        {
            if (path.is_relative())
            {
                def_path = Surge::Storage::getUserDefaultPath(
                    &(synth->storage), Surge::Storage::LastKBMPath,
                    synth->storage.datapath / "tuning_library" / "KBM Concert Pitch");
                def_path /= path;
                def_path += ".kbm";
            }
            else
            {
                def_path = path;
                def_path += ".kbm";
            }
            synth->storage.loadMappingFromKBM(def_path);
        }
    }

    // Modulation mapping
    else if (address1 == "mod")
    {
        bool muteMsg = false;
        if (message.size() < 2)
        {
            sendDataCountError("mod", "2");
            return;
        }

        std::getline(split, address2, '/');
        if (address2 == "mute")
        {
            muteMsg = true;
            std::getline(split, address2, '/');
        }

        int mscene = 0;
        bool scene_specified = false;
        if ((address2 == "a") || (address2 == "b"))
        {
            scene_specified = true;
            if (address2 == "b")
                mscene = 1;
            std::getline(split, address2, '/'); // get mod into address2
        }

        std::string mod = address2;
        int modnum = -1;
        for (int i = 0; i < n_modsources; i++)
        {
            if (modsource_names_tag[i] == address2)
            {
                modnum = i;
                break;
            }
        }

        if (modnum == -1)
        {
            sendError("Bad modulator name: " + address2);
            return;
        }

        int index = 0;
        std::getline(split, address3, '/'); // address3 = index, if any
        if (address3 != "")
        {
            try
            {
                index = stoi(address3);
            }
            catch (const std::exception &e)
            {
                sendError("Bad format for modulator index.");
                return;
            }
        }

        if ((index < 0) || (index >= synth->getMaxModulationIndex(mscene, (modsources)modnum)))
        {
            sendError("Modulator index out of range.");
            return;
        }

        if (!message[0].isString())
        {
            sendError("Bad OSC message format.");
            return;
        }
        if (!message[1].isFloat32())
        {
            sendNotFloatError("mod", "depth");
            return;
        }

        float depth = message[1].getFloat32();

        std::string parmAddr = message[0].getString().toStdString();
        auto *p = synth->storage.getPatch().parameterFromOSCName(parmAddr);
        if (p == NULL)
        {
            sendError("No parameter with OSC address of " + parmAddr);
            return;
        }

        if (scene_specified)
        {
            if ((p->scene - 1) != mscene)
            {
                sendError("Modulator scene must be the same as the target scene.");
                return;
            }
        }

        if (!synth->isValidModulation(p->id, (modsources)modnum))
        {
            sendError("Not a valid modulation.");
            return;
        }
        if (muteMsg)
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(p, modnum, mscene, index, depth, true));
        else
            sspPtr->oscRingBuf.push(
                SurgeSynthProcessor::oscToAudio(p, modnum, mscene, index, depth));
    }
    if (!synth->audio_processing_active)
    {
        // Audio isn't running so the queue wont be drained.
        // In this case do the (slightly hacky) drain-on-this-thread
        // approach. There's a small race condition here in that if
        // processing restarts while we have a message we are doing
        // here then maybe we go blammo. That's a super-duper edge
        // case which i'll mention here but not fix. (The fix is probably
        // to have an atomic book in processBlock and properly atomic
        // compare and set it and return if two threads are in process).
        sspPtr->processBlockOSC();
    }
}

bool OpenSoundControl::hasEnding(std::string const &fullString, std::string const &ending)
{
    if (fullString.length() >= ending.length())
    {
        return (0 ==
                fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else
    {
        return false;
    }
}

void OpenSoundControl::oscBundleReceived(const juce::OSCBundle &bundle)
{
    std::string msg;

#ifdef DEBUG
    std::cout << "OSC Listener: Got OSC bundle." << msg << std::endl;
#endif

    for (int i = 0; i < bundle.size(); ++i)
    {
        auto elem = bundle[i];
        if (elem.isMessage())
            oscMessageReceived(elem.getMessage());
        else if (elem.isBundle())
            oscBundleReceived(elem.getBundle());
    }
}

/* ----- OSC Sending  ----- */

bool OpenSoundControl::initOSCOut(int port, std::string ipaddr)
{
    if (port < 1)
    {
        return false;
    }

    // Send OSC messages to IP Address:UDP port number
    if (!juceOSCSender.connect(ipaddr, port))
    {
        return false;
    }

    // Add listener for patch changes, to send new path to OSC output
    // This will run on the juce::MessageManager thread so as to
    // not tie up the patch loading thread.
    synth->addPatchLoadedListener("OSC_OUT", [ssp = sspPtr](auto s) { ssp->patch_load_to_OSC(s); });

    // Add a listener for parameter changes
    sspPtr->addParamChangeListener("OSC_OUT",
                                   [ssp = sspPtr](auto str1, auto bool1, auto float1, auto str2) {
                                       ssp->param_change_to_OSC(str1, bool1, float1, str2);
                                   });
    // Add a listener for modulation changes
    synth->addModulationAPIListener(this);

    sendingOSC = true;
    oportnum = port;
    outIPAddr = ipaddr;
    synth->storage.oscSending = true;
    synth->storage.oscStartOut = true;

#ifdef DEBUG
    std::cout << "Surge: Sending OSC on port " << port << "." << std::endl;
#endif
    return true;
}

void OpenSoundControl::stopSending(bool updateOSCStartInStorage)
{
    if (!sendingOSC)
    {
        return;
    }

    sendingOSC = false;
    synth->storage.oscSending = false;

    synth->deletePatchLoadedListener("OSC_OUT");
    sspPtr->deleteParamChangeListener("OSC_OUT");

    if (updateOSCStartInStorage)
    {
        synth->storage.oscStartOut = false;
    }

#ifdef DEBUG
    std::cout << "Surge: Stopped sending OSC." << std::endl;
#endif
}

void OpenSoundControl::send(std::string addr, std::string msg)
{
    if (sendingOSC)
    {
        juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
        om.addString(msg);
        // Runs on the juce messenger thread
        juce::MessageManager::getInstance()->callAsync([this, om]() {
            if (!this->juceOSCSender.send(om))
                sendFailed();
        });
    }
}

void OpenSoundControl::send(std::string addr, float fval, std::string msg)
{
    if (sendingOSC)
    {
        juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
        om.addFloat32(fval);
        om.addString(msg);
        // Runs on the juce messenger thread
        juce::MessageManager::getInstance()->callAsync([this, om]() {
            if (!this->juceOSCSender.send(om))
                sendFailed();
        });
    }
}

void OpenSoundControl::sendFailed() { std::cout << "Error: could not send OSC message."; }

void OpenSoundControl::sendError(std::string errorMsg)
{
    if (sendingOSC)
        OpenSoundControl::send("/error", errorMsg);
    else
        std::cout << "OSC Error: " << errorMsg << std::endl;
}

void OpenSoundControl::sendNotFloatError(std::string addr, std::string msg)
{
    OpenSoundControl::sendError(
        "/" + addr + " data value '" + msg +
        "' is not expressed as a float. All data must be sent as OSC floats.");
}

void OpenSoundControl::sendDataCountError(std::string addr, std::string count)
{
    OpenSoundControl::sendError("Wrong number of data items supplied for /" + addr + "; expected " +
                                count + ".");
}

// Loop through all params, send them to OSC Out
void OpenSoundControl::sendAllParams()
{
    if (sendingOSC)
    {
        // Runs on the juce messenger thread
        juce::MessageManager::getInstance()->callAsync([this]() {
            // auto timer = new Surge::Debug::TimeBlock("ParameterDump");
            std::string valStr;
            int n = synth->storage.getPatch().param_ptr.size();
            // Dump all params except for macros
            for (int i = 0; i < n; i++)
            {
                Parameter *p = synth->storage.getPatch().param_ptr[i];
                sendParameter(p);
            }
            // Now do the macros
            for (int i = 0; i < n_customcontrollers; i++)
            {
                sendMacro(i);
            }
            // delete timer;    // This prints the elapsed time
        });
    }
}

// Loop through all modulators, send them to OSC Out
void OpenSoundControl::sendAllModulators()
{
    if (sendingOSC)
    {
        // Runs on the juce messenger thread
        juce::MessageManager::getInstance()->callAsync([this]() {
            // auto timer = new Surge::Debug::TimeBlock("ModulatorDump");
            std::vector<ModulationRouting> modlist_global =
                synth->storage.getPatch().modulation_global;
            std::vector<ModulationRouting> modlist_scene_A_scene =
                synth->storage.getPatch().scene[0].modulation_scene;
            std::vector<ModulationRouting> modlist_scene_B_scene =
                synth->storage.getPatch().scene[1].modulation_scene;
            std::vector<ModulationRouting> modlist_scene_A_voice =
                synth->storage.getPatch().scene[0].modulation_voice;
            std::vector<ModulationRouting> modlist_scene_B_voice =
                synth->storage.getPatch().scene[1].modulation_voice;

            for (ModulationRouting mod : modlist_global)
                sendModulator(mod, 0, true);
            for (ModulationRouting mod : modlist_scene_A_scene)
                sendModulator(mod, 0, false);
            for (ModulationRouting mod : modlist_scene_B_scene)
                sendModulator(mod, 1, false);
            for (ModulationRouting mod : modlist_scene_A_voice)
                sendModulator(mod, 0, false);
            for (ModulationRouting mod : modlist_scene_B_voice)
                sendModulator(mod, 1, false);
        });
    }
}

bool OpenSoundControl::sendModulator(ModulationRouting mod, int scene, bool global)
{
    bool supIndex = synth->supportsIndexedModulator(0, (modsources)mod.source_id);
    int modIndex = 0;
    if (supIndex)
    {
        modIndex = mod.source_index;
    }
    std::string addr = getModulatorOSCAddr(mod.source_id, scene, modIndex, false);

    int offset = global ? 0 : synth->storage.getPatch().scene_start[scene];
    Parameter *p = synth->storage.getPatch().param_ptr[mod.destination_id + offset];
    float val = synth->getModDepth01(p->id, (modsources)mod.source_id, scene, modIndex);
    // Send mod-to-param mapping and depth
    if (!modOSCout(addr, p->oscName, val, false))
        return false;
    // Now send the mute status for this mapping
    val = mod.muted ? 1.0 : 0.0;
    return (modOSCout(addr, p->oscName, val, true));
}

bool OpenSoundControl::modOSCout(std::string addr, std::string oscName, float val, bool reportMute)
{
    std::string paramAddr = addr;
    if (reportMute)
        paramAddr = addr.insert(4, "/mute");
    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    om.addString(oscName);
    om.addFloat32(val);

    if (!this->juceOSCSender.send(om))
    {
        sendFailed();
        return false;
    }

    return true;
}

std::string OpenSoundControl::getModulatorOSCAddr(int modid, int scene, int index, bool mute)
{
    std::string modName = modsource_names_tag[modid];
    std::string sceneStr = "";
    std::string indexStr = "";
    std::string muteStr = "";

    bool useScene = synth->isModulatorDistinctPerScene((modsources)modid);
    if (useScene)
    {
        if (scene == 0)
            sceneStr = "a/";
        else if (scene == 1)
            sceneStr = "b/";
    }

    bool supIndex = synth->supportsIndexedModulator(0, (modsources)modid);
    if (supIndex)
        indexStr = "/" + std::to_string(index);

    if (mute)
        muteStr = "mute/";

    return ("/mod/" + muteStr + sceneStr + modName + indexStr);
}

bool OpenSoundControl::sendMacro(long macnum)
{
    auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                    .scene[0]
                    .modsources[macnum + ms_ctrl1]);
    auto valStr = float_to_clocalestr_wprec(100 * cms->get_output(0), 3) + " %";
    float val01 = cms->get_output01(0);
    std::string addr = "/param/macro/" + std::to_string(macnum + 1);

    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    om.addFloat32(val01);
    if (valStr != "")
        om.addString(valStr);
    if (!this->juceOSCSender.send(om))
    {
        sendFailed();
        return false;
    }

    return true;
}

bool OpenSoundControl::sendParameter(const Parameter *p)
{
    std::string valStr = "";
    float val01 = 0.0;
    valStr = p->get_display(false, 0.0);

    switch (p->valtype)
    {
    case vt_int:
        val01 = float(p->val.i);
        break;

    case vt_bool:
        val01 = float(p->val.b);
        break;

    case vt_float:
    {
        val01 = p->value_to_normalized(p->val.f);
    }
    break;

    default:
        break;
    }

    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(p->oscName)));
    om.addFloat32(val01);
    if (valStr != "")
        om.addString(valStr);
    if (!this->juceOSCSender.send(om))
    {
        sendFailed();
        return false;
    }
    return true;
}

// Under construction, but tested as safe!
void OpenSoundControl::modSet(long ptag, modsources modsource, int modsourceScene, int index,
                              float val, bool isNew)
{
    sendMod(ptag, modsource, modsourceScene, index, val, false);
}

void OpenSoundControl::modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                                bool mute)
{
    sendMod(ptag, modsource, modsourceScene, index, mute ? 1.0 : 0.0, true);
}

void OpenSoundControl::modCleared(long ptag, modsources modsource, int modsourceScene, int index)
{
    sendMod(ptag, modsource, modsourceScene, index, 0.0, false);
}

void OpenSoundControl::sendMod(long ptag, modsources modsource, int modSourceScene, int index,
                               float val, bool mute)
{
    // Runs on the juce messenger thread
    juce::MessageManager::getInstance()->callAsync(
        [this, ptag, modsource, modSourceScene, index, val, mute]() {
            std::string modOSCaddr = getModulatorOSCAddr(modsource, modSourceScene, index, mute);
            Parameter *param = synth->storage.getPatch().param_ptr[ptag];
            modOSCout(modOSCaddr, param->get_osc_name(), val, false);
        });
}

} // namespace OSC
} // namespace Surge