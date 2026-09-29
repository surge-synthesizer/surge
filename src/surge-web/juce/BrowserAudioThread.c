/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <emscripten/version.h>
#if __EMSCRIPTEN_MAJOR__ != 6 || __EMSCRIPTEN_MINOR__ != 0 || __EMSCRIPTEN_TINY__ != 10
#error "Review AudioWorklet thread retirement before changing the pinned Emscripten 6.0.10 SDK"
#endif
#include "pthread_impl.h"

/* Emscripten 6.0.10's AudioWorklet initialization inserts pthread metadata at
 * the beginning of the caller-owned stack into musl's thread list. Its public
 * destroy_audio_context only suspends the context and does not unlink it.
 * After Chrome confirms close(), unlink that entry before reusing the stack.
 * This deliberately does not call pthread_exit/free_data: this is not a
 * pthread, and the stack is static, not a malloc-owned pthread allocation.
 * Keep this small SDK-internal dependency version-checked and isolated here.
 */
void surge_retire_audio_thread(void *stack)
{
    pthread_t thread = (pthread_t)stack;
    if (thread->self != thread)
        return;
    __tl_lock();
    thread->next->prev = thread->prev;
    thread->prev->next = thread->next;
    thread->prev = thread->next = thread;
    thread->self = 0;
    __tl_unlock();
}
