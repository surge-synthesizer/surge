// SPDX-License-Identifier: GPL-3.0-or-later
// Embedded EM_ASM JavaScript must not be reformatted as C++.
// clang-format off
#include <emscripten.h>
#include <emscripten/html5.h>
#include <set>
namespace juce
{
namespace
{
Point<float> browserMouse;
std::set<int> browserKeys;
int browserKeyCode(int domCode)
{
    return domCode >= 112 && domCode <= 135 ? KeyPress::F1Key + domCode - 112 : domCode;
}
int browserPeerId = 0;
TextInputTarget *browserCompositionTarget = nullptr;
uint64_t browserCompositionSerial = 0;
Component::SafePointer<Component> browserCompositionComponent;
} // namespace
class BrowserPeer final : public ComponentPeer
{
    const int inputPeerId{++browserPeerId};
    String canvasId;
    Rectangle<int> bounds;
    bool visible = true, fullscreen = false, dirty = true;
    double pixelScale = 1.;
    Image backing;
    std::vector<uint8_t> rgba;
    static ModifierKeys mouseModifiers(const EmscriptenMouseEvent &event)
    {
        return ModifierKeys((event.shiftKey ? ModifierKeys::shiftModifier : 0) |
                            (event.ctrlKey ? ModifierKeys::ctrlModifier : 0) |
                            (event.altKey ? ModifierKeys::altModifier : 0) |
                            (event.metaKey ? ModifierKeys::commandModifier : 0) |
                            (event.buttons & 1 ? ModifierKeys::leftButtonModifier : 0) |
                            (event.buttons & 2 ? ModifierKeys::rightButtonModifier : 0) |
                            (event.buttons & 4 ? ModifierKeys::middleButtonModifier : 0));
    }
    static bool mouse(int type, const EmscriptenMouseEvent *event, void *ctx)
    {
        auto &p = *static_cast<BrowserPeer *>(ctx);
        const auto modifiers = mouseModifiers(*event);
        ModifierKeys::currentModifiers = modifiers;
        browserMouse = {float(event->clientX), float(event->clientY)};
        if (type == EMSCRIPTEN_EVENT_MOUSEDOWN &&
            (p.getStyleFlags() & windowIgnoresKeyPresses) == 0)
            EM_ASM({ document.getElementById(UTF8ToString($0)).focus(); }, p.canvasId.toRawUTF8());
        p.handleMouseEvent(MouseInputSource::InputSourceType::mouse,
                           {float(event->targetX), float(event->targetY)}, modifiers,
                           MouseInputSource::invalidPressure, MouseInputSource::invalidOrientation,
                           int64(Time::getMillisecondCounter()));
        return true;
    }
    static bool focus(int type, const EmscriptenFocusEvent *, void *ctx)
    {
        auto &p = *static_cast<BrowserPeer *>(ctx);
        if (type == EMSCRIPTEN_EVENT_FOCUS)
        {
            if (!p.component.hasKeyboardFocus(true)) p.handleFocusGain();
        }
        else
        {
            browserKeys.clear();
            ModifierKeys::currentModifiers = ModifierKeys{};
            p.handleKeyUpOrDown(false);
            // Moving between a canvas and its accessible DOM controls retains
            // the same JUCE peer. In particular, do not close type-ahead editors.
            if (!EM_ASM_INT({ const c=document.getElementById(UTF8ToString($0));
                return c.surgeFocusDestination === c.id; }, p.canvasId.toRawUTF8()))
                p.handleFocusLoss();
        }
        return false;
    }
    static bool key(int type, const EmscriptenKeyboardEvent *event, void *ctx)
    {
        auto &p = *static_cast<BrowserPeer *>(ctx);
        p.syncInput();
        if (EM_ASM_INT({ return SurgeTextInput.isComposing($0); }, p.inputPeerId) ||
            (p.activeInput() && (event->keyCode == 229 || String::fromUTF8(event->key) == "Dead")))
            return false;
        const int code = event->keyCode;
        const bool down = type == EMSCRIPTEN_EVENT_KEYDOWN;
        if (down)
            browserKeys.insert(code);
        else
            browserKeys.erase(code);
        int flags = (event->shiftKey ? ModifierKeys::shiftModifier : 0) |
                    (event->ctrlKey ? ModifierKeys::ctrlModifier : 0) |
                    (event->altKey ? ModifierKeys::altModifier : 0) |
                    (event->metaKey ? ModifierKeys::commandModifier : 0);
        ModifierKeys::currentModifiers = ModifierKeys(flags);
        // Native JUCE reports modifier transitions separately from key presses.
        // Sending Alt/Shift/Ctrl/Meta as a key press prematurely completes the
        // shortcut editor's Learn operation before the chord's actual key.
        const String text = String::fromUTF8(event->key);
        if (text == "Shift" || text == "Control" || text == "Alt" || text == "Meta" ||
            text == "AltGraph")
        {
            p.handleModifierKeysChange();
            return false;
        }
        p.handleKeyUpOrDown(down);
        if (down)
        {
            const bool handled = p.handleKeyPress(browserKeyCode(code), text.length() == 1 ? text[0] : 0);
            p.syncInput();
            return handled;
        }
        return false;
    }
    static bool wheel(int, const EmscriptenWheelEvent *event, void *ctx)
    {
        auto &p = *static_cast<BrowserPeer *>(ctx);
        // DOM accessibility controls can own keyboard focus while the canvas receives
        // wheel input, so its current modifier state must come from this event.
        ModifierKeys::currentModifiers = mouseModifiers(event->mouse);
        MouseWheelDetails wheel;
        wheel.deltaX = float(-event->deltaX / 100.0);
        wheel.deltaY = float(-event->deltaY / 100.0);
        wheel.isSmooth = event->deltaMode == 0;
        p.handleMouseWheel(MouseInputSource::InputSourceType::mouse,
                           {float(event->mouse.targetX), float(event->mouse.targetY)},
                           int64(Time::getMillisecondCounter()), wheel);
        return true;
    }

