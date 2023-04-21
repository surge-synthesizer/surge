/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#ifndef SURGE_SRC_COMMON_DSP_UTILITIES_DSPUTILS_H
#define SURGE_SRC_COMMON_DSP_UTILITIES_DSPUTILS_H

#include <algorithm>
#include <cmath>

#include "globals.h"

template <class T, bool first_run_checks = true> class lag
{
  public:
    lag(T lp)
    {
        this->lp = lp;
        lpinv = 1 - lp;
        v = 0;
        target_v = 0;

        if (first_run_checks)
        {
            first_run = true;
        }
    }

    lag()
    {
        lp = 0.004;
        lpinv = 1 - lp;
        v = 0;
        target_v = 0;

        if (first_run_checks)
        {
            first_run = true;
        }
    }

    void setRate(T lp)
    {
        this->lp = lp;
        lpinv = 1 - lp;
    }

    inline void newValue(T f)
    {
        target_v = f;

        if (first_run_checks && first_run)
        {
            v = target_v;
            first_run = false;
        }
    }

    inline void startValue(T f)
    {
        target_v = f;
        v = f;

        if (first_run_checks && first_run)
        {
            first_run = false;
        }
    }

    inline void instantize() { v = target_v; }

    inline T getTargetValue() { return target_v; }

    inline void process() { v = v * lpinv + target_v * lp; }

    T v;
    T target_v;

  private:
    bool first_run;
    T lp, lpinv;
};

inline bool within_range(int lo, int value, int hi) { return ((value >= lo) && (value <= hi)); }

template <class T, bool first_run_checks = true> class lipol
{
  public:
    lipol() { reset(); }

    void reset()
    {
        if (first_run_checks)
        {
            first_run = true;
        }

        new_v = 0;
        v = 0;
        dv = 0;

        setBlockSize(BLOCK_SIZE);
    }

    inline void newValue(T f)
    {
        v = new_v;
        new_v = f;

        if (first_run_checks && first_run)
        {
            v = f;
            first_run = false;
        }

        dv = (new_v - v) * bs_inv;
    }

    inline T getTargetValue() { return new_v; }

    inline void instantize()
    {
        v = new_v;
        dv = 0;
    }

    inline void process() { v += dv; }

    void setBlockSize(int n) { bs_inv = 1 / (T)n; }

    T v;
    T new_v;
    T dv;

  private:
    T bs_inv;
    bool first_run;
};

// Use custom format (x^3) to represent gain internally, but save as decibel in XML-data
inline float amp_to_linear(float x)
{
    x = std::max(0.f, x);

    return x * x * x;
}

inline float linear_to_amp(float x) { return powf(std::clamp(x, 0.0000000001f, 1.f), 1.f / 3.f); }

inline float amp_to_db(float x) { return std::clamp((float)(18.f * log2(x)), -192.f, 96.f); }

inline float db_to_amp(float x) { return std::clamp(powf(2.f, x / 18.f), 0.f, 2.f); }

#endif // SURGE_SRC_COMMON_DSP_UTILITIES_DSPUTILS_H
