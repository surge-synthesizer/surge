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
#include "AccessibleHelpers.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "RuntimeFont.h"

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

    bool doX = (columns > 1 && rows < 2);
    bool doY = (rows > 1 && columns < 2);

    int mx = (int)((x * doX) / coefX);
    int my = (int)((y * doY) / coefY);

    if (columns * rows > 1)
    {
        return limit_range(mx + my * columns, 0, columns * rows - 1);
    }

    return 0;
}

float MultiSwitch::coordinateToValue(int x, int y)
{
    if (rows * columns <= 1)
    {
        return 0;
    }

    return 1.f * coordinateToSelection(x, y) / (rows * columns - 1);
}

void MultiSwitch::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    if (event.mods.isPopupMenu() || event.mods.isCommandDown())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    everDragged = false;
    isMouseDown = true;

    juce::Timer::callAfterDelay(250, [this]() { this->setCursorToArrow(); });
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

void MultiSwitch::setCursorToArrow()
{
    if (!isMouseDown)
    {
        return;
    }

    if (rows * columns > 1)
    {
        if (rows > columns)
        {
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
    }
}

void MultiSwitch::mouseDrag(const juce::MouseEvent &event)
{
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    if (draggable)
    {
        if (!everDragged)
        {
            everDragged = true;
            setCursorToArrow();

            if (storage && !Surge::GUI::showCursor(storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    true);
            }
        }

        int sel = coordinateToSelection(event.x, event.y);
        hoverSelection = sel;
        setValue(limit_range((float)sel / (rows * columns - 1), 0.f, 1.f));
        notifyValueChanged();
    }
}

void MultiSwitch::mouseUp(const juce::MouseEvent &event)
{
    isMouseDown = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);

    if (storage && !Surge::GUI::showCursor(storage))
    {
        juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
    }
}

void MultiSwitch::mouseEnter(const juce::MouseEvent &event) { startHover(event.position); }

void MultiSwitch::startHover(const juce::Point<float> &p)
{
    hoverSelection = coordinateToSelection(p.x, p.y);

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
    {
        return;
    }

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

struct MultiSwitchRadioButton : public juce::Component
{
    MultiSwitchRadioButton(MultiSwitch *s, float value, int ival, const std::string &label)
        : mswitch(s), val(value), ival(ival)
    {
        setDescription(label);
        setTitle(label);
        setInterceptsMouseClicks(false, false);
        setAccessible(true);
    }

    MultiSwitch *mswitch;
    float val;
    int ival;

    struct RBAH : public juce::AccessibilityHandler
    {
        explicit RBAH(MultiSwitchRadioButton *b, MultiSwitch *s)
            : button(b), mswitch(s), juce::AccessibilityHandler(
                                         *b, juce::AccessibilityRole::radioButton,
                                         juce::AccessibilityActions()
                                             .addAction(juce::AccessibilityActionType::press,
                                                        [this]() { this->press(); })
                                             .addAction(juce::AccessibilityActionType::showMenu,
                                                        [this]() { this->showMenu(); }))
        {
        }
        void press()
        {
            mswitch->notifyBeginEdit();
            mswitch->setValue(button->val);
            mswitch->notifyValueChanged();
            mswitch->notifyEndEdit();
            mswitch->repaint();
        }

        void showMenu()
        {
            auto m = juce::ModifierKeys().withFlags(juce::ModifierKeys::rightButtonModifier);
            mswitch->notifyControlModifierClicked(m);
        }

        juce::AccessibleState getCurrentState() const override
        {
            auto state = AccessibilityHandler::getCurrentState();
            state = state.withCheckable();
            if (mswitch->getIntegerValue() == button->ival)
                state = state.withChecked();

            return state;
        }

        MultiSwitch *mswitch;
        MultiSwitchRadioButton *button;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBAH);
    };
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler()
    {
        return std::make_unique<RBAH>(this, mswitch);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSwitchRadioButton);
};

