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

#include "MenuButtonFromIcon.h"
#include "AccessibleHelpers.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "SurgeImage.h"

#include <fmt/format.h>

namespace Surge
{
namespace Widgets
{

struct MenuButtonFromIconAH : public juce::AccessibilityHandler
{
    explicit MenuButtonFromIconAH(MenuButtonFromIcon *s)
        : mbutton(s), juce::AccessibilityHandler(
                          *s, juce::AccessibilityRole::button,
                          juce::AccessibilityActions().addAction(
                              juce::AccessibilityActionType::press, [this]() { this->press(); }))
    {
    }
    void press()
    {
        auto m = mbutton->menuFactory();
        mbutton->isActive = true;
        mbutton->repaint();

        m.showMenuAsync(juce::PopupMenu::Options(), [this](int) {
            mbutton->isActive = false;
            mbutton->repaint();
        });
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = AccessibilityHandler::getCurrentState();

        if (mbutton->isActive)
        {
            state = state.withChecked();
        }
        return state;
    }

    MenuButtonFromIcon *mbutton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuButtonFromIconAH);
};

std::unique_ptr<juce::AccessibilityHandler> MenuButtonFromIcon::createAccessibilityHandler()
{
    return std::make_unique<MenuButtonFromIconAH>(this);
}

juce::Component *MenuButtonFromIcon::getCurrentAccessibleSelectionComponent() { return this; }

MenuButtonFromIcon::MenuButtonFromIcon(std::function<juce::PopupMenu()> factory,
                                       std::unique_ptr<juce::Drawable> svgIcon)
    : juce::Component(), WidgetBaseMixin<MenuButtonFromIcon>(this), menuFactory(std::move(factory)),
      baseIcon(std::move(svgIcon))
{
    setRepaintsOnMouseActivity(true);
    setAccessible(true);
    setWantsKeyboardFocus(true);
}

MenuButtonFromIcon::~MenuButtonFromIcon() = default;

void MenuButtonFromIcon::paint(juce::Graphics &g)
{
    namespace clr = Colors::JuceWidgets::TextMultiSwitch;

    const bool isEn = isEnabled() && !isDeactivated;
    const float alpha = isEn ? 1.f : 0.35f;

    auto uph = skin->getColor(clr::UnpressedHighlight);
    bool royalMode = uph.getAlpha() > 0.f;

    auto b = getLocalBounds().toFloat().reduced(0.5f, 0.5f);
    const float corner = 2.f;
    float cornerIn = 1.5f;

    if (royalMode)
    {
        g.setColour(skin->getColor(clr::UnpressedHighlight).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b, corner);

        g.setColour(skin->getColor(clr::Background).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b.reduced(0.f, 1.f), corner);
        cornerIn = 0.5f;
    }
    else
    {
        g.setColour(skin->getColor(clr::Background).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b, corner);
    }

    g.setColour(skin->getColor(clr::Border).withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(b, corner, 1.f);

    if (isHovered || isActive)
    {
        g.setColour(skin->getColor(clr::HoverFill).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b.reduced(1.5f), cornerIn);
    }

    if (iconNormal && iconHovered)
    {
        auto iconBounds = b.withSizeKeepingCentre(b.getWidth() * 0.5f, b.getHeight() * 0.5f);
        auto *toDraw = (isHovered || isActive) ? iconHovered.get() : iconNormal.get();
        toDraw->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
    }
}

void MenuButtonFromIcon::onSkinChanged()
{
    if (!baseIcon || !skin)
    {
        return;
    }

    namespace clr = Colors::JuceWidgets::TextMultiSwitch;

    iconNormal = baseIcon->createCopy();
    iconNormal->replaceColour(juce::Colours::white, skin->getColor(clr::Text));
    iconHovered = baseIcon->createCopy();
    iconHovered->replaceColour(juce::Colours::white, skin->getColor(clr::Background));

    repaint();
}

void MenuButtonFromIcon::mouseEnter(const juce::MouseEvent &)
{
    isHovered = true;
    repaint();
}

void MenuButtonFromIcon::mouseExit(const juce::MouseEvent &)
{
    isHovered = false;
    repaint();
}

void MenuButtonFromIcon::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isLeftButtonDown())
    {
        isActive = true;
        repaint();

        auto m = menuFactory();
        m.showMenuAsync(juce::PopupMenu::Options(), [this](int) {
            isActive = false;
            repaint();
        });
    }
}

bool MenuButtonFromIcon::keyPressed(const juce::KeyPress &key)
{
    if (isEnabled() && !isDeactivated)
    {
        if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
        {
            isActive = true;
            repaint();

            auto m = menuFactory();
            m.showMenuAsync(juce::PopupMenu::Options(), [this](int) {
                isActive = false;
                repaint();
            });
            return true;
        }
    }
    return false;
}

} // namespace Widgets
} // namespace Surge
