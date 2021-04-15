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
#define UNIMPL_STACK DUNIMPL EscapeNS::Internal::printStack(__func__);
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
inline void printStack(const char *where)
{
#if MAC
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
    std::cout << oss.str() << std::endl;
#endif
}
#if DEBUG_EFVG_MEMORY
struct DebugAllocRecord
{
    DebugAllocRecord() { record("CONSTRUCT"); }
    void record(const std::string &where)
    {
#if DEBUG_EFVG_MEMORY_STACKS
#if MAC
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

inline std::unordered_set<FakeRefcount *> *getForgetMeLater()
{
    static std::unordered_set<FakeRefcount *> fml;
    return &fml;
}

inline void enqueueForget(FakeRefcount *c) { getForgetMeLater()->insert(c); }

} // namespace Internal

inline void efvgIdle()
{
    for (auto v : *(Internal::getForgetMeLater()))
        v->forget();
    Internal::getForgetMeLater()->clear();
}

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
    std::unique_ptr<juce::ColourGradient> grad;
    CGradient() { grad = std::make_unique<juce::ColourGradient>(); }
    struct ColorStopMap
    {
    };
    static CGradient *create(const ColorStopMap &m)
    {
        auto res = new CGradient();
        res->remember();
        return res;
    };
    void addColorStop(float pos, const CColor &c) { grad->addColour(pos, c.asJuceColour()); }
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
    CButtonState(const juce::ModifierKeys &m) { state = m.getRawFlags(); }
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
    CFontInternal() {}
    CFontInternal(const juce::Font &f) : font(f) {}
    float getSize() { return font.getHeight(); }
    void setStyle(int newStyle = kNormalFace) { OKUNIMPL; }
    void setSize(int newSize = 12) { OKUNIMPL; }
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
    void fillLinearGradient(CGraphicsPath *p, const CGradient &cg, const CPoint &, const CPoint &,
                            bool, CGraphicsTransform *tf)
    {
        g.setGradientFill(*(cg.grad));
        g.fillPath(p->path, tf->juceT);
    }

    struct Transform
    {
        juce::Graphics *g = nullptr;
        Transform(CDrawContext &dc, CGraphicsTransform &tf)
        {
            dc.g.saveState();
            dc.g.addTransform(tf.juceT);
            g = &(dc.g);
        }
        ~Transform() { g->restoreState(); }
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
            g.fillRect(r.asJuceFloatRect());
            break;
        }
        case kDrawStroked:
        {
            g.setColour(frameColor);
            g.drawRect(r.asJuceFloatRect());
            break;
        }
        case kDrawFilledAndStroked:
        {
            g.setColour(fillColor);
            g.fillRect(r.asJuceFloatRect());
            g.setColour(frameColor);
            g.drawRect(r.asJuceFloatRect());
        }
        }
    }

    void drawPolygon(const std::vector<CPoint> &l, CDrawStyle s)
    {
        juce::Path path;
        bool started = false;
        for (auto &p : l)
        {
            if (!started)
                path.startNewSubPath(p);
            else
                path.lineTo(p);
        }
        g.setColour(kRedCColor.asJuceColour());
        g.fillPath(path);
        OKUNIMPL;
    }

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
    void drawString(const char *c, const CPoint &p, int = 0, bool = true)
    {
        OKUNIMPL;
        auto w = font->font.getStringWidth(juce::CharPointer_UTF8(c));
        auto h = font->font.getHeight();
        auto r = CRect(CPoint(p.x, p.y - h), CPoint(w, h));
        drawString(c, r);
    }
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

    virtual void draw(CDrawContext *dc, const CRect &r, const CPoint &off = CPoint(),
                      float alpha = 1.0)
    {
        juce::Graphics::ScopedSaveState gs(dc->g);
        auto tl = r.asJuceIntRect().getTopLeft();

        auto t = juce::AffineTransform().translated(tl.x, tl.y).translated(-off.x, -off.y);
        dc->g.reduceClipRegion(r.asJuceIntRect());
        drawable->draw(dc->g, alpha, t);
    }
    CResourceDescription desc;
    std::unique_ptr<juce::Drawable> drawable;
};

