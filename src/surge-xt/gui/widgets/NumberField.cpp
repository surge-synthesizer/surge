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

#include "SurgeGUIEditor.h"
#include "NumberField.h"
#include "RuntimeFont.h"
#include "UserDefaults.h"
#include "UnitConversions.h"
#include "basic_dsp.h"
#include "SurgeGUIUtils.h"
#include "AccessibleHelpers.h"
#include <iomanip>
#include <fmt/core.h>

namespace Surge
{
namespace Widgets
{

std::string NumberField::valueToDisplay() const
{
    std::ostringstream oss;
    switch (controlMode)
    {
    case Skin::Parameters::NONE:
        oss << "-";
        break;
    case Skin::Parameters::MIDICHANNEL_FROM_127:
    {
        int mc = iValue / 8 + 1;
        oss << "Ch " << mc;
    }
    break;
    case Skin::Parameters::NOTENAME:
    {
        int oct_offset = 1;
        if (storage)
            oct_offset = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        char notename[16];
        oss << get_notename(notename, iValue, oct_offset);
    }
    break;
    case Skin::Parameters::POLY_COUNT:
        oss << poly << " / " << iValue;
        break;
    default:
        if (extended)
            return fmt::format("{:.2f}", (float)(iValue / 100.0));
        else
            oss << iValue;
        break;
    }
    return oss.str();
}
void NumberField::paint(juce::Graphics &g)
{
    jassert(skin);

    bg->draw(g, 1.0);

    if (bgHover && isHovered)
    {
        bgHover->draw(g, 1.0);
    }

    g.setFont(skin->getFont(Fonts::Widgets::NumberField));

    g.setColour(textColour);

    if (isHovered)
    {
        g.setColour(textHoverColour);
    }

    g.drawText(valueToDisplay(), getLocalBounds(), juce::Justification::centred);
}

void NumberField::bounceToInt()
{
    iValue = limit_range(Parameter::intUnscaledFromFloat(value, iMax, iMin), iMin, iMax);
}

void NumberField::setControlMode(Surge::Skin::Parameters::NumberfieldControlModes n,
                                 bool isExtended)
{
    controlMode = n;
    extended = isExtended;
    switch (controlMode)
    {
    case Skin::Parameters::NONE:
        iMin = 0;
        iMax = 127;
        break;
    case Skin::Parameters::POLY_COUNT:
        iMin = 2;
        iMax = 64;
        break;
    case Skin::Parameters::PB_DEPTH:
        iMin = 0;
        iMax = 24 * (isExtended ? 100 : 1);
        break;
    case Skin::Parameters::NOTENAME:
        iMin = 0;
        iMax = 127;
        break;
    case Skin::Parameters::MIDICHANNEL_FROM_127:
        iMin = 0;
        iMax = 127;
        break;
    case Skin::Parameters::MSEG_SNAP_H:
    case Skin::Parameters::MSEG_SNAP_V:
        iMin = 1;
        iMax = 100;
        break;
    }
    bounceToInt();
}
void NumberField::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    mouseMode = NONE;

    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    mouseDownLongHold(event);

    mouseMode = DRAG;
    notifyBeginEdit();

    if (!Surge::GUI::showCursor(storage))
    {
        juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        mouseDownOrigin = event.position;
    }

    lastDistanceChecked = 0;
}

void NumberField::mouseDrag(const juce::MouseEvent &event)
{
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    mouseDragLongHold(event);

    float d = -event.getDistanceFromDragStartY();
    float dD = d - lastDistanceChecked;
    float thresh = 10;
    if (dD > thresh)
    {
        changeBy(getChangeMultiplier(event));
        lastDistanceChecked = d;
    }
    if (dD < -thresh)
    {
        changeBy(-getChangeMultiplier(event));
        lastDistanceChecked = d;
    }
}
void NumberField::mouseUp(const juce::MouseEvent &event)
{
    mouseUpLongHold(event);

    if (mouseMode == DRAG)
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
            auto p = localPointToGlobal(mouseDownOrigin);
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
        }
        notifyEndEdit();
    }
    mouseMode = NONE;
}

void NumberField::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    notifyControlModifierDoubleClicked(event.mods);
    repaint();
}

void NumberField::mouseWheelMove(const juce::MouseEvent &event,
                                 const juce::MouseWheelDetails &wheel)
{
    int dir = wheelAccumulationHelper.accumulate(wheel);

    if (dir != 0)
    {
        changeBy(dir * getChangeMultiplier(event));
    }
}

bool NumberField::keyPressed(const juce::KeyPress &key)
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

    int amt = 1;
    if (action == Decrease)
    {
        amt = -1;
    }

    if (controlMode == Skin::Parameters::PB_DEPTH && extended && !key.getModifiers().isShiftDown())
    {
        amt = amt * 100;
    }

    notifyBeginEdit();
    changeBy(amt);
    notifyEndEdit();
    repaint();
    return true;
}

void NumberField::changeBy(int inc)
{
    bounceToInt();
    iValue = limit_range(iValue + inc, iMin, iMax);
    value = limit01(Parameter::intScaledToFloat(iValue, iMax, iMin));

    bounceToInt();

    notifyValueChanged();
    repaint();
}

int NumberField::getChangeMultiplier(const juce::MouseEvent &event)
{
    if (controlMode == Skin::Parameters::PB_DEPTH && extended && !event.mods.isShiftDown())
    {
        return 100;
    }
    return 1;
}

std::unique_ptr<juce::AccessibilityHandler> NumberField::createAccessibilityHandler()
{
    return std::make_unique<DiscreteAH<NumberField>>(this);
}
} // namespace Widgets
} // namespace Surge
