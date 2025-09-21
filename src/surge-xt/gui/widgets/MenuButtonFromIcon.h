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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUBUTTONFROMICON_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUBUTTONFROMICON_H

#include "AccessibleHelpers.h"
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{

struct MenuButtonFromIcon : public juce::Component,
                            public WidgetBaseMixin<MenuButtonFromIcon>,
                            public Surge::Widgets::HasAccessibleSubComponentForFocus
{
    MenuButtonFromIcon(std::function<juce::PopupMenu()> factory = {},
                       std::unique_ptr<juce::Drawable> svgIcon = nullptr);

    MenuButtonFromIcon();
    ~MenuButtonFromIcon();

    float getValue() const override { return 0.f; }
    void setValue(float) override {}
    void onSkinChanged() override;
    void paint(juce::Graphics &g) override;

    void mouseEnter(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;
    void mouseDown(const juce::MouseEvent &e) override;
    bool keyPressed(const juce::KeyPress &key) override;

    std::function<juce::PopupMenu()> menuFactory;
    std::unique_ptr<juce::Drawable> baseIcon;
    std::unique_ptr<juce::Drawable> iconNormal;
    std::unique_ptr<juce::Drawable> iconHovered;

    juce::Component *getCurrentAccessibleSelectionComponent() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    bool isDeactivated{false};
    bool isHovered{false};
    bool isActive{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuButtonFromIcon);
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUBUTTONFROMICON_H
