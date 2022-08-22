/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "MenuCustomComponents.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"
#include "UserDefaults.h"
#include <StringOps.h>

namespace Surge
{
namespace Widgets
{
int mg = 1;
int xp = 16;

void TinyLittleIconButton::paint(juce::Graphics &g)
{
    auto yp = offset * 20;
    auto xp = isHovered ? 20 : 0;
    auto t = juce::AffineTransform();

    t = t.translated(-xp, -yp);
    g.reduceClipRegion(getLocalBounds());

    if (getWidth() < 20 || getHeight() < 20)
    {
        t = t.scaled(getWidth() / 20.f);
    }

    if (icons)
    {
        icons->draw(g, 1.0, t);
    }
}

struct TinyLittleIconButtonAH : public juce::AccessibilityHandler
{
    explicit TinyLittleIconButtonAH(TinyLittleIconButton &itemComponentToWrap)
        : AccessibilityHandler(
              itemComponentToWrap, juce::AccessibilityRole::menuItem,
              juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,
                                                     [this]() { this->itemComponent.callback(); })),
          itemComponent(itemComponentToWrap)
    {
    }

    void onPress() {}

    juce::String getTitle() const override { return itemComponent.accLabel; }

    TinyLittleIconButton &itemComponent;
};

std::unique_ptr<juce::AccessibilityHandler> TinyLittleIconButton::createAccessibilityHandler()
{
    return std::make_unique<TinyLittleIconButtonAH>(*this);
}

void MenuTitleHelpComponent::getIdealSize(int &idealWidth, int &idealHeight)
{
    auto standardMenuItemHeight = 25;

    juce::Font font;

    auto ft = getLookAndFeel().getPopupMenuFont();
    ft = ft.withHeight(ft.getHeight() - 1);
    font = ft;

    if (isBoldened)
    {
        font = getLookAndFeel().getPopupMenuFont().boldened();
    }

    idealHeight =
        standardMenuItemHeight > 0 ? standardMenuItemHeight : std::round(font.getHeight() * 1.3f);
    idealWidth = font.getStringWidth(label) + idealHeight * 2;
    idealWidth += 20;
}

void MenuTitleHelpComponent::onSkinChanged()
{
    icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
    repaint();
}

void MenuTitleHelpComponent::paint(juce::Graphics &g)
{
    auto r = getLocalBounds().reduced(1);

    if (isItemHighlighted())
    {
        g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRect(r);

        g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
    }
    else
    {
        g.setColour(getLookAndFeel().findColour(juce::PopupMenu::ColourIds::textColourId));
    }

    if (!isCentered)
    {
        auto ft = getLookAndFeel().getPopupMenuFont();
        ft = ft.withHeight(ft.getHeight() - 1);
        g.setFont(ft);
    }

    if (isBoldened)
    {
        g.setFont(getLookAndFeel().getPopupMenuFont().boldened());
    }

    if (isItemHighlighted())
    {
        g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
    }
    else
    {
        g.setColour(findColour(juce::PopupMenu::headerTextColourId));
    }

    if (isCentered)
    {
        auto rText = r;
        g.drawText(label, rText, juce::Justification::centred);
    }
    else
    {
        auto rText = r.withTrimmedLeft(12 + 20 + 2); // not centered? trim 12 from the left here
        g.drawText(label, rText, juce::Justification::centredLeft);
    }

    auto yp = 4 * 20;
    auto xp = 0;

    if (isItemHighlighted())
    {
        xp = 20;
    }

    auto tl = r.getTopLeft();

    tl = tl.translated(isCentered ? 0 : 12, 2);

    auto clipBox = juce::Rectangle<int>(tl.x, tl.y, 20, 20);

    g.reduceClipRegion(clipBox);

    if (icons)
    {
        icons->drawAt(g, clipBox.getX() - xp, clipBox.getY() - yp, 1.0);
    }
}

void MenuTitleHelpComponent::mouseUp(const juce::MouseEvent &e) { launchHelp(); }

bool MenuTitleHelpComponent::keyPressed(const juce::KeyPress &k)
{
    if (k.isKeyCode(juce::KeyPress::returnKey) || k.isKeyCode(juce::KeyPress::spaceKey))
    {
        launchHelp();
        return true;
    }
    return false;
}

void MenuTitleHelpComponent::launchHelp()
{
    juce::URL(url).launchInDefaultBrowser();
    triggerMenuItem();
}

struct MenuTitleHelpComponentAH : public juce::AccessibilityHandler
{

