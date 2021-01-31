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
    std::cout << "Unimplemented : " << __func__ << " at " << __FILE__ << ":" << __LINE__           \
              << std::endl;
//#define UNIMPL void(0);
#define UNIMPL DUNIMPL
#define OKUNIMPL void(0);

typedef int VstKeyCode;

namespace EscapeFromVSTGUI
{
struct JuceVSTGUIEditorAdapterBase
{
    virtual juce::AudioProcessorEditor *getJuceEditor() = 0;
};
} // namespace EscapeFromVSTGUI

namespace EscapeNS
{
namespace Internal
{
#if DEBUG_EFVG_MEMORY
struct DebugAllocRecord
{
    DebugAllocRecord() { record("CONSTRUCT"); }
    void record(const std::string &where)
    {
#if DEBUG_EFVG_MEMORY_STACKS
#if MAC || LINUX
        void *callstack[128];
        int i, frames = backtrace(callstack, 128);
        char **strs = backtrace_symbols(callstack, frames);
        std::ostringstream oss;
        oss << "----- " << where << " -----\n";
        for (i = 3; i < frames - 1 && i < 20; ++i)
        {
            oss << strs[i] << "\n";
        }
        free(strs);
        records.push_back(oss.str());
#endif
#endif
    }
    int remembers = 0, forgets = 0;
    std::vector<std::string> records;
};
struct FakeRefcount;

inline std::unordered_map<FakeRefcount *, DebugAllocRecord> *getAllocMap()
{
    static std::unordered_map<FakeRefcount *, DebugAllocRecord> map;
    return &map;
}

struct RefActivity
{
    int creates = 0, deletes = 0;
};
inline RefActivity *getRefActivity()
{
    static RefActivity r;
    return &r;
}

#endif
struct FakeRefcount
{
    explicit FakeRefcount(bool doDebug = true, bool alwaysLeak = false)
        : doDebug(doDebug), alwaysLeak(alwaysLeak)
    {
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            getAllocMap()->emplace(std::make_pair(this, DebugAllocRecord()));
            getRefActivity()->creates++;
        }
#else
        if (alwaysLeak)
        {
            std::cout << "YOU LEFT ALWAYS LEAK ON SILLY" << std::endl;
        }
#endif
    }
    virtual ~FakeRefcount()
    {
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            getAllocMap()->erase(this);
            getRefActivity()->deletes++;
        }
#endif
    }

    virtual void remember()
    {
        refCount++;
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            (*getAllocMap())[this].remembers++;
            (*getAllocMap())[this].record("Remember");
        }
#endif
    }
    virtual void forget()
    {
        refCount--;
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            (*getAllocMap())[this].forgets++;
            (*getAllocMap())[this].record("Forget");
        }
#endif
        if (refCount == 0)
        {
            if (!alwaysLeak)
                delete this;
        }
    }
    int64_t refCount = 0;
    bool doDebug = true, alwaysLeak = false;
};

#if DEBUG_EFVG_MEMORY

inline void showMemoryOutstanding()
{
    std::cout << "Total activity : Create=" << getRefActivity()->creates
              << " Delete=" << getRefActivity()->deletes << std::endl;
    for (auto &p : *(getAllocMap()))
    {
        if (p.first->doDebug)
        {
            std::cout << "Still here! " << p.first << " " << p.first->refCount << " "
                      << p.second.remembers << " " << p.second.forgets << " "
                      << typeid(*(p.first)).name() << std::endl;
            for (auto &r : p.second.records)
                std::cout << r << "\n" << std::endl;
        }
    }
}
#endif
} // namespace Internal

#if MAC
inline CFBundleRef getBundleRef() { return nullptr; }
#endif

typedef juce::AudioProcessorEditor EditorType;
typedef const char *UTF8String; // I know I know

struct CFrame;
struct CBitmap;
struct CColor;
struct COptionMenu;

typedef float CCoord;
struct CPoint
{
    CPoint() : x(0), y(0) {}
    CPoint(float x, float y) : x(x), y(y) {}
    CPoint(const juce::Point<float> &p) : x(p.x), y(p.y) {}
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
};

struct CRect
{
    double top = 0, bottom = 0, left = 0, right = 0;
    CRect() = default;
    CRect(juce::Rectangle<int> rect) { DUNIMPL; }
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
    inline CPoint getTopRight() const { return CPoint(right, top); }
    inline CPoint getBottomRight() const { return CPoint(right, bottom); }

    inline float getWidth() const { return right - left; }
    inline float getHeight() const { return bottom - top; }

