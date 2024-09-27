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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_PARAMETERINFOWINDOW_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_PARAMETERINFOWINDOW_H

#include "Parameter.h"
#include "SkinSupport.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{
struct ParameterInfowindow : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    ParameterInfowindow();
    ~ParameterInfowindow();

    juce::Font font{juce::FontOptions()};
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

    int countdownHide{-1}, countdownFade{-1}, countdownFadeIn{-1};
    static constexpr int fadeOutOver{3}, fadeInOver{3};
    void setCountdownToHide(int c)
    {
        countdownHide = c;
        countdownFade = -1;
    }
    void startFadein()
    {
        if (countdownFadeIn < 0 && !isVisible())
            countdownFadeIn = fadeInOver;
    }
    void doHide(int afterIdles = -1);
    void idle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterInfowindow);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_PARAMETERINFOWINDOW_H
