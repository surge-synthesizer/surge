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
#include "SkinSupport.h"
#include "widgets/SurgeTextButton.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "OverlayComponent.h"
#include "AccessibleHelpers.h"

namespace Surge
{

namespace Overlays
{
struct MultiLineSkinLabel;

struct Alert : public Surge::Overlays::OverlayComponent,
               public Surge::GUI::SkinConsumingComponent,
               public juce::Button::Listener
{

    Alert();
    ~Alert();

    std::string title, label;
    std::unique_ptr<Surge::Widgets::SurgeTextButton> okButton;
    std::unique_ptr<Surge::Widgets::SurgeTextButton> cancelButton;
    std::unique_ptr<juce::ToggleButton> toggleButton;
    bool singleButton = false;
    void setWindowTitle(const std::string &t);
    void setLabel(const std::string &t);
    void setSingleButtonText(const std::string ok)
    {
        okButton->setButtonText(ok);
        singleButton = true;
        resetAccessibility();
    }
    void setOkCancelButtonTexts(const std::string &ok, const std::string &cancel)
    {
        okButton->setButtonText(ok);
        cancelButton->setButtonText(cancel);
        resetAccessibility();
    }
    void addToggleButtonAndSetText(const std::string &t);
    void resetAccessibility();

    std::function<void()> onOk;
    std::function<void()> onCancel;
    std::function<void(bool)> onOkForToggleState;
    std::function<void(bool)> onCancelForToggleState;
    void paint(juce::Graphics &g) override;
    void resized() override;
    void onSkinChanged() override;
    void buttonClicked(juce::Button *button) override;
    juce::Rectangle<int> getDisplayRegion();

    std::unique_ptr<MultiLineSkinLabel> labelComponent;

    void visibilityChanged() override;

    /*
     * Alerts should use default focus order not the wonky tag first and
     * then description and so on order for the main frame (which is laying out controls
     * in a differently structured way).
     */
    std::unique_ptr<juce::ComponentTraverser> createKeyboardFocusTraverser() override
    {
        return std::make_unique<juce::KeyboardFocusTraverser>();
    }
};

} // namespace Overlays
} // namespace Surge