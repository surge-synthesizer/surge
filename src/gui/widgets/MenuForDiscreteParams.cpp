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

#include "MenuForDiscreteParams.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
MenuForDiscreteParams::MenuForDiscreteParams() = default;
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
            imgt = imgt.translated(dragRegion.getWidth(), 0);
            glyphMenuBounds = glyphMenuBounds.withTrimmedLeft(dragRegion.getWidth());
            float db = 0.5 * (getHeight() - dragGlyphBoxSize);
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
        bg->draw(g, 1.0, imgt);
        if (isHovered && bghover)
        {
            bghover->draw(g, 1.0, imgt);
        }
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (!sge)
        return;

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
                dragGlyph->draw(g, 1.0, gt);

                if (isHovered && dragGlyphHover)
                {
                    dragGlyphHover->draw(g, 1.0, gt);
                }
            }
            else
            {
                std::cout << "No DragGlyph" << std::endl;
            }
        }

        auto r = glyphMenuBounds.withTrimmedLeft(6);
        g.setFont(Surge::GUI::getFontManager()->displayFont);

        auto valcol = skin->getColor(Colors::Menu::FilterValue);

        if (isHovered)
        {
            valcol = skin->getColor(Colors::Menu::FilterValueHover);
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
        g.setFont(Surge::GUI::getFontManager()->displayFont);

        auto labcol = skin->getColor(Colors::Menu::Name);
        auto valcol = skin->getColor(Colors::Menu::Value);
        if (isHovered)
        {
            labcol = skin->getColor(Colors::Menu::NameHover);
            valcol = skin->getColor(Colors::Menu::ValueHover);
        }
        if (isDeactivated)
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

void MenuForDiscreteParams::mouseDrag(const juce::MouseEvent &e)
{
    auto d = e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY();
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
void MenuForDiscreteParams::mouseUp(const juce::MouseEvent &e)
{
    if (isDraggingGlyph && !Surge::GUI::showCursor(storage))
    {
        juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
        auto p = localPointToGlobal(mouseDownOrigin);
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p.toFloat());
        notifyEndEdit();
    }
    isDraggingGlyph = false;
}

void MenuForDiscreteParams::mouseWheelMove(const juce::MouseEvent &e,
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
    if (!intOrdering.empty() && iMax == intOrdering.size() - 1)
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
} // namespace Widgets
} // namespace Surge