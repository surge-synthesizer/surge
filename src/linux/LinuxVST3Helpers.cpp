/*
** The various things we need to help with VST3 Linux
*/

#if TARGET_VST3
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/platform/platform_x11.h"
#include "vstgui/lib/platform/linux/x11platform.h"
#include "base/source/updatehandler.h"

using namespace VSTGUI;

static int ct = 0;
// Map Steinberg Vst Interface (Steinberg::Linux::IRunLoop) to VSTGUI Interface
// (VSTGUI::X11::RunLoop)
class RunLoop : public X11::IRunLoop, public AtomicReferenceCounted
{
public:
   struct EventHandler : Steinberg::Linux::IEventHandler, public Steinberg::FObject
   {
      X11::IEventHandler* handler{nullptr};

      void PLUGIN_API onFDIsSet(Steinberg::Linux::FileDescriptor) override
      {
         // std::cout << __func__ << " " << handler << " " << ct++ << std::endl;
         if (handler)
            handler->onEvent();
         // std::cout << __func__ << " END " << handler << std::endl;
      }
      DELEGATE_REFCOUNT(Steinberg::FObject)
      DEFINE_INTERFACES
      DEF_INTERFACE(Steinberg::Linux::IEventHandler)
      END_DEFINE_INTERFACES(Steinberg::FObject)
   };

   struct TimerHandler : Steinberg::Linux::ITimerHandler, public Steinberg::FObject
   {
      X11::ITimerHandler* handler{nullptr};

      void PLUGIN_API onTimer() final
      {
         // std::cout << __func__ << " " << handler << std::endl;
         if (handler)
            handler->onTimer();
         // std::cout << __func__ << " END " << handler << std::endl;
      }
      DELEGATE_REFCOUNT(Steinberg::FObject)
      DEFINE_INTERFACES
      DEF_INTERFACE(Steinberg::Linux::ITimerHandler)
      END_DEFINE_INTERFACES(Steinberg::FObject)
   };

   bool registerEventHandler(int fd, X11::IEventHandler* handler) final
   {
      if (!runLoop)
         return false;

      auto smtgHandler = Steinberg::owned(new EventHandler());
      smtgHandler->handler = handler;
      if (runLoop->registerEventHandler(smtgHandler, fd) == Steinberg::kResultTrue)
      {
         eventHandlers.push_back(smtgHandler);
         return true;
      }
      return false;
   }

   bool unregisterEventHandler(X11::IEventHandler* handler) final
   {
     if (!runLoop)
         return false;

      for (auto it = eventHandlers.begin(), end = eventHandlers.end(); it != end; ++it)
      {
         if ((*it)->handler == handler)
         {
            runLoop->unregisterEventHandler((*it));
            eventHandlers.erase(it);
            return true;
         }
      }
      return false;
   }
   bool registerTimer(uint64_t interval, X11::ITimerHandler* handler) final
   {
      if (!runLoop)
         return false;
      // std::cout << "Have a runloop" << std::endl;

      auto smtgHandler = Steinberg::owned(new TimerHandler());
      smtgHandler->handler = handler;
      if (runLoop->registerTimer(smtgHandler, interval) == Steinberg::kResultTrue)
      {
         timerHandlers.push_back(smtgHandler);
         return true;
      }
      return false;
   }
   bool unregisterTimer(X11::ITimerHandler* handler) final
   {
      if (!runLoop)
         return false;

      for (auto it = timerHandlers.begin(), end = timerHandlers.end(); it != end; ++it)
      {
         if ((*it)->handler == handler)
         {
            runLoop->unregisterTimer((*it));
            timerHandlers.erase(it);
            return true;
         }
      }
      return false;
   }

   RunLoop(Steinberg::Linux::IRunLoop* _runLoop) : runLoop(_runLoop)
   {
      // std::cout << "RunLoop is " << runLoop << " " << _runLoop << std::endl;
   }

   void idle()
   {
      // See comment in LInuxVST3Idle below
      for( auto h : eventHandlers )
         h->handler->onEvent();
   }

private:
   using EventHandlers = std::vector<Steinberg::IPtr<EventHandler>>;
   using TimerHandlers = std::vector<Steinberg::IPtr<TimerHandler>>;
   EventHandlers eventHandlers;
   TimerHandlers timerHandlers;
   // Steinberg::FUnknownPtr<Steinberg::Linux::IRunLoop> runLoop;
   Steinberg::Linux::IRunLoop* runLoop;
};

//-----------------------------------------------------------------------------
class UpdateHandlerInit
{
public:
   UpdateHandlerInit()
   {
      get();
   }
   Steinberg::UpdateHandler* get()
   {
      return Steinberg::UpdateHandler::instance();
   }
};

static UpdateHandlerInit gUpdateHandlerInit;

//-----------------------------------------------------------------------------
class IdleUpdateHandler
{
public:
   std::atomic<bool> running;
   pthread_t t;
   static void start()
   {
      auto& instance = get();
      if (++instance.users == 1)
      {
	      instance.running = true;
         pthread_create(&instance.t, NULL, IdleUpdateHandler::doDefUp, NULL);
      }
   }

   static void* doDefUp(void* x)
   {
      while (get().running)
      {
         // std::cout << "GUpdate" << std::endl;
         gUpdateHandlerInit.get()->triggerDeferedUpdates();
         usleep(1000 / 30.0);
      }
      return nullptr;
   }

   static void stop()
   {
      auto& instance = get();
      if (--instance.users == 0)
      {
	      instance.running = false;
	      pthread_join(instance.t, NULL);
      }
   }

protected:
   static IdleUpdateHandler& get()
   {
      static IdleUpdateHandler gInstance;
      return gInstance;
   }

   VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
   std::atomic<uint32_t> users{0};
};

void LinuxVST3Init(Steinberg::Linux::IRunLoop* rl)
{
   // std::cout << "irl is " << rl << std::endl;
   VSTGUI::X11::RunLoop::init(owned(new RunLoop(rl)));
}

void LinuxVST3FrameOpen(VSTGUI::CFrame* that, void* parent, const VSTGUI::PlatformType& pt)
{
   IPlatformFrameConfig* config = nullptr;
   X11::FrameConfig x11config;
   x11config.runLoop = VSTGUI::X11::RunLoop::get();
   config = &x11config;

   // std::cout << "Special Magical VST3 Open " << VSTGUI::X11::RunLoop::instance().getXcbConnection() << std::endl;
   that->open(parent, pt, config);
   // std::cout << "Are we done?" << std::endl;
   IdleUpdateHandler::start();
}

void LinuxVST3Detatch()
{
  IdleUpdateHandler::stop();

  // We need to downcount the usage on the RunLoop to allow xcb to restart
  VSTGUI::X11::RunLoop::exit();
}

void LinuxVST3Idle()
{
   /*
   ** Why is this here? With the VST3 runloop we should be
   ** parsimoniously notified by file descriptor polls and stuff right?
   ** Well, of course, that doesn't work. If you listen to every 
   ** hover event somewhere in the bowls of vst3sdk, DAW runloops,
   ** and so forth, you eventually hang up and lose events.
   ** So what this does is, every idle, just give a kick to every
   ** FDI poller to go and check. Basically just like VST2.
   **
   ** If you decide one day to eliminate this to optimize something
   ** or another, make sure that in bitwig and the juce host that
   ** as you mouse over hover assets they keep working forever and 
   ** don't hang up after 10-50 seconds. See #1817
   */
   auto xrlp = VSTGUI::X11::RunLoop::get().get();
   auto rlp = dynamic_cast<RunLoop*>(xrlp);
   if( rlp )
      rlp->idle();
}
#endif