    explicit MenuTitleHelpComponentAH(MenuTitleHelpComponent &itemComponentToWrap)
        : AccessibilityHandler(
              itemComponentToWrap, juce::AccessibilityRole::menuItem,
              juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,
                                                     [this]() { this->showHelp(); })),
          itemComponent(itemComponentToWrap)
    {
    }

    juce::String getTitle() const override { return itemComponent.getTitle(); }
    juce::String getDescription() const override { return itemComponent.getDescription(); }

    void showHelp() { itemComponent.launchHelp(); }

    MenuTitleHelpComponent &itemComponent;
};

std::unique_ptr<juce::AccessibilityHandler> MenuTitleHelpComponent::createAccessibilityHandler()
{
    return std::make_unique<MenuTitleHelpComponentAH>(*this);
}

void MenuCenteredBoldLabel::getIdealSize(int &idealWidth, int &idealHeight)
{
    getLookAndFeel().getIdealPopupMenuItemSize(label, false, -1, idealWidth, idealHeight);

    if (isSectionHeader)
    {
        idealHeight += 12;
    }
    else
    {
        idealHeight += 8;
    }
}

void MenuCenteredBoldLabel::paint(juce::Graphics &g)
{
    auto r = getLocalBounds();
    auto ft = getLookAndFeel().getPopupMenuFont();

    ft = ft.withHeight(ft.getHeight() - 1).boldened();

    g.setFont(ft);
    g.setColour(getLookAndFeel().findColour(juce::PopupMenu::ColourIds::textColourId));

    if (isSectionHeader)
    {
        g.drawText(label, r.withTrimmedLeft(12), juce::Justification::centredLeft);
    }
    else
    {
        g.drawText(label, r, juce::Justification::centred);
    }
}

void MenuCenteredBoldLabel::addToMenu(juce::PopupMenu &m, const std::string label)
{
    m.addCustomItem(-1, std::make_unique<MenuCenteredBoldLabel>(label), nullptr, label);
}

void MenuCenteredBoldLabel::addToMenuAsSectionHeader(juce::PopupMenu &m, const std::string label)
{
    auto q = std::make_unique<MenuCenteredBoldLabel>(label);
    q->isSectionHeader = true;
    m.addCustomItem(-1, std::move(q), nullptr, label);
}

//==============================================================================
struct MenuCenteredBoldLabelAH : public juce::AccessibilityHandler
{
    explicit MenuCenteredBoldLabelAH(MenuCenteredBoldLabel &itemComponentToWrap)
        : AccessibilityHandler(itemComponentToWrap, juce::AccessibilityRole::menuItem,
                               juce::AccessibilityActions()),
          itemComponent(itemComponentToWrap)
    {
    }

    juce::String getTitle() const override { return itemComponent.label; }

    MenuCenteredBoldLabel &itemComponent;
};

std::unique_ptr<juce::AccessibilityHandler> MenuCenteredBoldLabel::createAccessibilityHandler()
{
    return std::make_unique<MenuCenteredBoldLabelAH>(*this);
}

ModMenuCustomComponent::ModMenuCustomComponent(const std::string &s, const std::string &a,
                                               std::function<void(OpType)> cb)
    : juce::PopupMenu::CustomComponent(false), source(s), amount(a), callback(std::move(cb))
{
    auto tb = std::make_unique<TinyLittleIconButton>(1, [this]() {
        callback(CLEAR);
        triggerMenuItem();
    });
    addAndMakeVisible(*tb);
    clear = std::move(tb);
    clear->accLabel = "Clear " + s;

    tb = std::make_unique<TinyLittleIconButton>(2, [this]() {
        callback(MUTE);
        triggerMenuItem();
    });
    addAndMakeVisible(*tb);
    mute = std::move(tb);
    mute->accLabel = "Mute " + s;

    tb = std::make_unique<TinyLittleIconButton>(0, [this]() {
        callback(EDIT);
        triggerMenuItem();
    });
    addAndMakeVisible(*tb);
    edit = std::move(tb);
    edit->accLabel = "Edit " + s;

    setAccessible(true);
    setTitle(std::string("From ") + source + " by " + amount);
    setDescription(getTitle());
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
}

ModMenuCustomComponent::~ModMenuCustomComponent() noexcept = default;