struct CViewBase;

struct OnRemovedHandler
{
    virtual void onRemoved() = 0;
};

template <typename T> struct juceCViewConnector : public T, public OnRemovedHandler
{
    juceCViewConnector() : T() {}
    ~juceCViewConnector();
    void setViewCompanion(CViewBase *v);
    void onRemoved() override;

    CViewBase *viewCompanion = nullptr;
    void paint(juce::Graphics &g) override;

    CMouseEventResult traverseMouseParents(
        const juce::MouseEvent &e,
        std::function<CMouseEventResult(CViewBase *, const CPoint &, const CButtonState &)> vf,
        std::function<void(const juce::MouseEvent &)> jf);

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseMove(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseDoubleClick(const juce::MouseEvent &e) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;
    void mouseMagnify(const juce::MouseEvent &event, float scaleFactor) override;
    bool supressMoveAndUp = false;
};

struct CView;

// Clena this up obviously
struct CViewBase : public Internal::FakeRefcount
{
    CViewBase(const CRect &size) : size(size), ma(size) {}
    virtual ~CViewBase(){
        // Here we probably have to make sure that the juce component is removed
    };
    virtual juce::Component *juceComponent() = 0;

    virtual void draw(CDrawContext *dc){};
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
        return false;
    }

    virtual bool onWheel(const CPoint &where, const CMouseWheelAxis &, const float &distance,
                         const CButtonState &buttons)
    {
        return false;
    }

    void invalid() { juceComponent()->repaint(); }

    CRect ma;
    CRect getMouseableArea() { return ma; }
    void setMouseableArea(const CRect &r) { ma = r; }
    void setMouseEnabled(bool b) { OKUNIMPL; }
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
        OKUNIMPL;
        return true;
    }
    void setVisible(bool b)
    {
        if (juceComponent())
            juceComponent()->setVisible(b);
    }
    bool isVisible()
    {
        if (juceComponent())
            return juceComponent()->isVisible();
        return false;
    }
    void setSize(const CRect &s) { UNIMPL; }
    void setSize(float w, float h) { juceComponent()->setBounds(0, 0, w, h); }
    CRect size;

    CView *parentView = nullptr;
    CView *getParentView() { return parentView; }

    CFrame *frame = nullptr;
    CFrame *getFrame();
};

struct CView : public CViewBase
{
    CView(const CRect &size) : CViewBase(size) {}
    ~CView() {}

    virtual void onAdded() {}
    virtual void efvg_resolveDeferredAdds(){};

    std::unique_ptr<juceCViewConnector<juce::Component>> comp;
    juce::Component *juceComponent() override
    {
        if (!comp)
        {
            comp = std::make_unique<juceCViewConnector<juce::Component>>();
            comp->setBounds(size.asJuceIntRect());
            comp->setViewCompanion(this);
        }
        return comp.get();
    }

    EscapeFromVSTGUI::JuceVSTGUIEditorAdapterBase *ed = nullptr; // FIX ME obviously
};

#define GSPAIR(x, sT, gT, gTV)                                                                     \
    gT m_##x = gTV;                                                                                \
    virtual void set##x(sT v) { m_##x = v; }                                                       \
    virtual gT get##x() const { return m_##x; }

#define COLPAIR(x)                                                                                 \
    CColor m_##x;                                                                                  \
    virtual void set##x(const CColor &v) { m_##x = v; }                                            \
    virtual CColor get##x() const { return m_##x; }

