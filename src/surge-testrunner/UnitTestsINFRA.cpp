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
#include <iostream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "BiquadFilter.h"
#include "MemoryPool.h"

#include "sst/plugininfra/strnatcmp.h"

#include "catch2/catch_amalgamated.hpp"

inline size_t align_diff(const void *ptr, std::uintptr_t alignment) noexcept
{
    auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
    return (iptr % alignment);
}

TEST_CASE("Test Setup Is Correct", "[infra]")
{
    SECTION("No Patches, No Wavetables")
    {
        auto surge = Surge::Headless::createSurge(44100, false);
        REQUIRE(surge);
        REQUIRE(surge->storage.patch_list.empty());
        REQUIRE(surge->storage.wt_list.empty());
    }

    SECTION("Patches, Wavetables")
    {
        auto surge = Surge::Headless::createSurge(44100, true);
        REQUIRE(surge);
        REQUIRE(!surge->storage.patch_list.empty());
        REQUIRE(!surge->storage.wt_list.empty());
    }
}
TEST_CASE("Biquad Is SIMD Aligned", "[infra]")
{
    SECTION("Is It Aligned?")
    {
        std::vector<BiquadFilter *> pointers;
        for (int i = 0; i < 5000; ++i)
        {
            auto *f = new BiquadFilter();
            REQUIRE(align_diff(f, 16) == 0);
            pointers.push_back(f);
            if (rand() % 100 > 70 && !pointers.empty())
            {
                for (auto *d : pointers)
                    delete d;
                pointers.clear();
            }
        }
        if (pointers.empty())
            for (auto *d : pointers)
                delete d;
    }
}

TEST_CASE("QuadFilterUnit Is SIMD Aligned", "[infra]")
{
    SECTION("Single QuadFilterUnit")
    {
        std::vector<sst::filters::QuadFilterUnitState *> pointers;
        for (int i = 0; i < 5000; ++i)
        {
            auto *f = new sst::filters::QuadFilterUnitState();
            REQUIRE(align_diff(f, 16) == 0);
            pointers.push_back(f);
            if (rand() % 100 > 70 && !pointers.empty())
            {
                for (auto *d : pointers)
                    delete d;
                pointers.clear();
            }
        }
        if (pointers.empty())
            for (auto *d : pointers)
                delete d;
    }

    SECTION("Array of QuadFilterUnits")
    {
        int nqfus = 5;
        for (int i = 0; i < 5000; ++i)
        {
            auto *f = new sst::filters::QuadFilterUnitState[nqfus]();
            for (int j = 0; j < nqfus; ++j)
            {
                auto *q = &f[j];
                REQUIRE(align_diff(q, 16) == 0);
                if (j >= 1)
                {
                    auto *qp = &f[j - 1];
                    REQUIRE((size_t)q - (size_t)qp == sizeof(sst::filters::QuadFilterUnitState));
                }
            }
            delete[] f;
        }
    }
}

// A is just a test index to separate the classes
template <int A> struct CountAlloc
{
    CountAlloc()
    {
        alloc++;
        ct++;
    }
    ~CountAlloc() { ct--; }
    static int ct, alloc;
};
template <int A> int CountAlloc<A>::ct{0};
template <int A> int CountAlloc<A>::alloc{0};

TEST_CASE("Memory Pool Works", "[infra]")
{
    SECTION("Lots of random gets and returns")
    {
        {
            auto pool = std::make_unique<Surge::Memory::MemoryPool<CountAlloc<1>, 32, 4, 500>>();
            std::deque<CountAlloc<1> *> tmp;
            for (int i = 0; i < 100; ++i)
            {
                tmp.push_back(pool->getItem());
            }

            for (int i = 0; i < 100; ++i)
            {
                auto q = tmp.front();
                tmp.pop_front();
                pool->returnItem(q);
            }
        }
        REQUIRE(CountAlloc<1>::alloc == 100);
        REQUIRE(CountAlloc<1>::ct == 0);
    }

    SECTION("Resize Up")
    {
        {
            auto pool = std::make_unique<Surge::Memory::MemoryPool<CountAlloc<2>, 32, 4, 500>>();
            pool->setupPoolToSize(180);
        }
        REQUIRE(CountAlloc<2>::alloc == 180);
        REQUIRE(CountAlloc<2>::ct == 0);
    }

    SECTION("Resize Up, Reallocate Down")
    {
        {
            auto pool = std::make_unique<Surge::Memory::MemoryPool<CountAlloc<3>, 32, 4, 500>>();
            pool->setupPoolToSize(160);
            REQUIRE(CountAlloc<3>::ct == 160);
            pool->returnToPreAllocSize();
            REQUIRE(CountAlloc<3>::ct == 32);
        }
        REQUIRE(CountAlloc<3>::alloc == 160);
        REQUIRE(CountAlloc<3>::ct == 0);
    }
}

