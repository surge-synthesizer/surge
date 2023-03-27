#pragma once
/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "juce_osc/juce_osc.h"

namespace Surge
{
namespace OSC
{

static bool listening = false;
static void stopListening() {
     listening = false;
};

class OSCListener
          : private juce::OSCReceiver,
                    juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
     OSCListener(int port) {
          if (!connect(port)) {
               std::cout << "Error: could not connect to UDP port " << std::to_string(port) << std::endl;
          } else {
               addListener(this);
               listening = true;
               std::cout << "SurgeOSC: Listening for OSC on port " << port << "." << std::endl;
          }
     };

private:
     void oscMessageReceived (const juce::OSCMessage& message) override {
          std::string msg = "Got OSC msg.";
          // msg = message.getAddressPattern().toString();
          std::cout << "SurgeOSC: " << msg << std::endl;
     };

     void oscBundleReceived (const juce::OSCBundle &bundle) override {
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCListener)
};

}    // namespace OSC
}    // namespace Surge