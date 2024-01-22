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

#include "MenuForDiscreteParams.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Widgets
{
MenuForDiscreteParams::MenuForDiscreteParams()
    : juce::Component(), WidgetBaseMixin<MenuForDiscreteParams>(this){};
MenuForDiscreteParams::~MenuForDiscreteParams() = default;

void MenuForDiscreteParams::paint(juce::Graphics &g)
{
    auto imgt = juce::AffineTransform();
    auto glyphMenuBounds = getLocalBounds();
    auto glyphBox = juce::Rectangle<int>(0, 0, dragGlyphBoxSize, dragGlyphBoxSize);
    auto glyphBoxTransform = juce::AffineTransform();

    if (glyphMode)
    {
        imgt = imgt.translated(0, 2);
        glyphMenuBounds = glyphMenuBounds.withTrimmedTop(2).withTrimmedBottom(2);

        switch (glyphLocation)
        {
        case LEFT:
        {
            float db = 0.5 * (getHeight() - dragGlyphBoxSize);

            imgt = imgt.translated(dragRegion.getWidth(), 0);
            glyphMenuBounds = glyphMenuBounds.withTrimmedLeft(dragRegion.getWidth());
            glyphBoxTransform = glyphBoxTransform.translated(0, db);

            break;
        }
        default:
            jassert(false);
        }
    }

    glyphPosition = glyphBox.transformedBy(glyphBoxTransform).toFloat();

    if (bg)
    {
        float dOp = 1.0;

        if ((hasDeactivatedFn && isDeactivatedFn()) || isDeactivated)
        {
            dOp = 0.5;
        }

        bg->draw(g, dOp, imgt);

        if (isHovered && bghover)
        {
            bghover->draw(g, dOp, imgt);
        }
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (!sge)
    {
        return;
    }

    auto dt = sge->getDisplayForTag(getTag());

    if (glyphMode)
    {
        // draw the glyph
        {
            juce::Graphics::ScopedSaveState gs(g);
            g.addTransform(glyphBoxTransform);

            int iv = floor(getValue() * (iMax - iMin) + 0.5);
            int yv = iv;
            int xv = 0;

            if (!gli.empty())
            {
                auto gim = gli[iv];
                xv = gim.first;
                yv = gim.second;
            }

            auto gt =
                juce::AffineTransform().translated(-xv * dragGlyphBoxSize, -yv * dragGlyphBoxSize);

            g.reduceClipRegion(glyphBox);

            if (dragGlyph)
            {
                auto opacity = 1.0;

                if ((hasDeactivatedFn && isDeactivatedFn()) || isDeactivated)
                {
                    opacity = 0.5;
                }

                if (isHovered && dragGlyphHover)
                {
                    dragGlyphHover->draw(g, opacity, gt);
                }
                else
                {
                    dragGlyph->draw(g, opacity, gt);
                }
            }
            else
            {
                std::cout << "No DragGlyph" << std::endl;
            }
        }

        auto r = glyphMenuBounds.withTrimmedLeft(6);
        auto valcol = skin->getColor(Colors::Menu::FilterValue);

        g.setFont(skin->fontManager->displayFont);

        if (isHovered)
        {
            valcol = skin->getColor(Colors::Menu::FilterValueHover);
        }

        if ((hasDeactivatedFn && isDeactivatedFn()) || isDeactivated)
        {
            valcol = valcol.withAlpha(0.5f);
        }

        g.setColour(valcol);
        g.drawText(dt, r, juce::Justification::centredLeft);
    }
    else
    {
        auto r = getLocalBounds()
                     .withTrimmedTop(0)
                     .withTrimmedBottom(4)
                     .withTrimmedLeft(6)
                     .withTrimmedRight(15);
        g.setFont(skin->fontManager->displayFont);

        auto labcol = skin->getColor(Colors::Menu::Name);
        auto valcol = skin->getColor(Colors::Menu::Value);

        if (isHovered)
        {
            labcol = skin->getColor(Colors::Menu::NameHover);
            valcol = skin->getColor(Colors::Menu::ValueHover);
        }

        if ((hasDeactivatedFn && isDeactivatedFn()) || isDeactivated)
        {
            labcol = skin->getColor(Colors::Menu::NameDeactivated);
            valcol = skin->getColor(Colors::Menu::ValueDeactivated);
        }

        g.setColour(labcol);
        g.drawText(label, r, juce::Justification::centredLeft);
        g.setColour(valcol);
        g.drawText(dt, r, juce::Justification::centredRight);
    }
}

void MenuForDiscreteParams::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    mouseDownLongHold(event);

    if (glyphMode && glyphPosition.contains(event.position))
    {
        mouseDownOrigin = event.position.toInt();
        isDraggingGlyph = true;
        lastDragDistance = 0;
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }
        notifyBeginEdit();
        return;
    }
    isDraggingGlyph = false;
    notifyControlModifierClicked(event.mods, true);
}

