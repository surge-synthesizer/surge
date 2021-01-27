#pragma once
#include "AllLv2.h"
#include <list>
#include <chrono>

#if LINUX
#include "vstgui/lib/platform/platform_x11.h"
#include "vstgui/lib/platform/linux/x11platform.h"

class Lv2IdleRunLoop : public VSTGUI::X11::IRunLoop
{
  public:
    void execIdle();

    bool registerEventHandler(int fd, VSTGUI::X11::IEventHandler *handler) override;
    bool unregisterEventHandler(VSTGUI::X11::IEventHandler *handler) override;
    bool registerTimer(uint64_t interval, VSTGUI::X11::ITimerHandler *handler) override;
    bool unregisterTimer(VSTGUI::X11::ITimerHandler *handler) override;

    void forget() override {}
    void remember() override {}

  private:
    struct Event
    {
        int fd;
        VSTGUI::X11::IEventHandler *handler;
        bool alive;
    };
    struct Timer
    {
        std::chrono::microseconds interval;
        std::chrono::microseconds counter;
        bool lastTickValid;
        std::chrono::steady_clock::time_point lastTick;
        VSTGUI::X11::ITimerHandler *handler;
        bool alive;
    };

  private:
    template <class T> static void garbageCollectDeadHandlers(std::list<T> &handlers);

  private:
    std::list<Event> _events;
    std::list<Timer> _timers;
};
#endif

#if LINUX
namespace VSTGUI
{
void initializeSoHandle();
}
#endif