void ModMenuCustomComponent::getIdealSize(int &idealWidth, int &idealHeight)
{
    auto menuText = source + " " + amount;
    getLookAndFeel().getIdealPopupMenuItemSize(menuText, false, 20, idealWidth, idealHeight);
    auto h = idealHeight - 2 * mg;
    idealWidth += 3 * mg + 3 * (mg + h) + xp;
}
void ModMenuCustomComponent::paint(juce::Graphics &g)
{
    if (isItemHighlighted())
    {
        auto r = getLocalBounds().reduced(1);
        g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRect(r);

        g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
    }
    else
    {
        g.setColour(getLookAndFeel().findColour(juce::PopupMenu::ColourIds::textColourId));
    }
    auto h = getHeight() - 2 * mg;
    auto r = getLocalBounds().withTrimmedLeft(xp + 3 * mg + 3 * (h + mg) + mg).withTrimmedRight(4);
    auto ft = getLookAndFeel().getPopupMenuFont();
    ft = ft.withHeight(ft.getHeight() - 1);
    g.setFont(ft);
    g.drawText(source, r, juce::Justification::centredLeft);
    g.drawText(amount, r, juce::Justification::centredRight);
}

std::unique_ptr<juce::PopupMenu> ModMenuCustomComponent::createAccessibleSubMenu(SurgeStorage *s)
{
    bool doExpMen =
        Surge::Storage::getUserDefaultValue(s, Surge::Storage::ExpandModMenusWithSubMenus, false);

    if (!doExpMen)
        return nullptr;

    auto res = std::make_unique<juce::PopupMenu>();
    auto fn = callback;
    res->addItem(edit->accLabel, [fn]() { fn(EDIT); });
    res->addItem(mute->accLabel, [fn]() { fn(MUTE); });
    res->addItem(clear->accLabel, [fn]() { fn(CLEAR); });
    return std::move(res);
}

void ModMenuCustomComponent::setIsMuted(bool b)
{
    if (b)
    {
        mute->accLabel = "Unmute " + source;
        mute->offset = 3; // use the 2nd (mute with bar) icon
    }
    else
    {
        mute->accLabel = "Mute " + source;
        mute->offset = 2; // use the 2rd (speaker) icon
    }
}

void ModMenuCustomComponent::resized()
{
    auto h = getHeight() - 2 * mg;
    clear->setBounds(xp + mg + 0 * (h + mg), mg, h, h);
    clear->toFront(false);
    mute->setBounds(xp + mg + 1 * (h + mg), mg, h, h);
    mute->toFront(false);
    edit->setBounds(xp + mg + 2 * (h + mg), mg, h, h);
    edit->toFront(false);
}

void ModMenuCustomComponent::mouseUp(const juce::MouseEvent &e)
{
    callback(EDIT);
    triggerMenuItem();
}

bool ModMenuCustomComponent::keyPressed(const juce::KeyPress &k)
{
    if (k.isKeyCode(juce::KeyPress::returnKey) || k.isKeyCode(juce::KeyPress::spaceKey))
    {
        callback(EDIT);
        triggerMenuItem();
        return true;
    }
    return false;
}

void ModMenuCustomComponent::onSkinChanged()
{
    icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
    clear->icons = icons;
    edit->icons = icons;
    mute->icons = icons;
}

struct ModMenuCustomComponentAH : public juce::AccessibilityHandler
{
    explicit ModMenuCustomComponentAH(ModMenuCustomComponent &itemComponentToWrap)
        : AccessibilityHandler(
              itemComponentToWrap, juce::AccessibilityRole::menuItem,
              juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,
                                                     [this]() { this->onPress(); })),
          itemComponent(itemComponentToWrap)
    {
    }

    void onPress()
    {
        itemComponent.callback(ModMenuCustomComponent::EDIT);
        itemComponent.triggerMenuItem();
    }

    juce::String getTitle() const override
    {
        return itemComponent.source + " by " + itemComponent.amount;
    }

    ModMenuCustomComponent &itemComponent;
};

std::unique_ptr<juce::AccessibilityHandler> ModMenuCustomComponent::createAccessibilityHandler()
{
    return std::make_unique<ModMenuCustomComponentAH>(*this);
}

// bit of a hack - the menus mean something different so do a cb on a cb
ModMenuForAllComponent::ModMenuForAllComponent(std::function<void(AllAction)> cb)
    : allCB(cb),
      ModMenuCustomComponent(Surge::GUI::toOSCase("Apply to All"), "", [this](OpType op) {
          switch (op)
          {
          case ModMenuCustomComponent::CLEAR:
              allCB(CLEAR);
              break;
          case ModMenuCustomComponent::MUTE:
              allCB(UNMUTE);
              break;
          case ModMenuCustomComponent::EDIT:
              allCB(MUTE);
              break;
          }
      })
{
    edit->offset = 3;
}

} // namespace Widgets
} // namespace Surge