  public:
    BrowserPeer(Component &c, int flags)
        : ComponentPeer(c, flags), canvasId("juce-" + String(inputPeerId))
    {
        EM_ASM(
            {
                const c = document.createElement('canvas');
                c.id = UTF8ToString($0);
                c.style.zIndex = $3 ? 2000 : 0;
                if (!$2) c.tabIndex = 0;
                c.setAttribute('aria-label', UTF8ToString($1));
                document.body.appendChild(c);
                c.addEventListener('blur', event => { c.surgeFocusDestination=event.relatedTarget?.dataset.jucePeer || ""; }, true);
                c.addEventListener('contextmenu', e => e.preventDefault());
                SurgeBrowser.attachFileDrop(c, $4);
                c.addEventListener('pointerdown', e => c.setPointerCapture(e.pointerId));
                c.addEventListener('pointercancel', e => {
                    c.dispatchEvent(new MouseEvent('mouseup', {clientX:e.clientX, clientY:e.clientY, buttons:0}));
                });
            },
            canvasId.toRawUTF8(), c.getName().toRawUTF8(), (flags & windowIgnoresKeyPresses) != 0,
            (flags & windowIsTemporary) != 0, inputPeerId);
        const auto selector = "#" + canvasId;
        emscripten_set_mousedown_callback(selector.toRawUTF8(), this, true, mouse);
        emscripten_set_mouseup_callback(selector.toRawUTF8(), this, true, mouse);
        emscripten_set_mousemove_callback(selector.toRawUTF8(), this, true, mouse);
        emscripten_set_keydown_callback(selector.toRawUTF8(), this, true, key);
        emscripten_set_keyup_callback(selector.toRawUTF8(), this, true, key);
        emscripten_set_wheel_callback(selector.toRawUTF8(), this, true, wheel);
        emscripten_set_focus_callback(selector.toRawUTF8(), this, true, focus);
        emscripten_set_blur_callback(selector.toRawUTF8(), this, true, focus);
        setBounds(c.getBounds(), false);
    }
    ~BrowserPeer() override
    {
        finishInputComposition();
        EM_ASM({ SurgeTextInput.detach($0); }, inputPeerId);
        const auto selector = "#" + canvasId;
        emscripten_set_mousedown_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_mouseup_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_mousemove_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_keydown_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_keyup_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_wheel_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_focus_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        emscripten_set_blur_callback(selector.toRawUTF8(), nullptr, true, nullptr);
        EM_ASM({ document.getElementById(UTF8ToString($0)).remove(); }, canvasId.toRawUTF8());
    }
    void *getNativeHandle() const override { return const_cast<BrowserPeer *>(this); }
    void setVisible(bool value) override
    {
        visible = value;
        EM_ASM(
            { document.getElementById(UTF8ToString($0)).style.display = $1 ? 'block' : 'none'; },
            canvasId.toRawUTF8(), value);
    }
    void setTitle(const String &title) override
    {
        EM_ASM(
            {
                document.getElementById(UTF8ToString($0))
                    .setAttribute('aria-label', UTF8ToString($1));
            },
            canvasId.toRawUTF8(), title.toRawUTF8());
    }
    void setBounds(const Rectangle<int> &r, bool fs) override
    {
        bounds = r;
        fullscreen = fs;
        pixelScale = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; });
        if (r.getWidth() > 0 && r.getHeight() > 0)
        {
            backing = Image(Image::ARGB, int(std::ceil(r.getWidth() * pixelScale)),
                            int(std::ceil(r.getHeight() * pixelScale)), true);
            rgba.resize(size_t(backing.getWidth()) * backing.getHeight() * 4);
        }
        EM_ASM(
            {
                const c = document.getElementById(UTF8ToString($0));
                c.width = $5;
                c.height = $6;
                c.style.width = $3 + 'px';
                c.style.height = $4 + 'px';
                c.style.left = $1 + 'px';
                c.style.top = $2 + 'px';
            },
            canvasId.toRawUTF8(), r.getX(), r.getY(), r.getWidth(), r.getHeight(),
            backing.getWidth(), backing.getHeight());
        dirty = true;
        handleMovedOrResized();
    }
    Rectangle<int> getBounds() const override { return bounds; }
    Point<float> localToGlobal(Point<float> p) override
    {
        return p + bounds.getPosition().toFloat();
    }
    Point<float> globalToLocal(Point<float> p) override
    {
        return p - bounds.getPosition().toFloat();
    }
    void setMinimised(bool b) override { setVisible(!b); }
    bool isMinimised() const override { return !visible; }
    bool isShowing() const override { return visible; }
    void setFullScreen(bool b) override { fullscreen = b; }
    bool isFullScreen() const override { return fullscreen; }
    void setIcon(const Image &) override {}
    bool contains(Point<int> p, bool) const override
    {
        if (!bounds.withPosition(0, 0).contains(p))
            return false;
        return EM_ASM_INT({
            const canvas = document.getElementById(UTF8ToString($0));
            return document.elementFromPoint($1, $2) === canvas;
        }, canvasId.toRawUTF8(), p.x + bounds.getX(), p.y + bounds.getY());
    }
    OptionalBorderSize getFrameSizeIfPresent() const override
    {
        return OptionalBorderSize(BorderSize<int>{});
    }
    BorderSize<int> getFrameSize() const override { return {}; }
    bool setAlwaysOnTop(bool b) override
    {
        EM_ASM(
            { document.getElementById(UTF8ToString($0)).style.zIndex = $2 ? 2000 : ($1 ? 100 : 0); },
            canvasId.toRawUTF8(), b, (getStyleFlags() & windowIsTemporary) != 0);
        return true;
    }
    void toFront(bool focus) override
    {
        EM_ASM(
            {
                const c = document.getElementById(UTF8ToString($0));
                c.parentNode.appendChild(c);
            },
            canvasId.toRawUTF8());
        if (focus)
            grabFocus();
    }
    void toBehind(ComponentPeer *) override
    {
        EM_ASM(
            {
                const c = document.getElementById(UTF8ToString($0));
                c.parentNode.prepend(c);
            },
            canvasId.toRawUTF8());
    }
    String accessibilityCanvasId() const { return canvasId; }
    bool isFocused() const override
    {
        return EM_ASM_INT(
            { return document.activeElement === document.getElementById(UTF8ToString($0)) ||
                       document.activeElement?.dataset.jucePeer === UTF8ToString($0); },
            canvasId.toRawUTF8());
    }
    void grabFocus() override
    {
        if ((getStyleFlags() & windowIgnoresKeyPresses) == 0 && !isFocused())
            EM_ASM({ document.getElementById(UTF8ToString($0)).focus(); }, canvasId.toRawUTF8());
    }
    void repaint(const Rectangle<int> &) override { dirty = true; }
    void performAnyPendingRepaintsNow() override
    {
        if (pixelScale != EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; }))
            setBounds(bounds, fullscreen);
        callVBlankListeners(emscripten_get_now() / 1000.);
        if (dirty || (inputActive && (!isFocused() ||
            findCurrentTextInputTarget() != dynamic_cast<TextInputTarget *>(inputComponent.getComponent()))))
            syncInput();
        if (!dirty || !visible || backing.isNull())
            return;
        dirty = false;
        backing.clear(backing.getBounds());
        auto context = backing.createLowLevelContext();
        context->addTransform(AffineTransform::scale(float(pixelScale)));
        handlePaint(*context);
        paintInput();
        Image::BitmapData pixels(backing, Image::BitmapData::readOnly);
        for (int y = 0; y < backing.getHeight(); ++y)
            for (int x = 0; x < backing.getWidth(); ++x)
            {
                const auto color = pixels.getPixelColour(x, y);
                const auto offset = (size_t(y) * backing.getWidth() + x) * 4;
                rgba[offset] = color.getRed();
                rgba[offset + 1] = color.getGreen();
                rgba[offset + 2] = color.getBlue();
                rgba[offset + 3] = color.getAlpha();
            }
        EM_ASM(
            {
                const c = document.getElementById(UTF8ToString($0));
                const image = c.getContext('2d').createImageData(c.width, c.height);
                image.data.set(HEAPU8.subarray($1, $1 + c.width * c.height * 4));
                c.getContext('2d').putImageData(image, 0, 0);
            },
            canvasId.toRawUTF8(), rgba.data());
    }
    void setAlpha(float alpha) override
    {
        EM_ASM(
            { document.getElementById(UTF8ToString($0)).style.opacity = $1; }, canvasId.toRawUTF8(),
            alpha);
    }
    StringArray getAvailableRenderingEngines() override { return {"JUCE software renderer"}; }
    void textInputRequired(Point<int>, TextInputTarget &) override { syncInput(); }
    void dismissPendingTextInput() override { syncInput(); closeInputMethodContext(); }
