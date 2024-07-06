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

#include "FXOpenSoundControl.h"
#include "SurgeFXProcessor.h"
#include "Parameter.h"
#include "SurgeStorage.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include "UnitConversions.h"
#include "filesystem/import.h"

namespace SurgeFX
{
namespace FxOSC
{

FXOpenSoundControl::FXOpenSoundControl() {}

FXOpenSoundControl::~FXOpenSoundControl()
{
    if (listening)
    {
        stopListening(false);
    }
}

void FXOpenSoundControl::initOSC(SurgefxAudioProcessor *sfxp,
                                 const std::unique_ptr<SurgeStorage> &stor)
{
    // Init. pointers to synth and synth processor
    storage = stor.get();
    sfxPtr = sfxp;
}

void FXOpenSoundControl::tryOSCStartup()
{
    if (!storage || !sfxPtr)
    {
        storage->reportError("synth or sfxPtr are null", "OSC startup failure");
        return;
    }

    bool startOSCInNow = sfxPtr->oscStartIn;
    if (startOSCInNow)
    {
        int inPort = sfxPtr->oscPortIn;

        if (inPort > 0)
        {
            if (!initOSCIn(inPort))
            {
                sfxPtr->initOSCError(inPort);
            }
        }
    }
}

/* ----- OSC Receiver  ----- */

bool FXOpenSoundControl::initOSCIn(int port)
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
        sfxPtr->oscReceiving = true;
        sfxPtr->oscStartIn = true;
        sfxPtr->oscPortIn = port;
        return true;
    }

    return false;
}

void FXOpenSoundControl::stopListening(bool updateOSCStartInStorage)
{
    if (!listening)
    {
        return;
    }

    removeListener(this);
    listening = false;

    if (storage)
    {
        sfxPtr->oscReceiving = false;
        if (updateOSCStartInStorage)
        {
            sfxPtr->oscStartIn = false;
        }
    }
}

// Concatenates OSC message data strings separated by spaces into one string (with spaces)
std::string FXOpenSoundControl::getWholeString(const juce::OSCMessage &om)
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

void FXOpenSoundControl::oscMessageReceived(const juce::OSCMessage &message)
{
    std::string addr = message.getAddressPattern().toString().toStdString();
    std::stringstream msg;

    if (addr.empty() || addr.at(0) != '/')
    {
        storage->reportError("Bad OSC message format.", "OSC input error");
        return;
    }

    std::istringstream split(addr);
    // Scan past first '/'
    std::string throwaway;
    std::getline(split, throwaway, '/');

    // The only message accepted here is "/fx/param/<n>", where n >= 1 and n <= 12
    std::string addr_part = "";
    std::getline(split, addr_part, '/');

    if (addr_part == "fx")
    {
        int index = 0;
        std::getline(split, addr_part, '/');
        if (addr_part == "param")
        {
            std::getline(split, addr_part, '/');
            try
            {
                index = stoi(addr_part);
                if (index < 1 || index > 12)
                {
                    storage->reportError("Bad FX parameter index. Must be 1-12.",
                                         "OSC input error");
                    return;
                }
            }
            catch (const std::exception &e)
            {
                storage->reportError("Bad format for FX parameter index.", "OSC input error");
                return;
            }
            if (!message[0].isFloat32())
            {
                storage->reportError("Expected float value.", "OSC input error");
                return;
            }

            float newval = message[0].getFloat32();
            // Send message to audio thread
            sfxPtr->oscRingBuf.push(SurgefxAudioProcessor::oscToAudio(
                SurgefxAudioProcessor::FX_PARAM, newval, index - 1));
        }
    }
}

bool FXOpenSoundControl::hasEnding(std::string const &fullString, std::string const &ending)
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

void FXOpenSoundControl::oscBundleReceived(const juce::OSCBundle &bundle)
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

} // namespace FxOSC
} // namespace SurgeFX
