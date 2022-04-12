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

#ifndef SURGE_XT_ABOUTSCREEN_H
#define SURGE_XT_ABOUTSCREEN_H

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
