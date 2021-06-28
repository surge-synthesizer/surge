/*
 * Include this before ANY vstgui and after you have included JuceHeader and you stand a chance
 *
 * IF YOU ARE READING THIS and you haven't been following the convresation on discord it will
 * look terrible and broken and wierd. That's because right now it is. If you want some context
 * here *please please* chat with @baconpaul in the #surge-development channel on discord or
 * check the pinned 'status' issue on the surge github. This code has all sorts of problems
 * (dependencies out of order, memory leaks, etc...) which I'm working through as I bootstrap
 * our way out of vstgui.
 */

#ifndef SURGE_ESCAPE_FROM_VSTGUI_H
#define SURGE_ESCAPE_FROM_VSTGUI_H

#include <memory>
#include <unordered_set>
#include "DebugHelpers.h"

#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#if MAC
#define DEBUG_EFVG_MEMORY 1
#define DEBUG_EFVG_MEMORY_STACKS 0
#else
#define DEBUG_EFVG_MEMORY 0
#define DEBUG_EFVG_MEMORY_STACKS 0
#endif

#if DEBUG_EFVG_MEMORY
#include <unordered_map>

#if MAC || LINUX
#include <execinfo.h>
#include <cstdio>
#include <cstdlib>
#endif

#endif

#include <JuceHeader.h>

// The layers of unimpl. Really bad (D), standard, and we are OK with it for now (OKUNIMPL);
#define DUNIMPL                                                                                    \
    std::cout << "  - efvg unimplemented : " << __func__ << " at " << __FILE__ << ":" << __LINE__  \
              << std::endl;
#define UNIMPL void(0);
//#define UNIMPL DUNIMPL
#define UNIMPL_STACK DUNIMPL EscapeNS::Internal::printStack(__func__);
#define OKUNIMPL void(0);

typedef int VstKeyCode;

namespace EscapeNS
{

typedef float CCoord;
struct CPoint
{
    CPoint() : x(0), y(0) {}
    CPoint(float x, float y) : x(x), y(y) {}
    CPoint(const juce::Point<float> &p) : x(p.x), y(p.y) {}
    CPoint(const juce::Point<int> &p) : x(p.x), y(p.y) {}
    float x, y;

    bool operator==(const CPoint &that) const { return (x == that.x && y == that.y); }
    bool operator!=(const CPoint &that) const { return !(*this == that); }
    CPoint operator-(const CPoint &that) const { return {x - that.x, y - that.y}; }
    CPoint offset(float dx, float dy)
    {
        x += dx;
        y += dy;
        return *this;
    }

    operator juce::Point<float>() const { return juce::Point<float>(x, y); }
    operator juce::Point<int>() const { return juce::Point<int>(x, y); }
};

struct CRect
{
    double top = 0, bottom = 0, left = 0, right = 0;
    CRect() = default;
    CRect(juce::Rectangle<int> rect)
    {
        left = rect.getX();
        top = rect.getY();
        right = left + rect.getWidth();
        bottom = top + rect.getHeight();
    }
    CRect(const CPoint &corner, const CPoint &size)
    {
        top = corner.y;
        left = corner.x;
        right = left + size.x;
        bottom = top + size.y;
    }
    CRect(float left, float top, float right, float bottom)
    {
        this->left = left;
        this->top = top;
        this->right = right;
        this->bottom = bottom;
    }

    inline CPoint getSize() const { return CPoint(right - left, bottom - top); }
    inline CPoint getTopLeft() const { return CPoint(left, top); }
    inline CPoint getBottomLeft() const { return CPoint(left, bottom); }
    inline CPoint getTopRight() const { return CPoint(right, top); }
    inline CPoint getBottomRight() const { return CPoint(right, bottom); }

    inline float getWidth() const { return right - left; }
    inline float getHeight() const { return bottom - top; }

    CRect centerInside(const CRect &r)
    {
        UNIMPL;
        return r;
    }
    bool rectOverlap(const CRect &rect)
    {
        return right >= rect.left && left <= rect.right && bottom >= rect.top && top <= rect.bottom;
    }
    inline void setHeight(float h) { bottom = top + h; }
    inline void setWidth(float h) { right = left + h; }

