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

#include "MultiSwitch.h"
#include "SurgeImage.h"
#include "basic_dsp.h"

namespace Surge
{
namespace Widgets
{

MultiSwitch::MultiSwitch() { setRepaintsOnMouseActivity(true); }
MultiSwitch::~MultiSwitch() = default;

void MultiSwitch::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    auto y = -valueToOff(value) * heightOfOneImage;
    auto t = juce::AffineTransform().translated(0, y);
    g.reduceClipRegion(getLocalBounds());
    switchD->draw(g, 1.0, t);

    // Do hover
    if (isHovered)
    {
        int iv = getIntegerValue();
        if (iv == hoverSelection && hoverOnSwitchD)
        {
            hoverOnSwitchD->draw(g, 1.0, t);
        }
        else if (hoverSwitchD)
        {
            auto y2 = hoverSelection + frameOffset;
            auto t2 = juce::AffineTransform().translated(0, -y2 * heightOfOneImage);
            hoverSwitchD->draw(g, 1.0, t2);
        }
    }
}

int MultiSwitch::coordinateToSelection(int x, int y)
{
    double coefX = (double)getWidth() / (double)columns;
    double coefY = (double)getHeight() / (double)rows;

    int my = (int)(y / coefY);
    int mx = (int)(x / coefX);

    if (columns * rows > 1)
    {
        return (mx + my * columns);
    }
    return 0;
}

float MultiSwitch::coordinateToValue(int x, int y)
{
    if (rows * columns <= 1)
        return 0;

    return 1.f * coordinateToSelection(x, y) / (rows * columns - 1);
}

void MultiSwitch::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    setValue(coordinateToValue(event.x, event.y));
    notifyValueChanged();
}

void MultiSwitch::mouseMove(const juce::MouseEvent &event)
{
    int ohs = hoverSelection;
    hoverSelection = coordinateToSelection(event.x, event.y);
    if (ohs != hoverSelection || !isHovered)
    {
        repaint();
    }
    isHovered = true;
}

void MultiSwitch::mouseDrag(const juce::MouseEvent &event)
{
    if (draggable)
    {
        int sel = coordinateToSelection(event.x, event.y);
        hoverSelection = sel;
        setValue(limit_range((float)sel / (rows * columns - 1), 0.f, 1.f));
        notifyValueChanged();
    }
}

void MultiSwitch::mouseEnter(const juce::MouseEvent &event)
{
    hoverSelection = coordinateToSelection(event.x, event.y);
    isHovered = true;
}
void MultiSwitch::mouseExit(const juce::MouseEvent &event) { endHover(); }
void MultiSwitch::endHover()
{
    isHovered = false;
    repaint();
}
void MultiSwitch::mouseWheelMove(const juce::MouseEvent &event,
                                 const juce::MouseWheelDetails &wheel)
{
    if (!draggable)
        return;

    int dir = wheelHelper.accumulate(wheel);
    // Veritcally aligned switches have higher values at the bottom
    if (rows > 1)
    {
        dir = -dir;
    }
    if (dir != 0)
    {
        auto iv = limit_range(getIntegerValue() + dir, 0, rows * columns - 1);
        setValue(1.f * iv / (rows * columns - 1));
        notifyValueChanged();
    }
}

#if SURGE_JUCE_ACCESSIBLE

struct MultiSwitchAH : public juce::AccessibilityHandler
{
    struct MSValue : public juce::AccessibilityValueInterface
    {
        explicit MSValue(MultiSwitch *s) : mswitch(s) {}

        MultiSwitch *mswitch;

        bool isReadOnly() const override
        {
            // std::cout << __func__  << " " << __LINE__ << std::endl;
            return false;
        }
        double getCurrentValue() const override
        {
            // std::cout << __func__  << " " << __LINE__ << std::endl;
            return mswitch->getIntegerValue();
        }
        void setValue(double newValue) override
        {
            // std::cout << __func__  << " " << __LINE__ << _D(newValue) << std::endl;
            mswitch->setValue(newValue / (mswitch->rows * mswitch->columns - 1));
            mswitch->repaint();
            mswitch->notifyValueChanged();
        }
        virtual juce::String getCurrentValueAsString() const override
        {
            // std::cout << __func__  << " " << __LINE__ << std::endl;
            return std::to_string(mswitch->getIntegerValue());
        }
        virtual void setValueAsString(const juce::String &newValue) override
        {
            // std::cout << __func__  << " " << __LINE__ << _D(newValue) << std::endl;
            setValue(newValue.getIntValue());
        }
        AccessibleValueRange getRange() const override
        {
            // std::cout << __func__  << " " << __LINE__ << std::endl;
            return {{0, (double)(mswitch->rows * mswitch->columns - 1)}, 1};
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSValue);
    };

    explicit MultiSwitchAH(MultiSwitch *s)
        : mswitch(s), juce::AccessibilityHandler(
                          *s, juce::AccessibilityRole::radioButton,
                          juce::AccessibilityActions().addAction(
                              juce::AccessibilityActionType::showMenu,
                              [this]() { this->showMenu(); }),
                          juce::AccessibilityHandler::Interfaces{std::make_unique<MSValue>(s)})
    {
    }
    void showMenu()
    {
        auto m = juce::ModifierKeys().withFlags(juce::ModifierKeys::rightButtonModifier);
        mswitch->notifyControlModifierClicked(m);
    }

    MultiSwitch *mswitch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSwitchAH);
};

struct MultiSwitchAHMenuOnly : public juce::AccessibilityHandler
{
    explicit MultiSwitchAHMenuOnly(MultiSwitch *s)
        : mswitch(s), juce::AccessibilityHandler(
                          *s, juce::AccessibilityRole::button,
                          juce::AccessibilityActions().addAction(
                              juce::AccessibilityActionType::press, [this]() { this->press(); }))
    {
    }
    void press() { mswitch->notifyValueChanged(); }

    MultiSwitch *mswitch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSwitchAHMenuOnly);
};

std::unique_ptr<juce::AccessibilityHandler> MultiSwitch::createAccessibilityHandler()
{
    if (rows * columns <= 1)
    {
        return std::make_unique<MultiSwitchAHMenuOnly>(this);
    }
    else
    {
        return std::make_unique<MultiSwitchAH>(this);
    }
}
#endif

} // namespace Widgets
} // namespace Surge