#include "BrowserTextInput.inc"
};
bool isBrowserCompositionActive(const TextInputTarget *target)
{
    return browserCompositionComponent != nullptr && target == browserCompositionTarget &&
           target == dynamic_cast<TextInputTarget *>(browserCompositionComponent.getComponent());
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_browser_file_drop(int id, const char *paths, int x, int y)
{
    auto *peer = BrowserPeer::inputPeer(id);
    if (!peer || !peer->getComponent().isShowing()) return 0;
    const auto parsed = JSON::parse(String::fromUTF8(paths));
    const auto *items = parsed.getArray();
    if (!items || items->isEmpty()) return 0;
    ComponentPeer::DragInfo info;
    for (const auto &item : *items)
    {
        if (!item.isString()) return 0;
        info.files.add(item.toString());
    }
    info.position = {x, y};
    return peer->handleDragDrop(info) ? 1 : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_text_update(int id, int generation, int start, int end, char *data, int a, int b)
{
    const auto text = String::fromUTF8(data);
    std::free(data);
    if (auto *peer = BrowserPeer::inputPeer(id)) peer->updateInput(generation,start,end,text,a,b);
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_text_composition(int id, int generation, int begin)
{
    if (auto *peer = BrowserPeer::inputPeer(id)) peer->compositionInput(generation,begin != 0);
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_text_bounds(int id, int generation, int index)
{
    static String bounds;
    bounds = "[0,0,0,0]";
    if (auto *peer = BrowserPeer::inputPeer(id)) bounds = peer->characterInputBounds(generation,index);
    return bounds.toRawUTF8();
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_text_format(int id, int generation, int start, int end, int style, int thickness)
{
    if (auto *peer = BrowserPeer::inputPeer(id)) peer->formatInput(generation,start,end,style,thickness);
}
#include "BrowserAccessibility.inc"
ComponentPeer *Component::createNewPeer(int flags, void *) { return new BrowserPeer(*this, flags); }
bool Desktop::canUseSemiTransparentWindows() noexcept { return true; }
double Desktop::getDefaultMasterScale() { return 1.; }
Desktop::DisplayOrientation Desktop::getCurrentOrientation() const { return upright; }
void Desktop::allowedOrientationsChanged() {}
void Desktop::setScreenSaverEnabled(bool) {}
bool Desktop::isScreenSaverEnabled() { return true; }
void Desktop::setKioskComponent(Component *c, bool enabled, bool)
{
    if (c && c->getPeer())
        c->getPeer()->setFullScreen(enabled);
}
class Desktop::NativeDarkModeChangeDetectorImpl
{
};
std::unique_ptr<Desktop::NativeDarkModeChangeDetectorImpl>
Desktop::createNativeDarkModeChangeDetectorImpl()
{
    return {};
}
bool Desktop::isDarkModeActive() const
{
    return EM_ASM_INT({ return matchMedia('(prefers-color-scheme: dark)').matches; });
}
bool detail::MouseInputSourceList::addSource()
{
    if (!sources.isEmpty())
        return false;
    addSource(0, MouseInputSource::InputSourceType::mouse);
    return true;
}
bool detail::MouseInputSourceList::canUseTouch() const { return false; }
Point<float> MouseInputSource::getCurrentRawMousePosition() { return browserMouse; }
void MouseInputSource::setRawMousePosition(Point<float>) {}
bool KeyPress::isKeyCurrentlyDown(int key)
{
    if (key >= KeyPress::F1Key && key <= KeyPress::F24Key)
        key = 112 + key - KeyPress::F1Key;
    else if (key >= 'a' && key <= 'z')
        key -= 'a' - 'A';
    return browserKeys.count(key) != 0;
}
void Displays::findDisplays(const Desktop &desktop)
{
    Display d;
    d.isMain = true;
    d.scale = desktop.getGlobalScaleFactor();
    d.dpi = 96;
    d.totalArea = {0, 0, EM_ASM_INT({ return innerWidth; }), EM_ASM_INT({ return innerHeight; })};
    d.userArea = d.totalArea;
    displays.add(d);
}
bool WindowUtils::areThereAnyAlwaysOnTopWindows() { return false; }
bool Process::isForegroundProcess()
{
    return EM_ASM_INT({ return document.hasFocus(); });
}
void Process::makeForegroundProcess() {}
void Process::hide() {}
void LookAndFeel::playAlertSound() {}
struct MouseCursor::PlatformSpecificHandle
{
    explicit PlatformSpecificHandle(StandardCursorType t) : type(t) {}
    explicit PlatformSpecificHandle(const detail::CustomMouseCursorInfo &) : type(NormalCursor) {}
    StandardCursorType type;
    static void showInWindow(PlatformSpecificHandle *handle, ComponentPeer *)
    {
        const auto type = handle ? handle->type : NormalCursor;
        const char *css = type == NoCursor             ? "none"
                          : type == IBeamCursor        ? "text"
                          : type == PointingHandCursor ? "pointer"
                          : type == DraggingHandCursor ? "grabbing"
                                                       : "default";
        EM_ASM({ document.body.style.cursor = UTF8ToString($0); }, css);
    }
};
namespace
{
using ClipboardCallback = std::function<void(bool, const String &)>;
std::map<unsigned, ClipboardCallback> clipboardRequests;
unsigned clipboardRequestId = 0;
void requestClipboard(bool write, const String &text, ClipboardCallback callback)
{
    if (clipboardRequests.size() >= 32)
    {
        SystemClipboard::reportClipboardError("Too many pending clipboard requests. Try again.");
        callback(false, {});
        return;
    }
    do { ++clipboardRequestId; } while (clipboardRequests.count(clipboardRequestId));
    clipboardRequests.emplace(clipboardRequestId, std::move(callback));
    EM_ASM({
        const id = $0;
        SurgeClipboard.run(id, $1, UTF8ToString($2), (ok, value) => {
            const pointer = stringToNewUTF8(value);
            _surge_clipboard_complete(id, ok, pointer);
        });
    }, clipboardRequestId, write, text.toRawUTF8());
}
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_clipboard_complete(unsigned id, int ok, const char *text)
{
    const auto value = String::fromUTF8(text);
    std::free(const_cast<char *>(text));
    auto found = clipboardRequests.find(id);
    if (found == clipboardRequests.end()) return;
    auto callback = std::move(found->second);
    clipboardRequests.erase(found);
    callback(ok != 0, value);
}
void SystemClipboard::readTextAsync(std::function<void(bool, const String &)> callback)
{
    requestClipboard(false, {}, std::move(callback));
}
void SystemClipboard::writeTextAsync(const String &text, std::function<void(bool)> callback)
{
    requestClipboard(true, text, [callback = std::move(callback)](bool ok, const String &) { callback(ok); });
}
void SystemClipboard::reportClipboardError(const String &message)
{
    EM_ASM({ SurgeClipboard.report(UTF8ToString($0)); }, message.toRawUTF8());
}
void SystemClipboard::copyTextToClipboard(const String &text)
{
    writeTextAsync(text, [](bool) {});
}
// Chrome exposes no synchronous clipboard read. All JUCE text/code editor
// consumers are adapted to readTextAsync in the checked build-local overlay.
String SystemClipboard::getTextFromClipboard() { return {}; }
bool DragAndDropContainer::performExternalDragDropOfFiles(const StringArray &, bool, Component *,
                                                          std::function<void()>)
{
    return false;
}
bool DragAndDropContainer::performExternalDragDropOfText(const String &, Component *,
                                                         std::function<void()>)
{
    return false;
}
} // namespace juce
#include "juce_gui_basics/native/juce_NativeMessageBox_linux.cpp"

#include "BrowserKeys.h"
namespace juce::detail
{
Image WindowingHelpers::createIconForFile(const File &) { return {}; }
} // namespace juce::detail

// clang-format on

#include "BrowserFileChooser.cpp"