void MultiSwitch::setupAccessibility()
{
    if (rows * columns <= 1) // I use an alternate handler below
    {
        return;
    }

    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    if (selectionComponents.size() == rows * columns)
    {
        removeAllChildren();
        selectionComponents.clear();
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();
    jassert(sge);

    if (!sge)
    {
        return;
    }

    float dr = getHeight() / rows;
    float dc = getWidth() / columns;
    int sel = 0;

    for (int c = 0; c < columns; ++c)
    {
        for (int r = 0; r < rows; ++r)
        {
            float val = ((float)sel) / (rows * columns - 1);
            auto title = sge->getDisplayForTag(getTag(), true, val);
            auto ac = std::make_unique<MultiSwitchRadioButton>(this, val, sel, title);

            sel++;
            ac->getProperties().set("ControlGroup", (int)(c * columns + rows));
            ac->setBounds(juce::Rectangle<int>(c * dc, r * dr, dc, dr));
            addAndMakeVisible(*ac);
            selectionComponents.push_back(std::move(ac));
        }
    }
}

template <> struct DiscreteAHRange<MultiSwitch>
{
    static int iMaxV(MultiSwitch *t) { return t->rows * t->columns - 1; }
    static int iMinV(MultiSwitch *t) { return 0; }
};

template <> struct DiscreteRO<MultiSwitch>
{
    static int isReadOnly(MultiSwitch *t) { return true; }
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
        return std::make_unique<DiscreteAH<MultiSwitch, juce::AccessibilityRole::group>>(this);
    }
}
#else
void MultiSwitch::setupAccessibility() {}
#endif

void MultiSwitchSelfDraw::paint(juce::Graphics &g)
{
    namespace clr = Colors::JuceWidgets::TextMultiSwitch;

    auto uph = skin->getColor(clr::UnpressedHighlight);
    bool royalMode = false;
    if (uph.getAlpha() > 0)
    {
        royalMode = true;
    }

    // these are the classic skin colors just for now
    auto b = getLocalBounds().toFloat().reduced(0.5, 0.5);
    auto corner = 2.f, cornerIn = 1.5f;

    if (royalMode)
    {
        g.setColour(skin->getColor(clr::UnpressedHighlight));
        g.fillRoundedRectangle(b.toFloat(), corner);
        g.setColour(skin->getColor(clr::Background));
        g.fillRoundedRectangle(b.toFloat().reduced(0, 1), corner);
    }
    else
    {
        g.setColour(skin->getColor(clr::Background));
        g.fillRoundedRectangle(b.toFloat(), corner);
    }
    g.setColour(skin->getColor(clr::Border));
    g.drawRoundedRectangle(b.toFloat(), corner, 1);

    auto cw = 1.f * (getWidth() - 2) / columns;
    auto ch = 1.f * (getHeight() - 2) / rows;

    bool solo = rows * columns == 1;

    // Draw the separators
    g.setColour(skin->getColor(clr::Separator));

    float yInsetMul = royalMode ? 0.f : 1.f;
    if (rows == 1)
    {
        for (int c = 1; c < columns; ++c)
        {
            auto r = juce::Rectangle<float>(cw * c - 0.5 + 1, yInsetMul * 3, 1,
                                            getHeight() - yInsetMul * 5);
            g.fillRect(r);
        }
    }
    else if (columns == 1)
    {
        for (int r = 1; r < rows; ++r)
        {
            auto q = juce::Rectangle<float>(yInsetMul * 4, ch * r - 0.5 + 1,
                                            getWidth() - yInsetMul * 9, 1);
            g.fillRect(q);
        }
    }

    int idx = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (auto c = 0; c < columns; ++c)
        {
            auto rc = juce::Rectangle<float>(c * cw + 1, r * ch + 1, cw, ch);
            auto fc = rc.reduced(1.5, 1.5);
            if (royalMode)
                fc = rc;

            auto isOn = idx == getIntegerValue() && !solo;
            auto isHo = isHovered && hoverSelection == idx;
            auto isEn = isEnabled();

            auto fg = skin->getColor(clr::Text);

            if (!isEn)
            {
                fg = juce::Colour(skin->getColor(clr::DeactivatedText));
            }
            else if (isOn && isHo)
            {
                g.setColour(skin->getColor(clr::HoverOnFill));
                g.fillRoundedRectangle(fc.toFloat(), cornerIn);

                g.setColour(skin->getColor(clr::HoverOnBorder));
                g.drawRoundedRectangle(fc.toFloat().reduced(0.5), cornerIn, 1);

                fg = skin->getColor(clr::HoverOnText);
            }
            else if (isOn)
            {
                g.setColour(skin->getColor(clr::OnFill));
                g.fillRoundedRectangle(fc.toFloat(), cornerIn);

                fg = skin->getColor(clr::OnText);
            }
            else if (isHo)
            {
                if (solo)
                {
                    g.setColour(skin->getColor(clr::HoverOnFill));
                    g.fillRoundedRectangle(fc.toFloat(), cornerIn);

                    fg = skin->getColor(clr::HoverOnText);
                }
                else
                {
                    g.setColour(skin->getColor(clr::HoverFill));
                    g.fillRoundedRectangle(fc.toFloat(), cornerIn);
                    fg = skin->getColor(clr::HoverText);
                }
            }

            g.setColour(fg);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(8, juce::Font::bold));
            g.drawText(labels[idx], rc, juce::Justification::centred);

            idx++;
        }
    }
}
} // namespace Widgets
} // namespace Surge
