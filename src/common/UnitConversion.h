#pragma once
#include <stdio.h>
#include <algorithm>
#include <clocale>

static float env_phasemulti = 1000 / 44100.f;
static float lfo_range = 1000;

inline float lfo_phaseincrement(int samples, float rate)
{
    rate = 1 - rate;
    return samples * env_phasemulti / (1 + lfo_range * rate * rate * rate);
}

inline float dB_to_scamp(float in) // ff rev2
{
    float v = powf(10.f, -0.05f * in);
    v = std::max(0.f, v);
    v = std::min(1.f, v);
    return v;
}

inline double linear_to_dB(double in) { return 20 * log10(in); }

inline double dB_to_linear(double in) { return pow((double)10, 0.05 * in); }

inline float r2amp_to_dB(float in) { return 0; }

inline float timecent_to_seconds(float in) { return powf(2, in / 1200); }

inline float seconds_to_envtime(float in) // ff rev2
{
    float v = powf(in / 30.f, 1.f / 3.f);
    v = std::max(0.f, v);
    v = std::min(1.f, v);
    return v;
}

inline char *get_notename(char *s, int i_value, int i_offset)
{
    int ival = i_value < 0 ? i_value - 11 : i_value;
    int octave = (ival / 12) - i_offset;
    char notenames[13][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C"};
    // make sure to read the notenames array in reverse for negative i_values
    int note = (i_value < 0) ? (12 - abs(i_value % 12)) : (i_value % 12);

    sprintf(s, "%s%i", notenames[note], octave);
    return s;
}

inline float timecent_to_envtime(float in)
{
    // return seconds_to_envtime(timecent_to_seconds(in));
    return (in / 1200.f);
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
