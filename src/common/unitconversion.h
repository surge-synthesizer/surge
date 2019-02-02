#include <stdio.h>
#include <algorithm>

static float env_phasemulti = 1000 / 44100.f;
static float lfo_range = 1000;

#if MAC
#define min(x, y) std::min(x, y)
#define max(x, y) std::max(x, y)
#endif

inline float lfo_phaseincrement(int samples, float rate)
{
   rate = 1 - rate;
   return samples * env_phasemulti / (1 + lfo_range * rate * rate * rate);
}

inline float dB_to_scamp(float in) // ff rev2
{
   float v = powf(10.f, -0.05f * in);
   v = max(0.f, v);
   v = min(1.f, v);
   return v;
}

inline double linear_to_dB(double in)
{
   return 20 * log10(in);
}

inline double dB_to_linear(double in)
{
   return pow((double)10, 0.05 * in);
}

inline float r2amp_to_dB(float in)
{
   return 0;
}

inline float timecent_to_seconds(float in)
{
   return powf(2, in / 1200);
}

inline float seconds_to_envtime(float in) // ff rev2
{
   float v = powf(in / 30.f, 1.f / 3.f);
   v = max(0.f, v);
   v = min(1.f, v);
   return v;
}

inline char* get_notename(char* s, int i_value)
{
   int octave = (i_value / 12) - 2;
   char notenames[12][3] = {"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "};
   sprintf(s, "%s%i", notenames[i_value % 12], octave);
   return s;
}

inline float timecent_to_envtime(float in)
{
   // return seconds_to_envtime(timecent_to_seconds(in));
   return (in / 1200.f);
}
