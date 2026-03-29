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

#include "MenuButtonFromAsset.h"
#include "AccessibleHelpers.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "SurgeImage.h"

#include <fmt/format.h>

namespace Surge
{
namespace Widgets
{

struct MenuButtonFromAssetAH : public juce::AccessibilityHandler
{
    explicit MenuButtonFromAssetAH(MenuButtonFromAsset *s)
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

    MenuButtonFromAsset *mbutton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuButtonFromAssetAH);
};

std::unique_ptr<juce::AccessibilityHandler> MenuButtonFromAsset::createAccessibilityHandler()
{
    return std::make_unique<MenuButtonFromAssetAH>(this);
}

juce::Component *MenuButtonFromAsset::getCurrentAccessibleSelectionComponent() { return this; }

MenuButtonFromAsset::MenuButtonFromAsset(std::function<juce::PopupMenu()> factory)
    : juce::Component(), WidgetBaseMixin<MenuButtonFromAsset>(this), menuFactory(std::move(factory))
{
    setRepaintsOnMouseActivity(true);
    setAccessible(true);
    setWantsKeyboardFocus(true);
}

MenuButtonFromAsset::~MenuButtonFromAsset() = default;

void MenuButtonFromAsset::paint(juce::Graphics &g)
{
    const bool isEn = isEnabled() && !isDeactivated;
    const float alpha = isEn ? 1.f : 0.35f;

    if (!baseIcon)
    {
        return;
    }

    auto b = getLocalBounds().toFloat();
    bool showHover = isHovered || isActive;

    if (usesTwoFrameAsset)
    {
        // Clip to component bounds and show either frame 0 or frame 1
        g.saveState();
        g.reduceClipRegion(getLocalBounds());
        float yOffset =
            showHover ? 0.f : -b.getHeight(); // frame 0 = hover/active, frame 1 = default
        baseIcon->drawWithin(
            g,
            juce::Rectangle<float>(b.getX(), b.getY() + yOffset, b.getWidth(), b.getHeight() * 2.f),
            juce::RectanglePlacement::stretchToFit, alpha);
        g.restoreState();
    }
    else
    {
        // Single frame asset — swap between base and separate hover drawable
        auto &icon = (showHover && iconHovered) ? iconHovered : baseIcon;
        icon->drawWithin(g, b, juce::RectanglePlacement::centred, alpha);
    }
}

void MenuButtonFromAsset::onSkinChanged()
{
    if (!skin || !associatedBitmapStore || currentImageID < 0)
    {
        return;
    }

    baseIcon.reset();
    iconHovered.reset();
    usesTwoFrameAsset = false;

    // Always refresh baseIcon from the store so skin swaps pick up the new asset
    auto *baseImage = associatedBitmapStore->getImage(currentImageID);
    if (baseImage && baseImage->getDrawableButUseWithCaution())
        baseIcon = baseImage->getDrawableButUseWithCaution()->createCopy();

    if (baseIcon)
    {
        // Check if the base asset is a two-frame stacked SVG (height == 2 * width)
        // by inspecting the drawable bounds
        auto bounds = baseIcon->getDrawableBounds();
        usesTwoFrameAsset = (bounds.getHeight() >= bounds.getWidth() * 1.8f);

        if (!usesTwoFrameAsset)
        {
            // Single frame asset — look for a separate hover image in the store.
            auto *hoverImage = associatedBitmapStore->getImageByStringID(
                fmt::format("DEFAULT/hover{:05d}.svg", currentImageID));
            if (hoverImage && hoverImage->getDrawableButUseWithCaution())
                iconHovered = hoverImage->getDrawableButUseWithCaution()->createCopy();
        }
        // Two-frame asset: both states are baked into baseIcon stacked vertically.
        // paint() handles frame selection via clipping — no separate hover asset needed.
    }

    repaint();
}

void MenuButtonFromAsset::mouseEnter(const juce::MouseEvent &)
{
    isHovered = true;
    repaint();
}

void MenuButtonFromAsset::mouseExit(const juce::MouseEvent &)
{
    isHovered = false;
    repaint();
}

void MenuButtonFromAsset::mouseDown(const juce::MouseEvent &event)
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

bool MenuButtonFromAsset::keyPressed(const juce::KeyPress &key)
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

void MenuButtonFromAsset::focusGained(juce::Component::FocusChangeType cause)
{
    isActive = true;
    repaint();
}

void MenuButtonFromAsset::focusLost(juce::Component::FocusChangeType cause)
{
    isActive = false;
    repaint();
}

} // namespace Widgets
} // namespace Surge
