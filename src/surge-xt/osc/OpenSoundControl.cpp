/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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
#include <sstream>
#include <vector>
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
        stopListening();
}

void OpenSoundControl::initOSC(SurgeSynthProcessor *ssp,
                               const std::unique_ptr<SurgeSynthesizer> &surge)
{
    // Init. pointers to synth and synth processor
    synth = surge.get();
    sspPtr = ssp;
}

/* ----- OSC Receiver  ----- */

bool OpenSoundControl::initOSCIn(int port)
{
    if (connect(port))
    {
        addListener(this);
        listening = true;
        iportnum = port;
        synth->storage.oscListenerRunning = true;

#ifdef DEBUG
        std::cout << "SurgeOSC: Listening for OSC on port " << port << "." << std::endl;
#endif
        return true;
    }
    return false;
}

void OpenSoundControl::stopListening()
{
    if (!listening)
        return;

    removeListener(this);
    listening = false;

    if (synth)
        synth->storage.oscListenerRunning = false;

#ifdef DEBUG
    std::cout << "SurgeOSC: Stopped listening for OSC." << std::endl;
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

int OpenSoundControl::getNoteID(const juce::OSCMessage &om)
{
    // note id supplied by sender
    {
        if (!om[2].isFloat32())
        {
            sendNotFloatError("fnote", "noteID");
            return -1;
        }
        float msg2 = om[2].getFloat32();
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
        sendError("Badly-formed OSC message.");
        return;
    }

    std::istringstream split(addr);
    // Scan past first '/'
    std::string throwaway;
    std::getline(split, throwaway, '/');

    std::string address1, address2, address3;
    std::getline(split, address1, '/');

    if (address1 == "nex")
    {
        if (message.size() != 1)
        {
            sendDataCountError("note expression", "1");
        }

        int id = 444; // change this
        float val = 0.0;
        std::getline(split, address2, '/');
        if (address2 == "vol")
        {
            synth.setNoteExpression(SurgeVoice::VOLUME, id, -1, -1, -1);
        }
        else if (address2 == "pitch")
        {
            mk = 60;      // change this
            offset = 0.0; // change this
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                frequency, static_cast<char>(velocity), noteon, noteID));
        }
        else if (address2 == "pan")
        {
        }
        else if (address2 == "timb")
        {
        }
        else if (address2 == "press")
        {
        }
    }
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
            noteID = getNoteID(message);
            if (noteID == -1)
                return;
        }
        if (message.size() == 3)
        // note id supplied
        {
            if (!message[2].isFloat32())
            {
                sendNotFloatError("fnote", "noteID");
                return;
            }
            noteID = static_cast<int>(message[2].getFloat32());
        }

        float frequency = message[0].getFloat32();
        int velocity = static_cast<int>(message[1].getFloat32());
        constexpr float MAX_MIDI_FREQ = 12543.854;

        // ensure freq. is in MIDI note range
        if (frequency < Tunings::MIDI_0_FREQ || frequency > MAX_MIDI_FREQ)
        {
            sendError("Frequency '" + std::to_string(frequency) + "' is out of range. (" +
                      std::to_string(Tunings::MIDI_0_FREQ) + " - " + std::to_string(MAX_MIDI_FREQ) +
                      ").");
            return;
        }
        // check velocity range
        if (velocity < 0 || velocity > 127)
        {
            sendError("Velocity '" + std::to_string(velocity) + "' is out of range (0-127).");
            return;
        }
        bool noteon = (address2 != "rel") && (velocity != 0);

        // Make a noteID from frequency if not supplied
        if (noteID == 0)
            noteID = int(frequency * 10000);

        // queue packet to audio thread
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
            frequency, static_cast<char>(velocity), noteon, noteID));
    }

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
            noteID = getNoteID(message);
            if (noteID == -1)
                return;
        }

        int note = static_cast<int>(message[0].getFloat32());
        int velocity = static_cast<int>(message[1].getFloat32());

        // check note and velocity ranges
        if (note < 0 || note > 127)
        {
            sendError("Note '" + std::to_string(note) + "' is out of range (0-127).");
            return;
        }
        if (velocity < 0 || velocity > 127)
        {
            sendError("Velocity '" + std::to_string(velocity) + "' is out of range (0-127).");
            return;
        }

        bool noteon = (address2 != "rel") && (velocity != 0);
        if (noteID == 0)
            noteID = int(note);

        // Send packet to audio thread
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
            static_cast<char>(note), static_cast<char>(velocity), noteon, noteID));
    }

    else if (address1 == "param")
    {
        if (message.size() != 1)
        {
            sendDataCountError("param", "1");
            return;
        }

        auto *p = synth->storage.getPatch().parameterFromOSCName(addr);
        if (p == NULL)
        {
            sendError("No parameter with OSC address of " + addr);
            // Not a valid OSC address
            return;
        }

        if (!message[0].isFloat32())
        {
            // Not a valid data value
            sendNotFloatError("param", "");
            return;
        }

        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(p, message[0].getFloat32()));

#ifdef DEBUG_VERBOSE
        std::cout << "Parameter OSC name:" << p->get_osc_name() << "  ";
        std::cout << "Parameter full name:" << p->get_full_name() << std::endl;
#endif
    }

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

    else if (address1 == "send_all_parameters")
    {
        OpenSoundControl::sendAllParams();
    }
}

void OpenSoundControl::oscBundleReceived(const juce::OSCBundle &bundle)
{
    std::string msg;

#ifdef DEBUG
    std::cout << "OSCListener: Got OSC bundle." << msg << std::endl;
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

bool OpenSoundControl::initOSCOut(int port)
{
    // Send OSC messages to localhost:UDP port number:
    if (!juceOSCSender.connect("127.0.0.1", port))
    {
        return false;
    }
    sendingOSC = true;
    oportnum = port;
    synth->storage.oscSending = true;

#ifdef DEBUG
    std::cout << "SurgeOSC: Sending OSC on port " << port << "." << std::endl;
#endif
    return true;
}

void OpenSoundControl::stopSending()
{
    if (!sendingOSC)
        return;

    sendingOSC = false;
    synth->storage.oscSending = false;
#ifdef DEBUG
    std::cout << "SurgeOSC: Stopped sending OSC." << std::endl;
#endif
}

void OpenSoundControl::send(std::string addr, std::string msg)
{
    if (sendingOSC)
    {
        // Runs on the juce messenger thread
        juce::MessageManager::getInstance()->callAsync([this, msg, addr]() {
            if (!this->juceOSCSender.send(juce::OSCMessage(juce::String(addr), juce::String(msg))))
                std::cout << "Error: could not send OSC message.";
        });
    }
}

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
            for (int i = 0; i < n; i++)
            {
                Parameter p = *synth->storage.getPatch().param_ptr[i];
                switch (p.valtype)
                {
                case vt_int:
                    valStr = std::to_string(p.val.i);
                    break;

                case vt_bool:
                    valStr = std::to_string(p.val.b);
                    break;

                case vt_float:
                    valStr = float_to_clocalestr(p.val.f);
                    break;

                default:
                    break;
                }

                if (!this->juceOSCSender.send(
                        juce::OSCMessage(juce::String(p.oscName), juce::String(valStr))))
                    std::cout << "Error: could not send OSC message.";
            }
            // delete timer;    // This prints the elapsed time
        });
    }
}

} // namespace OSC
} // namespace Surge