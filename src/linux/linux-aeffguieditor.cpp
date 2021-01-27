// This file is part of VSTGUI. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/vstgui/LICENSE

#if TARGET_VST2

#include "linux-aeffguieditor.h"
#include "public.sdk/source/vst2.x/audioeffectx.h"
#include "vstgui/lib/platform/platform_x11.h"

namespace VSTGUI
{

/* X11 main loop, triggered by idle()
 * No timing guarantees, since the host calls it for us, but better than a black window!
 * Adapted from the vstgui gdk main loop code
 */

//------------------------------------------------------------------------
struct ExternalEventHandler
{
    X11::IEventHandler *eventHandler{nullptr};
    int fd{-1};
};

//------------------------------------------------------------------------
struct ExternalTimerHandler
{
    X11::ITimerHandler *timerHandler{nullptr};
    uint64_t interval{0};
};

//------------------------------------------------------------------------
class LinuxRunLoop : public X11::IRunLoop
{
  public:
    using IEventHandler = X11::IEventHandler;
    using ITimerHandler = X11::ITimerHandler;

    static LinuxRunLoop &instance();

    LinuxRunLoop(){};

    bool registerEventHandler(int fd, IEventHandler *handler) override;
    bool unregisterEventHandler(IEventHandler *handler) override;

    bool registerTimer(uint64_t interval, ITimerHandler *handler) override;
    bool unregisterTimer(ITimerHandler *handler) override;

    void forget() override {}
    void remember() override {}

    void idle();

  private:
    std::vector<ExternalEventHandler> eventHandlers;
    std::vector<ExternalTimerHandler> timerHandlers;
};

//------------------------------------------------------------------------
LinuxRunLoop &LinuxRunLoop::instance()
{
    static LinuxRunLoop instance;
    return instance;
}

//------------------------------------------------------------------------
bool LinuxRunLoop::registerEventHandler(int fd, IEventHandler *handler)
{
    // printf("%s %i %p\n", __func__, fd, handler);
    if (handler == nullptr)
        return false;
    eventHandlers.push_back({handler, fd});
    return true;
}

//------------------------------------------------------------------------
bool LinuxRunLoop::unregisterEventHandler(IEventHandler *handler)
{
    // printf("%s %p\n", __func__, handler);
    for (auto it = eventHandlers.begin(); it != eventHandlers.end(); it++)
    {
        if (it->eventHandler == handler)
        {
            eventHandlers.erase(it);
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------
bool LinuxRunLoop::registerTimer(uint64_t interval, ITimerHandler *handler)
{
    // printf("%s %lu %p\n", __func__, static_cast<unsigned long>(interval), handler);
    if (handler == nullptr)
        return false;
    timerHandlers.push_back({handler, interval});
    return true;
}

//------------------------------------------------------------------------
bool LinuxRunLoop::unregisterTimer(ITimerHandler *handler)
{
    // printf("%s %p\n", __func__, handler);
    for (auto it = timerHandlers.begin(); it != timerHandlers.end(); it++)
    {
        if (it->timerHandler == handler)
        {
            timerHandlers.erase(it);
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------
void LinuxRunLoop::idle()
{
    // TODO check timing in a smart way

    for (auto &timer : timerHandlers)
        timer.timerHandler->onTimer();

    for (auto &timer : eventHandlers)
        timer.eventHandler->onEvent();
}

//-----------------------------------------------------------------------------
// LinuxAEffGUIEditor Implementation
//-----------------------------------------------------------------------------
LinuxAEffGUIEditor::LinuxAEffGUIEditor(void *pEffect)
    : AEffEditor((AudioEffect *)pEffect), inIdle(false)
{
    ((AudioEffect *)pEffect)->setEditor(this);
    systemWindow = nullptr;
}

//-----------------------------------------------------------------------------
LinuxAEffGUIEditor::~LinuxAEffGUIEditor() {}

//-----------------------------------------------------------------------------
#if VST_2_1_EXTENSIONS
bool LinuxAEffGUIEditor::onKeyDown(VstKeyCode &keyCode)
{
    return frame ? frame->onKeyDown(keyCode) > 0 : false;
}

//-----------------------------------------------------------------------------
bool LinuxAEffGUIEditor::onKeyUp(VstKeyCode &keyCode)
{
    return frame ? frame->onKeyUp(keyCode) > 0 : false;
}
#endif

//-----------------------------------------------------------------------------
bool LinuxAEffGUIEditor::open(void *ptr)
{
    frame = new CFrame(CRect(0, 0, 0, 0), this);
    getFrame()->setTransparency(true);

    IPlatformFrameConfig *config = nullptr;
    X11::FrameConfig x11config;
    x11config.runLoop = &LinuxRunLoop::instance();
    config = &x11config;

    // printf("%s %p %p\n", __func__, frame, ptr);
    getFrame()->open(ptr, kDefaultNative, config);

    systemWindow = ptr;
    return AEffEditor::open(ptr);
}

//-----------------------------------------------------------------------------
bool LinuxAEffGUIEditor::idle2()
{
    if (inIdle)
    {
        fprintf(stderr, "%s: Caught recursive idle call\n", __func__);
        return false;
    }

    inIdle = true;

    AEffEditor::idle();
    if (frame)
        frame->idle();

    LinuxRunLoop::instance().idle();

    inIdle = false;
    return true;
}

//-----------------------------------------------------------------------------
int32_t LinuxAEffGUIEditor::knobMode = kCircularMode;

//-----------------------------------------------------------------------------
bool LinuxAEffGUIEditor::setKnobMode(int32_t val)
{
    LinuxAEffGUIEditor::knobMode = val;
    return true;
}

//-----------------------------------------------------------------------------
bool LinuxAEffGUIEditor::getRect(ERect **ppErect)
{
    *ppErect = &rect;
    return true;
}

// -----------------------------------------------------------------------------
bool LinuxAEffGUIEditor::beforeSizeChange(const CRect &newSize, const CRect &oldSize)
{
    AudioEffectX *eX = (AudioEffectX *)effect;
    if (eX && eX->canHostDo((char *)"sizeWindow"))
    {
        if (eX->sizeWindow((VstInt32)newSize.getWidth(), (VstInt32)newSize.getHeight()))
        {
            return true;
        }
        return false;
    }

    return true;
}

} // namespace VSTGUI

#endif // TARGET_VST guard
