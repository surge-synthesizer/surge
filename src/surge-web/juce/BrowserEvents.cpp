// SPDX-License-Identifier: GPL-3.0-or-later
#include <deque>
#include <mutex>
#include <emscripten.h>
namespace juce
{
namespace
{
std::mutex browserQueueMutex;
std::deque<MessageManager::MessageBase::Ptr> browserQueue;
} // namespace
namespace detail
{
bool dispatchNextMessageOnSystemQueue(bool)
{
    MessageManager::MessageBase::Ptr message;
    {
        const std::lock_guard<std::mutex> lock(browserQueueMutex);
        if (browserQueue.empty())
            return false;
        message = browserQueue.front();
        browserQueue.pop_front();
    }
    message->messageCallback();
    return true;
}
} // namespace detail
void MessageManager::doPlatformSpecificInitialisation() {}
void MessageManager::doPlatformSpecificShutdown()
{
    const std::lock_guard<std::mutex> lock(browserQueueMutex);
    browserQueue.clear();
}
bool MessageManager::postMessageToSystemQueue(MessageBase *message)
{
    const std::lock_guard<std::mutex> lock(browserQueueMutex);
    browserQueue.emplace_back(message);
    return true;
}
void MessageManager::broadcastMessage(const String &) {}
// The browser owns the event loop. Bound each dispatch turn so repaint and input
// cannot be starved by JUCE callbacks which continually post more work.
extern "C" EMSCRIPTEN_KEEPALIVE void surge_dispatch_messages()
{
    Timer::callPendingTimersSynchronously();
    for (int i = 0; i < 256 && detail::dispatchNextMessageOnSystemQueue(true); ++i)
    {
    }
}
} // namespace juce
