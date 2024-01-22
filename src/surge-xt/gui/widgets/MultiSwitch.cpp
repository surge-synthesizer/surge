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

MultiSwitch::MultiSwitch() : juce::Component(), WidgetBaseMixin<MultiSwitch>(this)
{
    setRepaintsOnMouseActivity(true);
    setAccessible(true);
    setWantsKeyboardFocus(true);
}
MultiSwitch::~MultiSwitch() = default;

void MultiSwitch::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState gs(g);
    auto y = -valueToOff(value) * heightOfOneImage;
    auto t = juce::AffineTransform().translated(0, y);

    float activationOpacity = isDeactivated ? 0.25 : 1.0;

    g.reduceClipRegion(getLocalBounds());

    switchD->draw(g, activationOpacity, t);

    if (isHovered)
    {
        int iv = getIntegerValue();

        if (iv == hoverSelection && hoverOnSwitchD)
        {
            hoverOnSwitchD->draw(g, activationOpacity, t);
        }
        else if (hoverSwitchD)
        {
            auto y2 = hoverSelection + frameOffset;
            auto t2 = juce::AffineTransform().translated(0, -y2 * heightOfOneImage);

            hoverSwitchD->draw(g, activationOpacity, t2);
        }
    }
}

int MultiSwitch::coordinateToSelection(int x, int y) const
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

float MultiSwitch::coordinateToValue(int x, int y) const
{
    if (rows * columns <= 1)
    {
        return 0;
    }

    return 1.f * coordinateToSelection(x, y) / (rows * columns - 1);
}

juce::Point<float> MultiSwitch::valueToCoordinate(float val) const
{
    if (rows * columns <= 1)
        return getLocalBounds().getCentre().toFloat();

    auto b = getLocalBounds();
    if (rows == 1)
    {
        auto y = b.getCentreY() * 1.f;
        auto x = b.getWidth() * (getIntegerValue() + 0.5f) / (columns);
        return {x, y};
    }
    else if (columns == 1)
    {
        auto x = b.getCentreX() * 1.f;
        auto y = b.getHeight() * (getIntegerValue() + 0.5f) / (rows);
        return {x, y};
    }
    else
    {
        jassertfalse;
        return getLocalBounds().getCentre().toFloat();
    }
}

void MultiSwitch::mouseDown(const juce::MouseEvent &event)
{
    if (middleClickable && event.mods.isMiddleButtonDown())
    {
        notifyControlModifierClicked(event.mods);
    }

    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    if (event.mods.isPopupMenu() || event.mods.isCommandDown())
    {
        notifyControlModifierClicked(event.mods);

        if (isHovered)
        {
            hoverSelection = coordinateToSelection(event.x, event.y);
        }

        return;
    }

    everDragged = false;
    isMouseDown = true;

    if (draggable)
    {
        juce::Timer::callAfterDelay(250, [that = juce::Component::SafePointer(this)]() {
            if (that)
                that->setCursorToArrow();
        });
    }

    mouseDownLongHold(event);
    setValue(coordinateToValue(event.x, event.y));
    notifyValueChangedWithBeginEnd();

    if (isHovered)
    {
        hoverSelection = getIntegerValue();
    }
}

void MultiSwitch::mouseMove(const juce::MouseEvent &event)
{
    int ohs = hoverSelection;

    mouseMoveLongHold(event);

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

    mouseDragLongHold(event);

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
        auto nv = limit_range((float)sel / (rows * columns - 1), 0.f, 1.f);
        if (getIntegerValueFrom(nv) != getIntegerValue())
        {
            hoverSelection = sel;

            setValue(limit_range((float)sel / (rows * columns - 1), 0.f, 1.f));
            notifyValueChanged();
        }
    }
}

void MultiSwitch::mouseUp(const juce::MouseEvent &event)
{
    mouseUpLongHold(event);
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
    if (everDragged && isMouseDown) // don't change hover state during a drag
    {
        hoverSelection = getIntegerValue();
        isHovered = true;
        return;
    }

    isHovered = true;
    hoverSelection = coordinateToSelection(p.x, p.y);

    repaint();
}