TEST_CASE("strnatcmp With Spaces", "[infra]")
{
    SECTION("Basic Comparison")
    {
        REQUIRE(strnatcmp("foo", "bar") == 1);
        REQUIRE(strnatcmp("bar", "foo") == -1);
        REQUIRE(strnatcmp("bar", "bar") == 0);
    }

    SECTION("Numbers Comparison")
    {
        REQUIRE(strnatcmp("1 foo", "2 foo") == -1);
        REQUIRE(strnatcmp("1 foo", "11 foo") == -1);
        REQUIRE(strnatcmp("1 foo", "91 foo") == -1);
        REQUIRE(strnatcmp("01 foo", "91 foo") == -1);
        REQUIRE(strnatcmp("91 foo", "1 foo") == 1);
        REQUIRE(strnatcmp("91 foo", "01 foo") == 1);
        REQUIRE(strnatcmp("91 foo", "001 foo") == 1);
        REQUIRE(strnatcmp("1 foo", "112 foo") == -1);
        REQUIRE(strnatcmp("91 foo", "112 foo") == -1);
        REQUIRE(strnatcmp("112 foo", "91 foo") == 1);
        REQUIRE(strnatcmp("01 foo", "2 foo") == -1);
        REQUIRE(strnatcmp("001 foo", "2 foo") == -1);
        REQUIRE(strnatcmp("001 foo", "002 foo") == -1);
        // Fix these one day
        // REQUIRE(strnatcmp("01 foo", "002 foo") == -1);
        // REQUIRE(strnatcmp("1 foo", "002 foo") == -1);
        // REQUIRE(strnatcmp("1 foo", "02 foo") == -1);
    }

    SECTION("Spaces vs Letters")
    {
        REQUIRE(strnatcmp("hello", "zebra") == -1);
        REQUIRE(strnatcmp("hello ", "zebra") == -1);
        REQUIRE(strnatcmp(" hello", "zebra") == -1);
        REQUIRE(strnatcmp("hello", " zebra") == -1);
        REQUIRE(strnatcmp("hello", "zebra ") == -1);
        REQUIRE(strnatcmp("hello", "z") == -1);
        REQUIRE(strnatcmp("hello", "z ") == -1);
        REQUIRE(strnatcmp("hello", " z") == -1);
        REQUIRE(strnatcmp(" hello", "z") == -1);
        REQUIRE(strnatcmp("hello ", "z") == -1);
    }

    SECTION("Central Spaces")
    {
        REQUIRE(strnatcmp("Spa Day", "SpaDay") == -1);
        REQUIRE(strnatcmp("SpaDay", "Spa Day") == 1);
    }

    SECTION("Doubled Spaces") { REQUIRE(strnatcmp("Spa  Day", "Spa Day") == 0); }
}

TEST_CASE("Template Patches are Rational", "[infra]")
{
    auto surge = Surge::Headless::createSurge(48000, true);

    int tempCat{-1};
    for (const auto &c : surge->storage.patch_category)
    {
        if (c.name == "Templates" && c.isFactory)
        {
            tempCat = c.internalid;
        }
    }
    REQUIRE(tempCat >= 0);

    int tested{0};
    int idx{0};
    for (auto &p : surge->storage.patch_list)
    {
        if (p.category == tempCat)
        {
            INFO("Testing " << p.name);

            surge->loadPatch(idx);

            for (int i = 0; i < 5; ++i)
                surge->process();

            auto &patch = surge->storage.getPatch();

            REQUIRE(patch.name == p.name);

            for (int sc = 0; sc < n_scenes; ++sc)
            {
                auto &scene = patch.scene[sc];
                // Make sure the filters and waveshapers aren't off in each scene
                REQUIRE(!scene.filterunit[0].type.deactivated);
                REQUIRE(!scene.filterunit[1].type.deactivated);
                REQUIRE(!scene.wsunit.type.deactivated);

                // Make sure at least one oscillator is not muted in each
                bool somethingOn = !scene.mute_o1.val.b || !scene.mute_o2.val.b ||
                                   !scene.mute_o3.val.b || !scene.mute_noise.val.b;
                REQUIRE(somethingOn);
            }
            tested++;
        }
        ++idx;
    }
    REQUIRE(tested);
}