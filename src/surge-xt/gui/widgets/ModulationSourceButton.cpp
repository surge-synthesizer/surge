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

#include "ModulationSourceButton.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SurgeImageStore.h"
#include "basic_dsp.h"
#include "SurgeGUIEditor.h"
#include "SurgeImage.h"
#include "SurgeGUIUtils.h"

namespace Surge
{
namespace Widgets
{
ModulationSourceButton::ModulationSourceButton()
{
#if SURGE_JUCE_ACCESSIBLE
    setDescription("Modulator");
#endif
}
void ModulationSourceButton::paint(juce::Graphics &g)
{
    /*
     * state
     * 0 - nothing
     * 1 - selected modeditor
     * 2 - selected modsource (locked)
     * 4 [bit 2] - selected arrow button [0, 1, 2 -> 4, 5, 6]
     */

    bool SelectedModSource = (state & 3) == 1;
    bool ActiveModSource = (state & 3) == 2;
    bool UsedOrActive = isUsed || (state & 3);

    juce::Colour FrameCol, FillCol, FontCol;

    FillCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Background)
                           : skin->getColor(Colors::ModSource::Unused::Background);
    FrameCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Border)
                            : skin->getColor(Colors::ModSource::Unused::Border);
    FontCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Text)
                           : skin->getColor(Colors::ModSource::Unused::Text);
    if (isHovered)
    {
        FrameCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::BorderHover)
                                : skin->getColor(Colors::ModSource::Unused::BorderHover);
        FontCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::TextHover)
                               : skin->getColor(Colors::ModSource::Unused::TextHover);
    }

    if (ActiveModSource || transientArmed)
    {
        FrameCol = skin->getColor(Colors::ModSource::Armed::Border);
        if (isHovered)
        {
            FrameCol = skin->getColor(Colors::ModSource::Armed::BorderHover);
        }

        FillCol = skin->getColor(Colors::ModSource::Armed::Background);
        FontCol = skin->getColor(Colors::ModSource::Armed::Text);

        if (isHovered)
        {
            FontCol = skin->getColor(Colors::ModSource::Armed::TextHover);
        }
    }
    else if (SelectedModSource)
    {
        FrameCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::Border)
                          : skin->getColor(Colors::ModSource::Selected::Border);
        if (isHovered)
        {
            FrameCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::BorderHover)
                              : skin->getColor(Colors::ModSource::Selected::BorderHover);
        }

        FillCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::Background)
                         : skin->getColor(Colors::ModSource::Selected::Background);
        FontCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::Text)
                         : skin->getColor(Colors::ModSource::Selected::Text);

        if (isHovered)
        {
            FontCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::TextHover)
                             : skin->getColor(Colors::ModSource::Selected::TextHover);
        }
    }
    else if (isTinted)
    {
        FillCol = skin->getColor(Colors::ModSource::Unused::Background);
        FrameCol = skin->getColor(Colors::ModSource::Unused::Border);

        if (isHovered)
        {
            FrameCol = skin->getColor(Colors::ModSource::Unused::BorderHover);
        }

        FontCol = skin->getColor(Colors::ModSource::Unused::Text);

        if (isHovered)
        {
            FontCol = skin->getColor(Colors::ModSource::Unused::TextHover);
        }
    }

    if (secondaryHoverActive)
    {
        FontCol = skin->getColor(Colors::ModSource::Used::UsedModHover);

        if (ActiveModSource)
        {
            FontCol = skin->getColor(Colors::ModSource::Armed::UsedModHover);
        }

        if (SelectedModSource)
        {
            FontCol = skin->getColor(Colors::ModSource::Selected::UsedModHover);
        }
    }

    auto fillRect = getLocalBounds();
    fillRect.reduce(1, 1);
    g.setColour(FillCol);
    g.fillRect(fillRect);

    auto btnFont = Surge::GUI::getFontManager()->getLatoAtSize(8, juce::Font::bold);

    if (!isMeta)
    {
        // modbutton name settings
        g.setColour(FontCol);
        g.setFont(btnFont);

        // modbutton name
        g.drawText(getCurrentModLabel(), getLocalBounds(), juce::Justification::centred);

        // modbutton frame
        g.setColour(FrameCol);
        g.drawRect(getLocalBounds(), 1);
    }
    else
    {
        // macro name area
        auto topRect = getLocalBounds().withHeight(splitHeight);

        g.setColour(FontCol);
        g.setFont(btnFont);
        g.drawText(getCurrentModLabel(), topRect, juce::Justification::centred);

        // macro slider area
        auto bottomRect = fillRect.withTrimmedTop(splitHeight - 2);

        // macro slider value fill
        auto valRect = bottomRect.toFloat();
        valRect.reduce(1, 1);

        auto sliderWidth = valRect.getWidth();

        // current value
        auto valPoint = value * sliderWidth;

        // macro slider background
        g.setColour(skin->getColor(Colors::ModSource::Macro::Background));
        g.fillRect(valRect);

        // macro slider frame
        g.setColour(skin->getColor(Colors::ModSource::Used::Background));
        g.drawRect(bottomRect);

        if (isBipolar)
        {
            auto bipolarCenter = valRect.getCentreX();

            if (valPoint + 2 <= bipolarCenter)
            {
                valRect = valRect.withLeft(valPoint + 2).withRight(bipolarCenter + 1);
            }
            else
            {
                valRect = valRect.withLeft(bipolarCenter).withRight(valPoint + 2);
            }
        }
        else
        {
            // make it so that the slider doesn't draw over its frame, but within it
            valRect.setX(2);
            valRect.setWidth(valPoint);
        }

        // draw macro slider value fill
        g.setColour(skin->getColor(Colors::ModSource::Macro::Fill));
        g.fillRect(valRect);

        // draw current value notch
        valPoint = (valPoint == sliderWidth) ? sliderWidth - 1 : valPoint;

        g.setColour(skin->getColor(Colors::ModSource::Macro::CurrentValue));
        g.fillRect(juce::Rectangle<float>(2 + valPoint, valRect.getY(), 1, valRect.getHeight()));

        // draw modbutton frame
        g.setColour(FrameCol);
        g.drawRect(getLocalBounds(), 1);
    }

    if (isLFO())
    {
        auto arrSze = getLocalBounds().withLeft(getLocalBounds().getRight() - 14).withHeight(16);
        float dy = (state >= 4) ? -16 : 0;

        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(arrSze);

        arrow->drawAt(g, arrSze.getX(), arrSze.getY() + dy, 1.0);
    }

    if (needsHamburger())
    {
        g.setColour(FrameCol);

        float ht = 1.5;
        int nburg = 3;
        float pxspace = 1.f * (hamburgerHome.getHeight() - ht) / (nburg);

        for (int i = 0; i < nburg; ++i)
        {
            auto r = juce::Rectangle<float>(hamburgerHome.getX() + 1,
                                            ht + hamburgerHome.getY() + i * pxspace,
                                            hamburgerHome.getWidth() - 1, ht);
            g.fillRect(r);
        }
    }
}

