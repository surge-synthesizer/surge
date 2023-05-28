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
#ifndef SURGE_SRC_SURGE_XT_OSC_OSCLISTENER_H
#define SURGE_SRC_SURGE_XT_OSC_OSCLISTENER_H
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

    int portnum = DEFAULT_OSC_PORT_IN;
    bool listening = false;

  private:
    SurgeSynthesizer *synth{nullptr};
    SurgeSynthProcessor *sspPtr{nullptr};
    std::string getWholeString(const juce::OSCMessage &message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCListener)
};

} // namespace OSC
} // namespace Surge
#endif // SURGE_SRC_SURGE_XT_OSC_OSCLISTENER_H
