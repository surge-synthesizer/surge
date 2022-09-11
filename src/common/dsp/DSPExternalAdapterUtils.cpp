//
// Created by Paul Walker on 9/11/22.
//

#include "DSPExternalAdapterUtils.h"
#include "sst/filters/FilterPlotter.h"

namespace surge
{
std::pair<std::vector<float>, std::vector<float>>
calculateFilterResponseCurve(sst::filters::FilterType type, sst::filters::FilterSubType subtype,
                             float cutoff, float resonance, float gain)
{
    auto fp = sst::filters::FilterPlotter(15);
    auto par = sst::filters::FilterPlotParameters();
    par.inputAmplitude *= gain;
    auto data = fp.plotFilterMagnitudeResponse(type, subtype, cutoff, resonance, par);
    return data;
}

} // namespace surge