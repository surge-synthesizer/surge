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

#ifndef SURGE_LANCZOSRESAMPLER_H
#define SURGE_LANCZOSRESAMPLER_H

#include "globals.h"
#include "portable_intrinsics.h"
#include <algorithm>
#include <utility>
#include <cmath>
#include <cstring>
#include "DebugHelpers.h"

/*
 * See https://en.wikipedia.org/wiki/Lanczos_resampling
 */

struct LanczosResampler
{
    static constexpr size_t A = 4;
    static constexpr size_t BUFFER_SZ = 4096;
    static constexpr size_t filterWidth = A * 2;
    static constexpr size_t tableObs = 8192;
    static constexpr double dx = 1.0 / (tableObs);

    // Fixme: Make this static and shared
    static float lanczosTable[tableObs][filterWidth], lanczosTableDX[tableObs][filterWidth];
    static bool tablesInitialized;

    float input[BUFFER_SZ * 2];
    int wp = 0;
    float sri, sro;
    double phaseI, phaseO, dPhaseI, dPhaseO;

    inline double kernel(double x)
    {
        if (fabs(x) < 1e-7)
            return 1;
        return A * std::sin(M_PI * x) * std::sin(M_PI * x / A) / (M_PI * M_PI * x * x);
    }

    LanczosResampler(float inputRate, float outputRate) : sri(inputRate), sro(outputRate)
    {
        phaseI = 0;
        phaseO = 0;

        dPhaseI = 1.0;
        dPhaseO = sri / sro;

        memset(input, 0, BUFFER_SZ * sizeof(float));
        if (!tablesInitialized)
        {
            for (int t = 0; t < tableObs; ++t)
            {
                double x0 = dx * t;
                for (int i = 0; i < filterWidth; ++i)
                {
                    double x = x0 + i - A;
                    lanczosTable[t][i] = kernel(x);
                }
            }
            for (int t = 0; t < tableObs; ++t)
            {
                for (int i = 0; i < filterWidth; ++i)
                {
                    lanczosTableDX[t][i] =
                        lanczosTable[(t + 1) & (tableObs - 1)][i] - lanczosTable[t][i];
                }
            }
            tablesInitialized = true;
        }
    }

    inline void push(float f)
    {
        input[wp] = f;
        input[wp + BUFFER_SZ] = f; // this way we can always wrap
        wp = (wp + 1) & (BUFFER_SZ - 1);
        phaseI += dPhaseI;
    }

    inline float readZOH(double xBack) const
    {
        double p0 = wp - xBack;
        int idx0 = (int)p0;
        idx0 = (idx0 + BUFFER_SZ) & (BUFFER_SZ - 1);
        if (idx0 <= A)
            idx0 += BUFFER_SZ;
        return input[idx0];
    }

    inline float readLin(double xBack) const
    {
        double p0 = wp - xBack;
        int idx0 = (int)p0;
        float frac = p0 - idx0;
        idx0 = (idx0 + BUFFER_SZ) & (BUFFER_SZ - 1);
        if (idx0 <= A)
            idx0 += BUFFER_SZ;
        return (1.0 - frac) * input[idx0] + frac * input[idx0 + 1];
    }

    inline float read(double xBack) const
    {
        double p0 = wp - xBack;
        int idx0 = floor(p0);
        double off0 = (p0 - idx0);

        idx0 = (idx0 + BUFFER_SZ) & (BUFFER_SZ - 1);
        idx0 += (idx0 <= A) * BUFFER_SZ;

        double off0byto = off0 * tableObs;
        int tidx = (int)(off0byto);
        double fidx = (off0byto - tidx);

        auto f0 = _mm_loadu_ps(&lanczosTable[tidx][0]);
        auto df0 = _mm_loadu_ps(&lanczosTableDX[tidx][0]);
        f0 = _mm_add_ps(f0, _mm_mul_ps(df0, _mm_set1_ps((float)fidx)));

        auto f1 = _mm_loadu_ps(&lanczosTable[tidx][4]);
        auto df1 = _mm_loadu_ps(&lanczosTableDX[tidx][4]);
        f1 = _mm_add_ps(f1, _mm_mul_ps(df1, _mm_set1_ps((float)fidx)));

        auto d0 = _mm_loadu_ps(&input[idx0 - A]);
        auto d1 = _mm_loadu_ps(&input[idx0]);
        auto rv = _mm_add_ps(_mm_mul_ps(f0, d0), _mm_mul_ps(f1, d1));
        auto res = vSum(rv);
        return res;
    }

    inline size_t inputsRequiredToGenerateOutputs(size_t desiredOutputs) const
    {
        /*
         * So (phaseI + dPhaseI * res - phaseO - dPhaseO * desiredOutputs) * sri > A + 1
         *
         * Use the fact that dPhaseI = sri and find
         * res > (A+1) - (phaseI - phaseO + dPhaseO * desiredOutputs) * sri
         */
        double res = A + 1 - (phaseI - phaseO - dPhaseO * desiredOutputs);

        return (size_t)std::max(res + 1, 0.0); // Check this calculation
    }

    size_t populateNext(float *f, size_t max)
    {
        int populated = 0;
        while (populated < max && (phaseI - phaseO) > A + 1)
        {
            f[populated] = read((phaseI - phaseO));
            // f[populated] = readZOH(phaseI-phaseO);
            // f[populated] = readLin(phaseI-phaseO);
            phaseO += dPhaseO;
            populated++;
        }
        return populated;
    }

    inline void advanceReadPointer(size_t n) { phaseO += n * dPhaseO; }
    inline void snapOutToIn()
    {
        phaseO = 0;
        phaseI = 0;
    }

    inline void renormalizePhases()
    {
        phaseI -= phaseO;
        phaseO = 0;
    }
};

#endif // SURGE_LANCZOSRESAMPLER_H
