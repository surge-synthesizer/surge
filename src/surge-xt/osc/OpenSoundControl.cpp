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

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "OpenSoundControl.h"
#include "Parameter.h"
#include "SurgeSynthProcessor.h"
#include "SurgeStorage.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include "UnitConversions.h"
#include "Tunings.h"
#include "filesystem/import.h"

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
        int defaultOSCInPort = synth->storage.getPatch().dawExtraState.oscPortIn;

        if (defaultOSCInPort > 0)
        {
            if (!initOSCIn(defaultOSCInPort))
            {
                sspPtr->initOSCError(defaultOSCInPort);
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
    if (port < 1 || port == oportnum)
    {
        return false;
    }

    if (connect(port))
    {
        addListener(this);
        listening = true;
        iportnum = port;
        synth->storage.oscReceiving = true;
        synth->storage.oscStartIn = true;
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
        synth->storage.oscReceiving = false;
        if (updateOSCStartInStorage)
        {
            synth->storage.oscStartIn = false;
        }
    }
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
    if (addr.empty() || addr.at(0) != '/')
    {
        sendError("Bad OSC message format.");
        return;
    }

    std::istringstream split(addr);
    // Scan past first '/'
    std::string throwaway;
    std::getline(split, throwaway, '/');

    std::string addr_part = "";
    std::getline(split, addr_part, '/');
    bool querying = false;

    // Queries (for params and modulators)
    if (addr_part == "q")
    {
        querying = true;
        // remove the '/q' from the address
        std::getline(split, addr_part, '/');
        addr = addr.substr(2);
        if (addr_part == "all_params")
        {
            OpenSoundControl::sendAllParams();
            return;
        }
        if (addr_part == "all_mods")
        {
            OpenSoundControl::sendAllModulators();
            return;
        }
    }

    // 'Frequency' notes
    if (addr_part == "fnote" && !querying)
    // Play a note at the given frequency and velocity
    {
        int32_t noteID = 0;
        std::getline(split, addr_part, '/'); // check for '/rel'

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

        bool noteon = (addr_part != "rel") && (velocity != 0);

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
        sspPtr->oscRingBuf.push(
            SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::FREQNOTE, nullptr, frequency, 0, 0,
                                            static_cast<char>(velocity), noteon, noteID, 0, 0));
    }

    // "MIDI-style" notes
    else if (addr_part == "mnote" && !querying)
    // OSC equivalent of MIDI note
    {
        int32_t noteID = 0;

        if (message.size() < 2 || message.size() > 3)
        {
            sendDataCountError("mnote", "2 or 3");
            return;
        }

        std::getline(split, addr_part, '/'); // check for '/rel'

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
        bool noteon = (addr_part != "rel") && (velocity != 0);

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
            SurgeSynthProcessor::MNOTE, nullptr, 0.0, 0, static_cast<char>(note),
            static_cast<char>(velocity), noteon, noteID, 0, 0));
    }
    else if (addr_part == "pbend" && !querying)
    {
        if (message.size() != 2)
        {
            sendDataCountError(addr_part, "2");
        }
        if (!message[0].isFloat32() || !message[1].isFloat32())
        {
            sendNotFloatError(addr_part, "channel or value");
            return;
        }

        int chan = static_cast<int>(message[0].getFloat32());
        if ((chan < 0) || (chan > 15))
        {
            sendError("/pbend channel must be >= 0. and <= 15.");
            return;
        }

        float bend = message[1].getFloat32();
        if ((bend >= -1.0) && (bend <= 1.0))
        {
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::PITCHBEND, nullptr, 0.0, static_cast<int>(bend * 8192),
                static_cast<char>(chan), 0, 0, 0, 0, 0));
        }
        else
            sendError("/pbend value must be between -1.0 and 1.0 .");
    }

    // Note expressions
    else if (addr_part == "ne" && !querying)
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

        std::getline(split, addr_part, '/');
        if (addr_part == "volume")
        {
            if (val < 0.0 || val > 4.0)
            {
                sendError("Note expression (volume) '" + std::to_string(val) +
                          "' is out of range (0.0 - 4.0).");
                return;
            }
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::NOTEX_VOL, nullptr, val, 0, 0, 0, 0, noteID, 0, 0));
        }
        else if (addr_part == "pitch")
        {
            if (val < -120.0 || val > 120.0)
            {
                sendError("Note expression (pitch) '" + std::to_string(val) +
                          "' is out of range (-120.0 - 120.0).");
                return;
            }
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::NOTEX_PITCH, nullptr, val, 0, 0, 0, 0, noteID, 0, 0));
        }
        else if (addr_part == "pan")
        {
            if (val < 0.0 || val > 1.0)
            {
                sendError("Note expression (pan) '" + std::to_string(val) +
                          "' is out of range (0.0 - 1.0).");
                return;
            }
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::NOTEX_PAN, nullptr, val, 0, 0, 0, 0, noteID, 0, 0));
        }
        else if (addr_part == "timbre")
        {
            if (val < 0.0 || val > 1.0)
            {
                sendError("Note expression (timbre) '" + std::to_string(val) +
                          "' is out of range (0.0 - 1.0).");
                return;
            }
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::NOTEX_TIMB, nullptr, val, 0, 0, 0, 0, noteID, 0, 0));
        }
        else if (addr_part == "pressure")
        {
            if (val < 0.0 || val > 1.0)
            {
                sendError("Note expression (pressure) '" + std::to_string(val) +
                          "' is out of range (0.0 - 1.0).");
                return;
            }
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::NOTEX_PRES, nullptr, val, 0, 0, 0, 0, noteID, 0, 0));
        }
    }
    else if (addr_part == "cc" && !querying)
    {
        if (message.size() != 3)
        {
            sendDataCountError(addr_part, "3");
        }
        if (!(message[0].isFloat32() && message[1].isFloat32() && message[2].isFloat32()))
        {
            sendNotFloatError(addr_part, "channel, control number, or value");
            return;
        }
        float chan = message[0].getFloat32();
        float cnum = message[1].getFloat32();
        float val = message[2].getFloat32();

        if ((chan >= 0.0) && (chan <= 15.) && (cnum >= 0.0) && (cnum <= 127.) && (val >= 0.0) &&
            (val <= 127.0))
        {
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::CC, nullptr, 0.0, static_cast<int>(val),
                static_cast<char>(chan), static_cast<char>(cnum), 0, 0, 0, 0));
        }
        else
            sendMidiBoundsError(addr_part);
    }

    else if (addr_part == "chan_at" && !querying)
    {
        if (message.size() != 2)
        {
            sendDataCountError(addr_part, "2");
        }
        if (!(message[0].isFloat32() && message[1].isFloat32()))
        {
            sendNotFloatError(addr_part, "channel or value");
            return;
        }
        float chan = message[0].getFloat32();
        float val = message[1].getFloat32();

        if ((chan >= 0.0) && (chan <= 15.) && (val >= 0.0) && (val <= 127.0))
        {
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::CHAN_ATOUCH, nullptr, 0.0, static_cast<int>(val),
                static_cast<char>(chan), 0, 0, 0, 0, 0));
        }
        else
            sendMidiBoundsError(addr_part);
    }

    else if (addr_part == "poly_at" && !querying)
    {
        if (message.size() != 3)
        {
            sendDataCountError(addr_part, "3");
        }
        if (!(message[0].isFloat32() && message[1].isFloat32() && message[2].isFloat32()))
        {
            sendNotFloatError(addr_part, "channel, note number, or value");
            return;
        }
        float chan = message[0].getFloat32();
        float nnum = message[1].getFloat32();
        float val = message[2].getFloat32();

        if ((chan >= 0.0) && (chan <= 15.) && (nnum >= 0.0) && (nnum <= 127.) && (val >= 0.0) &&
            (val <= 127.0))
        {
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::POLY_ATOUCH, nullptr, 0.0, static_cast<int>(val),
                static_cast<char>(chan), static_cast<char>(nnum), 0, 0, 0, 0));
        }
        else
            sendMidiBoundsError(addr_part);
    }

    // All notes off
    else if (addr_part == "allnotesoff")
    {
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::ALLNOTESOFF,
                                                                nullptr, 0.0, 0, 0, 0, 0, 0, 0, 0));
    }

    else if (addr_part == "allsoundoff")
    {
        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::ALLSOUNDOFF,
                                                                nullptr, 0.0, 0, 0, 0, 0, 0, 0, 0));
    }

    // Parameters
    else if (addr_part == "param")
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
        std::getline(split, addr_part, '/'); // check for '/macro'
        if (addr_part == "macro")
        {
            int macnum;
            std::getline(split, addr_part, '/'); // get macro num
            try
            {
                macnum = stoi(addr_part);
            }
            catch (...)
            {
                sendError("OSC /param/macro: Invalid macro number: " + addr_part);
                return;
            }
            if ((macnum <= 0) || (macnum > n_customcontrollers))
            {
                sendError("OSC /param/macro: Invalid macro number: " + addr_part);
                return;
            }
            if (querying)
            {
                OpenSoundControl::sendMacro(macnum - 1, true);
            }
            else
                sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                    SurgeSynthProcessor::MACRO, nullptr, val, --macnum, 0, 0, 0, 0, 0, 0));
        }

        // Special case for extended parameter options
        else if (hasEnding(addr, "+"))
        {
            size_t last_slash = addr.find_last_of("/");
            std::string rootaddr = addr.substr(0, last_slash);
            auto *p = synth->storage.getPatch().parameterFromOSCName(rootaddr);
            if (p == NULL)
            {
                sendError("No parameter with OSC address of " + addr);
                // Not a valid OSC address
                return;
            }
            std::string extension = addr.substr(last_slash + 1);
            extension.erase(extension.size() - 1);
            if (querying)
            {
                sendError("Can't query directly for parameter extended options. They are supplied "
                          "with the parameter query.");
                return;
            }
            else
            {
                if (extension == "abs")
                {
                    if (!p->can_be_absolute())
                        sendError("Param " + p->oscName + " can't be absolute.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::ABSOLUTE_X, p, 0.0, 0, 0, 0,
                            static_cast<bool>(val), 0, 0, 0));
                }
                else if (extension == "enable")
                {
                    if (!p->can_deactivate())
                        sendError("Param " + p->oscName + " doesn't support enabling/disabling.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::ENABLE_X, p, 0.0, 0, 0, 0, static_cast<bool>(val),
                            0, 0, 0));
                }
                else if (extension == "tempo_sync")
                {
                    if (!p->can_temposync())
                        sendError("Param " + p->oscName + " can't tempo-sync.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::TEMPOSYNC_X, p, 0.0, 0, 0, 0,
                            static_cast<bool>(val), 0, 0, 0));
                }
                else if (extension == "extend")
                {
                    if (!p->can_extend_range())
                        sendError("Param " + p->oscName + " can't extend range.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::EXTEND_X, p, 0.0, 0, 0, 0, static_cast<bool>(val),
                            0, 0, 0));
                }
                else if (extension == "deform")
                {
                    if (!p->has_deformoptions())
                        sendError("Param " + p->oscName + " doesn't have deform options.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::DEFORM_X, p, 0.0, static_cast<int>(val), 0, 0, 0,
                            0, 0, 0));
                }
                else if (extension == "const_rate")
                {
                    if (!p->has_portaoptions())
                        sendError("Param " + p->oscName + " doesn't have portamento options.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::PORTA_CONSTRATE_X, p, 0.0, 0, 0, 0,
                            static_cast<bool>(val), 0, 0, 0));
                }
                else if (extension == "gliss")
                {
                    if (!p->has_portaoptions())
                        sendError("Param " + p->oscName + " doesn't have portamento options.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::PORTA_GLISS_X, p, 0.0, 0, 0, 0,
                            static_cast<bool>(val), 0, 0, 0));
                }
                else if (extension == "retrig")
                {
                    if (!p->has_portaoptions())
                        sendError("Param " + p->oscName + " doesn't have portamento options.");
                    else
                        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                            SurgeSynthProcessor::PORTA_RETRIGGER_X, p, 0.0, 0, 0, 0,
                            static_cast<bool>(val), 0, 0, 0));
                }
                else if (extension == "curve")
                {
                    if (!p->has_portaoptions())
                        sendError("Param " + p->oscName + " doesn't have portamento options.");
                    else
                    {
                        int new_curve = static_cast<int>(val);
                        if ((new_curve < -1) || (new_curve > 1))
                            new_curve = 0;
                        sspPtr->oscRingBuf.push(
                            SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::PORTA_CURVE_X, p,
                                                            0.0, new_curve, 0, 0, false, 0, 0, 0));
                    }
                }
                else
                {
                    sendError("Unknown parameter option: " + extension + "+");
                }
            }
        }

        // Special case for /param/fx/<s>/<n>/deactivate, which is not a true 'parameter'
        else if ((addr_part == "fx") && (hasEnding(addr, "deactivate")))
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
                std::string deactivated = ""; // isDeact ? "deactivated" : "activated";
                std::string addr = "/param/" + shortOSCname + "/deactivate";
                float val = (float)isDeact;
                juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
                om.addFloat32(val);
                om.addString(juce::String(deactivated));
                OpenSoundControl::send(om, true);
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
                sspPtr->oscRingBuf.push(
                    SurgeSynthProcessor::oscToAudio(SurgeSynthProcessor::FX_DISABLE, nullptr, 0.0,
                                                    selected_mask, 0, 0, onoff > 0, 0, 0, 0));
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
                sendAllParameterInfo(p, true);
            }
            else
            {
                sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(p, val));
            }
        }
    }

    // Wavetable control
    else if (addr_part == "wavetable")
    {
        std::string scene_str = "";
        std::getline(split, scene_str, '/');
        if (!(scene_str == "a" || scene_str == "b"))
        {
            sendError("/wavetable must specify a scene (a or b)");
            return;
        }
        int scene_num = scene_str == "a" ? 0 : 1;

        std::getline(split, addr_part, '/');
        if (addr_part != "osc")
        {
            sendError("/wavetable: bad message format (no '/osc/')");
            return;
        }

        std::getline(split, addr_part, '/');
        if (!(addr_part == "1" || addr_part == "2" || addr_part == "3"))
        {
            sendError("/wavetable: oscillator number out of range (1-3)");
            return;
        }
        int osc_num = stoi(addr_part);

        OscillatorStorage *oscdata = &(synth->storage.getPatch().scene[scene_num].osc[osc_num - 1]);

        if (querying)
        {
            std::stringstream addr;
            addr << "/wavetable/" << scene_str << "/osc/" << osc_num << "/id";
            juce::OSCMessage om =
                juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr.str())));
            om.addFloat32(oscdata->wt.current_id);
            om.addString(synth->storage.getCurrentWavetableName(oscdata));

            OpenSoundControl::send(om, true);
            return;
        }

        std::getline(split, addr_part, '/');

        int new_id = 0;
        if (addr_part == "incr")
        {
            if (!querying)
            {
                new_id = synth->storage.getAdjacentWaveTable(oscdata->wt.current_id, true);
            }
        }
        else if (addr_part == "decr")
        {
            if (!querying)
            {
                new_id = synth->storage.getAdjacentWaveTable(oscdata->wt.current_id, false);
            }
        }
        else if (addr_part == "id")
        {
            if (!message[0].isFloat32())
            {
                sendNotFloatError("wavetable", "wavetable #");
                return;
            }

            // Select the new waveform (invalid ids are ignored)
            new_id = (int)(message[0].getFloat32());
        }
        oscdata->wt.queue_id = new_id;
    }

    // Patch changing
    else if (addr_part == "patch")
    {
        if (querying)
        {
            std::string patchpath = synth->storage.lastLoadedPatch.replace_extension().u8string();

            if (!patchpath.empty())
            {
                sendPath(patchpath);
            }
            else
            {
                sendPath("<unknown patch>");
            }
            return;
        }

        std::getline(split, addr_part, '/');
        if (addr_part == "load")
        {
            std::string patchPath = getWholeString(message) + ".fxp";
            if (!fs::exists(patchPath))
            {
                sendError("File not found: " + patchPath);
                return;
            }
            {
                std::lock_guard<std::mutex> mg(synth->patchLoadSpawnMutex);
                strncpy(synth->patchid_file, patchPath.c_str(), FILENAME_MAX);
                synth->has_patchid_file = true;
            }
            synth->processAudioThreadOpsWhenAudioEngineUnavailable();
        }

        else if (addr_part == "load_user")
        {
            std::string dataStr = getWholeString(message);
            fs::path patchPath = synth->storage.userPatchesPath;
            patchPath += dataStr += ".fxp";
            if (!fs::exists(patchPath))
            {
                sendError("File not found: " + patchPath.string());
                return;
            }
            {
                std::lock_guard<std::mutex> mg(synth->patchLoadSpawnMutex);
                strncpy(synth->patchid_file, patchPath.u8string().c_str(), FILENAME_MAX);
                synth->has_patchid_file = true;
            }
            synth->processAudioThreadOpsWhenAudioEngineUnavailable();
        }
        else if (addr_part == "save")
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
        else if (addr_part == "save_user")
        {
            // Run this on the juce messenger thread
            juce::MessageManager::getInstance()->callAsync([this, message]() {
                std::string dataStr = getWholeString(message);
                fs::path ppath = synth->storage.userPatchesPath;
                ppath += dataStr += ".fxp";

                if (!fs::exists(ppath.parent_path()))
                {
                    try
                    {
                        fs::create_directories(ppath.parent_path());
                    }
                    catch (const fs::filesystem_error &e)
                    {
                        sendError("User patch directory not available: " + std::string(e.what()));
                        return;
                    }
                }

                synth->savePatchToPath(ppath);
            });
        }
        else if (addr_part == "random")
        {
            synth->selectRandomPatch();
        }
        else if (addr_part == "incr")
        {
            synth->jogPatch(true);
        }
        else if (addr_part == "decr")
        {
            synth->jogPatch(false);
        }
        else if (addr_part == "incr_category")
        {
            synth->jogCategory(true);
        }
        else if (addr_part == "decr_category")
        {
            synth->jogCategory(false);
        }
    }

    // Tuning switching
    else if (addr_part == "tuning")
    {
        fs::path path = getWholeString(message);
        fs::path def_path;

        std::getline(split, addr_part, '/');
        // Tuning files path control
        if (addr_part == "path")
        {
            if (querying)
                return; // Not supported

            std::string dataStr = getWholeString(message);
            if ((dataStr != "_reset") && (!fs::exists(dataStr)))
            {
                sendError("An OSC 'tuning/path/...' message was received with a path which does "
                          "not exist: the default path will not change.");
                return;
            }

            fs::path ppath = fs::path(dataStr);
            std::getline(split, addr_part, '/');

            if (addr_part == "scl")
            {
                if (dataStr == "_reset")
                {
                    if (querying)
                        return; // Not sensical
                    ppath = synth->storage.datapath;
                    ppath /= "tuning_library/SCL";
                }
                Surge::Storage::updateUserDefaultPath(&(synth->storage),
                                                      Surge::Storage::LastSCLPath, ppath);
            }
            else if (addr_part == "kbm")
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
        else if (addr_part == "scl")
        {
            if (querying)
            {
                auto tuningLabel = path_to_string(fs::path(synth->storage.currentScale.name));
                tuningLabel = tuningLabel.substr(0, tuningLabel.find_last_of("."));
                if (tuningLabel == "Scale from patch")
                    tuningLabel = "(standard)";
                std::string addr = "/tuning/scl";
                juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
                om.addString(tuningLabel);
                OpenSoundControl::send(om, true);
                return;
            }

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
            synth->refresh_editor = true;
        }
        // KBM mapping file selection
        else if (addr_part == "kbm")
        {
            if (querying)
            {
                auto mappingLabel = synth->storage.currentMapping.name;
                mappingLabel = mappingLabel.substr(0, mappingLabel.find_last_of("."));
                if (mappingLabel == "")
                    mappingLabel = "(standard)";
                std::string addr = "/tuning/kbm";
                juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
                om.addString(mappingLabel);
                OpenSoundControl::send(om, true);
                return;
            }

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
            synth->refresh_editor = true;
        }
    }

    // Modulation mapping
    else if (addr_part == "mod")
    {
        if (querying && message.size() < 1)
        {
            sendError("Modulation query must specify both mod source and mod target.");
            return;
        }

        if (!querying && message.size() < 2)
        {
            sendDataCountError("mod", "2");
            return;
        }

        bool muteMsg = false;
        std::getline(split, addr_part, '/');
        if (addr_part == "mute")
        {
            muteMsg = true;
            std::getline(split, addr_part, '/');
        }

        int mscene = 0;
        bool scene_specified = false;
        if ((addr_part == "a") || (addr_part == "b"))
        {
            scene_specified = true;
            if (addr_part == "b")
                mscene = 1;
            std::getline(split, addr_part, '/'); // get mod into addr_part
        }

        int modnum = -1;
        for (int i = 0; i < n_modsources; i++)
        {
            if (modsource_names_tag[i] == addr_part)
            {
                modnum = i;
                break;
            }
        }

        if (modnum == -1)
        {
            sendError("Bad modulator name: " + addr_part);
            return;
        }

        int index = 0;
        addr_part = "";
        std::getline(split, addr_part, '/'); // addr_part = index, if any
        if (!addr_part.empty())
        {
            try
            {
                index = stoi(addr_part);
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

        if (!querying && !message[1].isFloat32())
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

        // modulator querying
        if (querying)
        {
            if (muteMsg)
            {
                bool modMuted = synth->isModulationMuted(p->id, (modsources)modnum, mscene, index);
                modOSCout(addr, p->oscName, modMuted, false);
            }
            else
            {
                float depth = synth->getModDepth01(p->id, (modsources)modnum, mscene, index);
                modOSCout(addr, p->oscName, depth, false);
            }
            return;
        }

        if (muteMsg)

            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::MOD_MUTE, p, depth, modnum, 0, 0, 0, 0, mscene, index));
        else
            sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscToAudio(
                SurgeSynthProcessor::MOD, p, depth, modnum, 0, 0, 0, 0, mscene, index));
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
    if (port < 1 || port == iportnum)
    {
        return false;
    }

    stopSending();

    // Send OSC messages to IP Address:UDP port number
    if (!juceOSCSender.connect(ipaddr, port))
    {
        return false;
    }

    // Add listener for patch changes, to send new path to OSC output
    // This will run on the juce::MessageManager thread so as to
    // not tie up the patch loading thread.
    synth->addPatchLoadedListener(
        "OSC_OUT", [ssp = sspPtr](auto s) { ssp->patch_load_to_OSC(s.replace_extension()); });

    // Add a listener for parameter changes
    sspPtr->addParamChangeListener("OSC_OUT", [ssp = sspPtr](auto str1, auto numvals, auto float0,
                                                             auto float1, auto float2, auto str2) {
        ssp->param_change_to_OSC(str1, numvals, float0, float1, float2, str2);
    });

    // Add a listener for parameter changes that happen on the audio thread
    //  (e.g. MIDI-'learned' parameters being changed by incoming MIDI messages)
    synth->addAudioParamListener(
        "OSC_OUT", [this, ssp = sspPtr](std::string oname, float fval, std::string valstr) {
            assert(juce::MessageManager::getInstanceWithoutCreating());
            auto *mm = juce::MessageManager::getInstanceWithoutCreating();
            if (mm)
            {
                mm->callAsync([ssp, oname, fval, valstr]() {
                    ssp->param_change_to_OSC(oname, 1, fval, 0., 0., valstr);
                });
            }
            else
            {
                std::cerr << "The juce message manager is not running. You are misconfigured"
                          << std::endl;
            }
        });

    // Add a listener for modulation changes
    synth->addModulationAPIListener(this);

    sendingOSC = true;
    oportnum = port;
    outIPAddr = ipaddr;
    synth->storage.oscSending = true;
    synth->storage.oscStartOut = true;

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
    synth->deleteAudioParamListener("OSC_OUT");
    sspPtr->deleteParamChangeListener("OSC_OUT");

    if (updateOSCStartInStorage)
    {
        synth->storage.oscStartOut = false;
    }
}