    inline CRect inset(float dx, float dy)
    {
        left += dx;
        right -= dx;
        top += dy;
        bottom -= dy;
        return *this;
    }
    inline CRect offset(float x, float y)
    {
        top += y;
        bottom += y;
        left += x;
        right += x;
        return *this;
    }
    inline CRect moveTo(CPoint p) { return moveTo(p.x, p.y); }
    inline CRect moveTo(float x, float y)
    {
        auto dx = x - left;
        auto dy = y - top;
        top += dy;
        left += dx;
        bottom += dy;
        right += dx;
        return *this;
    }
    inline bool pointInside(const CPoint &where) const { return asJuceFloatRect().contains(where); }
    inline CPoint getCenter()
    {
        auto p = asJuceFloatRect().getCentre();
        return p;
    }
    inline CRect extend(float dx, float dy) { return inset(-dx, -dy); }
    inline CRect bound(CRect &rect)
    {
        if (left < rect.left)
            left = rect.left;
        if (top < rect.top)
            top = rect.top;
        if (right > rect.right)
            right = rect.right;
        if (bottom > rect.bottom)
            bottom = rect.bottom;
        if (bottom < top)
            bottom = top;
        if (right < left)
            right = left;
        return *this;
    }

    operator juce::Rectangle<float>() const
    {
        return juce::Rectangle<float>(left, top, getWidth(), getHeight());
    }
    juce::Rectangle<float> asJuceFloatRect() const
    {
        return static_cast<juce::Rectangle<float>>(*this);
    }
    juce::Rectangle<int> asJuceIntRect() const
    {
        juce::Rectangle<float> f = *this;
        return f.toNearestIntEdges();
    }
};

enum CMouseEventResult
{
    kMouseEventNotImplemented = 0,
    kMouseEventHandled,
    kMouseEventNotHandled,
    kMouseDownEventHandledButDontNeedMovedOrUpEvents,
    kMouseMoveEventHandledButDontNeedMoreEvents
};

enum CHoriTxtAlign
{
    kLeftText = 0,
    kCenterText,
    kRightText
};

enum CButton
{
    /** left mouse button */
    kLButton = juce::ModifierKeys::leftButtonModifier,
    /** middle mouse button */
    kMButton = juce::ModifierKeys::middleButtonModifier,
    /** right mouse button */
    kRButton = juce::ModifierKeys::rightButtonModifier,
    /** shift modifier */
    kShift = juce::ModifierKeys::shiftModifier,
    /** control modifier (Command Key on Mac OS X and Control Key on Windows) */
    kControl = juce::ModifierKeys::ctrlModifier,
    /** alt modifier */
    kAlt = juce::ModifierKeys::altModifier,
    /** apple modifier (Mac OS X only. Is the Control key) */
    kApple = 1U << 29,
    /** 4th mouse button */
    kButton4 = 1U << 30,
    /** 5th mouse button */
    kButton5 = 1U << 31,
    /** mouse button is double click */
    kDoubleClick = 1U << 22,
    /** system mouse wheel setting is inverted (Only valid for onMouseWheel methods). The distance
       value is already transformed back to non inverted. But for scroll views we need to know if we
       need to invert it back. */
    kMouseWheelInverted = 1U << 11,
    /** event caused by touch gesture instead of mouse */
    kTouch = 1U << 12
};

// https://docs.juce.com/master/classModifierKeys.html#ae15cb452a97164e1b857086a1405942f
struct CButtonState
{
    int state = juce::ModifierKeys::noModifiers;
    CButtonState()
    {
        auto ms = juce::ModifierKeys::currentModifiers;
        state = ms.getRawFlags();
    }
    CButtonState(CButton &b) { state = (int)b; }
    CButtonState(int b) { state = b; }
    CButtonState(const juce::ModifierKeys &m) { state = m.getRawFlags(); }
    operator int() const { return state; }
    bool isRightButton() const { return (state & kRButton); }
    bool isTouch() const { return (state & kTouch); }
    int getButtonState() const { return state; }
};

#define CLASS_METHODS(a, b)
} // namespace EscapeNS

namespace VSTGUI = EscapeNS;

#endif // SURGE_ESCAPE_FROM_VSTGUI_H
