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

#include "ModMenuCustomComponent.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"

namespace Surge
{
namespace Widgets
{
int mg = 1;
int xp = 16;

struct TinyLittleIconButton : public juce::Component
{
    TinyLittleIconButton(int off, std::function<void()> cb) : offset(off), callback(std::move(cb))
    {
    }

    void setIcon(SurgeImage *img)
    {
        icons = img;
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        auto yp = offset * 20;
        auto xp = isHovered ? 20 : 0;
        g.reduceClipRegion(getLocalBounds());
        auto t = juce::AffineTransform().translated(-xp, -yp);
        if (icons)
            icons->draw(g, 1.0, t);
    }
    void mouseUp(const juce::MouseEvent &e) override { callback(); }
    void mouseEnter(const juce::MouseEvent &e) override
    {
        isHovered = true;
        repaint();
    }
    void mouseExit(const juce::MouseEvent &e) override
    {
        isHovered = false;
        repaint();
    }

    bool isHovered{false};
    std::function<void()> callback;
    SurgeImage *icons{nullptr};
    int offset{0};
};

ModMenuCustomComponent::ModMenuCustomComponent(const std::string &lab,
                                               std::function<void(OpType)> cb)
    : juce::PopupMenu::CustomComponent(false), menuText(lab), callback(std::move(cb))
{
    auto tb = std::make_unique<TinyLittleIconButton>(1, [this]() {
        callback(CLEAR);
        triggerMenuItem();
    });
    addAndMakeVisible(*tb);
    clear = std::move(tb);

    tb = std::make_unique<TinyLittleIconButton>(2, [this]() {
        callback(MUTE);
        triggerMenuItem();
    });
    addAndMakeVisible(*tb);
    mute = std::move(tb);

    tb = std::make_unique<TinyLittleIconButton>(0, [this]() {
        callback(EDIT);
        triggerMenuItem();
    });
    addAndMakeVisible(*tb);
    edit = std::move(tb);
}

ModMenuCustomComponent::~ModMenuCustomComponent() noexcept = default;

void ModMenuCustomComponent::getIdealSize(int &idealWidth, int &idealHeight)
{
    getLookAndFeel().getIdealPopupMenuItemSize(menuText, false, 22, idealWidth, idealHeight);
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
    auto r = getLocalBounds().withTrimmedLeft(xp + 3 * mg + 3 * (h + mg) + mg);
    g.setFont(getLookAndFeel().getPopupMenuFont());
    g.drawText(menuText, r, juce::Justification::centredLeft);
}

void ModMenuCustomComponent::setIsMuted(bool b)
{
    if (b)
        mute->offset = 3; // use the 2nd (mute with bar) icon
    else
        mute->offset = 2; // use the 2rd (speaker) icon
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

void ModMenuCustomComponent::onSkinChanged()
{
    icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
    clear->icons = icons;
    edit->icons = icons;
    mute->icons = icons;
}
} // namespace Widgets
} // namespace Surge