    CRect centerInside(const CRect &r)
    {
        UNIMPL;
        return r;
    }
    bool rectOverlap(const CRect &r)
    {
        UNIMPL;
        return false;
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

struct CColor
{
    constexpr CColor() : red(0), green(0), blue(0), alpha(255) {}
    constexpr CColor(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b), alpha(255) {}
    constexpr CColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        : red(r), green(g), blue(b), alpha(a)
    {
    }

    bool operator==(const CColor &that)
    {
        return red == that.red && green == that.green && blue == that.blue && alpha == that.alpha;
    }
    juce::Colour asJuceColour() const { return juce::Colour(red, green, blue, alpha); }
    uint8_t red, green, blue, alpha;
};

struct CGradient : public Internal::FakeRefcount
{
    struct ColorStopMap
    {
    };
    static CGradient *create(const ColorStopMap &m) { return nullptr; };
    void addColorStop(float pos, const CColor &c) { UNIMPL; }
};

constexpr const CColor kBlueCColor = CColor(0, 0, 255);
constexpr const CColor kWhiteCColor = CColor(255, 255, 255);
constexpr const CColor kRedCColor = CColor(255, 0, 0);
constexpr const CColor kBlackCColor = CColor(0, 0, 0);
constexpr const CColor kTransparentCColor = CColor(0, 0, 0, 0);

enum CDrawModeFlags : uint32_t
{
    kAliasing = 0,
    kAntiAliasing = 1,
    kNonIntegralMode = 0xF000000
};

enum CDrawStyle : uint32_t
{
    kDrawStroked = 0,
    kDrawFilled,
    kDrawFilledAndStroked
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

enum CCursorType
{
    /** arrow cursor */
    kCursorDefault = 0,
    /** wait cursor */
    kCursorWait,
    /** horizontal size cursor */
    kCursorHSize,
    /** vertical size cursor */
    kCursorVSize,
    /** size all cursor */
    kCursorSizeAll,
    /** northeast and southwest size cursor */
    kCursorNESWSize,
    /** northwest and southeast size cursor */
    kCursorNWSESize,
    /** copy cursor (mainly for drag&drop operations) */
    kCursorCopy,
    /** not allowed cursor (mainly for drag&drop operations) */
    kCursorNotAllowed,
    /** hand cursor */
    kCursorHand,
    /** i beam cursor */
    kCursorIBeam,
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
    operator int() const { return state; }
    bool isRightButton() const { return (state & kRButton); }
    bool isTouch() const { return (state & kTouch); }
    int getButtonState() const { return state; }
};

enum CMouseWheelAxis
{
    kMouseWheelAxisX,
    kMouseWheelAxisY
};

struct CLineStyle
{
    enum LineCap
    {
        kLineCapButt = 0,
        kLineCapRound,
        kLineCapSquare
    };

    enum LineJoin
    {
        kLineJoinMiter = 0,
        kLineJoinRound,
        kLineJoinBevel
    };
    constexpr CLineStyle() {}
    constexpr CLineStyle(const LineCap c, const LineJoin j, int a = 0, int b = 0, bool cb = false)
    {
    }
};

constexpr CLineStyle kLineSolid;

struct CDrawMode
{
    CDrawMode() = default;
    CDrawMode(const CDrawModeFlags &f) { OKUNIMPL; }
    CDrawMode(uint32_t x) { OKUNIMPL; }
};

struct CGraphicsTransform
{
    juce::AffineTransform juceT;
    CGraphicsTransform(const juce::AffineTransform &t) : juceT(t) {}

    CGraphicsTransform() {}
    CGraphicsTransform scale(float sx, float sy)
    {
        return CGraphicsTransform(juceT.scaled(sx, sy));
    }
    CGraphicsTransform translate(float dx, float dy)
    {
        return CGraphicsTransform(juceT.translated(dx, dy));
    }
    CGraphicsTransform inverse() { return CGraphicsTransform(juceT.inverted()); }

    CRect transform(CRect &r)
    {
        auto jr = r.asJuceFloatRect();
        auto tl = jr.getTopLeft();
        auto br = jr.getBottomRight();

        juceT.transformPoint(tl.x, tl.y);
        juceT.transformPoint(br.x, br.y);

        r = CRect(tl.x, tl.y, br.x, br.y);
        return r;
    }
    CPoint transform(CPoint &r)
    {
        float x = r.x;
        float y = r.y;
        juceT.transformPoint(x, y);
        r.x = x;
        r.y = y;
        return CPoint(x, y);
    }
};

// Sure why not.
enum CTxtFace
{
    kNormalFace = 0,
    kBoldFace = 1 << 1,
    kItalicFace = 1 << 2,
    kUnderlineFace = 1 << 3,
    kStrikethroughFace = 1 << 4
};

struct CFontInternal
{
    juce::Font font;
    CFontInternal(const juce::Font &f) : font(f) {}
    float getSize() { return font.getHeight(); }
    void remember() {}
    void forget() {}
};
typedef std::shared_ptr<CFontInternal> CFontRef; // this is a gross signature

struct CGraphicsPath : public Internal::FakeRefcount
{
    juce::Path path;
    void beginSubpath(float x, float y) { path.startNewSubPath(x, y); }
    void addLine(float x, float y) { path.lineTo(x, y); }
};

struct CDrawContext
{
    typedef std::vector<CPoint> PointList;
    enum PathDrawMode
    {
        kPathFilled = 1,
        kPathStroked = 2
    };

    juce::Graphics &g;
    explicit CDrawContext(juce::Graphics &g) : g(g) {}

    void setLineStyle(const CLineStyle &s) { OKUNIMPL; }
    void setDrawMode(uint32_t m) { OKUNIMPL; }
    void setDrawMode(const CDrawMode &m) { OKUNIMPL; }
    CDrawMode getDrawMode()
    {
        OKUNIMPL;
        return CDrawMode();
    }
    float getStringWidth(const std::string &s) { return font->font.getStringWidth(s); }
    void fillLinearGradient(CGraphicsPath *p, const CGradient &, const CPoint &, const CPoint &,
                            bool, CGraphicsTransform *tf)
    {
        UNIMPL;
    }

    struct Transform
    {
        Transform(CDrawContext &dc, CGraphicsTransform &tf) { UNIMPL; }
    };

    void saveGlobalState() { g.saveState(); }

    void restoreGlobalState() { g.restoreState(); }

    void getClipRect(CRect &r) const { UNIMPL; }
    void setClipRect(const CRect &r) { g.reduceClipRegion(r.asJuceIntRect()); }

    float lineWidth = 1;
    void setLineWidth(float w) { lineWidth = w; }
    float getLineWidth() { return lineWidth; }

    juce::Colour fillColor = juce::Colour(255, 0, 0), frameColor = juce::Colour(255, 0, 0),
                 fontColor = juce::Colour(255, 0, 0);
    void setFontColor(const CColor &c) { fontColor = c.asJuceColour(); }
    CColor getFontColor()
    {
        UNIMPL;
        return CColor();
    }

    void setFillColor(const CColor &c) { fillColor = c.asJuceColour(); }

    void setFrameColor(const CColor &c) { frameColor = c.asJuceColour(); }

    void drawRect(const CRect &r, CDrawStyle s = kDrawStroked)
    {
        switch (s)
        {
        case kDrawFilled:
        {
            g.setColour(fillColor);
            g.fillRect(r.asJuceIntRect());
            break;
        }
        case kDrawStroked:
        {
            g.setColour(frameColor);
            g.drawRect(r.asJuceIntRect());
            break;
        }
        case kDrawFilledAndStroked:
        {
            g.setColour(fillColor);
            g.fillRect(r.asJuceIntRect());
            g.setColour(frameColor);
            g.drawRect(r.asJuceIntRect());
        }
        }
    }

    void drawPolygon(const std::vector<CPoint> &, CDrawStyle s) { UNIMPL; }

    CGraphicsPath *createRoundRectGraphicsPath(const CRect &r, float radius)
    {
        auto ct = new CGraphicsPath();
        ct->path.addRoundedRectangle(r.left, r.top, r.getWidth(), r.getHeight(), radius);
        ct->remember();
        return ct;
    }

    void drawGraphicsPath(CGraphicsPath *p, int style, CGraphicsTransform *tf = nullptr)
    {
        g.setColour(frameColor);
        if (tf)
        {
            g.strokePath(p->path, juce::PathStrokeType(1.0), tf->juceT);
        }
        else
        {
            g.strokePath(p->path, juce::PathStrokeType(1.0));
        }
    }
    void drawLine(const CPoint &start, const CPoint &end)
    {
        g.setColour(frameColor);
        g.drawLine(start.x, start.y, end.x, end.y);
    }

    CFontRef font;
    void setFont(CFontRef f) { font = f; }
    CFontRef getFont() { return font; }

    void drawString(const char *t, const CRect &r, int align = kLeftText, bool = true)
    {
        auto al = juce::Justification::centredLeft;
        if (align == kCenterText)
            al = juce::Justification::centred;
        if (align == kRightText)
            al = juce::Justification::centredRight;

        g.setFont(font->font);
        g.setColour(fontColor);
        g.drawText(juce::CharPointer_UTF8(t), r, al);
    }
    void drawString(const char *, const CPoint &, int = 0, bool = true) { UNIMPL; }
    void drawEllipse(const CRect &r, const CDrawStyle &s)
    {
        if (s == kDrawFilled || s == kDrawFilledAndStroked)
        {
            g.setColour(fillColor);
            g.fillEllipse(r);
        }
        if (s == kDrawStroked || s == kDrawFilledAndStroked)
        {
            g.setColour(frameColor);
            g.drawEllipse(r, lineWidth);
        }
    }

    CGraphicsPath *createGraphicsPath()
    {
        auto res = new CGraphicsPath();
        res->remember();
        return res;
    }
};

struct COffscreenContext : public CDrawContext
{
    static COffscreenContext *create(CFrame *frame, CCoord width, CCoord height,
                                     double scaleFactor = 1.);
    void beginDraw() { UNIMPL; }
    void endDraw() { UNIMPL; }
    CBitmap *getBitmap()
    {
        UNIMPL;
        return nullptr;
    }
};

struct CResourceDescription
{
    enum Type : uint32_t
    {
        kIntegerType = 0
    };
    CResourceDescription(int id)
    {
        u.id = id;
        u.type = kIntegerType;
    }
    Type type;
    struct U
    {
        int32_t id = 0;
        Type type = kIntegerType;
    } u;
};

struct CBitmap : public Internal::FakeRefcount
{
    CBitmap(const CResourceDescription &d) : desc(d), Internal::FakeRefcount(false, false) {}
    virtual ~CBitmap()
    {
        // std::cout << " Deleting bitmap with " << desc.u.id << std::endl;
    }

    void draw(CDrawContext *dc, const CRect &r, const CPoint &off = CPoint(), float alpha = 1.0)
    {
        juce::Graphics::ScopedSaveState gs(dc->g);
        auto tl = r.asJuceIntRect().getTopLeft();

        auto t = juce::AffineTransform().translated(tl.x, tl.y).translated(-off.x, -off.y);
        dc->g.reduceClipRegion(r.asJuceIntRect());
        drawable->draw(dc->g, 1.0, t);
    }
    CResourceDescription desc;
    std::unique_ptr<juce::Drawable> drawable;
};

template <typename T> struct CViewWithJuceComponent;

template <typename T> struct juceCViewConnector : public T
{
    juceCViewConnector() : T() {}
    void setViewCompanion(CViewWithJuceComponent<T> *v)
    {
        viewCompanion = v;
        // viewCompanion->remember();
    }
    CViewWithJuceComponent<T> *viewCompanion = nullptr;
    void paint(juce::Graphics &g) override;
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseMove(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
};

// Clena this up obviously
template <typename T> struct CViewWithJuceComponent : public Internal::FakeRefcount
{
    CViewWithJuceComponent(const CRect &size) : size(size) {}
    virtual ~CViewWithJuceComponent(){
        // Here we probably have to make sure that the juce component is removed
    };
    virtual juce::Component *juceComponent()
    {
        if (!comp)
        {
            comp = std::make_unique<juceCViewConnector<T>>();
            comp->setBounds(size.asJuceIntRect());
            comp->setViewCompanion(this);
        }
        return comp.get();
    }
    virtual void draw(CDrawContext *dc) { UNIMPL; };
    CRect getViewSize()
    {
        // FIXME - this should reajju just use the cast-operator
        auto b = juceComponent()->getBounds();
        return CRect(CPoint(b.getX(), b.getY()),
                     CPoint(juceComponent()->getWidth(), juceComponent()->getHeight()));
    }

    CPoint getTopLeft()
    {
        auto b = juceComponent()->getBounds();
        return CPoint(b.getX(), b.getY());
    }

    void setViewSize(const CRect &r) { juceComponent()->setBounds(r.asJuceIntRect()); }
    float getHeight() { return juceComponent()->getHeight(); }
    float getWidth() { return juceComponent()->getWidth(); }

    virtual bool magnify(CPoint &where, float amount) { return false; }

    virtual CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseUp(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseEntered(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseExited(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseMoved(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual bool onWheel(const CPoint &where, const float &distance, const CButtonState &buttons)
    {
        UNIMPL;
        return false;
    }

    virtual bool onWheel(const CPoint &where, const CMouseWheelAxis &, const float &distance,
                         const CButtonState &buttons)
    {
        UNIMPL;
        return false;
    }

    void invalid() { juceComponent()->repaint(); }

    CRect ma;
    CRect getMouseableArea() { return ma; }
    void setMouseableArea(const CRect &r) { ma = r; }

    CPoint localToFrame(CPoint &w)
    {
        UNIMPL;
        return w;
    }

    void setDirty(bool b = true)
    {
        if (b)
            invalid();
    }
    bool isDirty()
    {
        UNIMPL;
        return true;
    }
    void setVisible(bool b) { UNIMPL; }
    bool isVisible()
    {
        UNIMPL;
        return true;
    }
    void setSize(const CRect &s) { UNIMPL; }
    void setSize(float w, float h) { UNIMPL; }
    std::unique_ptr<juceCViewConnector<T>> comp;
    CRect size;
};

#define GSPAIR(x, sT, gT, gTV)                                                                     \
    gT m_##x = gTV;                                                                                \
    virtual void set##x(sT v) { m_##x = v; }                                                       \
    virtual gT get##x() const { return m_##x; }

#define COLPAIR(x)                                                                                 \
    CColor m_##x;                                                                                  \
    virtual void set##x(const CColor &v) { m_##x = v; }                                            \
    virtual CColor get##x() const { return m_##x; }

struct CView : public CViewWithJuceComponent<juce::Component>
{
    CView(const CRect &size) : CViewWithJuceComponent<juce::Component>(size) {}
    ~CView() {}

    CFrame *frame = nullptr;
    CFrame *getFrame() { return frame; }

    CView *parentView = nullptr;
    CView *getParentView() { return parentView; }

    virtual void onAdded() {}
    virtual void efvg_resolveDeferredAdds(){};

    EscapeFromVSTGUI::JuceVSTGUIEditorAdapterBase *ed = nullptr; // FIX ME obviously
};

struct CViewContainer : public CView
{
    CViewContainer(const CRect &size) : CView(size) {}
    ~CViewContainer()
    {
        for (auto e : views)
        {
            e->forget();
        }
        views.clear();
    }
    bool transparent = true;
    void setTransparency(bool b) { transparent = b; }

    CBitmap *bg = nullptr;
    void setBackground(CBitmap *b)
    {
        b->remember();
        if (bg)
            bg->forget();
        bg = b;
    }
    CColor bgcol;
    void setBackgroundColor(const CColor &c) { bgcol = c; }
    int getNbViews() { return views.size(); }
    CView *getView(int i)
    {
        if (i >= 0 && i < views.size())
            return views[i];
        return nullptr;
    }

    void onAdded() override { std::cout << "CVC Added" << std::endl; }
    void addView(CView *v)
    {
        v->remember();
        v->ed = ed;
        v->frame = frame;
        v->parentView = this;
        auto jc = v->juceComponent();
        jc->setTransform(juceComponent()->getTransform());
        if (jc && ed)
        {
            ed->getJuceEditor()->addAndMakeVisible(*jc);
            v->efvg_resolveDeferredAdds();
            v->onAdded();
        }
        else
        {
            deferredAdds.push_back(v);
        }
        views.push_back(v);
    }
    void removeView(CView *v, bool doForget = true)
    {
        auto pos = std::find(views.begin(), views.end(), v);
        if (pos == views.end())
        {
            std::cout << "WIERD ERROR";
            jassert(false);
            return;
        }
        views.erase(pos);
        juceComponent()->removeChildComponent(v->juceComponent());
        v->parentView = nullptr;
        if (doForget)
            v->forget();
        invalid();
    }

    void addView(COptionMenu *)
    {
        // JUCE menus work differently
    }
    void removeView(COptionMenu *, bool)
    {
        // Juce menus work differently
    }
    CView *getViewAt(const CPoint &p)
    {
        UNIMPL;
        return nullptr;
    }
    bool isChild(CView *v)
    {
        UNIMPL;
        return true;
    }
    CView *getFocusView()
    {
        // UNIMPL;
        // FIXME!!
        return nullptr;
    }
    void removeAll(bool b = true)
    {
        for (auto e : views)
        {
            ed->getJuceEditor()->removeChildComponent(e->juceComponent());
            e->forget();
        }
        views.clear();
    }
    void draw(CDrawContext *dc) override
    {
        if (bg)
        {
            bg->draw(dc, getViewSize());
        }
        else if (!transparent)
        {
            dc->setFillColor(bgcol);
            dc->drawRect(getViewSize(), kDrawFilled);
        }
        else
        {
        }
    }

    void efvg_resolveDeferredAdds() override
    {
        for (auto v : deferredAdds)
        {
            v->ed = ed;
            juceComponent()->addAndMakeVisible(v->juceComponent());
            v->efvg_resolveDeferredAdds();
        }
        deferredAdds.clear();
    }

    std::vector<CView *> deferredAdds;
    std::vector<CView *> views;
};

struct CFrame : public CViewContainer
{
    CFrame(CRect &rect, EscapeFromVSTGUI::JuceVSTGUIEditorAdapterBase *ed) : CViewContainer(rect)
    {
        this->ed = ed;
        this->frame = this;

        juceComponent()->setBounds(rect.asJuceIntRect());
        ed->getJuceEditor()->addAndMakeVisible(juceComponent());
    }
    ~CFrame() = default;

    void setCursor(CCursorType c) { UNIMPL; }
    void invalid()
    {
        for (auto v : views)
            v->invalid();
    }
    void invalidRect(const CRect &r)
    {
        invalid();
    }
    void setDirty(bool b = true) { invalid(); }

    void open(void *parent, int) { UNIMPL; }
    void close() { removeAll(); }

    COLPAIR(FocusColor);

    void localToFrame(CRect &r) { UNIMPL; }
    void localToFrame(CPoint &p) { UNIMPL; };
    void setZoom(float z) { UNIMPL; }
    CGraphicsTransform getTransform() { return CGraphicsTransform(); }
    void getPosition(float &x, float &y) { UNIMPL; }
    void getCurrentMouseLocation(CPoint &w) { UNIMPL; }
    CButtonState getCurrentMouseButtons() { return CButtonState(); }
};

struct CControl;
struct IControlListener
{
    virtual void valueChanged(CControl *p) = 0;
    virtual int32_t controlModifierClicked(CControl *p, CButtonState s) { return false; }
};

struct IKeyboardHook
{
    virtual int32_t onKeyDown(const VstKeyCode &code, CFrame *frame) = 0;
    virtual int32_t onKeyUp(const VstKeyCode &code, CFrame *frame) = 0;
};

struct CControl : public CView
{
    CControl(const CRect &r, IControlListener *l = nullptr, int32_t tag = 0, CBitmap *bg = nullptr)
        : CView(r), listener(l), tag(tag)
    {
        juceComponent()->setBounds(r.asJuceIntRect());
        setBackground(bg);
    }
    ~CControl()
    {
        /* if (bg)
            bg->forget();
            It seems that is not the semantic of CControl!
            */
    }

    virtual void looseFocus() { UNIMPL; }
    virtual void takeFocus() { UNIMPL; }
    CButtonState getMouseButtons() { return CButtonState(); }
    virtual float getRange()
    {
        UNIMPL;
        return 0;
    }
    virtual float getValue() { return value; }
    virtual void setValue(float v) { value = v; }
    virtual void setDefaultValue(float v) { vdef = v; }
    virtual void setVisible(bool b) { UNIMPL; }
    virtual void setMouseEnabled(bool) { UNIMPL; }
    virtual bool getMouseEnabled()
    {
        UNIMPL;
        return true;
    }

    uint32_t tag;
    virtual uint32_t getTag() { return tag; }
    virtual void setMax(float f) { vmax = f; }
    void valueChanged() { UNIMPL; }
    virtual void setMin(float f) { vmin = f; }
    virtual void bounceValue() { UNIMPL; }

    virtual void beginEdit()
    {
        // UNIMPL;
    }

    virtual void endEdit()
    {
        // UNIMPL;
    }

    COLPAIR(BackColor);

    CBitmap *bg = nullptr;
    void setBackground(CBitmap *b)
    {
        // This order in case b == bg
        if (b)
            b->remember();
        if (bg)
            bg->forget();

        bg = b;
    }
    CBitmap *getBackground() { return bg; }

    GSPAIR(Font, CFontRef, CFontRef, nullptr);

    COLPAIR(FontColor);
    GSPAIR(Transparency, bool, bool, false);
    GSPAIR(HoriAlign, CHoriTxtAlign, CHoriTxtAlign, CHoriTxtAlign::kLeftText);
    COLPAIR(FrameColor);
    COLPAIR(TextColorHighlighted);
    COLPAIR(FrameColorHighlighted);

    void setGradient(CGradient *g) { UNIMPL; }

    void setGradientHighlighted(CGradient *g) { UNIMPL; }
    CGradient *getGradientHighlighted()
    {
        UNIMPL;
        return nullptr;
    }
    void setTextColor(const CColor &c) { UNIMPL; }
    void setBackgroundColor(const CColor &c) { UNIMPL; }
    CColor getTextColor()
    {
        UNIMPL;
        return CColor();
    }
    void setTextAlignment(CHoriTxtAlign c) { UNIMPL; }
    CGradient *getGradient()
    {
        UNIMPL;
        return nullptr;
    }
    CGradient *getGradientHover()
    {
        UNIMPL;
        return nullptr;
    }
    CColor getFrameColor()
    {
        UNIMPL;
        return CColor();
    }
    CColor getFillColor()
    {
        UNIMPL;
        return CColor();
    }

    float value = 0.f;
    float vmax = 1.f;
    float vmin = 0.f;
    float vdef = 0.f;
    bool editing = false;
    IControlListener *listener = nullptr;
};

struct OfCourseItHasAStringType
{
    OfCourseItHasAStringType(const char *x) : s(x) {}
    OfCourseItHasAStringType(const std::string &q) : s(q) {}
    operator std::string() const { return s; }

    std::string getString() const { return s; }

    std::string s;
};

struct CTextLabel : public CControl
{
    CTextLabel(const CRect &r, const char *s, void * = nullptr) : CControl(r)
    {
        lab = std::make_unique<juce::Label>("fromVSTGUI", juce::CharPointer_UTF8(s));
        lab->setBounds(r.asJuceIntRect());
    }

    void setFont(CFontRef v) override
    {
        lab->setFont(v->font);

        CControl::setFont(v);
    }
    juce::Component *juceComponent() override { return lab.get(); }

    GSPAIR(Text, const OfCourseItHasAStringType &, OfCourseItHasAStringType, "");
    std::unique_ptr<juce::Label> lab;
};

struct CTextEdit : public CControl
{
    CTextEdit(const CRect &r, IControlListener *l = nullptr, long tag = -1,
              const char *txt = nullptr)
        : CControl(r, l, tag)
    {
        UNIMPL;
    }
    GSPAIR(Text, const OfCourseItHasAStringType &, OfCourseItHasAStringType, "");
    GSPAIR(ImmediateTextChange, bool, bool, true);
};

struct CCheckBox : public CControl
{
    CCheckBox(const CRect &r, IControlListener *l = nullptr, long tag = -1,
              const char *txt = nullptr)
        : CControl(r, l, tag)
    {
        UNIMPL;
    }

    COLPAIR(BoxFrameColor);
    COLPAIR(BoxFillColor);
    COLPAIR(CheckMarkColor);
    void sizeToFit() { UNIMPL; }
};

struct CTextButton : public CControl
{
    CTextButton(const CRect &r, IControlListener *l, int32_t tag, std::string lab)
        : CControl(r, l, tag)
    {
        UNIMPL;
    }
    void setRoundRadius(float f) { UNIMPL; }
};

struct CHorizontalSwitch : public CControl
{
    // FIX THIS obviously
    CHorizontalSwitch(const CRect &size, IControlListener *listener, int32_t tag, int frames,
                      int height, int width, CBitmap *background, CPoint offset)
        : CControl(size, listener, tag), heightOfOneImage(height)
    {
        setBackground(background);
    }
    float heightOfOneImage = 0;
};

struct CSlider : public CControl
{
    enum
    {
        kHorizontal = 1U << 0U,
        kVertical = 1U << 1U,
        kTop = 1U << 31U,
        kRight = kTop,
        kBottom = kTop,
    };
};

template <typename T> inline void juceCViewConnector<T>::paint(juce::Graphics &g)
{
    T::paint(g);
    if (viewCompanion)
    {
        auto dc = std::make_unique<CDrawContext>(g);
        auto b = T::getBounds();
        auto t = juce::AffineTransform().translated(-b.getX(), -b.getY());
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(t);
        viewCompanion->draw(dc.get());
    }
}

template <typename T> inline void juceCViewConnector<T>::mouseMove(const juce::MouseEvent &e)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);

    auto r = viewCompanion->onMouseMoved(w, CButtonState());
    if (r == kMouseEventNotHandled)
    {
        T::mouseMove(e);
    }
}

template <typename T> inline void juceCViewConnector<T>::mouseDrag(const juce::MouseEvent &e)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);

    auto r = viewCompanion->onMouseMoved(w, CButtonState());
    if (r == kMouseEventNotHandled)
    {
        T::mouseDrag(e);
    }
}

template <typename T> inline void juceCViewConnector<T>::mouseUp(const juce::MouseEvent &e)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);

    auto r = viewCompanion->onMouseUp(w, CButtonState());
    if (r == kMouseEventNotHandled)
    {
        T::mouseUp(e);
    }
}
template <typename T> inline void juceCViewConnector<T>::mouseDown(const juce::MouseEvent &e)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);
    auto r = viewCompanion->onMouseDown(w, CButtonState());
    if (r == kMouseEventNotHandled)
    {
        T::mouseDown(e);
    }
}
template <typename T> inline void juceCViewConnector<T>::mouseEnter(const juce::MouseEvent &e)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);

    auto r = viewCompanion->onMouseEntered(w, CButtonState());
    if (r == kMouseEventNotHandled)
    {
        T::mouseEnter(e);
    }
}
template <typename T> inline void juceCViewConnector<T>::mouseExit(const juce::MouseEvent &e)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);

    auto r = viewCompanion->onMouseExited(w, CButtonState());
    if (r == kMouseEventNotHandled)
    {
        T::mouseExit(e);
    }
}