void ModulationSourceButton::buildHamburgerMenu(juce::PopupMenu &menu,
                                                const bool addedToModbuttonContextMenu)
{
    int idx{0};
    std::string hu;
    auto modsource = getCurrentModSource();
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (modsource >= ms_ctrl1 && modsource <= ms_ctrl8)
    {
        hu = sge->helpURLForSpecial("macro-modbutton");
    }
    else if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
    {
        hu = sge->helpURLForSpecial("lfo-modbutton");
    }
    else if ((modsource >= ms_ampeg && modsource <= ms_filtereg) ||
             (modsource >= ms_random_bipolar && modsource <= ms_alternate_unipolar))
    {
        hu = sge->helpURLForSpecial("internalmod-modbutton");
    }
    else
    {
        hu = sge->helpURLForSpecial("other-modbutton");
    }

    if (!addedToModbuttonContextMenu)
    {
        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            sge->addHelpHeaderTo(sge->modulatorName(modsource, false), lurl, menu);
        }
        else
        {
            menu.addItem(sge->modulatorName(modsource, false), []() {});
        }

        menu.addSeparator();
    }
    if (modlist.size() > 1)
    {
        for (auto e : modlist)
        {
            auto modName = std::get<3>(e);

            if (addedToModbuttonContextMenu)
                modName = "Switch to " + modName;

            menu.addItem(modName, [this, idx]() {
                this->modlistIndex = idx;
                mouseMode = HAMBURGER;
                notifyValueChanged();
                mouseMode = NONE;
                repaint();
            });

            idx++;
        }
    }
}

