#pragma once

#include <cmath>
#include <vector>

/** A 2-D lookup table for std::pow, used by BBDFilters */
struct PowLUT
{
    PowLUT(float baseMin, float baseMax, float expMin, float expMax, int nTables, int nPoints)
    {
        tables.resize(nTables);
        for (auto &table : tables)
            table.resize(nPoints, 0.0f);

        baseOffset = baseMin;
        baseScale = (float)nTables / (baseMax - baseMin);
        expOffset = expMin;
        expScale = (float)nTables / (expMax - expMin);

        for (int i = 0; i < nTables; ++i)
            for (int j = 0; j < nPoints; ++j)
                tables[i][j] =
                    std::pow((float)i / baseScale + baseOffset, (float)j / expScale + expOffset);
    }

    inline float operator()(float base, float exp) const noexcept
    {
        auto baseIdx = size_t((base - baseOffset) * baseScale);
        auto expIdx = size_t((base - expOffset) * expScale);
        return tables[baseIdx][expIdx];
    }

  private:
    std::vector<std::vector<float>> tables;

    float baseOffset, baseScale;
    float expOffset, expScale;
};

static PowLUT powLut{-1.0f, 1.0f, -0.1f, 0.1f, 256, 128};
