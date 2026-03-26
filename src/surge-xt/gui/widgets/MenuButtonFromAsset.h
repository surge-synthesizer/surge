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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUBUTTONFROMASSET_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUBUTTONFROMASSET_H

#include "AccessibleHelpers.h"
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{

struct MenuButtonFromAsset : public juce::Component,
                             public WidgetBaseMixin<MenuButtonFromAsset>,
                             public Surge::Widgets::HasAccessibleSubComponentForFocus
{
    MenuButtonFromAsset(std::function<juce::PopupMenu()> factory = {});
    ~MenuButtonFromAsset();

    float getValue() const override { return 0.f; }
    void setValue(float) override {}
    void onSkinChanged() override;
    void paint(juce::Graphics &g) override;

    void mouseEnter(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;
    void mouseDown(const juce::MouseEvent &e) override;
    bool keyPressed(const juce::KeyPress &key) override;
    void focusGained(juce::Component::FocusChangeType cause) override;
    void focusLost(juce::Component::FocusChangeType cause) override;

    juce::Component *getCurrentAccessibleSelectionComponent() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::function<juce::PopupMenu()> menuFactory;
    int currentImageID{-1};
    bool isDeactivated{false};
    bool isHovered{false};
    bool isActive{false};

  private:
    std::unique_ptr<juce::Drawable> baseIcon;
    std::unique_ptr<juce::Drawable> iconHovered;
    bool usesTwoFrameAsset{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuButtonFromAsset);
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUBUTTONFROMASSET_H