inline CFrame *CViewBase::getFrame()
{
    if (!frame)
    {
        if (parentView)
        {
            return parentView->getFrame();
        }
    }
    return frame;
}

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
    bool transparent = false;
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

    void onAdded() override {}
    void addView(CView *v)
    {
        if (!v)
            return;

        v->remember();
        v->ed = ed;
        v->frame = frame;
        v->parentView = this;
        auto jc = v->juceComponent();
        if (jc && ed)
        {
            juceComponent()->addAndMakeVisible(*jc);
            v->efvg_resolveDeferredAdds();
            v->onAdded();
        }
        else
        {
            deferredAdds.push_back(v);
        }
        views.push_back(v);
    }

    std::unordered_set<std::unique_ptr<juce::Component>> rawAddedComponents;
    virtual void takeOwnership(std::unique_ptr<juce::Component> c)
    {
        rawAddedComponents.insert(std::move(c));
    }

    void removeViewInternals(CView *v, bool forgetChildren = true)
    {
        auto cvc = dynamic_cast<CViewContainer *>(v);
        if (cvc)
        {
            cvc->removeAll(forgetChildren);
        }

        juceComponent()->removeChildComponent(v->juceComponent());
        auto orh = dynamic_cast<OnRemovedHandler *>(v->juceComponent());
        if (orh)
        {
            orh->onRemoved();
        }
        v->parentView = nullptr;
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
        removeViewInternals(v, doForget);
        views.erase(pos);
        if (doForget)
            v->forget();
        invalid();
    }

    /*void addView(COptionMenu *)
    {
        // JUCE menus work differently
    }
    void removeView(COptionMenu *, bool)
    {
        // Juce menus work differently
    }*/

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
        for (auto &j : rawAddedComponents)
        {
            juceComponent()->removeChildComponent(j.get());
        }
        rawAddedComponents.clear();

        for (auto e : views)
        {
            removeViewInternals(e, b);
            if (b)
            {
                e->forget();
            }
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

    CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons) override
    {
        return kMouseEventNotHandled;
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

    void draw(CDrawContext *dc) override { CViewContainer::draw(dc); }
    void setCursor(CCursorType c)
    {
        auto ct = juce::MouseCursor::StandardCursorType::NormalCursor;
        switch (c)
        {
        case kCursorDefault:
            break;
        case kCursorWait:
            ct = juce::MouseCursor::StandardCursorType::WaitCursor;
            break;
        case kCursorHSize:
            ct = juce::MouseCursor::StandardCursorType::LeftRightResizeCursor;
            break;
        case kCursorVSize:
            ct = juce::MouseCursor::StandardCursorType::UpDownResizeCursor;
            break;
        case kCursorSizeAll:
            ct = juce::MouseCursor::StandardCursorType::UpDownLeftRightResizeCursor;
            break;
        case kCursorNESWSize:
            ct = juce::MouseCursor::StandardCursorType::TopLeftCornerResizeCursor;
            break;
        case kCursorNWSESize:
            ct = juce::MouseCursor::StandardCursorType::TopRightCornerResizeCursor;
            break;
        case kCursorCopy:
            ct = juce::MouseCursor::StandardCursorType::CopyingCursor;
            break;
        case kCursorHand:
            ct = juce::MouseCursor::StandardCursorType::PointingHandCursor;
            break;
        case kCursorIBeam:
            ct = juce::MouseCursor::StandardCursorType::IBeamCursor;
            break;
        case kCursorNotAllowed: // what is this?
            ct = juce::MouseCursor::StandardCursorType::CrosshairCursor;
            break;
        }

        ed->getJuceEditor()->setMouseCursor(juce::MouseCursor(ct));
    }
    void invalid()
    {
        for (auto v : views)
            v->invalid();
    }
    void invalidRect(const CRect &r) { invalid(); }
    void setDirty(bool b = true) { invalid(); }

    void open(void *parent, int) { OKUNIMPL; }
    void close() { removeAll(); }

    COLPAIR(FocusColor);

    void localToFrame(CRect &r) { UNIMPL; }
    void localToFrame(CPoint &p) { UNIMPL; };
    void setZoom(float z) { OKUNIMPL; }
    CGraphicsTransform getTransform() { return CGraphicsTransform(); }
    void getPosition(float &x, float &y)
    {
        auto b = juceComponent()->getScreenPosition();
        x = b.x;
        y = b.y;
    }
    void getCurrentMouseLocation(CPoint &w)
    {
        auto pq = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
        auto b = juceComponent()->getLocalPoint(nullptr, pq);
        w.x = b.x;
        w.y = b.y;
    }
    CButtonState getCurrentMouseButtons() { return CButtonState(); }
};