void OpenSoundControl::send(juce::OSCMessage om, bool needsMessageThread)
{
    if (sendingOSC)
    {
        if (needsMessageThread)
        {
            // Runs on the juce messenger thread
            juce::MessageManager::getInstance()->callAsync([this, om]() {
                if (!this->juceOSCSender.send(om))
                    sendFailed();
            });
        }
        else
        {
            // Send OSC directly (used when already on the messenger thread)
            {
                if (!this->juceOSCSender.send(om))
                    sendFailed();
            }
        }
    }
}

void OpenSoundControl::sendFailed() { std::cout << "Error: could not send OSC message."; }

void OpenSoundControl::sendError(std::string errorMsg)
{
    if (sendingOSC)
    {
        juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String("/error")));
        om.addString(errorMsg);
        OpenSoundControl::send(om, true);
    }
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

void OpenSoundControl::sendMidiBoundsError(std::string addr)
{
    OpenSoundControl::sendError("All values for '/" + addr +
                                "/...' messages must be greater "
                                "than 0.0 and less than 127.0");
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
                sendAllParameterInfo(p, false);
            }
            // Now do the macros
            for (int i = 0; i < n_customcontrollers; i++)
            {
                sendMacro(i, false);
            }
            // delete timer;    // This prints the elapsed time
        });
    }
}

