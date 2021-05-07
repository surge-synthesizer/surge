#include <iostream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "BiquadFilter.h"
#include "QuadFilterUnit.h"

#include "catch2/catch2.hpp"

inline size_t align_diff(const void *ptr, std::uintptr_t alignment) noexcept
{
    auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
    return (iptr % alignment);
}

TEST_CASE("Biquad Aligned", "[infra]")
{
    SECTION("Is it aligned?")
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

TEST_CASE("QFU is Aligned", "[infra]")
{
    SECTION("Single QFU")
    {
        std::vector<QuadFilterUnitState *> pointers;
        for (int i = 0; i < 5000; ++i)
        {
            auto *f = new QuadFilterUnitState();
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

    SECTION("QFU Array")
    {
        int nqfus = 5;
        std::vector<QuadFilterUnitState *> pointers;
        for (int i = 0; i < 5000; ++i)
        {
            auto *f = new QuadFilterUnitState[nqfus]();
            for (int j = 0; j < nqfus; ++j)
            {
                auto *q = &f[j];
                REQUIRE(align_diff(q, 16) == 0);
                if (j >= 1)
                {
                    auto *qp = &f[j - 1];
                    REQUIRE((size_t)q - (size_t)qp == sizeof(QuadFilterUnitState));
                }
            }
            delete[] f;
        }
    }
}