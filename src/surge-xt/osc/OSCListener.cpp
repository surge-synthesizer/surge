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

#include "OSCListener.h"
#include "Parameter.h"
#include "SurgeSynthProcessor.h"
#include <sstream>
#include <vector>
#include <string>

namespace Surge
{
namespace OSC
{

OSCListener::OSCListener() {}

OSCListener::~OSCListener()
{
    if (listening)
        stopListening();
}

bool OSCListener::init(SurgeSynthProcessor *ssp, const std::unique_ptr<SurgeSynthesizer> &surge,
                       int port)
{
    if (!connect(port))
    {
#ifdef DEBUG
        std::cout << "Error: could not connect to UDP port " << std::to_string(port) << std::endl;
#endif
        return false;
    }
    else
    {
        addListener(this);
        listening = true;
        portnum = port;
        synth = surge.get();
        sspPtr = ssp;

        synth->storage.oscListenerRunning = true;

#ifdef DEBUG
        std::cout << "SurgeOSC: Listening for OSC on port " << port << "." << std::endl;
#endif
        return true;
    }
}

void OSCListener::stopListening()
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
std::string OSCListener::getWholeString(const juce::OSCMessage &om)
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

void OSCListener::oscMessageReceived(const juce::OSCMessage &message)
{
    std::string addr = message.getAddressPattern().toString().toStdString();
    if (addr.at(0) != '/')
        // ignore malformed OSC
        return;

    std::istringstream split(addr);
    // Scan past first '/'
    std::string throwaway;
    std::getline(split, throwaway, '/');

    std::string address1, address2, address3;
    std::getline(split, address1, '/');

    if (address1 == "param")
    {
        auto *p = synth->storage.getPatch().parameterFromOSCName(addr);
        if (p == NULL)
        {
#ifdef DEBUG
            std::cout << "No parameter with OSC address of " << addr << std::endl;
#endif
            // Not a valid OSC address
            return;
        }

        if (!message[0].isFloat32())
        {
            // Not a valid data value
            return;
        }

        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscMsg(p, message[0].getFloat32()));

#ifdef DEBUG_VERBOSE
        std::cout << "Parameter OSC name:" << p->get_osc_name() << "  ";
        std::cout << "Parameter full name:" << p->get_full_name() << std::endl;
#endif
    }
    else if (address1 == "user_default")
    {
        std::getline(split, address2, '/');
        if (address2 == "path")
        {
            std::string dataStr = getWholeString(message);
            std::getline(split, address3, '/');
            if (address3 == "scl")
            {
                Surge::Storage::updateUserDefaultPath(&(synth->storage),
                                                      Surge::Storage::LastSCLPath, dataStr);
            }
            else if (address3 == "kbm")
            {
                Surge::Storage::updateUserDefaultPath(&(synth->storage),
                                                      Surge::Storage::LastKBMPath, dataStr);
            }
        }
    }
    else if (address1 == "tuning")
    {
        std::string dataStr = getWholeString(message);
        std::getline(split, address2, '/');

        if (address2 == "scl")
        {
            auto scl_path = synth->storage.datapath / "tuning_library" / "SCL";
            scl_path = Surge::Storage::getUserDefaultPath(&(synth->storage),
                                                          Surge::Storage::LastSCLPath, scl_path);
            scl_path /= dataStr;
            scl_path += ".scl";
            synth->storage.loadTuningFromSCL(scl_path);
        }
        else if (address2 == "kbm")
        {
            auto kbm_path = synth->storage.datapath / "tuning_library" / "KBM Concert Pitch";
            kbm_path = Surge::Storage::getUserDefaultPath(&(synth->storage),
                                                          Surge::Storage::LastKBMPath, kbm_path);
            kbm_path /= dataStr;
            kbm_path += ".kbm";
            synth->storage.loadMappingFromKBM(kbm_path);
        }
    }
}

void OSCListener::oscBundleReceived(const juce::OSCBundle &bundle)
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

} // namespace OSC
} // namespace Surge
