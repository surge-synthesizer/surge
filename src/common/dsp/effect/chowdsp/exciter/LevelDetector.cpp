#include "LevelDetector.h"

namespace chowdsp
{

void LevelDetector::reset(double sample_rate)
{
    exp_factor = -2.0 * M_PI * 1000.0 / sample_rate;

    // zero state
    z[0] = 0.0f;
    z[1] = 0.0f;
}

void LevelDetector::set_attack_time(float time_ms) { tau_attack = calc_time_constant(time_ms); }

void LevelDetector::set_release_time(float time_ms) { tau_release = calc_time_constant(time_ms); }

float LevelDetector::calc_time_constant(float time_ms)
{
    return time_ms < 1.0e-3f ? 0.f : std::exp(exp_factor / time_ms);
}

} // namespace chowdsp