void ModulationSourceButton::mouseDown(const juce::MouseEvent &event)
{
    mouseMode = CLICK;
    everDragged = false;
    mouseDownLocation = event.position;

    if (needsHamburger() && hamburgerHome.contains(event.position.toInt()))
    {
        auto menu = juce::PopupMenu();

        buildHamburgerMenu(menu, false);

        mouseMode = HAMBURGER;

        if (!juce::PopupMenu::dismissAllActiveMenus())
        {
            menu.showMenuAsync(juce::PopupMenu::Options(), [this](int) { endHover(); });
        }
        return;
    }

    if (event.mods.isPopupMenu())
    {
        mouseMode = NONE;
        notifyControlModifierClicked(event.mods);

        return;
    }

    if (isMeta)
    {
        // match the mouseable area to the painted macro slider area (including slider frame)
        auto frameRect = getLocalBounds();
        frameRect.reduce(1, 1);
        auto bottomRect = frameRect.withTrimmedTop(splitHeight - 2);

        if (bottomRect.contains(event.position.toInt()))
        {
            mouseMode = PREDRAG_VALUE;
            return;
        }
    }

    if (isLFO())
    {
        auto arrSze = getLocalBounds().withLeft(getLocalBounds().getRight() - 14).withHeight(16);

        if (arrSze.contains(event.position.toInt()))
        {
            mouseMode = CLICK_ARROW;
        }
    }

    mouseDownBounds = getBounds();
    componentDragger.startDraggingComponent(this, event);
}

void ModulationSourceButton::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (isMeta)
    {
        auto topRect = getLocalBounds().withHeight(splitHeight);

        // rename macro on double-click
        if (topRect.contains(event.position.toInt()))
        {
            auto ccid = (int)getCurrentModSource() - ms_ctrl1;
            auto sge = firstListenerOfType<SurgeGUIEditor>();

            sge->openMacroRenameDialog(ccid, topRect.getTopLeft(), this);

            return;
        }

        // match the mouseable area to the painted macro slider area (including slider frame)
        auto frameRect = getLocalBounds();
        frameRect.reduce(1, 1);
        auto bottomRect = frameRect.withTrimmedTop(splitHeight - 2);

        if (bottomRect.contains(event.position.toInt()))
        {
            value = isBipolar ? 0.5f : 0.f;

            mouseMode = DRAG_VALUE;
            notifyBeginEdit();
            notifyValueChanged();
            notifyEndEdit();
            repaint();
            mouseMode = NONE;

            return;
        }
    }
}

void ModulationSourceButton::mouseEnter(const juce::MouseEvent &event)
{
    isHovered = true;
    repaint();
}

void ModulationSourceButton::mouseExit(const juce::MouseEvent &event) { endHover(); }

void ModulationSourceButton::endHover()
{
    bool oh = isHovered;
    isHovered = false;

    if (oh != isHovered)
    {
        repaint();
    }
}

void ModulationSourceButton::onSkinChanged()
{
    arrow = associatedBitmapStore->getImage(IDB_MODSOURCE_SHOW_LFO);
}

void ModulationSourceButton::mouseUp(const juce::MouseEvent &event)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
    transientArmed = false;
    if (mouseMode == CLICK || mouseMode == CLICK_ARROW)
    {
        notifyValueChanged();
    }

    if (mouseMode == DRAG_COMPONENT_HAPPEN)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();
        auto q = event.position.translated(getBounds().getX(), getBounds().getY());

        sge->modSourceButtonDroppedAt(this, q.toInt());
        setBounds(mouseDownBounds);
    }

    if (mouseMode == DRAG_VALUE)
    {
        auto p = event.mouseDownPosition;

        if (value != valAtMouseDown)
        {
            p = juce::Point<float>(value * getWidth(), 20);
        }

        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
            p = localPointToGlobal(p);
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
        }

        notifyEndEdit();
    }

    mouseMode = NONE;

    return;
}

