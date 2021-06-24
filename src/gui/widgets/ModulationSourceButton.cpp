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
#include "SurgeBitmaps.h"
#include "basic_dsp.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Widgets
{
ModulationSourceButton::ModulationSourceButton() {}
void ModulationSourceButton::paint(juce::Graphics &g)
{
    /*
     * state
     * 0 - nothing
     * 1 - selected modeditor
     * 2 - selected modsource (locked)
     * 4 [bit 2] - selected arrowbutton [0,1,2 -> 4,5,6]
     */

    bool SelectedModSource = (state & 3) == 1;
    bool ActiveModSource = (state & 3) == 2;
    bool UsedOrActive = isUsed || (state & 3);

    juce::Colour FrameCol, FillCol, FontCol;

    FillCol = skin->getColor(Colors::ModSource::Used::Background);
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

    if (ActiveModSource)
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

    if (!isMeta)
    {
        // modbutton name settings
        g.setColour(FontCol);
        g.setFont(Surge::GUI::getFontManager()->displayFont);

        // modbutton name
        if (hasAlternate && useAlternate)
        {
            g.drawText(alternateLabel, getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            g.drawText(label, getLocalBounds(), juce::Justification::centred);
        }

        // modbutton frame
        g.setColour(FrameCol);
        g.drawRect(getLocalBounds(), 1);
    }
    else
    {
        // macro name area
        auto topRect = getLocalBounds().withHeight(splitHeight);
        g.setColour(FontCol);
        g.setFont(Surge::GUI::getFontManager()->displayFont);
        g.drawText(label, topRect, juce::Justification::centred);

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

    if (modSource >= ms_lfo1 && modSource <= ms_slfo6)
    {
        auto arrSze = getLocalBounds().withLeft(getLocalBounds().getRight() - 14).withHeight(16);
        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(arrSze);
        float dy = (state >= 4) ? -16 : 0;

        arrow->drawAt(g, arrSze.getX(), arrSze.getY() + dy, 1.0);
    }
}

void ModulationSourceButton::mouseDown(const juce::MouseEvent &event)
{
    mouseMode = CLICK;
    everDragged = false;

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
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
            mouseMode = DRAG_VALUE;
            valAtMouseDown = value;
            return;
        }
    }

    if (modSource >= ms_lfo1 && modSource <= ms_slfo6)
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

        // TODO: double-click to rename macro?
        // if (topRect.contains(event.position.toInt()))
        //{
        //    return;
        //}

        // match the mouseable area to the painted macro slider area (including slider frame)
        auto frameRect = getLocalBounds();
        frameRect.reduce(1, 1);
        auto bottomRect = frameRect.withTrimmedTop(splitHeight - 2);

        if (bottomRect.contains(event.position.toInt()))
        {
            value = isBipolar ? 0.5f : 0.f;

            notifyValueChanged();
            repaint();

            return;
        }
    }
}

void ModulationSourceButton::mouseEnter(const juce::MouseEvent &event)
{
    isHovered = true;
    repaint();
}

void ModulationSourceButton::mouseExit(const juce::MouseEvent &event)
{
    isHovered = false;
    repaint();
}

void ModulationSourceButton::onSkinChanged()
{
    arrow = associatedBitmapStore->getDrawable(IDB_MODSOURCE_SHOW_LFO);
}

void ModulationSourceButton::mouseUp(const juce::MouseEvent &event)
{
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

    if (mouseMode == DRAG_VALUE && everDragged == true)
    {
        juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
        auto p = juce::Point<float>(value * getWidth(), 20);
        p = localPointToGlobal(p);
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
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

    if (event.getDistanceFromDragStart() < 2)
    {
        return;
    }

    toFront(false);
    mouseMode = DRAG_COMPONENT_HAPPEN;
    componentDragger.dragComponent(this, event, nullptr);
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
}
} // namespace Widgets
} // namespace Surge