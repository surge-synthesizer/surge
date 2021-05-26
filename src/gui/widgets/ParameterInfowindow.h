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

#ifndef SURGE_XT_PARAMETERINFOWINDOW_H
#define SURGE_XT_PARAMETERINFOWINDOW_H

#include <JuceHeader.h>
#include "Parameter.h"
#include "SkinSupport.h"

namespace Surge
{
namespace Widgets
{
struct ParameterInfowindow : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    ParameterInfowindow();
    ~ParameterInfowindow();

    juce::Font font;
    void setFont(const juce::Font &f)
    {
        font = f;
        repaint();
    }
    void paint(juce::Graphics &g) override;

    std::string name, display, displayAlt;
    void setLabels(const std::string &n, const std::string &d, const std::string &dalt = "")
    {
        name = n;
        display = d;
        displayAlt = dalt;
        repaint();
    }

    // Juce can do a way better job of bounds setting
    void setBoundsToAccompany(const juce::Rectangle<int> &controlRect,
                              const juce::Rectangle<int> &parentRect);

    ModulationDisplayInfoWindowStrings mdiws;
    bool hasModDisInf{false};
    void setMDIWS(const ModulationDisplayInfoWindowStrings &mdiws)
    {
        hasModDisInf = true;
        this->mdiws = mdiws;
    }

    bool hasExt{false};
    void setExtendedMDIWS(bool b) { hasExt = b; }
    bool hasMDIWS() { return hasModDisInf; }
    void clearMDIWS() { hasModDisInf = false; }
    int getMaxLabelLen() { return 30; }
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_PARAMETERINFOWINDOW_H