void MultiSwitch::mouseExit(const juce::MouseEvent &event) { endHover(); }

void MultiSwitch::endHover()
{
    if (stuckHover)
        return;

    isHovered = false;
    repaint();
}

void MultiSwitch::mouseWheelMove(const juce::MouseEvent &event,
                                 const juce::MouseWheelDetails &wheel)
{
    // If we aren't draggable and a drag is happening, ignore the mouse wheel gesture
    // This (vs just a plain !draggable) reverts the 1.9 behaviour of mouse wheel through
    // the presets.
    if (!draggable && (event.mouseWasDraggedSinceMouseDown() || event.getLengthOfMousePress() > 0))
    {
        return;
    }

    int dir = wheelHelper.accumulate(wheel);

    // Vertically aligned switches have higher values at the bottom
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

bool MultiSwitch::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu)
    {
        notifyControlModifierClicked(juce::ModifierKeys(), true);
        return true;
    }

    bool doit =
        action != Increase || action != Decrease || (rows * columns == 1 && action == Return);

    if (!doit)
        return false;

    int dir = 1;
    if (action == Decrease)
    {
        dir = -1;
    }

    auto iv = limit_range(getIntegerValue() + dir, 0, rows * columns - 1);

    if (rows * columns == 1)
    {
        setValue(0);
    }
    else
    {
        _DBGCOUT << "Setting integer value to " << iv << " " << 1.f * iv / (rows * columns - 1)
                 << std::endl;
        setValue(1.f * iv / (rows * columns - 1));
    }
    notifyBeginEdit();
    notifyValueChanged();
    notifyEndEdit();
    repaint();

    return true;
}

template <juce::AccessibilityRole ROLE> struct MultiSwitchAccOverlayButton : public juce::Component
{
    MultiSwitchAccOverlayButton(MultiSwitch *s, float value, int ival, const std::string &label)
        : mswitch(s), val(value), ival(ival)
    {
        setDescription(label);
        setTitle(label);
        setInterceptsMouseClicks(false, false);
        setAccessible(true);
        setWantsKeyboardFocus(true);
    }

    MultiSwitch *mswitch;
    float val;
    int ival;

    struct RBAH : public juce::AccessibilityHandler
    {
        explicit RBAH(MultiSwitchAccOverlayButton *b, MultiSwitch *s)
            : button(b), mswitch(s),
              juce::AccessibilityHandler(
                  *b, ROLE,
                  juce::AccessibilityActions()
                      .addAction(juce::AccessibilityActionType::press, [this]() { this->press(); })
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
        MultiSwitchAccOverlayButton *button;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBAH);
    };
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler()
    {
        return std::make_unique<RBAH>(this, mswitch);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSwitchAccOverlayButton);
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
            std::unique_ptr<juce::Component> ac;
            if (isAlwaysAccessibleMomentary())
            {
                title = getTitle().toStdString() + " " + title;
                ac = std::make_unique<MultiSwitchAccOverlayButton<juce::AccessibilityRole::button>>(
                    this, val, sel, title);
            }
            else
            {
                ac = std::make_unique<
                    MultiSwitchAccOverlayButton<juce::AccessibilityRole::radioButton>>(this, val,
                                                                                       sel, title);
            }
            sel++;
            ac->getProperties().set("ControlGroup", (int)(c * columns + rows));
            ac->setBounds(juce::Rectangle<int>(c * dc, r * dr, dc, dr));
            ac->setAccessible(true);
            addAndMakeVisible(*ac);
            selectionComponents.push_back(std::move(ac));
        }
    }
}

juce::Component *MultiSwitch::getCurrentAccessibleSelectionComponent()
{
    if (rows * columns <= 1)
        return this;

    if (getIntegerValue() < 0 || getIntegerValue() >= selectionComponents.size())
    {
        return nullptr;
    }
    return selectionComponents[getIntegerValue()].get();
}

