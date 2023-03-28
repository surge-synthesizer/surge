/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "OSCListener.h"

using namespace Surge::OSC;

OSCListener::OSCListener() = default;
OSCListener::~OSCListener() = default;

void OSCListener::connectToOSC(int port) {
     if (!connect(port)) {
          std::cout << "Error: could not connect to UDP port " << std::to_string(port) << std::endl;
     } else {
          addListener(this);
          listening = true;
          std::cout << "SurgeOSC: Listening for OSC on port " << port << "." << std::endl;
     }
}

void OSCListener::stopListening() {
     removeListener(this);
     listening = false;
     std::cout << "SurgeOSC: Stopped listening for OSC." << std::endl;
};

void OSCListener::oscMessageReceived (const juce::OSCMessage& message) {
     std::string addr = "Got OSC msg.: " + 
          message.getAddressPattern().toString().toStdString();
     
     std::cout << "SurgeOSC: " << addr << " " << message[0].getFloat32() << std::endl;
};

void OSCListener::oscBundleReceived (const juce::OSCBundle &bundle) {
     std::string msg = "Got OSC bundle.";
     std::cout << "SurgeOSC: " << msg << std::endl;
     
     for (int i = 0; i < bundle.size(); ++i)
     {
          auto elem = bundle[i];
          if (elem.isMessage())
               oscMessageReceived (elem.getMessage());
          else if (elem.isBundle())
               oscBundleReceived (elem.getBundle());
     }
}


