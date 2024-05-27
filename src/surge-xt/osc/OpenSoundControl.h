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
                         public SurgeSynthesizer::ModulationAPIListener,
                         juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
  public:
    OpenSoundControl();
    ~OpenSoundControl();

    void initOSC(SurgeSynthProcessor *ssp, const std::unique_ptr<SurgeSynthesizer> &surge);
    bool initOSCIn(int port);
    bool initOSCOut(int port, std::string ipaddr);
    void stopListening(bool updateOSCStartInStorage = true);
    void tryOSCStartup();

    int iportnum = DEFAULT_OSC_PORT_IN;
    int oportnum = DEFAULT_OSC_PORT_OUT;
    std::string outIPAddr = DEFAULT_OSC_IPADDR_OUT;
    bool listening = false;
    bool sendingOSC = false;

    void oscMessageReceived(const juce::OSCMessage &message) override;
    void oscBundleReceived(const juce::OSCBundle &bundle) override;

    void send(juce::OSCMessage om, bool needsMessageThread);
    void sendAllParams();
    void sendAllModulators();
    void stopSending(bool updateOSCStartInStorage = true);

    // ModulationAPIListener methods
    void modSet(long ptag, modsources modsource, int modsourceScene, int index, float value,
                bool isNew) override;
    void modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                  bool mute) override;
    void modCleared(long ptag, modsources modsource, int modsourceScene, int index) override;
    void modBeginEdit(long ptag, modsources modsource, int modsourceScene, int index,
                      float depth01) override;
    void modEndEdit(long ptag, modsources modsource, int modsourceScene, int index,
                    float depth01) override;

    void modOSCout(std::string addr, std::string oscName, float val, bool reportMute);

  private:
    SurgeSynthesizer *synth{nullptr};
    SurgeSynthProcessor *sspPtr{nullptr};
    std::string getWholeString(const juce::OSCMessage &message);
    int getNoteID(const juce::OSCMessage &om, int pos);
    juce::OSCSender juceOSCSender;
    void sendError(std::string errorMsg);
    void sendNotFloatError(std::string addr, std::string msg);
    void sendDataCountError(std::string addr, std::string count);
    void sendMidiBoundsError(std::string addr);
    void sendParameter(const Parameter *p, bool needsMessageThread, std::string extension = "");
    void sendParameterExtOptions(const Parameter *p, bool needsMessageThread);
    void sendAllParameterInfo(const Parameter *p, bool needsMessageThread);

    void sendParameterDocs(const Parameter *p, bool needsMessageThread);
    void sendParameterExtDocs(const Parameter *p, bool needsMessageThread);
    void sendMacro(long macnum, bool needsMsgThread);
    void sendModulator(ModulationRouting mod, int scene, bool global);
    void sendPath(std::string pathStr);

    std::string getModulatorOSCAddr(int modid, int scene, int index, bool mute);
    void sendMod(long ptag, modsources modsource, int modsourceScene, int index, float val,
                 bool reportMute);
    void sendFailed();
    bool hasEnding(std::string const &fullString, std::string const &ending);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSoundControl)
};

// Makes sure that decimal *points* are used, not commas
inline std::string float_to_clocalestr_wprec(float value, int precision)
{
    /** c++20/fmt-10 fix this has to be a const expression */
    assert(precision == 3);
    return fmt::format(std::locale::classic(), "{:.3Lf}", value);
    // return fmt::format(std::locale::classic(), "{:." + std::to_string(precision) + "Lf}", value);
    // return fmt::format(std::locale::classic(), "{:.{}Lf}", value, precision); is I think the
    // answer value);
}

} // namespace OSC
} // namespace Surge
#endif // SURGE_SRC_SURGE_XT_OSC_OSCLISTENER_H