// Send one message for every extended option for the given parameter
void OpenSoundControl::sendParameterExtOptions(const Parameter *p, bool needsMessageThread)
{
    if (p->can_be_absolute())
        sendParameter(p, needsMessageThread, "abs");
    if (p->can_deactivate())
        sendParameter(p, needsMessageThread, "enable");
    if (p->can_temposync())
        sendParameter(p, needsMessageThread, "tempo_sync");
    if (p->can_extend_range())
        sendParameter(p, needsMessageThread, "extend");
    if (p->has_deformoptions())
        sendParameter(p, needsMessageThread, "deform");
    if (p->has_portaoptions())
    {
        sendParameter(p, needsMessageThread, "const_rate");
        sendParameter(p, needsMessageThread, "gliss");
        sendParameter(p, needsMessageThread, "retrig");
        sendParameter(p, needsMessageThread, "curve");
    }
}

// Loop throuh all modulators, send them to OSC Out
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

void OpenSoundControl::sendModulator(ModulationRouting mod, int scene, bool global)
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
    modOSCout(addr, p->oscName, val, false);
    // Now send the mute status for this mapping
    val = mod.muted ? 1.0 : 0.0;
    modOSCout(addr, p->oscName, val, true);
}

void OpenSoundControl::modOSCout(std::string addr, std::string oscName, float val, bool reportMute)
{
    std::string paramAddr = addr;
    if (reportMute)
        paramAddr = addr.insert(4, "/mute");
    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    om.addString(oscName);
    om.addFloat32(val);

    // Sending directly here, because we're already on the JUCE messenger thread
    OpenSoundControl::send(om, false);
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

void OpenSoundControl::sendMacro(long macnum, bool needsMessageThread)
{
    auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                    .scene[0]
                    .modsources[macnum + ms_ctrl1]);
    auto valStr = float_to_clocalestr_wprec(100 * cms->get_output(0), 3) + " %";
    float val01 = cms->get_output01(0);
    std::string addr = "/param/macro/" + std::to_string(macnum + 1);

    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    om.addFloat32(val01);
    if (!valStr.empty())
        om.addString(valStr);

    OpenSoundControl::send(om, needsMessageThread);
}