// The ownership here is all ascreed p and if you make the refcout del it crashes
struct CMenuItem
{
    virtual ~CMenuItem() {}
    void setChecked(bool b = true) { OKUNIMPL; }
    void setEnabled(bool b = true) { OKUNIMPL; }

    virtual void remember() {}
    virtual void forget() {}

    std::string name;
};
struct CCommandMenuItem : public CMenuItem
{
    struct Desc
    {
        std::string name;
        Desc(const std::string &s) : name(s) {}
    } desc;
    CCommandMenuItem(const Desc &d) : desc(d) {}
    void setActions(std::function<void(CCommandMenuItem *)> f, void *userData = nullptr)
    {
        func = f;
    }
    std::function<void(CCommandMenuItem *)> func;
};
struct COptionMenu : public CControl
{
    enum
    {
        kMultipleCheckStyle = 1,
        kNoDrawStyle = 2
    };

    juce::PopupMenu menu;

    COptionMenu(const CRect &r, IControlListener *l, int32_t tag, int, int = 0, int = 0)
        : CControl(r, l), menu()
    {
        // Obviously fix this
        alwaysLeak = true;
        doDebug = false;
        OKUNIMPL;
    }
    void setNbItemsPerColumn(int c) { UNIMPL; }
    void setEnabled(bool) { UNIMPL; }
    void addSeparator(int s = -1) { menu.addSeparator(); }
    CMenuItem *addEntry(const std::string &s, int eid = -1)
    {
        menu.addItem(juce::CharPointer_UTF8(s.c_str()), []() { UNIMPL; });

        auto res = new CMenuItem();
        res->remember();
        return res;
    }
    CMenuItem *addEntry(CCommandMenuItem *r)
    {
        r->remember();
        auto op = r->func;
        menu.addItem(juce::CharPointer_UTF8(r->desc.name.c_str()), [op]() { op(nullptr); });
        auto res = new CMenuItem();
        res->remember();
        return res;
    }
    CMenuItem *addEntry(COptionMenu *m, const std::string &nm)
    {
        menu.addSubMenu(juce::CharPointer_UTF8(nm.c_str()), m->menu);
        auto res = new CMenuItem();
        res->remember();
        return res;
    }
    CMenuItem *addEntry(COptionMenu *m, const char *nm)
    {
        m->remember();
        menu.addSubMenu(juce::CharPointer_UTF8(nm), m->menu);
        auto res = new CMenuItem();
        res->remember();
        return res;
    }
    void checkEntry(int, bool) { UNIMPL; }
    void popup() { menu.show(); }
    inline void setHeight(float h) { UNIMPL; }
    void cleanupSeparators(bool b) { UNIMPL; }
    int getNbEntries() { return 0; }
};