struct CControl;
struct IControlListener
{
    virtual void valueChanged(CControl *p) = 0;
    virtual int32_t controlModifierClicked(CControl *p, CButtonState s) { return false; }

    virtual void controlBeginEdit(CControl *control){};
    virtual void controlEndEdit(CControl *control){};
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
    virtual void setVisible(bool b)
    {
        if (juceComponent())
            juceComponent()->setVisible(b);
    }
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
        if (listener)
            listener->controlBeginEdit(this);
    }

    virtual void endEdit()
    {

        if (listener)
            listener->controlEndEdit(this);
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

    GSPAIR(Font, CFontRef, CFontRef, std::make_shared<CFontInternal>());

    COLPAIR(FontColor);
    GSPAIR(Transparency, bool, bool, false);
    GSPAIR(HoriAlign, CHoriTxtAlign, CHoriTxtAlign, CHoriTxtAlign::kLeftText);
    COLPAIR(FrameColor);
    COLPAIR(TextColorHighlighted);
    COLPAIR(FrameColorHighlighted);

    GSPAIR(Gradient, CGradient *, CGradient *, nullptr);
    GSPAIR(GradientHighlighted, CGradient *, CGradient *, nullptr);

    COLPAIR(FillColor);
    COLPAIR(TextColor);
    COLPAIR(BackgroundColor);

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
    CTextLabel(const CRect &r, const char *s, CBitmap *bg = nullptr) : CControl(r)
    {
        if (bg)
        {
            img = std::make_unique<juceCViewConnector<juce::Component>>();
            img->setBounds(r.asJuceIntRect());
            img->addAndMakeVisible(*(bg->drawable.get()));
            img->setViewCompanion(this);
        }
        else
        {
            lab = std::make_unique<juceCViewConnector<juce::Label>>();
            lab->setText(juce::CharPointer_UTF8(s), juce::dontSendNotification);
            lab->setBounds(r.asJuceIntRect());
            lab->setViewCompanion(this);
            lab->setJustificationType(juce::Justification::centred);
        }
    }

    void setBackColor(const CColor &v) override
    {
        CControl::setBackColor(v);
        if (lab)
            lab->setColour(juce::Label::backgroundColourId, v.asJuceColour());
    }

    void setHoriAlign(CHoriTxtAlign v) override
    {
        CControl::setHoriAlign(v);
        if (lab)
        {
            switch (v)
            {
            case kRightText:
                lab->setJustificationType(juce::Justification::centredRight);
                break;
            case kLeftText:
                lab->setJustificationType(juce::Justification::centredLeft);
                break;
            case kCenterText:
                lab->setJustificationType(juce::Justification::centred);
                break;
            }
        }
    }
    void setFont(CFontRef v) override
    {
        if (lab)
            lab->setFont(v->font);

        CControl::setFont(v);
    }
    juce::Component *juceComponent() override
    {
        if (lab)
            return lab.get();
        return img.get();
    }

    CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons) override
    {
        std::cout << "TL OMD" << std::endl;
        return CViewBase::onMouseDown(where, buttons);
    }

    void setFontColor(const CColor &v) override
    {
        CControl::setFontColor(v);
        if (lab)
        {
            lab->setColour(juce::Label::ColourIds::textColourId, v.asJuceColour());
        }
    }

    void setAntialias(bool b) { OKUNIMPL; }
    GSPAIR(Text, const OfCourseItHasAStringType &, OfCourseItHasAStringType, "");
    std::unique_ptr<juceCViewConnector<juce::Label>> lab;
    std::unique_ptr<juceCViewConnector<juce::Component>> img;
};

