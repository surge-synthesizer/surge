// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Development executable only. Observe C++ heap operations on this thread;
// this is not a malloc interposer or a full callback allocation audit.
#include <cstdlib>
#include <new>
namespace AllocationProbe
{
inline thread_local bool enabled = false;
inline thread_local unsigned allocations = 0, deallocations = 0;
inline void begin() { allocations = deallocations = 0; enabled = true; }
inline bool end() { enabled = false; return allocations == 0 && deallocations == 0; }
}
void *operator new(std::size_t size)
{
    if (AllocationProbe::enabled) ++AllocationProbe::allocations;
    if (auto *p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept
{
    if (p && AllocationProbe::enabled) ++AllocationProbe::deallocations;
    std::free(p);
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete(void *p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::size_t) noexcept { ::operator delete(p); }
