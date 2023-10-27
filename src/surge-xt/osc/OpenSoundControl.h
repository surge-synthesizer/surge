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
#ifndef SURGE_SRC_SURGE_XT_OSC_OPENSOUNDCONTROL_H
#define SURGE_SRC_SURGE_XT_OSC_OPENSOUNDCONTROL_H
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
#include <fmt/core.h>
#include <fmt/format.h>

class SurgeSynthProcessor;

namespace Surge
{
namespace OSC
{

class OpenSoundControl : public juce::OSCReceiver,
                         juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
  public:
    OpenSoundControl();
    ~OpenSoundControl();

    void initOSC(SurgeSynthProcessor *ssp, const std::unique_ptr<SurgeSynthesizer> &surge);
    bool initOSCIn(int port);
    bool initOSCOut(int port);
    void stopListening(bool updateOSCStartInStorage = true);
    void tryOSCStartup();

    int iportnum = DEFAULT_OSC_PORT_IN;
    int oportnum = DEFAULT_OSC_PORT_OUT;
    bool listening = false;
    bool sendingOSC = false;

    void oscMessageReceived(const juce::OSCMessage &message) override;
    void oscBundleReceived(const juce::OSCBundle &bundle) override;

    void send(std::string addr, std::string msg);
    void sendAllParams();
    void stopSending(bool updateOSCStartInStorage = true);

  private:
    SurgeSynthesizer *synth{nullptr};
    SurgeSynthProcessor *sspPtr{nullptr};
    std::string getWholeString(const juce::OSCMessage &message);
    int getNoteID(const juce::OSCMessage &om, int pos);
    juce::OSCSender juceOSCSender;
    void sendError(std::string errorMsg);
    void sendNotFloatError(std::string addr, std::string msg);
    void sendDataCountError(std::string addr, std::string count);
    float getNormValue(Parameter *p, float fval);
    std::string getParamValStr(const Parameter *p);
    std::string getMacroValStr(long macnum);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSoundControl)
};

// Makes sure that decimal *points* are used, not commas
inline std::string float_to_clocalestr_wprec(float value, int precision)
{
    return fmt::format(std::locale::classic(), "{:." + std::to_string(precision) + "Lf}", value);
}

} // namespace OSC
} // namespace Surge
#endif // SURGE_SRC_SURGE_XT_OSC_OSCLISTENER_H
