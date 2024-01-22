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

    std::string defaultPath = "";
    if (loadAllPatches)
    {
        try
        {
            auto pt = fs::path{"resources/data/patches_factory"};
            if (fs::exists(pt) && fs::is_directory(pt))
            {
                defaultPath = "resources/data";
            }
        }
        catch (const fs::filesystem_error &)
        {
        }
    }
    auto surge = std::shared_ptr<SurgeSynthesizer>(new SurgeSynthesizer(
        parent.get(), loadAllPatches ? defaultPath : SurgeStorage::skipPatchLoadDataPathSentinel));
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
