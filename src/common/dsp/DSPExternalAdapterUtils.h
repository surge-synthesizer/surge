/*
 * API points we need for the rack port, basically, because of the somewhat
 * screwy make system
 */

#ifndef SURGE_DSPEXTERNALADAPTERUTILS_H
#define SURGE_DSPEXTERNALADAPTERUTILS_H

#include <vector>
#include <sst/filters.h>
namespace surge
{
std::pair<std::vector<float>, std::vector<float>>
calculateFilterResponseCurve(sst::filters::FilterType type, sst::filters::FilterSubType subtype,
                             float cutoff, float resonance, float gain);
}

#endif // RACK_HACK_DSPEXTERNALADAPTERUTILS_H