void OpenSoundControl::sendPath(std::string pathString)
{
    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String("/patch")));
    om.addString(pathString);
    OpenSoundControl::send(om, true);
}

/* Send the OSC address and value of the supplied parameter or extended option
    If 'extension' is not empty, it specifies the extended parameter option to report.
    'extension' equals "", by default.
*/
void OpenSoundControl::sendParameter(const Parameter *p, bool needsMessageThread,
                                     std::string extension)
{
    std::string valStr = "";
    float val01 = 0.0;
    std::string addr = "";

    if (extension == "")
    {
        addr = p->oscName;
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
    }
    else
    {
        if (extension == "abs")
            val01 = (float)p->absolute;
        if (extension == "enable")
            val01 = (float)!p->deactivated;
        if (extension == "tempo_sync")
            val01 = (float)p->temposync;
        if (extension == "extend")
            val01 = (float)p->extend_range;
        if (extension == "deform")
            val01 = (float)p->deform_type;
        if (extension == "const_rate")
            val01 = (float)p->porta_constrate;
        if (extension == "gliss")
            val01 = (float)p->porta_gliss;
        if (extension == "retrig")
            val01 = (float)p->porta_retrigger;
        if (extension == "curve")
            val01 = (float)p->porta_curve;
        addr = p->oscName + "/" + extension + "+";
    }

    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    om.addFloat32(val01);
    if (!valStr.empty())
        om.addString(valStr);
    OpenSoundControl::send(om, needsMessageThread);
}

