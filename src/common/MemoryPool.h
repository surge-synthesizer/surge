/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_MEMORYPOOL_H
#define SURGE_SRC_COMMON_MEMORYPOOL_H

namespace Surge
{
namespace Memory
{
// pre-alloc must be at least one
template <typename T, size_t preAlloc, size_t growBy, size_t capacity = 16384> struct MemoryPool
{
    template <typename... Args> MemoryPool(Args &&...args)
    {
        while (position < preAlloc)
            refreshPool(std::forward<Args>(args)...);
    }
    ~MemoryPool()
    {
        for (size_t i = 0; i < position; ++i)
            delete pool[i];
    }
    template <typename... Args> T *getItem(Args &&...args)
    {
        if (position == 0)
        {
            refreshPool(std::forward<Args>(args)...);
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
    template <typename... Args> void refreshPool(Args &&...args)
    {
        // In an ideal world, this grow would be off thread.
        // For XT 1 leave it here and have a reasonable prealloc
        assert(position < (growBy + capacity));
        for (size_t i = 0; i < growBy; ++i)
        {
            pool[position] = new T(std::forward<Args>(args)...);
            position++;
        }
    }

    template <typename... Args> void setupPoolToSize(size_t upTo, Args &&...args)
    {
        while (position < upTo)
        {
            pool[position] = new T(std::forward<Args>(args)...);
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
