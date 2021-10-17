/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_MEMORYPOOL_H
#define SURGE_MEMORYPOOL_H

#include <unordered_set>
#include <iostream>

namespace Surge
{
namespace Memory
{
// pre-alloc must be at least one
template <typename T, size_t preAlloc, size_t growBy, size_t capacity = 16384> struct MemoryPool
{
    MemoryPool()
    {
        while (position < preAlloc)
            refreshPool();
    }
    ~MemoryPool()
    {
        std::cout << "Cleaning Up MemProol with " << position << std::endl;
        for (size_t i = 0; i < position; ++i)
            delete pool[i];
    }
    T *getItem()
    {
        if (position == 0)
        {
            refreshPool();
        }
        auto q = pool[position - 1];
        pool[position - 1] = nullptr; // just to flag bugs
        position--;
        return q;
    }
    void returnItem(T *t)
    {
        pool[position] = t;
        position++;
    }
    void refreshPool()
    {
        // In an ideal world, this grow would be off thread.
        // For XT 1 leave it here and have a reasonable prealloc
        assert(position < (growBy + capacity));
        for (size_t i = 0; i < growBy; ++i)
        {
            pool[position] = new T();
            position++;
        }
    }

    void setupPoolToSize(size_t upTo)
    {
        while (position < upTo)
        {
            pool[position] = new T();
            position++;
        }
    }

    void returnToPreAllocSize()
    {
        while (position > preAlloc)
        {
            delete pool[position - 1];
            pool[position - 1] = nullptr;
            position--;
        }
    }

    std::array<T *, capacity> pool;

    /*
     * Position is the location of the next *free* slot. That is
     * the pointer should be returned to position, and retrieved from
     * position -1. position == 0 is a sentinel to rebuild.
     */
    size_t position{0};
};
} // namespace Memory
} // namespace Surge

#endif // SURGE_MEMORYPOOL_H
