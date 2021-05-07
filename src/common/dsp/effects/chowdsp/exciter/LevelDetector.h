#pragma once

#include <cmath>
#include "DSPUtils.h"

namespace chowdsp
{

/*
** Simple level detector using a ballistic filter,
** with a diode rectifier characteristic.
*/
class LevelDetector
{
  public:
    LevelDetector() = default;

    void reset(double sample_rate);
    void set_attack_time(float time_ms);
    void set_release_time(float time_ms);

    inline float process_sample(float x, size_t ch) noexcept
    {
        x = diode_rect(x);
        auto tau = x > z[ch] ? tau_attack : tau_release;
        z[ch] = x + tau * (z[ch] - x);
        return z[ch];
    }

  private:
    float calc_time_constant(float time_ms);

    inline float diode_rect(float x) const noexcept
    {
        constexpr float alpha = 0.05f / 0.0259f;
        constexpr float beta = 0.2f;
        return 25.0f * beta * (std::exp(alpha * x) - 1.0f);
    }

    float tau_attack = 0.0f;
    float tau_release = 0.0f;
    float exp_factor = 0.0f;

    float z[2];
};

} // namespace chowdsp
