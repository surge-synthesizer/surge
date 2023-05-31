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
#include "SkinSupport.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{

namespace Widgets
{
struct SurgeTextButton;
}

namespace Overlays
{

struct Alert : public juce::Component,
               public Surge::GUI::SkinConsumingComponent,
               public juce::Button::Listener
{

    Alert();
    ~Alert();

    std::string title, label;
    std::unique_ptr<Surge::Widgets::SurgeTextButton> okButton;
    std::unique_ptr<Surge::Widgets::SurgeTextButton> cancelButton;
    void setWindowTitle(const std::string &t)
    {
        title = t;
        setTitle(title);
    }
    void setLabel(const std::string &t) { label = t; }
    std::function<void()> onOk;
    void paint(juce::Graphics &g) override;
    void resized() override;
    void onSkinChanged() override;
    void buttonClicked(juce::Button *button) override;
    juce::Rectangle<int> getDisplayRegion();
};

} // namespace Overlays
} // namespace Surge