void OpenSoundControl::sendParameterDocs(const Parameter *p, bool needsMessageThread)
{
    // fetch param name/description and set unused params to 'Disabled'
    // note: unused parameters will still return doc values!
    std::string paramName = p->get_name();
    if (paramName.find("Param ") != std::string::npos || paramName == "-")
        paramName = "Disabled"; // Not a great solution. Maybe turn into a bool.

    // determine value type and set min/max value
    std::string valMin = "";
    std::string valMax = "";
    std::string paramType = "";

    switch (p->valtype)
    {
    case vt_int:
        paramType = "int";
        valMin = std::to_string(p->val_min.i);
        valMax = std::to_string(p->val_max.i);
        break;

    case vt_bool:
        paramType = "bool";
        valMin = std::to_string(p->val_min.b);
        valMax = std::to_string(p->val_max.b);
        break;

    case vt_float:
        paramType = "float";
        valMin = std::to_string(p->val_min.f);
        valMax = std::to_string(p->val_max.f);
        break;

    default:
        paramType = std::to_string(p->valtype);
        break;
    }

    // Adding the doc prefix to the address again
    std::string addr = "/doc" + p->oscName;

    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    om.addString(paramName);
    om.addString(paramType);
    om.addString(valMin);
    om.addString(valMax);

    OpenSoundControl::send(om, needsMessageThread);
}

