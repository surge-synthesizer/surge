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
#ifndef SURGE_SRC_COMMON_UNITCONVERSIONS_H
#define SURGE_SRC_COMMON_UNITCONVERSIONS_H
#include <stdio.h>
#include <algorithm>
#include <locale>
#include <sstream>
#include <string>
#include <cstring>
#include <fmt/core.h>
#include <fmt/format.h>

static float env_phasemulti = 1000 / 44100.f;
static float lfo_range = 1000;

inline float lfo_phaseincrement(int samples, float rate)
{
    rate = 1 - rate;
    return samples * env_phasemulti / (1 + lfo_range * rate * rate * rate);
}

inline double linear_to_dB(double in) { return 20 * log10(in); }

inline double dB_to_linear(double in) { return pow((double)10, 0.05 * in); }

inline float timecent_to_seconds(float in) { return powf(2, in / 1200); }

inline std::string get_notename(int i_value, int i_offset)
{
    int ival = i_value < 0 ? i_value - 11 : i_value;
    int octave = (ival / 12) - i_offset;
    char notenames[13][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C"};
    // make sure to read the notenames array in reverse for negative i_values
    int note = (i_value < 0) ? (12 - abs(i_value % 12)) : (i_value % 12);

    return fmt::format("{:s}{:d}", notenames[note], octave);
}

/*
 * Turn a float into a string, but do it in an internatliazation independent
 * way appropriate for streaming into XML and the like
 */
inline std::string float_to_clocalestr(float value)
{
    return fmt::format(std::locale::classic(), "{:L}", value);
}

/*
 * Parse a number a user typed in, independent of the active locale. Accepts
 * either '.' or ',' as the decimal separator, since what a user types depends
 * on their keyboard and locale rather than on ours. std::atof follows
 * LC_NUMERIC, so in a comma-decimal locale it stops at the '.' in "0.5" and
 * silently yields 0, and in a dot-decimal locale it does the same to "0,5".
 */
inline double clocalestr_to_double(const char *s, bool *parsed)
{
    if (parsed)
    {
        *parsed = false;
    }

    if (!s)
    {
        return 0.0;
    }

    std::string t{s};
    std::replace(t.begin(), t.end(), ',', '.');

    // strtod is LC_NUMERIC-dependent, so parse through a classic-locale stream
    std::istringstream is{t};
    is.imbue(std::locale::classic());

    double res{0.0};
    is >> res;

    if (is.fail())
    {
        // match atof, which returns 0 for input that isn't a number
        return 0.0;
    }

    if (parsed)
    {
        *parsed = true;
    }

    return res;
}

inline double clocalestr_to_double(const char *s) { return clocalestr_to_double(s, nullptr); }

inline double clocalestr_to_double(const std::string &s)
{
    return clocalestr_to_double(s.c_str(), nullptr);
}

#endif // SURGE_SRC_COMMON_UNITCONVERSIONS_H