void MultiSwitch::updateAccessibleStateOnUserValueChange()
{
    if (isAlwaysAccessibleMomentary())
        return;

    if (getIntegerValue() < 0 || getIntegerValue() >= selectionComponents.size())
    {
        return;
    }
    selectionComponents[getIntegerValue()]->grabKeyboardFocus();
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

void MultiSwitchSelfDraw::onSkinChanged()
{
    MultiSwitch::onSkinChanged();
    font = skin->getFont(Fonts::Widgets::SelfDrawSwitchFont);
}

void MultiSwitchSelfDraw::paint(juce::Graphics &g)
{
    namespace clr = Colors::JuceWidgets::TextMultiSwitch;

    auto uph = skin->getColor(clr::UnpressedHighlight);
    bool royalMode = false;
    const bool isEn = isEnabled() && !isDeactivated;
    const float alpha = isEn ? 1.f : 0.35f;

    if (uph.getAlpha() > 0)
    {
        royalMode = true;
    }

    auto b = getLocalBounds().toFloat().reduced(0.5, 0.5);
    auto corner = 2.f, cornerIn = 1.5f;

    if (royalMode)
    {
        g.setColour(skin->getColor(clr::UnpressedHighlight).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b.toFloat(), corner);
        g.setColour(skin->getColor(clr::Background).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b.toFloat().reduced(0, 1), corner);
    }
    else
    {
        g.setColour(skin->getColor(clr::Background).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b.toFloat(), corner);
    }

    g.setColour(skin->getColor(clr::Border).withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(b.toFloat(), corner, 1);

    auto cw = 1.f * (getWidth() - 2) / columns;
    auto ch = 1.f * (getHeight() - 2) / rows;

    bool solo = rows * columns == 1;

    int idx = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (auto c = 0; c < columns; ++c)
        {
            auto rc = juce::Rectangle<float>(c * cw + 1, r * ch + 1, cw, ch);
            auto fc = rc.reduced(1.5, 1.5);

            if (royalMode)
            {
                fc = rc;
                cornerIn = 0.5;
            }

            auto isOn = isCellOn(r, c);
            auto isHo = isHovered && hoverSelection == idx;

            auto fg = isEn ? skin->getColor(clr::Text) : skin->getColor(clr::DeactivatedText);

            if (!isEn)
            {
                fg = juce::Colour(skin->getColor(clr::DeactivatedText));
            }
            else if (isOn && isHo)
            {
                g.setColour(skin->getColor(clr::HoverOnFill));
                g.fillRoundedRectangle(fc.toFloat(), cornerIn);

                if (!royalMode)
                {
                    g.setColour(skin->getColor(clr::HoverOnBorder));
                    g.drawRoundedRectangle(fc.toFloat().reduced(0.5), cornerIn, 1);
                }

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
                    g.setColour(skin->getColor(clr::HoverFill));
                    g.fillRoundedRectangle(fc.toFloat(), cornerIn);

                    fg = skin->getColor(royalMode ? clr::Text : clr::TextHover);
                }
                else
                {
                    g.setColour(skin->getColor(clr::HoverFill));
                    g.fillRoundedRectangle(fc.toFloat(), cornerIn);

                    fg = skin->getColor(clr::TextHover);
                }
            }

            g.setColour(fg);
            g.setFont(font);
            g.drawText(labels[idx], rc, juce::Justification::centred);

            idx++;
        }

        // Draw the separators
        g.setColour(skin->getColor(clr::Separator));

        float sepThickness = royalMode ? 0.5f : 1.f;
        float sepOffset = royalMode ? 0.75f : 0.5f;
        float sepInsetMul = royalMode ? 0.f : 1.f;

        if (rows == 1)
        {
            for (int c = 1; c < columns; ++c)
            {
                auto r = juce::Line<float>(cw * c + sepOffset, sepInsetMul * 2.5f,
                                           cw * c + sepOffset, getHeight() - sepInsetMul * 2.5f);
                g.drawLine(r, sepThickness);
            }
        }
        else if (columns == 1)
        {
            for (int r = 1; r < rows; ++r)
            {
                auto q = juce::Line<float>(2.5f * sepInsetMul, ch * r + sepOffset,
                                           getWidth() - sepInsetMul * 2.5f, ch * r + sepOffset);
                g.drawLine(q, sepThickness);
            }
        }
    }
}
} // namespace Widgets
} // namespace Surge