void OpenSoundControl::sendParameterExtDocs(const Parameter *p, bool needsMessageThread)
{
    // Note: this message is always sent, even when no ext params are found.
    // This way OSC clients can reliably determine the available ext params: there is a
    // 'null value' in the form of an empty message.
    std::vector<std::string> available_ext;

    if (p->can_be_absolute())
        available_ext.push_back("abs");
    if (p->can_deactivate())
        available_ext.push_back("enable");
    if (p->can_temposync())
        available_ext.push_back("tempo_sync");
    if (p->can_extend_range())
        available_ext.push_back("extend");
    if (p->has_deformoptions())
        available_ext.push_back("deform");
    if (p->has_portaoptions())
    {
        for (const auto &ext : {"const_rate", "gliss", "retrig", "curve"})
        {
            available_ext.push_back(ext);
        }
    };

    // Adding the doc prefix to the address again
    std::string addr = "/doc" + p->oscName + "/ext";
    juce::OSCMessage om = juce::OSCMessage(juce::OSCAddressPattern(juce::String(addr)));
    for (const auto &ext : available_ext)
    {
        om.addString(ext);
    }
    OpenSoundControl::send(om, needsMessageThread);
}

void OpenSoundControl::sendAllParameterInfo(const Parameter *p, bool needsMessageThread)
{
    sendParameter(p, needsMessageThread);
    sendParameterExtOptions(p, needsMessageThread);
    sendParameterDocs(p, needsMessageThread);
    sendParameterExtDocs(p, needsMessageThread);
}

// ModulationAPIListener Implementation
std::atomic<bool> modChanging = false;

void OpenSoundControl::modSet(long ptag, modsources modsource, int modsourceScene, int index,
                              float val, bool isNew)
{
    if (!modChanging)
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

void OpenSoundControl::modBeginEdit(long ptag, modsources modsource, int modsourceScene, int index,
                                    float depth01)
{
    modChanging = true;
};

void OpenSoundControl::modEndEdit(long ptag, modsources modsource, int modsourceScene, int index,
                                  float depth01)
{
    modChanging = false;
    sendMod(ptag, modsource, modsourceScene, index, depth01, false);
};

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