struct CTextEdit : public CControl, public juce::TextEditor::Listener
{
    CTextEdit(const CRect &r, IControlListener *l = nullptr, long tag = -1,
              const char *txt = nullptr)
        : CControl(r, l, tag)
    {
        texted = std::make_unique<juceCViewConnector<juce::TextEditor>>();
        texted->setText(juce::CharPointer_UTF8(txt), juce::dontSendNotification);
        texted->setBounds(r.asJuceIntRect());
        texted->setViewCompanion(this);
        texted->addListener(this);
    }

    void textEditorReturnKeyPressed(juce::TextEditor &editor) override
    {
        if (listener)
            listener->valueChanged(this);
    }

    void setTextInset(const CPoint &inset) { OKUNIMPL; }

    std::unique_ptr<juceCViewConnector<juce::TextEditor>> texted;
    juce::Component *juceComponent() override { return texted.get(); }

    void setText(const OfCourseItHasAStringType &st)
    {
        texted->setText(juce::CharPointer_UTF8(st.getString().c_str()));
    }
    OfCourseItHasAStringType getText()
    {
        return OfCourseItHasAStringType(texted->getText().toRawUTF8());
    }

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
    void sizeToFit() { OKUNIMPL; }
};

struct CTextButton : public CControl, juce::Button::Listener
{
    CTextButton(const CRect &r, IControlListener *l, int32_t tag, std::string lab)
        : CControl(r, l, tag)
    {
        textb = std::make_unique<juceCViewConnector<juce::TextButton>>();
        textb->setButtonText(juce::CharPointer_UTF8(lab.c_str()));
        textb->setBounds(r.asJuceIntRect());
        textb->setViewCompanion(this);
        textb->addListener(this);
    }
    ~CTextButton() {}
    void setRoundRadius(float f) { OKUNIMPL; }

    void buttonClicked(juce::Button *button) override
    {
        if (listener)
            listener->valueChanged(this);
    }

    GSPAIR(TextAlignment, CHoriTxtAlign, CHoriTxtAlign, CHoriTxtAlign::kCenterText);
    std::unique_ptr<juceCViewConnector<juce::TextButton>> textb;
    juce::Component *juceComponent() override { return textb.get(); }
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

template <typename T> inline juceCViewConnector<T>::~juceCViewConnector() {}

template <typename T> inline void juceCViewConnector<T>::onRemoved()
{
    // Internal::enqueueForget(viewCompanion);
    viewCompanion = nullptr;
}
template <typename T> inline void juceCViewConnector<T>::setViewCompanion(CViewBase *v)
{
    viewCompanion = v;
    // viewCompanion->remember();
}

template <typename T> inline void juceCViewConnector<T>::paint(juce::Graphics &g)
{
    if (viewCompanion)
    {
        auto dc = std::make_unique<CDrawContext>(g);
        auto b = T::getBounds();
        auto t = juce::AffineTransform().translated(-b.getX(), -b.getY());
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(t);
        viewCompanion->draw(dc.get());
    }
    T::paint(g);
}

template <typename T>
inline CMouseEventResult juceCViewConnector<T>::traverseMouseParents(
    const juce::MouseEvent &e,
    std::function<CMouseEventResult(CViewBase *, const CPoint &, const CButtonState &)> vf,
    std::function<void(const juce::MouseEvent &)> jf)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);
    CViewBase *curr = viewCompanion;
    auto r = kMouseEventNotHandled;
    while (curr && r == kMouseEventNotHandled)
    {
        r = vf(curr, w, CButtonState(e.mods));
        curr = curr->getParentView();
    }
    if (r == kMouseEventNotHandled)
    {
        jf(e);
    }

    return r;
}