void MenuForDiscreteParams::mouseDrag(const juce::MouseEvent &event)
{
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    mouseDragLongHold(event);

    auto d = event.getDistanceFromDragStartX() - event.getDistanceFromDragStartY();

    if (fabs(d - lastDragDistance) > 10)
    {
        int inc = 1;

        if (d - lastDragDistance < 0)
        {
            inc = -1;
        }

        setValue(nextValueInOrder(value, inc));
        notifyValueChanged();
        repaint();
        lastDragDistance = d;
    }
}

void MenuForDiscreteParams::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (glyphMode && glyphPosition.contains(event.position))
    {
        notifyControlModifierDoubleClicked(event.mods.allKeyboardModifiers);
    }
}

void MenuForDiscreteParams::mouseUp(const juce::MouseEvent &event)
{
    mouseUpLongHold(event);

    if (isDraggingGlyph && !Surge::GUI::showCursor(storage))
    {
        juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
        auto p = localPointToGlobal(mouseDownOrigin);
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p.toFloat());
        notifyEndEdit();
    }
    isDraggingGlyph = false;
}

void MenuForDiscreteParams::mouseWheelMove(const juce::MouseEvent &event,
                                           const juce::MouseWheelDetails &w)
{
    int dir = wheelAccumulationHelper.accumulate(w, false, true);
    if (dir != 0)
    {
        notifyBeginEdit();
        setValue(nextValueInOrder(value, -dir));
        notifyValueChanged();
        notifyEndEdit();
        repaint();
    }
}

float MenuForDiscreteParams::nextValueInOrder(float v, int inc)
{
    int iv = Parameter::intUnscaledFromFloat(v, iMax, iMin);
    if (!intOrdering.empty())
    {
        int pidx = 0;

        for (int idx = 0; idx < intOrdering.size(); idx++)
        {
            if (intOrdering[idx] == iv)
            {
                pidx = idx;
                break;
            }
        }

        int nidx = pidx + inc;

        if (nidx < 0)
        {
            nidx = intOrdering.size() - 1;
        }
        else if (nidx >= intOrdering.size())
        {
            nidx = 0;
        }

        while (intOrdering[nidx] < 0)
        {
            nidx += inc;

            if (nidx < 0)
            {
                nidx = intOrdering.size() - 1;
            }

            if (nidx >= intOrdering.size())
            {
                nidx = 0;
            }
        }

        iv = intOrdering[nidx];

        if (iv < 0)
        {
            iv = iMax;
        }

        if (iv > iMax)
        {
            iv = 0;
        }
    }
    else
    {
        iv = iv + inc;

        if (iv < 0)
        {
            iv = iMax;
        }

        if (iv > iMax)
        {
            iv = 0;
        }
    }

    // This is the get_value_f01 code
    float r = Parameter::intScaledToFloat(iv, iMax, iMin);

    return r;
}

std::unique_ptr<juce::AccessibilityHandler> MenuForDiscreteParams::createAccessibilityHandler()
{
    // Why a slider? combo box announces wrong
    return std::make_unique<DiscreteAH<MenuForDiscreteParams, juce::AccessibilityRole::slider>>(
        this);
}

bool MenuForDiscreteParams::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu)
    {
        notifyControlModifierClicked(juce::ModifierKeys(), true);
        return true;
    }

    if (action != Increase && action != Decrease)
        return false;

    int dir = 1;
    if (action == Decrease)
    {
        dir = -1;
    }

    notifyBeginEdit();
    setValue(nextValueInOrder(value, -dir));
    notifyValueChanged();
    notifyEndEdit();

    rebuildOnFocus = true;
    repaint();
    return true;
}

void MenuForDiscreteParams::focusLost(juce::Component::FocusChangeType cause)
{
    endHover();
    if (rebuildOnFocus)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();
        if (sge)
        {
            sge->queue_refresh = true;
        }
    }
}

} // namespace Widgets
} // namespace Surge
