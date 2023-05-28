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

#include "OSCSender.h"
#include <sstream>
#include <vector>
#include <string>

namespace Surge
{
namespace OSC
{

OSCSender::OSCSender() {}

OSCSender::~OSCSender() { juceOSCSender.disconnect(); }

bool OSCSender::init(const std::unique_ptr<SurgeSynthesizer> &surge, int port)
{
    synth = surge.get();
    // specify here where to send OSC messages to: host URL and UDP port number
    if (!juceOSCSender.connect("127.0.0.1", port))
    {
#ifdef DEBUG
        std::cout << "Surge OSCSender: failed to connect to UDP port " << port << "." << std::endl;
#endif
        return false;
    }
    sendingOSC = true;
    portnum = port;
    synth->storage.oscSending = true;

#ifdef DEBUG
    std::cout << "SurgeOSC: Sending OSC on port " << port << "." << std::endl;
#endif
    return true;
}

void OSCSender::stopSending()
{
    if (!sendingOSC)
        return;

    sendingOSC = false;
    synth->storage.oscSending = false;
#ifdef DEBUG
    std::cout << "SurgeOSC: Stopped sending OSC." << std::endl;
#endif
}

/*
    {
        // create and send an OSC message with an address and a float value:
        if (!juceOSCSender.send("/juce/rotaryknob", (float)rotaryKnob.getValue()))
            showConnectionErrorMessage("Error: could not send OSC message.");
    };

*/

} // namespace OSC
} // namespace Surge