template <typename T> inline void juceCViewConnector<T>::mouseDown(const juce::MouseEvent &e)
{
    supressMoveAndUp = false;
    auto res = traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseDown(b, c); },
        [this](auto e) { T::mouseDown(e); });
    if (res == kMouseDownEventHandledButDontNeedMovedOrUpEvents)
        supressMoveAndUp = true;
}

template <typename T> inline void juceCViewConnector<T>::mouseUp(const juce::MouseEvent &e)
{
    if (!supressMoveAndUp)
    {
        traverseMouseParents(
            e, [](auto *a, auto b, auto c) { return a->onMouseUp(b, c); },
            [this](auto e) { T::mouseUp(e); });
    }
    supressMoveAndUp = false;
}
template <typename T> inline void juceCViewConnector<T>::mouseMove(const juce::MouseEvent &e)
{
    // Don't supress check here because only DRAG will be called in the down
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseMoved(b, c); },
        [this](auto e) { T::mouseMove(e); });
}

template <typename T> inline void juceCViewConnector<T>::mouseDrag(const juce::MouseEvent &e)
{
    if (supressMoveAndUp)
        return;
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseMoved(b, c); },
        [this](auto e) { T::mouseDrag(e); });
}

template <typename T> inline void juceCViewConnector<T>::mouseEnter(const juce::MouseEvent &e)
{
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseEntered(b, c); },
        [this](auto e) { T::mouseEnter(e); });
}
template <typename T> inline void juceCViewConnector<T>::mouseExit(const juce::MouseEvent &e)
{
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseExited(b, c); },
        [this](auto e) { T::mouseExit(e); });
}

template <typename T> inline void juceCViewConnector<T>::mouseDoubleClick(const juce::MouseEvent &e)
{
    OKUNIMPL; // wanna do parent thing
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);
    auto bs = CButtonState() | kDoubleClick;

    auto r = viewCompanion->onMouseDown(w, CButtonState(bs));
    if (r == kMouseEventNotHandled)
    {
        T::mouseDoubleClick(e);
    }
}
template <typename T>
inline void juceCViewConnector<T>::mouseWheelMove(const juce::MouseEvent &e,
                                                  const juce::MouseWheelDetails &wheel)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);

    OKUNIMPL;
    float mouseScaleFactor = 3;

    auto r = viewCompanion->onWheel(w, kMouseWheelAxisX, mouseScaleFactor * wheel.deltaX,
                                    CButtonState()) ||
             viewCompanion->onWheel(w, kMouseWheelAxisY, -mouseScaleFactor * wheel.deltaY,
                                    CButtonState());

    if (!r)
    {
        r = viewCompanion->onWheel(w, -mouseScaleFactor * wheel.deltaY, CButtonState());
    }
    if (!r)
    {
        T::mouseWheelMove(e, wheel);
    }
}
template <typename T>
inline void juceCViewConnector<T>::mouseMagnify(const juce::MouseEvent &event, float scaleFactor)
{
    std::cout << "scaleFactor is " << scaleFactor << std::endl;
    auto b = T::getBounds().getTopLeft();
    CPoint w(event.x + b.x, event.y + b.y);
    if (!viewCompanion->magnify(w, scaleFactor))
    {
        T::mouseMagnify(event, scaleFactor);
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
        : CControl(r, l, tag), menu()
    {
        // Obviously fix this
        alwaysLeak = true;
        doDebug = false;
    }
    void setNbItemsPerColumn(int c) { OKUNIMPL; }
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
    void popup() { menu.showMenuAsync(juce::PopupMenu::Options()); }
    inline void setHeight(float h) { UNIMPL; }
    void cleanupSeparators(bool b) { UNIMPL; }
    int getNbEntries() { return 0; }

    CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons) override
    {
        popup();
        return kMouseEventHandled;
    }
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
    virtual void controlBeginEdit(VSTGUI::CControl *p){};
    virtual void controlEndEdit(VSTGUI::CControl *p){};
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
