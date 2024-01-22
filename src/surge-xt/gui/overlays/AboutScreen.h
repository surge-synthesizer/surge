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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_ABOUTSCREEN_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_ABOUTSCREEN_H

#include "SkinSupport.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeGUIEditor;
class SurgeStorage;

namespace Surge
{
namespace Overlays
{
struct ClipboardCopyButton;

struct AboutScreen : public juce::Component,
                     public Surge::GUI::SkinConsumingComponent,
                     public juce::Button::Listener
{
    AboutScreen();
    ~AboutScreen() noexcept;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    SurgeGUIEditor *editor{nullptr};
    void setEditor(SurgeGUIEditor *ed) { editor = ed; }

    std::string host, wrapper;
    void setHostProgram(const std::string &h) { host = h; }
    void setWrapperType(const std::string &w) { wrapper = w; }
    void setDevModeGrid(const int &dmg) { devModeGrid = dmg; }

    void populateData();

    void resized() override;

    void paint(juce::Graphics &c) override;
    void mouseUp(const juce::MouseEvent &event) override;

    void buttonClicked(juce::Button *button) override;

    void onSkinChanged() override;
    SurgeImage *logo{nullptr};
    int logoW{666}, logoH{179}, devModeGrid{-1};

    // label, value, url
    std::vector<std::tuple<std::string, std::string, std::string>> lowerLeft, lowerRight;

    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::Button>> buttons;
    std::vector<std::unique_ptr<juce::Component>> icons;

    std::unique_ptr<ClipboardCopyButton> copyButton;

    juce::Colour fillColour{juce::Colour(0, 0, 0).withAlpha(0.85f)};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutScreen);
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_ABOUTSCREEN_H