enum DragOperation
{

};
struct DragEventData
{
};

#define CLASS_METHODS(a, b)
} // namespace EscapeNS

namespace VSTGUI = EscapeNS;

namespace EscapeFromVSTGUI
{
struct JuceVSTGUIEditorAdapter : public JuceVSTGUIEditorAdapterBase
{
    JuceVSTGUIEditorAdapter(juce::AudioProcessorEditor *parentEd) : parentEd(parentEd) {}
    virtual ~JuceVSTGUIEditorAdapter() = default;
    virtual bool open(void *parent) { return true; };
    virtual void close(){};
    virtual void controlBeginEdit(VSTGUI::CControl *p) = 0;
    virtual void controlEndEdit(VSTGUI::CControl *p) = 0;
    virtual void beginEdit(int32_t id) {}
    virtual void endEdit(int32_t id) {}

    juce::AudioProcessorEditor *getJuceEditor() { return parentEd; }

    VSTGUI::CRect rect;
    VSTGUI::CFrame *frame = nullptr;
    VSTGUI::CFrame *getFrame() { return frame; }

    juce::AudioProcessorEditor *parentEd;
};

struct JuceVSTGUIEditorAdapterConcreteTestOnly : public JuceVSTGUIEditorAdapter
{
    JuceVSTGUIEditorAdapterConcreteTestOnly(juce::AudioProcessorEditor *parentEd)
        : JuceVSTGUIEditorAdapter(parentEd)
    {
    }
    bool open(void *parent) override { return true; }
    void close() override {}
    void controlBeginEdit(VSTGUI::CControl *p) override {}
    void controlEndEdit(VSTGUI::CControl *p) override {}
};
} // namespace EscapeFromVSTGUI

#endif // SURGE_ESCAPE_FROM_VSTGUI_H
