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
#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"

class SurgeSynthProcessor;

namespace Surge
{
namespace OSC
{

class OSCListener : public juce::OSCReceiver,
                    juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
  public:
    OSCListener();
    ~OSCListener();

    bool init(SurgeSynthProcessor *ssp, const std::unique_ptr<SurgeSynthesizer> &surge, int port);
    void stopListening();

    void oscMessageReceived(const juce::OSCMessage &message) override;
    void oscBundleReceived(const juce::OSCBundle &bundle) override;

    int portnum = 0;
    bool listening = false;

  private:
    SurgeSynthesizer *surgePtr{nullptr};
    SurgeSynthProcessor *sspPtr{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCListener)
};

} // namespace OSC
} // namespace Surge