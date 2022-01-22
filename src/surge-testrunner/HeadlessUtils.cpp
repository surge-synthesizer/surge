#include "HeadlessUtils.h"
#include "HeadlessPluginLayerProxy.h"

#include <iostream>
#include <iomanip>

namespace Surge
{
namespace Headless
{
static std::unique_ptr<HeadlessPluginLayerProxy> parent = nullptr;
std::shared_ptr<SurgeSynthesizer> createSurge(int sr, bool loadAllPatches)
{
    if (parent.get() == nullptr)
        parent.reset(new HeadlessPluginLayerProxy());
    auto surge = std::shared_ptr<SurgeSynthesizer>(new SurgeSynthesizer(
        parent.get(), loadAllPatches ? "" : SurgeStorage::skipPatchLoadDataPathSentinel));
    surge->setSamplerate(sr);
    surge->time_data.tempo = 120;
    surge->time_data.ppqPos = 0;
    return surge;
}

void writeToStream(const float *data, int nSamples, int nChannels, std::ostream &str)
{
    int overSample = 8;
    float avgOut = 0;
    for (auto i = 0; i < nSamples; i += nChannels)
    {
        for (auto j = 0; j < nChannels; ++j)
            avgOut += data[i + j] / nChannels;
        if ((i % overSample) == 0)
        {
            avgOut /= overSample;
            int gWidth = (int)((avgOut + 1) * 40);
            str << "Sample: " << std::setw(15) << avgOut << std::setw(gWidth) << "X" << std::endl;

            avgOut = 0;
        }
    }
}

} // namespace Headless
} // namespace Surge
