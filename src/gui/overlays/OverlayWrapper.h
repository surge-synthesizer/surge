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

#ifndef SURGE_XT_OVERLAYWRAPPER_H
#define SURGE_XT_OVERLAYWRAPPER_H

#include <JuceHeader.h>
#include "SkinSupport.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
struct OverlayWrapper : public juce::Component,
                        public Surge::GUI::SkinConsumingComponent,
                        public juce::Button::Listener
{
    OverlayWrapper();

    static constexpr int titlebarSize = 14, margin = 1;

    std::string title;
    void setTitle(const std::string t) { title = t; }

    void paint(juce::Graphics &g) override;

    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e) { editor = e; }

    std::unique_ptr<juce::Component> primaryChild;
    void addAndTakeOwnership(std::unique_ptr<juce::Component> c);
    std::unique_ptr<juce::TextButton> closeButton;
    void buttonClicked(juce::Button *button) override;

    juce::Drawable *icon{nullptr};
    void setIcon(juce::Drawable *d) { icon = d; }

    bool showCloseButton{true};
    void setShowCloseButton(bool b) { showCloseButton = b; }

    std::function<void()> closeOverlay = []() {};
    void setCloseOverlay(std::function<void()> f) { closeOverlay = std::move(f); }
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_OVERLAYWRAPPER_H
