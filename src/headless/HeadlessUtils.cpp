#include "HeadlessUtils.h"
#include "HeadlessPluginLayerProxy.h"

#include <iostream>
#include <iomanip>

#if LIBSNDFILE
#include <sndfile.h>
#endif

namespace Surge
{
namespace Headless
{
static std::unique_ptr<HeadlessPluginLayerProxy> parent = nullptr;
std::shared_ptr<SurgeSynthesizer> createSurge(int sr)
{
    if (parent.get() == nullptr)
        parent.reset(new HeadlessPluginLayerProxy());
    auto surge = std::shared_ptr<SurgeSynthesizer>(new SurgeSynthesizer(parent.get()));
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

void writeToWav(const float *data, int nSamples, int nChannels, float sampleRate,
                std::string wavFileName)
{
#if LIBSNDFILE
    SF_INFO sfinfo;
    sfinfo.channels = nChannels;
    sfinfo.samplerate = sampleRate;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE *of = sf_open(wavFileName.c_str(), SFM_WRITE, &sfinfo);
    sf_count_t count = sf_write_float(of, &data[0], nSamples * nChannels);
    sf_write_sync(of);
    sf_close(of);
#else
    std::cout << "writeToWav requires libsndfile which is not available on your platform."
              << std::endl;
#endif
}

} // namespace Headless
} // namespace Surge
