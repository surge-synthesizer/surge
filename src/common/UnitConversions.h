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
#ifndef SURGE_SRC_COMMON_UNITCONVERSIONS_H
#define SURGE_SRC_COMMON_UNITCONVERSIONS_H
#include <stdio.h>
#include <algorithm>
#include <clocale>
#include <cstring>
#include <fmt/core.h>

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
** Locales are kinda units aren't they? Anyway thanks to @falktx for this
** handy little guard class, as shown in issue #1900
*/
namespace Surge
{
class ScopedLocale
{
  public:
    ScopedLocale() noexcept
#if WINDOWS
        : locale(::_strdup(::setlocale(LC_NUMERIC, nullptr)))
#else
        : locale(::strdup(::setlocale(LC_NUMERIC, nullptr)))
#endif
    {
        ::setlocale(LC_NUMERIC, "C");
    }

    ~ScopedLocale() noexcept
    {
        if (locale != nullptr)
        {
            ::setlocale(LC_NUMERIC, locale);
            ::free(locale);
        }
    }

  private:
    char *const locale;
};
} // namespace Surge

#endif // SURGE_SRC_COMMON_UNITCONVERSIONS_H
