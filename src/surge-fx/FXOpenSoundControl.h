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

#ifndef SURGE_SRC_SURGE_FX_FXOPENSOUNDCONTROL_H
#define SURGE_SRC_SURGE_FX_FXOPENSOUNDCONTROL_H

#include <juce_gui_extra/juce_gui_extra.h>

#include "juce_osc/juce_osc.h"
#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"

class SurgefxAudioProcessor;

namespace SurgeFX
{
namespace FxOSC
{

class FXOpenSoundControl : public juce::OSCReceiver,
                           juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
  public:
    FXOpenSoundControl();
    ~FXOpenSoundControl();

    void initOSC(SurgefxAudioProcessor *sfxp, const std::unique_ptr<SurgeStorage> &storage);
    bool initOSCIn(int port);
    void stopListening(bool updateOSCStartInStorage = true);
    void tryOSCStartup();

    int iportnum = DEFAULT_OSC_PORT_IN;
    bool listening = false;

    void oscMessageReceived(const juce::OSCMessage &message) override;
    void oscBundleReceived(const juce::OSCBundle &bundle) override;

  private:
    std::unique_ptr<juce::DatagramSocket> inputSocket;
    SurgeStorage *storage{nullptr};
    SurgefxAudioProcessor *sfxPtr{nullptr};
};

} // namespace FxOSC
} // namespace SurgeFX

#endif // SURGE_SRC_SURGE_FX_FXOPENSOUNDCONTROL_H