void ModulationSourceButton::mouseDrag(const juce::MouseEvent &event)
{
    if (mouseMode == NONE)
    {
        return;
    }

    auto distance = event.position.getX() - mouseDownLocation.getX();

    if (mouseMode == PREDRAG_VALUE && distance == 0)
        return;

    if (mouseMode == PREDRAG_VALUE)
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }
        notifyBeginEdit();
        mouseMode = DRAG_VALUE;
        valAtMouseDown = value;
    }

    if (mouseMode == DRAG_VALUE)
    {
        float mul = 1.f;

        if (event.mods.isShiftDown())
        {
            mul = 0.1f;
        }

        value = limit01(valAtMouseDown + mul * event.getDistanceFromDragStartX() / getWidth());
        notifyValueChanged();
        repaint();

        return;
    }

    if (event.getDistanceFromDragStart() < 4)
    {
        return;
    }

    getParentComponent()->toFront(false);
    toFront(false);

    if (mouseMode != DRAG_COMPONENT_HAPPEN)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    mouseMode = DRAG_COMPONENT_HAPPEN;
    componentDragger.dragComponent(this, event, nullptr);
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    auto q = event.position.translated(getBounds().getX(), getBounds().getY());
    auto ota = transientArmed;
    if (sge)
    {
        /* transientArmed = */ sge->modSourceButtonDraggedOver(this, q.toInt());
    }
    if (ota != transientArmed)
        repaint();
    everDragged = true;
}

void ModulationSourceButton::mouseWheelMove(const juce::MouseEvent &event,
                                            const juce::MouseWheelDetails &wheel)
{
    if (isMeta)
    {
        float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;

        if (delta == 0)
        {
            return;
        }

#if MAC
        float speed = 1.2;
#else
        float speed = 0.42666;
#endif
        if (event.mods.isShiftDown())
        {
            speed = speed / 10.0;
        }

        value = limit01(value + speed * delta);
        mouseMode = DRAG_VALUE;

        notifyValueChanged();

        mouseMode = NONE;

        repaint();
    }
    else if (needsHamburger())
    {
        int dir = wheelAccumulationHelper.accumulate(wheel, false, true);

        if (dir != 0)
        {
            auto n = this->modlistIndex - dir;

            if (n < 0)
            {
                n = this->modlist.size() - 1;
            }
            else if (n >= modlist.size())
            {
                n = 0;
            }

            this->modlistIndex = n;
            mouseMode = HAMBURGER;

            notifyValueChanged();

            mouseMode = NONE;

            repaint();
        }
    }
}

void ModulationSourceButton::resized()
{
    hamburgerHome = getLocalBounds().withWidth(11).reduced(2, 2);
}

void ModulationOverviewLaunchButton::buttonClicked(Button *button)
{
    editor->toggleOverlay(SurgeGUIEditor::MODULATION_EDITOR);
    repaint();
}

void ModulationOverviewLaunchButton::paintButton(juce::Graphics &g,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    auto FillCol = skin->getColor(Colors::ModSource::Unused::Background);
    auto FrameCol = skin->getColor(Colors::ModSource::Unused::Border);
    auto FontCol = skin->getColor(Colors::ModSource::Unused::Text);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        FrameCol = skin->getColor(Colors::ModSource::Unused::BorderHover);
        FontCol = skin->getColor(Colors::ModSource::Unused::TextHover);
    }

    g.fillAll(FillCol);
    g.setColour(FrameCol);
    g.drawRect(getLocalBounds(), 1);

    std::string msg = "List";

    if (editor->isAnyOverlayPresent(SurgeGUIEditor::MODULATION_EDITOR))
    {
        msg = "Close";
    }

    auto f = Surge::GUI::getFontManager()->getLatoAtSize(9);
    auto h = f.getHeight() * 0.9f;
    auto sh = h * msg.length();
    auto y0 = (getHeight() - sh) / 2.f;

    g.setFont(f);
    g.setColour(FontCol);

    for (auto c : msg)
    {
        auto s = std::string("") + c;

        g.drawText(s, juce::Rectangle<int>(0, y0, getWidth(), h), juce::Justification::centred);

        y0 += h;
    }
}

} // namespace Widgets
} // namespace Surge